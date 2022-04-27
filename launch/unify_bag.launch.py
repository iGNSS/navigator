from os import path, environ

from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.actions import IncludeLaunchDescription
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.actions import ExecuteProcess
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

from ament_index_python import get_package_share_directory

import subprocess

bag_path = "/home/main/voltron/assets/bags/april16/rosbag2_2022_04_16-22_02_11"
svo_path = "/home/main/voltron/assets/bags/april16/HD720_SN34750148_17-02-06_trimmed.svo"


def generate_launch_description():

    # run a bag in an infinite loop
    bag_process = subprocess.Popen(
        f"ros2 bag play -l {bag_path}".split())

    launch_path = path.realpath(__file__)
    launch_dir = path.join(path.dirname(launch_path), '..')
    param_dir = path.join(launch_dir, "param")
    interface = "vcan0"
    # /usr/loca/zed/get_python_api.py
    zed_unpacker = Node(
        package='bag_tools',
        executable='zed_unpacker',
        parameters=[{
            'use_real_camera': 'true'
        }]
    )

    urdf_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        arguments=[path.join(launch_dir, "data", "hail_bopp.urdf")]
    )

    viz = Node(
        package='nova_viz',
        executable='nova_viz_exe'
    )

    # CONTROL
    gnss_pub = Node(
        package="bag_tools",
        executable="gnss_log_publisher"
    )

    lidar_fusion = Node(
        package='lidar_fusion',
        name='lidar_fusion_node',
        executable='lidar_fusion_node',
        remappings=[
            ('/lidar_front', '/lidar_front/velodyne_points'),
            ('/lidar_rear', '/lidar_rear/velodyne_points'),
            ('/lidar_fused', '/lidar_fused')
        ]
    )

    pcl_localization = Node(
        package='pcl_localization_ros2',
        executable='pcl_localization_node',
        remappings=[
            ('/cloud', '/lidar_fused'),
            ('/imu', '/sensors/zed/imu'),
            ('/initialpose', '/sensors/gnss/odom'),
            ('/pcl_pose', '/pose/ndt2')
        ],
        output='screen',
        parameters=[
            '/home/main/navigator-2/src/atlas/pcl_localization_ros2/param/localization.yaml']
    )

    map_odom_ukf = Node(
        package='robot_localization',
        executable='ukf_node',
        name='localization_map_odom',
        parameters=[
            "/home/wheitman/navigator-2/param/atlas/map_odom.param.yaml"],
        remappings=[
            ("/odom0", "/gnss_odom"),
            ("/imu0", "/sensors/zed/imu")
        ]
    )

    odom_bl_link = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        arguments=[
            '0.0', '0.0', '0.0', '0.0', '0.0', '0.0', '1.0', 'odom', 'base_link'
        ]
    )

    scan_matcher = Node(
        package='scan_matching',
        executable='scan_matching_node'
    )

    # LIO-SAM only needs three inputs: IMU, Lidar, and GPS
    return LaunchDescription([
        gnss_pub,
        lidar_fusion,
        urdf_publisher,
        # map_odom_ukf,
        viz,
        odom_bl_link,
        zed_unpacker,
        # pcl_localization,
        scan_matcher,
    ])
