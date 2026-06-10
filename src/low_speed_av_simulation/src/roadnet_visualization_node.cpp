#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/path.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/color_rgba.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>

#include <low_speed_av_interfaces/msg/global_route.hpp>
#include <low_speed_av_interfaces/msg/trajectory.hpp>

#include "low_speed_av_planning/roadnet_loader.hpp"

namespace low_speed_av_simulation {
namespace {

using low_speed_av_planning::Pose2d;
using low_speed_av_planning::RoadnetLoader;
using low_speed_av_planning::RoadnetPackage;
using low_speed_av_planning::Waypoint;
using visualization_msgs::msg::Marker;
using visualization_msgs::msg::MarkerArray;

std_msgs::msg::ColorRGBA color(float r, float g, float b, float a = 1.0F)
{
  std_msgs::msg::ColorRGBA c;
  c.r = r;
  c.g = g;
  c.b = b;
  c.a = a;
  return c;
}

geometry_msgs::msg::Point point(double x, double y, double z = 0.0)
{
  geometry_msgs::msg::Point p;
  p.x = x;
  p.y = y;
  p.z = z;
  return p;
}

geometry_msgs::msg::Quaternion quaternion_from_yaw(double yaw)
{
  geometry_msgs::msg::Quaternion q;
  q.x = 0.0;
  q.y = 0.0;
  q.z = std::sin(yaw * 0.5);
  q.w = std::cos(yaw * 0.5);
  return q;
}

Marker base_marker(
  const std::string & frame_id,
  const rclcpp::Time & stamp,
  const std::string & ns,
  int id,
  int type)
{
  Marker marker;
  marker.header.frame_id = frame_id;
  marker.header.stamp = stamp;
  marker.ns = ns;
  marker.id = id;
  marker.type = type;
  marker.action = Marker::ADD;
  marker.pose.orientation.w = 1.0;
  return marker;
}

}  // namespace

class RoadnetVisualizationNode : public rclcpp::Node {
public:
  RoadnetVisualizationNode()
  : rclcpp::Node("roadnet_visualization_node")
  {
    declare_parameter<std::string>("roadnet.package_path", "");
    declare_parameter<bool>("roadnet.reject_failed_validation", true);
    declare_parameter<bool>("roadnet.verify_checksums", true);
    declare_parameter<std::string>("frame_id", "map");
    declare_parameter<double>("publish_rate_hz", 1.0);
    declare_parameter<std::string>("topics.global_route_topic", "/planning/global_route");
    declare_parameter<std::string>("topics.trajectory_topic", "/planning/trajectory");
    declare_parameter<std::string>("topics.localization_pose_topic", "/localization/pose");
    declare_parameter<std::string>("topics.roadnet_markers", "/simulation/roadnet_markers");
    declare_parameter<std::string>("topics.route_markers", "/simulation/route_markers");
    declare_parameter<std::string>("topics.trajectory_path", "/simulation/trajectory_path");
    declare_parameter<std::string>("topics.vehicle_markers", "/simulation/vehicle_markers");

    frame_id_ = get_parameter("frame_id").as_string();
    roadnet_markers_pub_ = create_publisher<MarkerArray>(
      get_parameter("topics.roadnet_markers").as_string(), rclcpp::QoS(1).transient_local());
    route_markers_pub_ = create_publisher<MarkerArray>(
      get_parameter("topics.route_markers").as_string(), 10);
    trajectory_path_pub_ = create_publisher<nav_msgs::msg::Path>(
      get_parameter("topics.trajectory_path").as_string(), 10);
    vehicle_markers_pub_ = create_publisher<MarkerArray>(
      get_parameter("topics.vehicle_markers").as_string(), 10);

    route_sub_ = create_subscription<low_speed_av_interfaces::msg::GlobalRoute>(
      get_parameter("topics.global_route_topic").as_string(), 10,
      [this](low_speed_av_interfaces::msg::GlobalRoute::SharedPtr msg) {
        on_route(*msg);
      });
    trajectory_sub_ = create_subscription<low_speed_av_interfaces::msg::Trajectory>(
      get_parameter("topics.trajectory_topic").as_string(), 10,
      [this](low_speed_av_interfaces::msg::Trajectory::SharedPtr msg) {
        on_trajectory(*msg);
      });
    pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      get_parameter("topics.localization_pose_topic").as_string(), 10,
      [this](geometry_msgs::msg::PoseStamped::SharedPtr msg) {
        on_pose(*msg);
      });

    load_package();
    const double hz = std::max(0.1, get_parameter("publish_rate_hz").as_double());
    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / hz),
      [this]() {
        publish_roadnet_markers();
      });
  }

