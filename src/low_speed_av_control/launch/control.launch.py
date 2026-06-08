from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    # Default to this package's installed config, while allowing explicit override.
    params = LaunchConfiguration("params")
    default_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_control"),
        "config",
        "control_params.yaml",
    ])
    return LaunchDescription([
        DeclareLaunchArgument(
            "params",
            default_value=default_params,
            description="control_params.yaml path",
        ),
        Node(
            package="low_speed_av_control",
            executable="control_node",
            name="low_speed_av_control",
            output="screen",
            parameters=[params],
        ),
    ])
