#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/path.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <low_speed_av_interfaces/msg/module_status.hpp>
#include <low_speed_av_interfaces/msg/trajectory.hpp>

#include "low_speed_av_planning/roadnet_loader.hpp"

namespace low_speed_av_simulation {
namespace {

struct ReplayPose {
  double x{0.0};
  double y{0.0};
  double yaw{0.0};
  double s{0.0};
  double speed_mps{0.0};
  int gear{1};
  std::string waypoint_id;
  std::string edge_id;
  std::string behavior;
};

struct PathState {
  std::vector<ReplayPose> points;
  std::string source;
  std::string trajectory_id;
  std::string signature;
  std::string status;
  bool emergency_stop{false};
  bool valid{false};
  bool arrived{false};
  double total_length_m{0.0};
};

double normalize_angle(double angle)
{
  constexpr double pi = 3.14159265358979323846;
  while (angle > pi) {
    angle -= 2.0 * pi;
  }
  while (angle < -pi) {
    angle += 2.0 * pi;
  }
  return angle;
}

geometry_msgs::msg::Quaternion quaternion_from_yaw(double yaw)
{
  geometry_msgs::msg::Quaternion q;
  q.x = 0.0;
  q.y = 0.0;
  q.z = std::sin(yaw * 0.5);
  q.w = std::cos(yaw * 0.5);
  const double norm = std::sqrt(q.z * q.z + q.w * q.w);
  if (norm > 1e-12) {
    q.z /= norm;
    q.w /= norm;
  }
  return q;
}

double distance(const ReplayPose & a, const ReplayPose & b)
{
  return std::hypot(a.x - b.x, a.y - b.y);
}

bool finite_pose(const ReplayPose & p)
{
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.yaw);
}

bool is_failure_stop_path(const low_speed_av_interfaces::msg::Trajectory & msg)
{
  if (msg.emergency_stop) {
    return true;
  }
  const auto status = msg.status;
  if (status.find("failure") != std::string::npos || status.find("emergency") != std::string::npos) {
    return true;
  }
  return std::any_of(msg.points.begin(), msg.points.end(), [](const auto & point) {
    return point.behavior.find("failure_stop") != std::string::npos ||
      point.behavior.find("emergency_stop") != std::string::npos;
  });
}

std::string compact_double(double value)
{
  std::ostringstream oss;
  oss.precision(4);
  oss << std::fixed << value;
  return oss.str();
}

}  // namespace

class SimLocalizationPosePublisherNode : public rclcpp::Node {
public:
  SimLocalizationPosePublisherNode()
  : rclcpp::Node("sim_localization_pose_publisher")
  {
    declare_parameters();
    read_parameters();
    load_roadnet_package();
    reset_to_initial_pose();

    pose_pub_ = create_publisher<geometry_msgs::msg::PoseStamped>(pose_topic_, 10);
    status_pub_ = create_publisher<low_speed_av_interfaces::msg::ModuleStatus>("/simulation/status", 10);
    pose_path_pub_ = create_publisher<nav_msgs::msg::Path>("/simulation/pose_path", 10);

    full_reference_sub_ = create_subscription<low_speed_av_interfaces::msg::Trajectory>(
      full_reference_path_topic_, 10,
      [this](low_speed_av_interfaces::msg::Trajectory::SharedPtr msg) {
        on_path(*msg, "full_reference_path");
      });
    trajectory_sub_ = create_subscription<low_speed_av_interfaces::msg::Trajectory>(
      trajectory_topic_, 10,
      [this](low_speed_av_interfaces::msg::Trajectory::SharedPtr msg) {
        on_path(*msg, "trajectory");
      });

    start_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/start",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        paused_ = false;
        response->success = true;
        response->message = "simulated localization started";
        publish_status("active", "start requested");
      });
    pause_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/pause",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        paused_ = true;
        response->success = true;
        response->message = "simulated localization paused";
        publish_status("paused", "pause requested");
      });
    reset_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/reset",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        reset_to_initial_pose();
        if (reset_clears_path_) {
          active_path_ = PathState{};
        }
        response->success = true;
        response->message = "simulated localization reset";
        publish_status("reset", response->message);
      });
    rewind_srv_ = create_service<std_srvs::srv::Trigger>(
      "/simulation/rewind_path",
      [this](
        const std::shared_ptr<std_srvs::srv::Trigger::Request>,
        std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
        path_progress_m_ = 0.0;
        active_path_.arrived = false;
        response->success = true;
        response->message = "simulated path progress rewound";
        publish_status("active", response->message);
      });

    last_tick_time_ = now();
    const double hz = std::max(1.0, publish_rate_hz_);
    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / hz),
      [this]() {
        on_timer();
      });
  }

