from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    roadnet_package_path = LaunchConfiguration("roadnet_package_path")
    use_sim_pose = LaunchConfiguration("use_sim_pose")
    pose_mode = LaunchConfiguration("pose_mode")
    publish_rate_hz = LaunchConfiguration("publish_rate_hz")
    frame_id = LaunchConfiguration("frame_id")
    start_paused = LaunchConfiguration("start_paused")
    launch_planning_control = LaunchConfiguration("launch_planning_control")
    rviz = LaunchConfiguration("rviz")

    simulation_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_simulation"),
        "config",
        "simulation_params.yaml",
    ])
    rviz_config = PathJoinSubstitution([
        FindPackageShare("low_speed_av_simulation"),
        "rviz",
        "roadnet_simulation.rviz",
    ])
    planning_control_launch = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"),
        "launch",
        "planning_control_demo.launch.py",
    ])
    default_roadnet_package = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"),
        "sample_ad_package",
    ])

    return LaunchDescription([
        DeclareLaunchArgument("roadnet_package_path", default_value=default_roadnet_package),
        DeclareLaunchArgument("use_sim_pose", default_value="true"),
        DeclareLaunchArgument("pose_mode", default_value="fixed_pose"),
        DeclareLaunchArgument("publish_rate_hz", default_value="20.0"),
        DeclareLaunchArgument("frame_id", default_value="map"),
        DeclareLaunchArgument("start_paused", default_value="false"),
        DeclareLaunchArgument("launch_planning_control", default_value="false"),
        DeclareLaunchArgument("rviz", default_value="true"),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(planning_control_launch),
            condition=IfCondition(launch_planning_control),
            launch_arguments={
                "roadnet_package_path": roadnet_package_path,
            }.items(),
        ),
        Node(
            package="low_speed_av_simulation",
            executable="roadnet_visualization_node",
            name="roadnet_visualization_node",
            output="screen",
            parameters=[
                simulation_params,
                {
                    "roadnet.package_path": roadnet_package_path,
                    "frame_id": frame_id,
                },
            ],
        ),
        Node(
            package="low_speed_av_simulation",
            executable="sim_localization_pose_publisher_node",
            name="sim_localization_pose_publisher",
            output="screen",
            condition=IfCondition(use_sim_pose),
            parameters=[
                simulation_params,
                {
                    "roadnet.package_path": roadnet_package_path,
                    "mode": pose_mode,
                    "publish_rate_hz": publish_rate_hz,
                    "frame_id": frame_id,
                    "start_paused": start_paused,
                },
            ],
        ),
        Node(
            package="rviz2",
            executable="rviz2",
            name="rviz2",
            output="screen",
            arguments=["-d", rviz_config],
            condition=IfCondition(rviz),
        ),
    ])
