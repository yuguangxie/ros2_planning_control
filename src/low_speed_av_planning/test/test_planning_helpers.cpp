#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "low_speed_av_planning/planning_helpers.hpp"

namespace low_speed_av_planning {
namespace {

RoadnetPackage helper_package()
{
  RoadnetPackage package;
  package.nodes = {{"A", {0.0, 0.0, 0.0}, "normal"}, {"B", {3.0, 0.0, 0.0}, "normal"}};
  TopologyEdge edge;
  edge.id = "AB"; edge.from_node_id = "A"; edge.to_node_id = "B";
  edge.length_m = 3.0; edge.cost = 3.0; edge.speed_limit_mps = 0.5;
  package.edges.push_back(edge);
  for (std::size_t i = 0; i < 4U; ++i) {
    Waypoint waypoint;
    waypoint.index = i; waypoint.waypoint_id = "w" + std::to_string(i); waypoint.edge_id = "AB";
    waypoint.x_m = static_cast<double>(i); waypoint.edge_s_m = static_cast<double>(i);
    waypoint.target_speed_mps = 0.5;
    package.waypoints.push_back(waypoint);
  }
  package.waypoint_index_by_edge["AB"] = {0U, 4U, 4U, false};
  return package;
}

TEST(PlanningHelpersProduction, NullSemanticNodeFallsBackToLinkedEdge)
{
  auto package = helper_package();
  SemanticPoint point;
  point.id = "task"; point.linked_node_id = "null"; point.linked_edge_id = "AB";
  point.pose = {2.1, 0.0, 0.0};
  package.task_points.emplace(point.id, point);
  std::string diagnostic;
  const auto anchor = make_semantic_anchor(
    package, "task", package.task_points, "task point", RoadnetAnchor::Type::TaskPoint, true, &diagnostic);
  ASSERT_TRUE(anchor.has_value()) << diagnostic;
  EXPECT_EQ(anchor->node_id, "B");
  EXPECT_EQ(anchor->edge_id, "AB");
  EXPECT_TRUE(anchor->require_final_stop);
}

TEST(PlanningHelpersProduction, CurrentPoseAnchorKeepsCurrentEdgeRemainder)
{
  const auto package = helper_package();
  std::string diagnostic;
  const auto anchor = make_current_pose_anchor(
    package, Pose2d{1.1, 0.0, 0.0}, "A", 0.5, 1.0, true, &diagnostic);
  ASSERT_TRUE(anchor.has_value()) << diagnostic;
  EXPECT_EQ(anchor->edge_id, "AB");
  EXPECT_EQ(anchor->node_id, "B");
  EXPECT_EQ(anchor->waypoint_index, 1U);
}

TEST(PlanningHelpersProduction, TaskParkingChargingLinkedNodeAnchorsAreResolved)
{
  auto package = helper_package();
  for (const auto & id : {"task", "parking", "charging"}) {
    SemanticPoint point;
    point.id = id; point.linked_node_id = "B"; point.pose = {3.0, 0.0, 0.0};
    std::map<std::string, SemanticPoint> points{{id, point}};
    const auto anchor = make_semantic_anchor(
      package, id, points, id, RoadnetAnchor::Type::TaskPoint, true, nullptr);
    ASSERT_TRUE(anchor.has_value()) << id;
    EXPECT_EQ(anchor->node_id, "B");
    EXPECT_FALSE(anchor->has_edge);
  }
  EXPECT_TRUE(normalize_optional_identifier(" null ").empty());
  EXPECT_TRUE(normalize_optional_identifier("NONE").empty());
}

TEST(PlanningHelpersProduction, SameEdgeTerminalSegmentEndsAtSemanticStop)
{
  const auto package = helper_package();
  RoadnetAnchor start;
  start.has_edge = true; start.edge_id = "AB"; start.waypoint_index = 1U;
  start.s_on_edge_m = 1.0; start.has_pose = true; start.x_m = 1.1;
  RoadnetAnchor goal;
  goal.has_edge = true; goal.edge_id = "AB"; goal.waypoint_index = 3U;
  goal.s_on_edge_m = 3.0; goal.has_pose = true; goal.x_m = 2.8; goal.require_final_stop = true;
  const auto trajectory = build_edge_segment_between(package, start, goal, false);
  ASSERT_EQ(trajectory.size(), 3U);
  EXPECT_NEAR(trajectory.front().x_m, 1.1, 1e-9);
  EXPECT_NEAR(trajectory.back().x_m, 2.8, 1e-9);
  EXPECT_DOUBLE_EQ(trajectory.back().target_speed_mps, 0.0);
  EXPECT_TRUE(goal_behind_on_same_edge(goal, start));
}

TEST(PlanningHelpersProduction, GoalBehindRequiresExplicitReverseSegment)
{
  const auto package = helper_package();
  RoadnetAnchor start;
  start.has_edge = true; start.edge_id = "AB"; start.waypoint_index = 3U; start.s_on_edge_m = 3.0;
  RoadnetAnchor goal;
  goal.has_edge = true; goal.edge_id = "AB"; goal.waypoint_index = 1U; goal.s_on_edge_m = 1.0;
  EXPECT_TRUE(goal_behind_on_same_edge(start, goal));
  EXPECT_TRUE(build_edge_segment_between(package, start, goal, false).empty());
  const auto reverse = build_edge_segment_between(package, start, goal, true);
  ASSERT_FALSE(reverse.empty());
  for (const auto & waypoint : reverse) {EXPECT_EQ(waypoint.gear, 2);}
}

TEST(PlanningHelpersProduction, RouteSummaryAndContinuityUseFullReferenceGeometry)
{
  auto trajectory = helper_package().waypoints;
  regenerate_route_s(trajectory);
  PlanResult route;
  recompute_route_summary(route, trajectory);
  EXPECT_NEAR(route.length_m, 3.0, 1e-9);
  EXPECT_GT(route.estimated_time_s, 0.0);
  std::string diagnostic;
  EXPECT_TRUE(trajectory_is_continuous(trajectory, 1.1, &diagnostic));
  trajectory.back().x_m = 30.0;
  EXPECT_FALSE(trajectory_is_continuous(trajectory, 2.0, &diagnostic));
  EXPECT_FALSE(diagnostic.empty());
}

TEST(PlanningHelpersProduction, SpeedZoneIsAppliedWithoutExceedingInputSpeed)
{
  auto trajectory = helper_package().waypoints;
  SemanticArea zone;
  zone.id = "slow"; zone.type = "speed_zone"; zone.speed_limit_mps = 0.2;
  zone.polygon = {{-1.0, -1.0, 0.0}, {4.0, -1.0, 0.0}, {4.0, 1.0, 0.0}, {-1.0, 1.0, 0.0}};
  apply_semantic_speed_limits(trajectory, {zone});
  for (const auto & waypoint : trajectory) {
    EXPECT_DOUBLE_EQ(waypoint.target_speed_mps, 0.2);
    EXPECT_EQ(waypoint.behavior, "semantic_speed_zone:slow");
  }
}

TEST(PlanningHelpersProduction, ProgressWindowDoesNotJumpAcrossLoopAndResetsIdentity)
{
  Trajectory trajectory;
  for (std::size_t i = 0U; i < 10U; ++i) {
    Waypoint waypoint;
    waypoint.waypoint_id = "w" + std::to_string(i);
    waypoint.x_m = i < 5U ? static_cast<double>(i) : static_cast<double>(9U - i);
    waypoint.y_m = i < 5U ? 0.0 : 0.05;
    waypoint.yaw_rad = i < 5U ? 0.0 : 3.14159265358979323846;
    trajectory.push_back(waypoint);
  }
  regenerate_route_s(trajectory);
  TrajectoryProgressTracker tracker;
  ProgressSearchOptions options;
  options.horizon_distance_m = 100.0; options.forward_window_points = 3U;
  const auto first = tracker.crop(trajectory, Pose2d{0.1, 0.05, 0.0}, "route-a", options);
  ASSERT_FALSE(first.empty());
  EXPECT_EQ(first.back().waypoint_id, trajectory.back().waypoint_id);
  EXPECT_LT(tracker.progress_index(), 5U);
  (void)tracker.crop(trajectory, Pose2d{3.1, 0.0, 0.0}, "route-a", options);
  EXPECT_GE(tracker.progress_index(), 3U);
  (void)tracker.crop(trajectory, Pose2d{0.0, 0.0, 0.0}, "route-b", options);
  EXPECT_EQ(tracker.progress_index(), 0U);
}

}  // namespace
}  // namespace low_speed_av_planning
