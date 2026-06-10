#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <low_speed_av_interfaces/msg/trajectory.hpp>

#include "low_speed_av_planning/roadnet_loader.hpp"

namespace low_speed_av_simulation {
namespace {

struct ReplayPose {
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  double s{0.0};
};

geometry_msgs::msg::Quaternion quaternion_from_yaw(double yaw)
{
  geometry_msgs::msg::Quaternion q;
  q.x = 0.0;
  q.y = 0.0;
  q.z = std::sin(yaw * 0.5);
  q.w = std::cos(yaw * 0.5);
  return q;
}

double distance(const ReplayPose & a, const ReplayPose & b)
{
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return std::hypot(dx, dy);
}

bool finite_pose(const ReplayPose & p)
{
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.yaw);
}

}  // namespace

class SimLocalizationPosePublisherNode : public rclcpp::Node {
public:
  SimLocalizationPosePublisherNode()
  : rclcpp::Node("sim_localization_pose_publisher")
  {
    declare_parameter<std::string>("roadnet.package_path", "");
    declare_parameter<bool>("roadnet.reject_failed_validation", true);
    declare_parameter<bool>("roadnet.verify_checksums", true);
    declare_parameter<double>("publish_rate_hz", 20.0);
    declare_parameter<std::string>("frame_id", "map");
    declare_parameter<std::string>("mode", "fixed_pose");
    declare_parameter<double>("initial_x", 0.554);
    declare_parameter<double>("initial_y", 1.473);
    declare_parameter<double>("initial_yaw", -0.9178);
    declare_parameter<double>("speed_mps", 0.5);
    declare_parameter<bool>("loop", true);
    declare_parameter<bool>("start_paused", false);
    declare_parameter<std::string>("trajectory_topic", "/planning/trajectory");
    declare_parameter<std::string>("pose_topic", "/localization/pose");

    mode_ = get_parameter("mode").as_string();
    frame_id_ = get_parameter("frame_id").as_string();
    speed_mps_ = std::max(0.0, get_parameter("speed_mps").as_double());
    loop_ = get_parameter("loop").as_bool();
    paused_ = get_parameter("start_paused").as_bool();

    pose_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(
      get_parameter("pose_topic").as_string(), 10);
    trajectory_sub_ = create_subscription<low_speed_av_interfaces::msg::Trajectory>(
      get_parameter("trajectory_topic").as_string(), 10,
      [this](low_speed_av_interfaces::msg::Trajectory::SharedPtr msg) {
        on_trajectory(*msg);
      });
    start_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/start",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        paused_ = false;
        response->success = true;
        response->message = "simulated localization started";
      });
    pause_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/pause",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        paused_ = true;
        response->success = true;
        response->message = "simulated localization paused";
      });
    reset_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/reset",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        replay_distance_m_ = 0.0;
        last_tick_time_ = now();
        response->success = true;
        response->message = "simulated localization reset";
      });

    load_roadnet_waypoints();
    last_tick_time_ = now();
    const double hz = std::max(1.0, get_parameter("publish_rate_hz").as_double());
    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / hz),
      [this]() {
        on_timer();
      });
  }