private:
  void load_package()
  {
    const auto package_path = get_parameter("roadnet.package_path").as_string();
    if (package_path.empty()) {
      RCLCPP_WARN(get_logger(), "roadnet.package_path is empty; visualization has no base roadnet");
      return;
    }
    try {
      RoadnetLoader::Options options;
      options.reject_failed_validation = get_parameter("roadnet.reject_failed_validation").as_bool();
      options.verify_checksums = get_parameter("roadnet.verify_checksums").as_bool();
      package_ = std::make_shared<RoadnetPackage>(loader_.load(package_path, options));
      frame_id_ = package_->global_frame.empty() ? frame_id_ : package_->global_frame;
      RCLCPP_INFO(
        get_logger(), "loaded roadnet for visualization: %zu nodes, %zu edges, %zu waypoints",
        package_->nodes.size(), package_->edges.size(), package_->waypoints.size());
    } catch (const std::exception & e) {
      package_.reset();
      RCLCPP_ERROR(get_logger(), "failed to load roadnet for visualization: %s", e.what());
    }
  }

  void publish_roadnet_markers()
  {
    if (!package_) {
      return;
    }
    MarkerArray array;
    const auto stamp = now();

    auto nodes = base_marker(frame_id_, stamp, "topology_nodes", 0, Marker::SPHERE_LIST);
    nodes.scale.x = 0.25;
    nodes.scale.y = 0.25;
    nodes.scale.z = 0.25;
    nodes.color = color(0.1F, 0.4F, 1.0F, 1.0F);
    for (const auto & node : package_->nodes) {
      nodes.points.push_back(point(node.pose.x_m, node.pose.y_m, 0.12));
    }
    array.markers.push_back(nodes);

    auto edges = base_marker(frame_id_, stamp, "topology_edges", 1, Marker::LINE_LIST);
    edges.scale.x = 0.04;
    edges.color = color(0.2F, 0.8F, 0.9F, 0.8F);
    for (const auto & edge : package_->edges) {
      const auto from = std::find_if(package_->nodes.begin(), package_->nodes.end(), [&](const auto & node) {
        return node.id == edge.from_node_id;
      });
      const auto to = std::find_if(package_->nodes.begin(), package_->nodes.end(), [&](const auto & node) {
        return node.id == edge.to_node_id;
      });
      if (from != package_->nodes.end() && to != package_->nodes.end()) {
        edges.points.push_back(point(from->pose.x_m, from->pose.y_m, 0.05));
        edges.points.push_back(point(to->pose.x_m, to->pose.y_m, 0.05));
      }
    }
    array.markers.push_back(edges);

    auto waypoints = base_marker(frame_id_, stamp, "reference_waypoints", 2, Marker::LINE_STRIP);
    waypoints.scale.x = 0.025;
    waypoints.color = color(0.95F, 0.95F, 0.95F, 0.75F);
    for (const auto & wp : package_->waypoints) {
      waypoints.points.push_back(point(wp.x_m, wp.y_m, 0.08));
    }
    array.markers.push_back(waypoints);

    int marker_id = 10;
    add_semantic_points(array, stamp, "task_points", package_->task_points, color(1.0F, 0.7F, 0.1F));
    add_semantic_points(array, stamp, "parking_points", package_->parking_points, color(0.2F, 1.0F, 0.2F));
    add_semantic_points(array, stamp, "charging_points", package_->charging_points, color(0.6F, 0.2F, 1.0F));
    add_semantic_points(array, stamp, "route_points", package_->route_points, color(1.0F, 0.4F, 0.2F));
    for (const auto & area : package_->areas) {
      auto marker = base_marker(frame_id_, stamp, "semantic_areas", marker_id++, Marker::LINE_STRIP);
      marker.scale.x = 0.06;
      if (area.type == "no_go_area" || area.type == "keepout") {
        marker.color = color(1.0F, 0.0F, 0.0F, 0.95F);
      } else if (area.type == "speed_zone") {
        marker.color = color(1.0F, 1.0F, 0.0F, 0.95F);
      } else {
        marker.color = color(0.1F, 0.8F, 0.1F, 0.65F);
      }
      for (const auto & p : area.polygon) {
        marker.points.push_back(point(p.x_m, p.y_m, 0.02));
      }
      if (!area.polygon.empty()) {
        marker.points.push_back(point(area.polygon.front().x_m, area.polygon.front().y_m, 0.02));
      }
      array.markers.push_back(marker);
    }

    auto text = base_marker(frame_id_, stamp, "roadnet_status_text", 1000, Marker::TEXT_VIEW_FACING);
    text.pose.position = point(0.0, 0.0, 1.2);
    text.scale.z = 0.45;
    text.color = color(1.0F, 1.0F, 1.0F, 1.0F);
    text.text = "roadnet loaded: nodes=" + std::to_string(package_->nodes.size()) +
      " edges=" + std::to_string(package_->edges.size()) +
      " waypoints=" + std::to_string(package_->waypoints.size());
    array.markers.push_back(text);

    roadnet_markers_pub_->publish(array);
  }

  void add_semantic_points(
    MarkerArray & array,
    const rclcpp::Time & stamp,
    const std::string & ns,
    const std::map<std::string, low_speed_av_planning::SemanticPoint> & points,
    const std_msgs::msg::ColorRGBA & marker_color)
  {
    auto marker = base_marker(frame_id_, stamp, ns, 0, Marker::SPHERE_LIST);
    marker.scale.x = 0.35;
    marker.scale.y = 0.35;
    marker.scale.z = 0.35;
    marker.color = marker_color;
    for (const auto & item : points) {
      marker.points.push_back(point(item.second.pose.x_m, item.second.pose.y_m, 0.2));
    }
    array.markers.push_back(marker);
  }

  void on_route(const low_speed_av_interfaces::msg::GlobalRoute & msg)
  {
    MarkerArray array;
    const auto stamp = now();
    auto route_line = base_marker(frame_id_, stamp, "planned_global_route", 0, Marker::LINE_STRIP);
    route_line.scale.x = 0.12;
    route_line.color = color(1.0F, 0.35F, 0.05F, 1.0F);
    if (package_) {
      for (const auto & edge_id : msg.edge_ids) {
        const auto range_it = package_->waypoint_index_by_edge.find(edge_id);
        if (range_it == package_->waypoint_index_by_edge.end()) {
          continue;
        }
        for (std::size_t i = range_it->second.start_index;
          i < range_it->second.end_index_exclusive && i < package_->waypoints.size(); ++i)
        {
          const auto & wp = package_->waypoints[i];
          route_line.points.push_back(point(wp.x_m, wp.y_m, 0.22));
        }
      }
    }
    array.markers.push_back(route_line);

    if (package_ && !msg.node_ids.empty()) {
      add_route_endpoint(array, stamp, msg.node_ids.front(), 1, "route_start", color(0.0F, 1.0F, 0.0F));
      add_route_endpoint(array, stamp, msg.node_ids.back(), 2, "route_goal", color(1.0F, 0.0F, 0.0F));
    }

    route_markers_pub_->publish(array);
  }

  void add_route_endpoint(
    MarkerArray & array,
    const rclcpp::Time & stamp,
    const std::string & node_id,
    int id,
    const std::string & ns,
    const std_msgs::msg::ColorRGBA & marker_color)
  {
    const auto node = std::find_if(package_->nodes.begin(), package_->nodes.end(), [&](const auto & item) {
      return item.id == node_id;
    });
    if (node == package_->nodes.end()) {
      return;
    }
    auto marker = base_marker(frame_id_, stamp, ns, id, Marker::SPHERE);
    marker.pose.position = point(node->pose.x_m, node->pose.y_m, 0.35);
    marker.scale.x = 0.45;
    marker.scale.y = 0.45;
    marker.scale.z = 0.45;
    marker.color = marker_color;
    array.markers.push_back(marker);
  }

  void on_trajectory(const low_speed_av_interfaces::msg::Trajectory & msg)
  {
    nav_msgs::msg::Path path;
    path.header.frame_id = frame_id_;
    path.header.stamp = now();
    for (const auto & point_msg : msg.points) {
      geometry_msgs::msg::PoseStamped pose;
      pose.header = path.header;
      pose.pose.position.x = point_msg.x_m;
      pose.pose.position.y = point_msg.y_m;
      pose.pose.position.z = 0.18;
      pose.pose.orientation = quaternion_from_yaw(point_msg.yaw_rad);
      path.poses.push_back(pose);
    }
    trajectory_path_pub_->publish(path);

    MarkerArray array;
    auto line = base_marker(frame_id_, path.header.stamp, "planned_trajectory", 0, Marker::LINE_STRIP);
    line.scale.x = 0.08;
    line.color = color(0.2F, 1.0F, 0.2F, 1.0F);
    for (const auto & pose : path.poses) {
      line.points.push_back(pose.pose.position);
    }
    array.markers.push_back(line);
    route_markers_pub_->publish(array);
  }

  void on_pose(const geometry_msgs::msg::PoseStamped & msg)
  {
    MarkerArray array;
    auto vehicle = base_marker(frame_id_, now(), "sim_or_current_vehicle_pose", 0, Marker::ARROW);
    vehicle.pose = msg.pose;
    vehicle.pose.position.z = 0.35;
    vehicle.scale.x = 0.9;
    vehicle.scale.y = 0.18;
    vehicle.scale.z = 0.18;
    vehicle.color = color(0.1F, 0.9F, 1.0F, 1.0F);
    array.markers.push_back(vehicle);
    vehicle_markers_pub_->publish(array);
  }

  RoadnetLoader loader_;
  std::shared_ptr<RoadnetPackage> package_;
  std::string frame_id_{"map"};
  rclcpp::Publisher<MarkerArray>::SharedPtr roadnet_markers_pub_;
  rclcpp::Publisher<MarkerArray>::SharedPtr route_markers_pub_;
  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr trajectory_path_pub_;
  rclcpp::Publisher<MarkerArray>::SharedPtr vehicle_markers_pub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::GlobalRoute>::SharedPtr route_sub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::Trajectory>::SharedPtr trajectory_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace low_speed_av_simulation

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<low_speed_av_simulation::RoadnetVisualizationNode>());
  rclcpp::shutdown();
  return 0;
}
