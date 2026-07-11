#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

#include "low_speed_av_planning/astar_planner.hpp"
#include "low_speed_av_planning/constant_speed_planner.hpp"
#include "low_speed_av_planning/curvature_speed_planner.hpp"
#include "low_speed_av_planning/dijkstra_planner.hpp"
#include "low_speed_av_planning/obstacle_aware_speed_planner.hpp"
#include "low_speed_av_planning/reference_line_motion_planner.hpp"
#include "low_speed_av_planning/stop_and_wait_motion_planner.hpp"
#include "low_speed_av_planning/topology_graph.hpp"

namespace low_speed_av_planning {
namespace {

RoadnetPackage graph_fixture() {
  RoadnetPackage package;
  package.nodes = {{"A", {0.0, 0.0, 0.0}, "normal"},
                   {"B", {1.0, 0.0, 0.0}, "normal"},
                   {"C", {2.0, 0.0, 0.0}, "normal"},
                   {"D", {1.0, 1.0, 0.0}, "normal"}};
  auto edge = [](const std::string &id, const std::string &from,
                 const std::string &to, double cost,
                 const std::string &direction = "forward") {
    TopologyEdge value;
    value.id = id;
    value.from_node_id = from;
    value.to_node_id = to;
    value.direction = direction;
    value.length_m = cost;
    value.cost = cost;
    value.speed_limit_mps = 1.0;
    return value;
  };
  package.edges = {edge("AB", "A", "B", 1.0), edge("BC", "B", "C", 1.0),
                   edge("AD", "A", "D", 5.0), edge("DC", "D", "C", 5.0),
                   edge("CA_R", "C", "A", 1.0, "reverse")};
  return package;
}

TEST(PlanningGraphProduction, DijkstraFindsShortestAndHandlesStartEqualsGoal) {
  const TopologyGraph graph(graph_fixture());
  DijkstraPlanner planner;
  const auto route = planner.plan(graph, "A", "C", GlobalPlannerOptions{});
  ASSERT_TRUE(route.success);
  EXPECT_EQ(route.edge_ids, (std::vector<std::string>{"AB", "BC"}));
  const auto same = planner.plan(graph, "A", "A", GlobalPlannerOptions{});
  EXPECT_TRUE(same.success);
  EXPECT_TRUE(same.edge_ids.empty());
}

TEST(PlanningGraphProduction, BlockedAndDisabledEdgesAreNotUsed) {
  auto package = graph_fixture();
  package.edges[0].enabled = false;
  TopologyGraph graph(package);
  GlobalPlannerOptions options;
  options.blocked_edges.insert("AD");
  EXPECT_FALSE(DijkstraPlanner().plan(graph, "A", "C", options).success);
}

TEST(PlanningGraphProduction, ReversePolicyIsEnforced) {
  const TopologyGraph graph(graph_fixture());
  GlobalPlannerOptions options;
  options.allow_reverse = false;
  EXPECT_FALSE(DijkstraPlanner().plan(graph, "C", "A", options).success);
  options.allow_reverse = true;
  EXPECT_TRUE(DijkstraPlanner().plan(graph, "C", "A", options).success);
}

TEST(PlanningGraphProduction, AstarMatchesDijkstraWithZeroHeuristic) {
  const TopologyGraph graph(graph_fixture());
  GlobalPlannerOptions options;
  options.heuristic_weight = 0.0;
  const auto dijkstra = DijkstraPlanner().plan(graph, "A", "C", options);
  const auto astar = AstarPlanner().plan(graph, "A", "C", options);
  ASSERT_TRUE(dijkstra.success);
  ASSERT_TRUE(astar.success);
  EXPECT_EQ(astar.edge_ids, dijkstra.edge_ids);
  EXPECT_DOUBLE_EQ(astar.length_m, dijkstra.length_m);
}

RoadnetPackage trajectory_fixture() {
  RoadnetPackage package;
  package.waypoints = {{0, "w0", "e0", "p", 0.0, 0.0, 0.0, 0.0, 0.0},
                       {1, "shared", "e0", "p", 1.0, 0.0, 0.0, 0.1, 1.0},
                       {2, "shared", "e1", "p", 1.0, 0.0, 0.0, 0.1, 0.0},
                       {3, "w3", "e1", "p", 2.0, 0.0, 0.0, 0.2, 1.0},
                       {4, "w4", "e1", "p", 3.0, 0.0, 0.0, 0.0, 2.0}};
  for (auto &point : package.waypoints) {
    point.target_speed_mps = 0.8;
  }
  package.waypoint_index_by_edge["e0"] = {0, 2, 2, false};
  package.waypoint_index_by_edge["e1"] = {2, 5, 3, false};
  return package;
}

TEST(MotionPlannerProduction, StitchesDeduplicatesAndRegeneratesRouteS) {
  MotionPlannerOptions options;
  options.horizon_distance_m = 100.0;
  const auto trajectory = ReferenceLineMotionPlanner().make_trajectory(
      trajectory_fixture(), {"e0", "e1"}, nullptr, options);
  ASSERT_EQ(trajectory.size(), 4U);
  EXPECT_EQ(trajectory.front().waypoint_id, "w0");
  EXPECT_EQ(trajectory.back().waypoint_id, "w4");
  for (std::size_t i = 1; i < trajectory.size(); ++i) {
    EXPECT_GE(trajectory[i].route_s_m, trajectory[i - 1].route_s_m);
  }
}

TEST(MotionPlannerProduction, CropsByPoseAndHorizon) {
  MotionPlannerOptions options;
  options.horizon_distance_m = 2.1;
  const Pose2d pose{1.0, 0.0, 0.0};
  const auto trajectory = ReferenceLineMotionPlanner().make_trajectory(
      trajectory_fixture(), {"e0", "e1"}, &pose, options);
  ASSERT_FALSE(trajectory.empty());
  EXPECT_EQ(trajectory.front().waypoint_id, "shared");
  EXPECT_LE(trajectory.back().route_s_m, options.horizon_distance_m);
}

TEST(MotionPlannerProduction, StopAndWaitForcesZeroSpeed) {
  MotionPlannerOptions options;
  options.horizon_distance_m = 100.0;
  const auto trajectory = StopAndWaitMotionPlanner().make_trajectory(
      trajectory_fixture(), {"e0", "e1"}, nullptr, options);
  ASSERT_FALSE(trajectory.empty());
  for (const auto &point : trajectory) {
    EXPECT_DOUBLE_EQ(point.target_speed_mps, 0.0);
    EXPECT_EQ(point.behavior, "stop_and_wait");
  }
}

TEST(SpeedPlannerProduction, ConstantAndCurvatureRemainFiniteAndBounded) {
  auto trajectory = ReferenceLineMotionPlanner().make_trajectory(
      trajectory_fixture(), {"e0", "e1"}, nullptr,
      MotionPlannerOptions{100.0, true, true});
  SpeedPlannerOptions options;
  options.default_speed_mps = 0.6;
  options.max_speed_mps = 0.7;
  ConstantSpeedPlanner().apply(trajectory, options);
  for (const auto &point : trajectory) {
    EXPECT_DOUBLE_EQ(point.target_speed_mps, 0.6);
  }
  CurvatureSpeedPlanner().apply(trajectory, options);
  for (const auto &point : trajectory) {
    EXPECT_TRUE(std::isfinite(point.target_speed_mps));
    EXPECT_GE(point.target_speed_mps, 0.0);
    EXPECT_LE(point.target_speed_mps, options.max_speed_mps);
  }
}

TEST(SpeedPlannerProduction, ObstacleAwareStubStopsDownstreamPoints) {
  auto trajectory = ReferenceLineMotionPlanner().make_trajectory(
      trajectory_fixture(), {"e0", "e1"}, nullptr,
      MotionPlannerOptions{100.0, true, true});
  SpeedPlannerOptions options;
  options.obstacle_distance_m = 1.5;
  options.obstacle_stop_distance_m = 2.0;
  ObstacleAwareSpeedPlanner().apply(trajectory, options);
  for (const auto &point : trajectory) {
    if (point.route_s_m >= 1.5) {
      EXPECT_DOUBLE_EQ(point.target_speed_mps, 0.0);
      EXPECT_EQ(point.behavior, "obstacle_stop");
    }
  }
}

} // namespace
} // namespace low_speed_av_planning
