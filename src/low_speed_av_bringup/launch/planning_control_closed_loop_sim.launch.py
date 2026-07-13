from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def _bundled_sample_physical_root() -> str:
    """Resolve only the repository-controlled demo sample under symlink install."""
    installed_root = (
        Path(get_package_share_directory("low_speed_av_bringup"))
        / "sample_ad_package"
    )
    manifest = installed_root / "project_manifest.json"
    physical_root = manifest.resolve(strict=True).parent
    if not (physical_root / "project_manifest.json").is_file():
        raise RuntimeError("bundled sample project_manifest.json is unavailable")
    return str(physical_root)


def generate_launch_description():
    roadnet_package_path = LaunchConfiguration("roadnet_package_path")
    planning_params = LaunchConfiguration("planning_params")
    control_params = LaunchConfiguration("control_params")
    simulation_params = LaunchConfiguration("simulation_params")
    controller_algorithm = LaunchConfiguration("controller_algorithm")
    vehicle_model = LaunchConfiguration("vehicle_model")
    rviz = LaunchConfiguration("rviz")
    start_paused = LaunchConfiguration("start_paused")

    default_planning_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"), "config", "planning_params.yaml"
    ])
    default_control_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"), "config", "control_params.yaml"
    ])
    control_sim_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"), "config", "control_sim_params.yaml"
    ])
    default_simulation_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_simulation"),
        "config",
        "closed_loop_simulation_params.yaml",
    ])
    rviz_config = PathJoinSubstitution([
        FindPackageShare("low_speed_av_simulation"),
        "rviz",
        "roadnet_simulation.rviz",
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            "roadnet_package_path", default_value=_bundled_sample_physical_root()
        ),
        DeclareLaunchArgument("planning_params", default_value=default_planning_params),
        DeclareLaunchArgument("control_params", default_value=default_control_params),
        DeclareLaunchArgument(
            "simulation_params", default_value=default_simulation_params
        ),
        DeclareLaunchArgument("controller_algorithm", default_value="lqr"),
        DeclareLaunchArgument("vehicle_model", default_value="front_ackermann"),
        DeclareLaunchArgument("rviz", default_value="true"),
        DeclareLaunchArgument("start_paused", default_value="false"),
        Node(
            package="low_speed_av_planning",
            executable="planning_node",
            name="low_speed_av_planning",
            output="screen",
            parameters=[
                planning_params,
                {
                    "roadnet.package_path": roadnet_package_path,
                    # Keep one trajectory identity in SIL. Control already crops
                    # its production progress window from the current pose.
                    "planning.local_trajectory_from_current_pose": False,
                },
            ],
        ),
        Node(
            package="low_speed_av_control",
            executable="control_node",
            name="low_speed_av_control",
            output="screen",
            parameters=[
                control_params,
                control_sim_params,
                {
                    "controller.algorithm": controller_algorithm,
                    "vehicle.model": vehicle_model,
                },
            ],
        ),
        Node(
            package="low_speed_av_simulation",
            executable="sim_localization_pose_publisher_node",
            name="sim_localization_pose_publisher",
            output="screen",
            parameters=[
                simulation_params,
                {
                    "roadnet.package_path": roadnet_package_path,
                    "start_paused": start_paused,
                },
            ],
        ),
        Node(
            package="low_speed_av_simulation",
            executable="roadnet_visualization_node",
            name="roadnet_visualization_node",
            output="screen",
            parameters=[
                simulation_params,
                {"roadnet.package_path": roadnet_package_path},
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
