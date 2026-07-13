from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def _bundled_sample_physical_root() -> str:
    """Resolve only the trusted bundled sample through a symlink install."""
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
    # Demo launch starts planning and control together. Defaults point to
    # installed bringup config and sample AD Package, with launch arg overrides.
    planning_params = LaunchConfiguration("planning_params")
    control_params = LaunchConfiguration("control_params")
    roadnet_package_path = LaunchConfiguration("roadnet_package_path")
    default_planning_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"),
        "config",
        "planning_params.yaml",
    ])
    default_control_params = PathJoinSubstitution([
        FindPackageShare("low_speed_av_bringup"),
        "config",
        "control_params.yaml",
    ])
    # The default is repository-controlled content. Resolving its manifest link
    # makes the complete physical source directory the package root under
    # --symlink-install. User overrides are passed through unchanged and remain
    # subject to RoadnetLoader's strict containment checks.
    default_roadnet_package = _bundled_sample_physical_root()
    return LaunchDescription([
        DeclareLaunchArgument("planning_params", default_value=default_planning_params),
        DeclareLaunchArgument("control_params", default_value=default_control_params),
        DeclareLaunchArgument("roadnet_package_path", default_value=default_roadnet_package),
        Node(
            package="low_speed_av_planning",
            executable="planning_node",
            name="low_speed_av_planning",
            output="screen",
            parameters=[
                planning_params,
                {"roadnet.package_path": roadnet_package_path},
            ],
        ),
        Node(
            package="low_speed_av_control",
            executable="control_node",
            name="low_speed_av_control",
            output="screen",
            parameters=[control_params],
        ),
    ])
