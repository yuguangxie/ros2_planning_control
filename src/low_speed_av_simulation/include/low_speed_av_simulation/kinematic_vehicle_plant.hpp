#pragma once

#include "low_speed_av_simulation/simulation_types.hpp"

namespace low_speed_av_simulation {

class KinematicVehiclePlant {
public:
  explicit KinematicVehiclePlant(const PlantOptions &options = PlantOptions{});

  void validate_options() const;
  void reset(const PlantState &state = PlantState{});
  PlantStepResult step(const PlantCommand &command, double dt_s);
  const PlantState &state() const;
  bool goal_reached(double goal_x_m, double goal_y_m,
                    double goal_yaw_rad) const;

private:
  void force_stop(const std::string &reason);

  PlantOptions options_;
  PlantState state_;
};

} // namespace low_speed_av_simulation
