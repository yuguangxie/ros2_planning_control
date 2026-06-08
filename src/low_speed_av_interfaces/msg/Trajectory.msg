# 规划模块发布的参考轨迹，供控制模块订阅。
std_msgs/Header header
string trajectory_id
string source_package_id
string planner_algorithm
TrajectoryPoint[] points
bool emergency_stop
string status
