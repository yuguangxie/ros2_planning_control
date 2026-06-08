#include "low_speed_av_planning/reference_line_motion_planner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "low_speed_av_planning/frenet_lite_motion_planner.hpp"
#include "low_speed_av_planning/hybrid_astar_parking_planner.hpp"
#include "low_speed_av_planning/stop_and_wait_motion_planner.hpp"
namespace low_speed_av_planning {
namespace {

double distance(const Waypoint & a, const Waypoint & b)
{
  return std::hypot(a.x_m - b.x_m, a.y_m - b.y_m);
}

}  // namespace

Trajectory ReferenceLineMotionPlanner::make_trajectory(
  const RoadnetPackage & package,
  const std::vector<std::string> & edge_ids,
  const Pose2d * crop_pose,
  const MotionPlannerOptions & options) const
{
  Trajectory out;
  for (const auto & edge_id : edge_ids) {
    // Stitch edge-local waypoint ranges in global route order.
    const auto it = package.waypoint_index_by_edge.find(edge_id);
    if (it == package.waypoint_index_by_edge.end()) {
      continue;
    }
    for (std::size_t i = it->second.start_index; i < it->second.end_index_exclusive && i < package.waypoints.size(); ++i) {
      if (options.deduplicate_edge_boundary_points && !out.empty() &&
        out.back().waypoint_id == package.waypoints[i].waypoint_id)
      {
        continue;
      }
      out.push_back(package.waypoints[i]);
    }
  }
  if (crop_pose && !out.empty()) {
    // Drop points behind the nearest route point so controllers receive a
    // forward-looking reference.
    std::size_t nearest = 0;
    double best = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < out.size(); ++i) {
      const double d = std::hypot(out[i].x_m - crop_pose->x_m, out[i].y_m - crop_pose->y_m);
      if (d < best) {
        best = d;
        nearest = i;
      }
    }
    out.erase(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(nearest));
  }
  if (options.regenerate_route_s && !out.empty()) {
    // Recompute route-level station after edge concatenation. Edge-local s_m
    // from the editor is intentionally not reused across multiple edges.
    out.front().route_s_m = 0.0;
    for (std::size_t i = 1; i < out.size(); ++i) {
      out[i].route_s_m = out[i - 1].route_s_m + distance(out[i - 1], out[i]);
    }
  }
  if (options.horizon_distance_m > 0.0) {
    out.erase(std::remove_if(out.begin(), out.end(), [&](const Waypoint & wp) {
      return wp.route_s_m > options.horizon_distance_m;
    }), out.end());
  }
  return out;
}

std::unique_ptr<MotionPlannerBase> MotionPlannerFactory::create(const std::string & algorithm)
{
  if (algorithm == "reference_line") {
    return std::make_unique<ReferenceLineMotionPlanner>();
  }
  if (algorithm == "stop_and_wait") {
    return std::make_unique<StopAndWaitMotionPlanner>();
  }
  if (algorithm == "frenet_lite") {
    return std::make_unique<FrenetLiteMotionPlanner>();
  }
  if (algorithm == "hybrid_astar_parking") {
    return std::make_unique<HybridAstarParkingPlanner>();
  }
  throw std::invalid_argument("unknown motion planner: " + algorithm);
}

}  // namespace low_speed_av_planning
