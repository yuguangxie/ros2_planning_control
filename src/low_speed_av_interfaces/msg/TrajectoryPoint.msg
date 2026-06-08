# 单个参考轨迹点。字段单位采用 SI：
# x/y 为 m，yaw 为 rad，kappa 为 1/m，速度为 m/s。
std_msgs/Header header
uint32 index
string waypoint_id
string edge_id
string path_id
float64 x_m
float64 y_m
float64 yaw_rad
float64 kappa_1pm
float64 s_m
float64 v_mps
float64 a_mps2
float64 relative_time_s
# gear: 0 UNKNOWN, 1 DRIVE, 2 REVERSE, 3 PARK
int8 gear
string behavior