private:
  void declare_parameters()
  {
    declare_parameter<std::string>("roadnet.package_path", "");
    declare_parameter<bool>("roadnet.reject_failed_validation", true);
    declare_parameter<bool>("roadnet.verify_checksums", true);

    declare_parameter<bool>("simulation.localization.enabled", true);
    declare_parameter<std::string>("simulation.localization.pose_topic", "/localization/pose");
    declare_parameter<double>("simulation.localization.publish_rate_hz", 20.0);
    declare_parameter<std::string>("simulation.localization.frame_id", "map");
    declare_parameter<std::string>("simulation.localization.mode", "path_follow");
    declare_parameter<std::string>("simulation.localization.initial_pose.source", "explicit");
    declare_parameter<double>("simulation.localization.initial_pose.x", 0.554);
    declare_parameter<double>("simulation.localization.initial_pose.y", 1.473);
    declare_parameter<double>("simulation.localization.initial_pose.yaw", -0.9178);
    declare_parameter<std::string>("simulation.localization.initial_pose.waypoint_id", "");
    declare_parameter<std::string>("simulation.localization.initial_pose.task_point_id", "");
    declare_parameter<std::string>("simulation.localization.initial_pose.edge_id", "");
    declare_parameter<double>("simulation.localization.initial_pose.edge_progress", 0.0);
    declare_parameter<std::string>("simulation.localization.follow.follow_source", "full_reference_path");
    declare_parameter<bool>("simulation.localization.follow.fallback_to_local_trajectory", true);
    declare_parameter<bool>("simulation.localization.follow.reanchor_on_new_path", true);
    declare_parameter<bool>("simulation.localization.follow.restart_on_new_path", false);
    declare_parameter<bool>("simulation.localization.follow.use_trajectory_speed", true);
    declare_parameter<double>("simulation.localization.follow.default_speed_mps", 1.0);
    declare_parameter<double>("simulation.localization.follow.max_speed_mps", 1.0);
    declare_parameter<double>("simulation.localization.follow.acceleration_limit_mps2", 0.5);
    declare_parameter<bool>("simulation.localization.follow.stop_at_goal", true);
    declare_parameter<double>("simulation.localization.follow.goal_tolerance_m", 0.3);
    declare_parameter<double>("simulation.localization.follow.yaw_tolerance_rad", 0.35);
    declare_parameter<bool>("simulation.localization.follow.hold_on_failure_stop", true);
    declare_parameter<bool>("simulation.localization.follow.hold_when_no_path", true);
    declare_parameter<bool>("simulation.localization.follow.ignore_republished_same_path", true);
    declare_parameter<std::string>("simulation.localization.follow.full_reference_path_topic", "/planning/full_reference_path");
    declare_parameter<std::string>("simulation.localization.follow.trajectory_topic", "/planning/trajectory");

    // Legacy flat parameters kept for existing launch files and command lines.
    declare_parameter<double>("publish_rate_hz", 20.0);
    declare_parameter<std::string>("frame_id", "map");
    declare_parameter<std::string>("mode", "path_follow");
    declare_parameter<double>("initial_x", 0.554);
    declare_parameter<double>("initial_y", 1.473);
    declare_parameter<double>("initial_yaw", -0.9178);
    declare_parameter<std::string>("initial_source", "");
    declare_parameter<std::string>("initial_waypoint_id", "");
    declare_parameter<std::string>("initial_task_point_id", "");
    declare_parameter<std::string>("initial_edge_id", "");
    declare_parameter<double>("initial_edge_progress", 0.0);
    declare_parameter<double>("speed_mps", 0.5);
    declare_parameter<bool>("loop", true);
    declare_parameter<bool>("start_paused", false);
    declare_parameter<std::string>("trajectory_topic", "/planning/trajectory");
    declare_parameter<std::string>("full_reference_path_topic", "/planning/full_reference_path");
    declare_parameter<std::string>("pose_topic", "/localization/pose");
    declare_parameter<bool>("reset_clears_path", true);
  }

  void read_parameters()
  {
    enabled_ = get_parameter("simulation.localization.enabled").as_bool();
    mode_ = prefer_string("simulation.localization.mode", "mode", "path_follow");
    frame_id_ = prefer_string("simulation.localization.frame_id", "frame_id", "map");
    pose_topic_ = prefer_string("simulation.localization.pose_topic", "pose_topic", "/localization/pose");
    publish_rate_hz_ = prefer_double("simulation.localization.publish_rate_hz", "publish_rate_hz", 20.0);
    initial_source_ = prefer_string("simulation.localization.initial_pose.source", "initial_source", "");
    if (initial_source_.empty()) {
      initial_source_ = "explicit";
    }
    initial_x_ = prefer_double("simulation.localization.initial_pose.x", "initial_x", 0.554);
    initial_y_ = prefer_double("simulation.localization.initial_pose.y", "initial_y", 1.473);
    initial_yaw_ = prefer_double("simulation.localization.initial_pose.yaw", "initial_yaw", -0.9178);
    initial_waypoint_id_ = prefer_string(
      "simulation.localization.initial_pose.waypoint_id", "initial_waypoint_id", "");
    initial_task_point_id_ = prefer_string(
      "simulation.localization.initial_pose.task_point_id", "initial_task_point_id", "");
    initial_edge_id_ = prefer_string("simulation.localization.initial_pose.edge_id", "initial_edge_id", "");
    initial_edge_progress_ =
      prefer_double("simulation.localization.initial_pose.edge_progress", "initial_edge_progress", 0.0);
    full_reference_path_topic_ =
      prefer_string(
      "simulation.localization.follow.full_reference_path_topic", "full_reference_path_topic",
      "/planning/full_reference_path");
    trajectory_topic_ =
      prefer_string("simulation.localization.follow.trajectory_topic", "trajectory_topic", "/planning/trajectory");
    follow_source_ = get_parameter("simulation.localization.follow.follow_source").as_string();
    if (mode_ == "trajectory_replay" && follow_source_ == "full_reference_path") {
      follow_source_ = "trajectory";
    }
    fallback_to_local_trajectory_ =
      get_parameter("simulation.localization.follow.fallback_to_local_trajectory").as_bool();
    reanchor_on_new_path_ = get_parameter("simulation.localization.follow.reanchor_on_new_path").as_bool();
    restart_on_new_path_ = get_parameter("simulation.localization.follow.restart_on_new_path").as_bool();
    use_trajectory_speed_ = get_parameter("simulation.localization.follow.use_trajectory_speed").as_bool();
    default_speed_mps_ = get_parameter("simulation.localization.follow.default_speed_mps").as_double();
    max_speed_mps_ = get_parameter("simulation.localization.follow.max_speed_mps").as_double();
    acceleration_limit_mps2_ = get_parameter("simulation.localization.follow.acceleration_limit_mps2").as_double();
    stop_at_goal_ = get_parameter("simulation.localization.follow.stop_at_goal").as_bool();
    goal_tolerance_m_ = get_parameter("simulation.localization.follow.goal_tolerance_m").as_double();
    yaw_tolerance_rad_ = get_parameter("simulation.localization.follow.yaw_tolerance_rad").as_double();
    hold_on_failure_stop_ = get_parameter("simulation.localization.follow.hold_on_failure_stop").as_bool();
    hold_when_no_path_ = get_parameter("simulation.localization.follow.hold_when_no_path").as_bool();
    ignore_republished_same_path_ =
      get_parameter("simulation.localization.follow.ignore_republished_same_path").as_bool();
    legacy_speed_mps_ = std::max(0.0, get_parameter("speed_mps").as_double());
    loop_ = get_parameter("loop").as_bool();
    paused_ = get_parameter("start_paused").as_bool();
    reset_clears_path_ = get_parameter("reset_clears_path").as_bool();
  }

  std::string prefer_string(
    const std::string & preferred,
    const std::string & legacy,
    const std::string & legacy_default) const
  {
    const auto preferred_value = get_parameter(preferred).as_string();
    const auto legacy_value = get_parameter(legacy).as_string();
    if (legacy_value != legacy_default) {
      return legacy_value;
    }
    return preferred_value.empty() ? legacy_value : preferred_value;
  }

  double prefer_double(const std::string & preferred, const std::string & legacy, double legacy_default) const
  {
    const auto legacy_value = get_parameter(legacy).as_double();
    if (std::fabs(legacy_value - legacy_default) > 1e-9) {
      return legacy_value;
    }
    return get_parameter(preferred).as_double();
  }

  void load_roadnet_package()
  {
    const auto package_path = get_parameter("roadnet.package_path").as_string();
    if (package_path.empty()) {
      return;
    }
    try {
      low_speed_av_planning::RoadnetLoader::Options options;
      options.reject_failed_validation = get_parameter("roadnet.reject_failed_validation").as_bool();
      options.verify_checksums = get_parameter("roadnet.verify_checksums").as_bool();
      roadnet_package_ = loader_.load(package_path, options);
      roadnet_waypoints_.clear();
      for (const auto & wp : roadnet_package_->waypoints) {
        ReplayPose pose;
        pose.x = wp.x_m;
        pose.y = wp.y_m;
        pose.yaw = wp.yaw_rad;
        pose.s = wp.route_s_m > 0.0 ? wp.route_s_m : wp.edge_s_m;
        pose.speed_mps = wp.target_speed_mps;
        pose.gear = wp.gear;
        pose.waypoint_id = wp.waypoint_id;
        pose.edge_id = wp.edge_id;
        pose.behavior = wp.behavior;
        if (finite_pose(pose)) {
          roadnet_waypoints_.push_back(pose);
        }
      }
      RCLCPP_INFO(get_logger(), "loaded %zu roadnet replay waypoints", roadnet_waypoints_.size());
    } catch (const std::exception & e) {
      RCLCPP_ERROR(get_logger(), "failed to load roadnet waypoints for simulation: %s", e.what());
    }
  }

  void reset_to_initial_pose()
  {
    current_pose_ = resolve_initial_pose();
    path_progress_m_ = 0.0;
    current_speed_mps_ = 0.0;
    active_path_.arrived = false;
    pose_history_.clear();
    append_pose_history(current_pose_, now());
  }

  ReplayPose resolve_initial_pose() const
  {
    if (roadnet_package_) {
      if (initial_source_ == "waypoint" && !initial_waypoint_id_.empty()) {
        for (const auto & wp : roadnet_package_->waypoints) {
          if (wp.waypoint_id == initial_waypoint_id_) {
            return {wp.x_m, wp.y_m, wp.yaw_rad, wp.edge_s_m, wp.target_speed_mps, wp.gear, wp.waypoint_id, wp.edge_id,
              wp.behavior};
          }
        }
        RCLCPP_WARN(get_logger(), "initial waypoint was not found: %s", initial_waypoint_id_.c_str());
      }
      if (initial_source_ == "task_point" && !initial_task_point_id_.empty()) {
        if (const auto pose = semantic_initial_pose(initial_task_point_id_)) {
          return *pose;
        }
        RCLCPP_WARN(get_logger(), "initial semantic point was not found: %s", initial_task_point_id_.c_str());
      }
      if (initial_source_ == "edge_progress" && !initial_edge_id_.empty()) {
        if (const auto pose = edge_progress_initial_pose(initial_edge_id_, initial_edge_progress_)) {
          return *pose;
        }
        RCLCPP_WARN(get_logger(), "initial edge/progress was invalid: %s", initial_edge_id_.c_str());
      }
      if (!roadnet_waypoints_.empty() && initial_source_ != "explicit") {
        RCLCPP_WARN(
          get_logger(), "invalid initial_pose source '%s'; falling back to first roadnet waypoint",
          initial_source_.c_str());
        return roadnet_waypoints_.front();
      }
    }
    return {initial_x_, initial_y_, initial_yaw_, 0.0, 0.0, 1, "initial_explicit", "", "fixed_pose"};
  }

  std::optional<ReplayPose> semantic_initial_pose(const std::string & id) const
  {
    if (!roadnet_package_) {
      return std::nullopt;
    }
    const auto make_pose = [&](const auto & point) {
      return ReplayPose{point.pose.x_m, point.pose.y_m, point.pose.yaw_rad, 0.0, 0.0, 1, point.id,
        point.linked_edge_id, "semantic_initial_pose"};
    };
    if (const auto it = roadnet_package_->task_points.find(id); it != roadnet_package_->task_points.end()) {
      return make_pose(it->second);
    }
    if (const auto it = roadnet_package_->parking_points.find(id); it != roadnet_package_->parking_points.end()) {
      return make_pose(it->second);
    }
    if (const auto it = roadnet_package_->charging_points.find(id); it != roadnet_package_->charging_points.end()) {
      return make_pose(it->second);
    }
    return std::nullopt;
  }

  std::optional<ReplayPose> edge_progress_initial_pose(const std::string & edge_id, double progress) const
  {
    if (!roadnet_package_) {
      return std::nullopt;
    }
    const auto range_it = roadnet_package_->waypoint_index_by_edge.find(edge_id);
    if (range_it == roadnet_package_->waypoint_index_by_edge.end() || range_it->second.count == 0U ||
      roadnet_package_->waypoints.empty())
    {
      return std::nullopt;
    }
    const double clamped = std::clamp(progress, 0.0, 1.0);
    const auto offset = static_cast<std::size_t>(
      std::round(clamped * static_cast<double>(range_it->second.count - 1U)));
    const auto index = std::min(range_it->second.start_index + offset, roadnet_package_->waypoints.size() - 1U);
    const auto & wp = roadnet_package_->waypoints[index];
    return ReplayPose{wp.x_m, wp.y_m, wp.yaw_rad, wp.edge_s_m, wp.target_speed_mps, wp.gear, wp.waypoint_id,
      wp.edge_id, "edge_progress_initial_pose"};
  }

  void on_path(const low_speed_av_interfaces::msg::Trajectory & msg, const std::string & source)
  {
    if (is_failure_stop_path(msg) && hold_on_failure_stop_) {
      active_path_ = PathState{};
      holding_failure_stop_ = true;
      current_speed_mps_ = 0.0;
      publish_status("holding_failure_stop", "trajectory reported failure_stop or emergency_stop");
      return;
    }

    if (source == "full_reference_path" && follow_source_ == "trajectory") {
      return;
    }
    if (source == "trajectory" && follow_source_ == "full_reference_path" &&
      fallback_to_local_trajectory_ && active_path_.valid && active_path_.source == "full_reference_path")
    {
      return;
    }
    if (source == "trajectory" && follow_source_ == "full_reference_path" &&
      !fallback_to_local_trajectory_ && active_path_.source != "trajectory")
    {
      return;
    }

    auto path = path_from_trajectory(msg, source);
    if (!path.valid) {
      active_path_ = std::move(path);
      current_speed_mps_ = 0.0;
      publish_status("invalid_path", "received invalid or empty path from " + source);
      return;
    }
    if (ignore_republished_same_path_ && active_path_.valid && path.signature == active_path_.signature) {
      RCLCPP_DEBUG(
        get_logger(), "ignored republished same %s path: %s", source.c_str(), path.trajectory_id.c_str());
      return;
    }

    const bool reanchored = reanchor_on_new_path_ && !restart_on_new_path_;
    const double new_progress = reanchored ? nearest_s_on_path(path.points, current_pose_) : 0.0;
    active_path_ = std::move(path);
    path_progress_m_ = std::clamp(new_progress, 0.0, active_path_.total_length_m);
    holding_failure_stop_ = false;
    std::ostringstream oss;
    oss << "new " << active_path_.source << " path accepted"
      << " trajectory_id=" << active_path_.trajectory_id
      << " points=" << active_path_.points.size()
      << " reanchor=" << (reanchored ? "true" : "false")
      << " progress_s=" << path_progress_m_;
    RCLCPP_INFO(get_logger(), "%s", oss.str().c_str());
    publish_status("following_path", oss.str());
  }

  PathState path_from_trajectory(
    const low_speed_av_interfaces::msg::Trajectory & msg,
    const std::string & source) const
  {
    PathState path;
    path.source = source;
    path.trajectory_id = msg.trajectory_id;
    path.status = msg.status;
    path.emergency_stop = msg.emergency_stop;
    if (is_failure_stop_path(msg)) {
      return path;
    }
    for (const auto & point : msg.points) {
      ReplayPose pose;
      pose.x = point.x_m;
      pose.y = point.y_m;
      pose.yaw = point.yaw_rad;
      pose.s = point.s_m;
      pose.speed_mps = point.v_mps;
      pose.gear = point.gear;
      pose.waypoint_id = point.waypoint_id;
      pose.edge_id = point.edge_id;
      pose.behavior = point.behavior;
      if (finite_pose(pose)) {
        path.points.push_back(pose);
      }
    }
    path.valid = path.points.size() >= 2U;
    path.total_length_m = compute_total_length(path.points);
    path.signature = make_path_signature(msg, path);
    return path;
  }

  std::string make_path_signature(
    const low_speed_av_interfaces::msg::Trajectory & msg,
    const PathState & path) const
  {
    if (path.points.empty()) {
      return msg.status + ":empty";
    }
    const auto & first = path.points.front();
    const auto & last = path.points.back();
    std::ostringstream oss;
    oss << msg.source_package_id << "|" << msg.status << "|" << msg.emergency_stop << "|"
      << path.points.size() << "|" << first.waypoint_id << "|" << first.edge_id << "|"
      << compact_double(first.x) << "," << compact_double(first.y) << "|"
      << last.waypoint_id << "|" << last.edge_id << "|"
      << compact_double(last.x) << "," << compact_double(last.y) << "|"
      << compact_double(path.total_length_m);
    return oss.str();
  }

  void on_timer()
  {
    const auto current_time = now();
    const double dt = std::max(0.0, (current_time - last_tick_time_).seconds());
    last_tick_time_ = current_time;

    if (!enabled_) {
      publish_status("disabled", "simulated localization disabled");
      return;
    }

    if (!paused_) {
      advance_pose(dt);
    }

    publish_pose(current_time);
    publish_pose_path(current_time);
    publish_periodic_status();
  }

  void advance_pose(double dt)
  {
    if (mode_ == "fixed_pose") {
      current_speed_mps_ = 0.0;
      return;
    }
    if (mode_ == "roadnet_waypoint_replay") {
      advance_on_points(roadnet_waypoints_, dt, loop_, "roadnet_waypoint_replay");
      return;
    }
    if (mode_ == "trajectory_replay" || mode_ == "path_follow") {
      if (holding_failure_stop_) {
        current_speed_mps_ = 0.0;
        return;
      }
      if (!active_path_.valid) {
        if (!hold_when_no_path_ && mode_ == "trajectory_replay") {
          advance_on_points(roadnet_waypoints_, dt, loop_, "roadnet_fallback");
        }
        return;
      }
      advance_on_points(active_path_.points, dt, false, active_path_.source);
      return;
    }

    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "unknown simulation pose mode: %s", mode_.c_str());
  }

  void advance_on_points(
    const std::vector<ReplayPose> & points,
    double dt,
    bool loop,
    const std::string & source)
  {
    if (points.empty()) {
      return;
    }
    if (points.size() == 1U) {
      current_pose_ = points.front();
      current_speed_mps_ = 0.0;
      return;
    }

    const double target_speed = speed_for_progress(points, path_progress_m_);
    const double speed_delta = std::clamp(
      target_speed - current_speed_mps_,
      -std::max(0.0, acceleration_limit_mps2_) * dt,
      std::max(0.0, acceleration_limit_mps2_) * dt);
    current_speed_mps_ = std::clamp(current_speed_mps_ + speed_delta, 0.0, std::max(0.0, max_speed_mps_));
    path_progress_m_ += current_speed_mps_ * std::max(0.0, dt);

    const double total = compute_total_length(points);
    if (path_progress_m_ >= total) {
      if (loop) {
        path_progress_m_ = std::fmod(path_progress_m_, std::max(total, 1e-6));
      } else {
        path_progress_m_ = total;
        current_speed_mps_ = 0.0;
        active_path_.arrived = source == active_path_.source;
      }
    }
    if (sample_pose(points, path_progress_m_, &current_pose_)) {
      append_pose_history(current_pose_, now());
    }
  }

  double speed_for_progress(const std::vector<ReplayPose> & points, double s) const
  {
    if (points.empty()) {
      return 0.0;
    }
    const std::size_t index = nearest_index_for_s(points, s);
    double speed = use_trajectory_speed_ ? points[index].speed_mps : default_speed_mps_;
    if (!std::isfinite(speed) || speed <= 1e-3) {
      speed = mode_ == "trajectory_replay" ? std::max(legacy_speed_mps_, default_speed_mps_) : default_speed_mps_;
    }
    return std::clamp(speed, 0.0, std::max(0.0, max_speed_mps_));
  }

  static double compute_total_length(const std::vector<ReplayPose> & points)
  {
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
      total += distance(points[i - 1], points[i]);
    }
    return total;
  }

  static std::size_t nearest_index_for_s(const std::vector<ReplayPose> & points, double target_s)
  {
    if (points.size() < 2U) {
      return 0U;
    }
    double accumulated = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
      accumulated += distance(points[i - 1], points[i]);
      if (accumulated >= target_s) {
        return i - 1U;
      }
    }
    return points.size() - 1U;
  }

  static double nearest_s_on_path(const std::vector<ReplayPose> & points, const ReplayPose & pose)
  {
    if (points.empty()) {
      return 0.0;
    }
    double best_s = 0.0;
    double best_distance = std::numeric_limits<double>::infinity();
    double accumulated = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
      const auto & a = points[i - 1];
      const auto & b = points[i];
      const double vx = b.x - a.x;
      const double vy = b.y - a.y;
      const double segment_length_sq = vx * vx + vy * vy;
      const double t = segment_length_sq > 1e-12 ?
        std::clamp(((pose.x - a.x) * vx + (pose.y - a.y) * vy) / segment_length_sq, 0.0, 1.0) :
        0.0;
      const double px = a.x + vx * t;
      const double py = a.y + vy * t;
      const double d = std::hypot(pose.x - px, pose.y - py);
      if (d < best_distance) {
        best_distance = d;
        best_s = accumulated + std::sqrt(segment_length_sq) * t;
      }
      accumulated += std::sqrt(segment_length_sq);
    }
    return best_s;
  }

  static bool sample_pose(const std::vector<ReplayPose> & points, double s, ReplayPose * output)
  {
    if (!output || points.empty()) {
      return false;
    }
    if (points.size() == 1U) {
      *output = points.front();
      return true;
    }

    double accumulated = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
      const double segment = distance(points[i - 1], points[i]);
      if (accumulated + segment >= s) {
        const double ratio = segment > 1e-6 ? (s - accumulated) / segment : 0.0;
        output->x = points[i - 1].x + (points[i].x - points[i - 1].x) * ratio;
        output->y = points[i - 1].y + (points[i].y - points[i - 1].y) * ratio;
        const double yaw_delta = normalize_angle(points[i].yaw - points[i - 1].yaw);
        output->yaw = points[i - 1].yaw + yaw_delta * ratio;
        if (!std::isfinite(output->yaw)) {
          output->yaw = std::atan2(points[i].y - points[i - 1].y, points[i].x - points[i - 1].x);
        }
        output->s = s;
        output->speed_mps = points[i - 1].speed_mps;
        output->gear = points[i - 1].gear;
        output->waypoint_id = points[i - 1].waypoint_id;
        output->edge_id = points[i - 1].edge_id;
        output->behavior = points[i - 1].behavior;
        return true;
      }
      accumulated += segment;
    }
    *output = points.back();
    output->s = accumulated;
    return true;
  }

  void publish_pose(const rclcpp::Time & stamp)
  {
    if (!finite_pose(current_pose_)) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "simulated pose is not finite");
      return;
    }
    geometry_msgs::msg::PoseStamped msg;
    msg.header.stamp = stamp;
    msg.header.frame_id = frame_id_;
    msg.pose.position.x = current_pose_.x;
    msg.pose.position.y = current_pose_.y;
    msg.pose.position.z = 0.0;
    msg.pose.orientation = quaternion_from_yaw(current_pose_.yaw);
    pose_pub_->publish(msg);
  }

  void append_pose_history(const ReplayPose & pose, const rclcpp::Time & stamp)
  {
    geometry_msgs::msg::PoseStamped msg;
    msg.header.stamp = stamp;
    msg.header.frame_id = frame_id_;
    msg.pose.position.x = pose.x;
    msg.pose.position.y = pose.y;
    msg.pose.position.z = 0.0;
    msg.pose.orientation = quaternion_from_yaw(pose.yaw);
    if (pose_history_.poses.empty() ||
      std::hypot(
        pose_history_.poses.back().pose.position.x - msg.pose.position.x,
        pose_history_.poses.back().pose.position.y - msg.pose.position.y) > 0.02)
    {
      pose_history_.header = msg.header;
      pose_history_.poses.push_back(msg);
      constexpr std::size_t max_history = 2000U;
      if (pose_history_.poses.size() > max_history) {
        pose_history_.poses.erase(pose_history_.poses.begin());
      }
    }
  }

  void publish_pose_path(const rclcpp::Time & stamp)
  {
    pose_history_.header.stamp = stamp;
    pose_history_.header.frame_id = frame_id_;
    pose_path_pub_->publish(pose_history_);
  }

  void publish_periodic_status()
  {
    if (paused_) {
      publish_status("paused", "simulation paused");
      return;
    }
    if (holding_failure_stop_) {
      publish_status("holding_failure_stop", "holding current pose due to failure_stop");
      return;
    }
    if (active_path_.arrived && stop_at_goal_) {
      publish_status("arrived", status_detail("arrived at path endpoint"));
      return;
    }
    if (mode_ == "path_follow" || mode_ == "trajectory_replay") {
      if (!active_path_.valid) {
        publish_status("waiting_for_path", "publishing initial/current pose while waiting for planning path");
        return;
      }
      publish_status("following_path", status_detail("following path"));
      return;
    }
    publish_status("active", "publishing simulated pose mode=" + mode_);
  }

  std::string status_detail(const std::string & prefix) const
  {
    std::ostringstream oss;
    oss << prefix << "; source=" << active_path_.source
      << "; trajectory_id=" << active_path_.trajectory_id
      << "; progress_s=" << path_progress_m_
      << "; total_s=" << active_path_.total_length_m
      << "; points=" << active_path_.points.size();
    if (!active_path_.points.empty()) {
      oss << "; endpoint_distance=" << distance(current_pose_, active_path_.points.back());
    }
    return oss.str();
  }

  void publish_status(const std::string & state, const std::string & message)
  {
    low_speed_av_interfaces::msg::ModuleStatus status;
    status.header.stamp = now();
    status.module_name = "sim_localization_pose_publisher";
    status.state = state;
    status.level = state == "invalid_path" ? 1 : 0;
    status.message = message;
    status_pub_->publish(status);
  }

  low_speed_av_planning::RoadnetLoader loader_;
  std::optional<low_speed_av_planning::RoadnetPackage> roadnet_package_;
  bool enabled_{true};
  std::string mode_{"path_follow"};
  std::string frame_id_{"map"};
  std::string pose_topic_{"/localization/pose"};
  std::string trajectory_topic_{"/planning/trajectory"};
  std::string full_reference_path_topic_{"/planning/full_reference_path"};
  std::string follow_source_{"full_reference_path"};
  std::string initial_source_{"explicit"};
  std::string initial_waypoint_id_;
  std::string initial_task_point_id_;
  std::string initial_edge_id_;
  double initial_edge_progress_{0.0};
  double initial_x_{0.554};
  double initial_y_{1.473};
  double initial_yaw_{-0.9178};
  double publish_rate_hz_{20.0};
  double default_speed_mps_{1.0};
  double max_speed_mps_{1.0};
  double acceleration_limit_mps2_{0.5};
  double goal_tolerance_m_{0.3};
  double yaw_tolerance_rad_{0.35};
  double legacy_speed_mps_{0.5};
  bool fallback_to_local_trajectory_{true};
  bool reanchor_on_new_path_{true};
  bool restart_on_new_path_{false};
  bool use_trajectory_speed_{true};
  bool stop_at_goal_{true};
  bool hold_on_failure_stop_{true};
  bool hold_when_no_path_{true};
  bool ignore_republished_same_path_{true};
  bool loop_{true};
  bool paused_{false};
  bool holding_failure_stop_{false};
  bool reset_clears_path_{true};
  double path_progress_m_{0.0};
  double current_speed_mps_{0.0};
  ReplayPose current_pose_;
  PathState active_path_;
  nav_msgs::msg::Path pose_history_;
  rclcpp::Time last_tick_time_;
  std::vector<ReplayPose> roadnet_waypoints_;
  rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr pose_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::ModuleStatus>::SharedPtr status_pub_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr pose_path_pub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::Trajectory>::SharedPtr full_reference_sub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::Trajectory>::SharedPtr trajectory_sub_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr start_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr pause_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr reset_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr rewind_srv_;
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