private:
  void load_roadnet_waypoints()
  {
    const auto package_path = get_parameter("roadnet.package_path").as_string();
    if (package_path.empty()) {
      return;
    }
    try {
      low_speed_av_planning::RoadnetLoader::Options options;
      options.reject_failed_validation = get_parameter("roadnet.reject_failed_validation").as_bool();
      options.verify_checksums = get_parameter("roadnet.verify_checksums").as_bool();
      const auto package = loader_.load(package_path, options);
      roadnet_waypoints_.clear();
      for (const auto & wp : package.waypoints) {
        ReplayPose pose;
        pose.x = wp.x_m;
        pose.y = wp.y_m;
        pose.yaw = wp.yaw_rad;
        pose.s = wp.route_s_m > 0.0 ? wp.route_s_m : wp.edge_s_m;
        if (finite_pose(pose)) {
          roadnet_waypoints_.push_back(pose);
        }
      }
      RCLCPP_INFO(get_logger(), "loaded %zu roadnet replay waypoints", roadnet_waypoints_.size());
    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), "failed to load roadnet waypoints for simulation: %s", e.what());
    }
  }

  void on_trajectory(const low_speed_av_interfaces::msg::Trajectory & msg)
  {
    trajectory_points_.clear();
    for (const auto & point : msg.points) {
      ReplayPose pose;
      pose.x = point.x_m;
      pose.y = point.y_m;
      pose.yaw = point.yaw_rad;
      pose.s = point.s_m;
      if (finite_pose(pose)) {
        trajectory_points_.push_back(pose);
      }
    }
    if (!trajectory_points_.empty()) {
      replay_distance_m_ = 0.0;
      RCLCPP_INFO(get_logger(), "trajectory replay received %zu points", trajectory_points_.size());
    }
  }

  void on_timer()
  {
    if (paused_) {
      return;
    }
    const auto current_time = now();
    const double dt = std::max(0.0, (current_time - last_tick_time_).seconds());
    last_tick_time_ = current_time;

    ReplayPose pose;
    if (mode_ == "fixed_pose") {
      pose.x = get_parameter("initial_x").as_double();
      pose.y = get_parameter("initial_y").as_double();
      pose.yaw = get_parameter("initial_yaw").as_double();
    } else if (mode_ == "trajectory_replay") {
      replay_distance_m_ += speed_mps_ * dt;
      if (!sample_replay_pose(trajectory_points_, &pose)) {
        if (!sample_replay_pose(roadnet_waypoints_, &pose)) {
          return;
        }
      }
    } else if (mode_ == "roadnet_waypoint_replay") {
      replay_distance_m_ += speed_mps_ * dt;
      if (!sample_replay_pose(roadnet_waypoints_, &pose)) {
        return;
      }
    } else {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "unknown simulation pose mode: %s", mode_.c_str());
      return;
    }

    if (!finite_pose(pose)) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "simulated pose is not finite");
      return;
    }

    geometry_msgs::msg::PoseStamped msg;
    msg.header.stamp = current_time;
    msg.header.frame_id = frame_id_;
    msg.pose.position.x = pose.x;
    msg.pose.position.y = pose.y;
    msg.pose.position.z = 0.0;
    msg.pose.orientation = quaternion_from_yaw(pose.yaw);
    pose_pub_->publish(msg);
  }

  bool sample_replay_pose(const std::vector<ReplayPose> & poses, ReplayPose * output)
  {
    if (!output || poses.empty()) {
      return false;
    }
    if (poses.size() == 1U) {
      *output = poses.front();
      return true;
    }

    double total_length = 0.0;
    for (std::size_t i = 1; i < poses.size(); ++i) {
      total_length += distance(poses[i - 1], poses[i]);
    }
    if (total_length <= 1e-6) {
      *output = poses.front();
      return true;
    }
    if (replay_distance_m_ > total_length) {
      if (loop_) {
        replay_distance_m_ = std::fmod(replay_distance_m_, total_length);
      } else {
        replay_distance_m_ = total_length;
        paused_ = true;
      }
    }

    double accumulated = 0.0;
    for (std::size_t i = 1; i < poses.size(); ++i) {
      const double segment = distance(poses[i - 1], poses[i]);
      if (accumulated + segment >= replay_distance_m_) {
        const double ratio = segment > 1e-6 ? (replay_distance_m_ - accumulated) / segment : 0.0;
        output->x = poses[i - 1].x + (poses[i].x - poses[i - 1].x) * ratio;
        output->y = poses[i - 1].y + (poses[i].y - poses[i - 1].y) * ratio;
        output->yaw = poses[i - 1].yaw;
        if (!std::isfinite(output->yaw)) {
          output->yaw = std::atan2(poses[i].y - poses[i - 1].y, poses[i].x - poses[i - 1].x);
        }
        return true;
      }
      accumulated += segment;
    }
    *output = poses.back();
    return true;
  }

  low_speed_av_planning::RoadnetLoader loader_;
  std::string mode_{"fixed_pose"};
  std::string frame_id_{"map"};
  double speed_mps_{0.5};
  bool loop_{true};
  bool paused_{false};
  double replay_distance_m_{0.0};
  rclcpp::Time last_tick_time_;
  std::vector<ReplayPose> trajectory_points_;
  std::vector<ReplayPose> roadnet_waypoints_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::Trajectory>::SharedPtr trajectory_sub_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr pause_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_srv_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace low_speed_av_simulation

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<low_speed_av_simulation::SimLocalizationPosePublisherNode>());
  rclcpp::shutdown();
  return 0;
}
