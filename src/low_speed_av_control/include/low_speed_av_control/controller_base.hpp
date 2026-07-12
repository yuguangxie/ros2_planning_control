#pragma once

#include <memory>
#include <string>
#include <vector>

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

struct ControllerOptions {
  // Pure Pursuit lookahead configuration.
  double lookahead_min_m{0.8};
  double lookahead_max_m{3.0};
  double lookahead_speed_gain{1.2};
  double stanley_k{0.8};
  double stanley_epsilon_mps{0.1};
  double max_correction_rad{0.5};
  double wheel_base_m{1.2};
  double control_dt_s{0.02};
  // Discrete kinematic-bicycle LQR options for the [e_y, e_psi] tracking model.
  double lqr_q_lateral_error{3.0};
  double lqr_q_heading_error{2.0};
  double lqr_r_steering{1.0};
  int lqr_max_iterations{80};
  double lqr_convergence_eps{1.0e-6};
  double lqr_min_speed_mps{0.2};
  double lqr_preview_time_s{0.2};
  bool lqr_use_curvature_feedforward{true};
  double lqr_max_steering_angle_rad{0.52};
  // Experimental MPC sampler options. The sampler is deterministic and avoids
  // heavy solver dependencies.
  int mpc_horizon_steps{10};
  double mpc_dt_s{0.1};
  std::vector<double> mpc_curvature_samples{-0.2, -0.1, 0.0, 0.1, 0.2};
  double mpc_lateral_error_weight{1.0};
  double mpc_heading_error_weight{0.5};
  double mpc_speed_error_weight{0.2};
  double mpc_steering_effort_weight{0.05};
};

class ControllerBase {
public:
  virtual ~ControllerBase() = default;
  virtual std::string name() const = 0;
  // Compute a raw tracking command from pose, vehicle state and trajectory.
  // Limiting, smoothing and safety overrides are applied outside controllers.
  virtual ControlCommand compute(const Pose2d &pose, const VehicleState &state,
                                 const Trajectory &trajectory,
                                 const ControllerOptions &options) const = 0;
};

} // namespace low_speed_av_control
