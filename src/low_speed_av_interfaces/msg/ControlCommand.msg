# 控制模块输出的 Ackermann 控制命令。
# steering_angle_rad 保留为兼容字段，默认等同 front_steering_angle_rad。
std_msgs/Header header
float64 speed_mps
float64 acceleration_mps2
float64 steering_angle_rad
float64 front_steering_angle_rad
float64 rear_steering_angle_rad
float64 brake
# gear: 0 UNKNOWN, 1 DRIVE, 2 REVERSE, 3 PARK
int8 gear
bool enable
bool emergency_stop
string controller_algorithm
string vehicle_model
string reason
