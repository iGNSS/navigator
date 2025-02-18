/*
 * Package:   obstacle_drawer
 * Filename:  obstacle_drawer_node.cpp
 * Author:    Ragib "Rae" Arnab
 * Email:     ria190000@utdallas.edu
 * Copyright: 2021, Nova UTD
 * License:   MIT License
 */

#include "obstacle_drawer/obstacle_drawer_node.hpp"
#include "rclcpp/rclcpp.hpp"
#include "visualization_msgs/msg/marker.hpp"
#include "visualization_msgs/msg/marker_array.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "voltron_msgs/msg/obstacle3_d_array.hpp"
#include "voltron_msgs/msg/bounding_box3_d.hpp"
#include "obstacle_classes/obstacle_classes.hpp"
#include <memory>
#include <chrono>
#include <array>

using namespace std::chrono_literals;
using std::placeholders::_1;

namespace navigator {
namespace obstacle_drawer {

ObstacleDrawer::ObstacleDrawer() : Node("obstacle_drawer_node") {
    this->obstacle_marker_array_publisher = this->create_publisher<visualization_msgs::msg::MarkerArray>("obstacle_marker_array", 10);
    this->obstacles_subscription = this->create_subscription<voltron_msgs::msg::Obstacle3DArray>(
        "nova_obstacle_array", 10, std::bind(&ObstacleDrawer::draw_obstacles, this, _1));
}

void ObstacleDrawer::draw_obstacles(const voltron_msgs::msg::Obstacle3DArray::SharedPtr msg) const {

    auto marker_array = visualization_msgs::msg::MarkerArray();

    for (const auto obstacle : msg->obstacles) {

        // setting up marker settings
        auto marker = visualization_msgs::msg::Marker();
        marker.header.frame_id = msg->header.frame_id;
        marker.header.stamp = this->now();
        marker.action = visualization_msgs::msg::Marker::ADD;
        marker.lifetime = rclcpp::Duration(250ms);
        marker.type = visualization_msgs::msg::Marker::LINE_LIST;
        marker.id = obstacle.id;    // id must be unique to each marker, or rviz will ignore duplicates

        // adjust color of bboxes by class
        switch (obstacle.label) {
            case navigator::obstacle_classes::OBSTACLE_CLASS::VEHICLE:  // blue for vehicles
                marker.scale.x = 0.2;
                marker.color.r = 0.0;
                marker.color.g = 0.0;
                marker.color.b = 1.0;
                marker.color.a = 1.0;
                break;
            case navigator::obstacle_classes::OBSTACLE_CLASS::PEDESTRIAN:   // yellow for pedestrians
                marker.scale.x = 0.1;
                marker.color.r = 1.0;
                marker.color.g = 1.0;
                marker.color.b = 0.0;
                marker.color.a = 1.0;
                break;
            default:    // unrecognized objects should be just invisible if alpha is 0.0
                marker.color.a = 0.0;
                break;
        }

        // draw 3D box
        for (int i = 0; i < 4; i++){
            // top line segments (0,1), (1,2), (2,3), (3,0)
            marker.points.push_back(obstacle.bounding_box.corners[i]);
            marker.points.push_back(obstacle.bounding_box.corners[(i+1) % 4]);
            // bottom line segments (4,5), (5,6), (6,7), (7,4)
            marker.points.push_back(obstacle.bounding_box.corners[i+4]);
            marker.points.push_back(obstacle.bounding_box.corners[(i+1) % 4 + 4]);
            // side line segments (0, 4), (1, 5), (2, 6), (3, 7)
            marker.points.push_back(obstacle.bounding_box.corners[i]);
            marker.points.push_back(obstacle.bounding_box.corners[i+4]);
        }

        marker_array.markers.push_back(marker);

    }
    this->obstacle_marker_array_publisher->publish(marker_array);
}
}
}

