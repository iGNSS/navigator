#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <rclcpp/rclcpp.hpp>

#include <iostream>
#include <fstream>
#include <iomanip>

#include <ctime>
#include <cstdlib> 

using namespace std;

#include "path_publisher/PathPublisherNode.hpp"
/*
PSEUDOCODE

On start:
	1. Read xodr file
	2. Iterate over each lane in each road
		a. For each lane, gather vertices and generate triangles
		b. Put tris in array.
	3. Convert tri array to triangle list Marker message (or MarkerArray?)

Every n seconds:
	1. Publish tri array

*/

using geometry_msgs::msg::Point;
using geometry_msgs::msg::Vector3;
using std_msgs::msg::ColorRGBA;
using visualization_msgs::msg::Marker;
using visualization_msgs::msg::MarkerArray;
using namespace std::chrono_literals;

PathPublisherNode::PathPublisherNode() : Node("path_publisher_node") {

	srand (static_cast <unsigned> (time(0)));

	this->declare_parameter<std::string>("xodr_path", "/home/main/navigator/data/maps/town10/Town10HD_Opt.xodr");
	this->declare_parameter<double>("draw_detail", 2.0);
	marker_pub = this->create_publisher<visualization_msgs::msg::MarkerArray>("/map/viz", 1);

	map_pub_timer = this->create_wall_timer(5s, std::bind(&PathPublisherNode::publishMarkerArray, this));

	// Read map from file, using our path param
	std::string xodr_path = this->get_parameter("xodr_path").as_string();
	double draw_detail = this->get_parameter("draw_detail").as_double();
	RCLCPP_INFO(this->get_logger(), "Reading from " + xodr_path);
	odr::OpenDriveMap odr(xodr_path, true, true, false, true);

	// Iterate through all roads->lanesections->lanes
	// For each lane: Construct Line Strip markers for left and right bound
		// Append markers to MarkerArray

	/**
	 * ELEMENT COLORS
	 **/
	
	ColorRGBA line_color;
	line_color.a = 1.0;
	line_color.r = 0.59;
	line_color.g = 0.67;
	line_color.b = 0.69;

	ColorRGBA driving_color;
	driving_color.a = 1.0;
	driving_color.r = 0.22;
	driving_color.g = 0.27;
	driving_color.b = 0.27;

	ColorRGBA shoulder_color;
	shoulder_color.a = 1.0;
	shoulder_color.r = 0.71;
	shoulder_color.g = 0.77;
	shoulder_color.b = 0.79;

	ColorRGBA sidewalk_color;
	sidewalk_color.a = 1.0;
	sidewalk_color.r = 0.89;
	sidewalk_color.g = 0.91;
	sidewalk_color.b = 0.91;

	/** 
	 * Mesh markers (triangle lists) and line list marker
	 **/

	Marker trilist_driving;
	trilist_driving.type = trilist_driving.TRIANGLE_LIST;
	trilist_driving.header.stamp = now();
	trilist_driving.header.frame_id = "map";
	trilist_driving.ns = "lanes_driving";
	trilist_driving.action = trilist_driving.MODIFY;
	trilist_driving.frame_locked = true;
	trilist_driving.color = driving_color;
	trilist_driving.scale.x = 1.0;
	trilist_driving.scale.y = 1.0;
	trilist_driving.scale.z = 1.0;

	Marker trilist_shoulder = trilist_driving;
	trilist_shoulder.ns = "lanes_shoulder";
	trilist_shoulder.color = shoulder_color;

	Marker trilist_sidewalk = trilist_driving;
	trilist_sidewalk.ns = "lanes_sidewalk";
	trilist_sidewalk.color = sidewalk_color;

	Marker line_list; // Stores borders for lane bounds, sidewalks, etc.
	line_list.type = line_list.LINE_LIST;
	line_list.header.stamp = now();
	line_list.header.frame_id = "map";
	line_list.ns = "lanes";
	line_list.id = 883883; // Why not? WSH.
	line_list.action = line_list.MODIFY;
	line_list.scale.x = 0.3; // Only scale.x is used
	line_list.frame_locked = true; // Move with the Rviz camera
	line_list.color = line_color;
	line_list.pose.position.z = 0.1; // Set lines ever-so-slightly above the surface to prevent overlap.


	/**
	 * Iterate through every lane in the map.
	 * For each lane, get its mesh, including vertices and indices.
	 * For each index (which corresponds to a triange's point in the mesh),
	 * 	- Add the point to the appropriate mesh
	 *  - Add the point and its predecessor to the line list (borders etc)
	 **/
	for (std::shared_ptr<odr::Road> road : odr.get_roads()) {
		for(auto lsec : road->get_lanesections()) {
			for (auto lane : lsec->get_lanes()) {
				// Convert lane curves to triangles. The last get_mesh param describes resolution.
				auto mesh = lane->get_mesh(lsec->s0, lsec->get_end(), draw_detail); 
				auto pts = mesh.vertices; // Points are triangle vertices
				auto indices = mesh.indices; // Describes order of verts to make tris

				for (auto idx : indices) {
					Point p;
					p.x = pts[idx][0];
					p.y = pts[idx][1];
					if (lane->type=="driving") {
						trilist_driving.points.push_back(p);
					} else if (lane->type=="shoulder") {
						p.z -= 0.03; // Prevent overlap glitching. WSH.
						trilist_shoulder.points.push_back(p);
					} else if (lane->type == "sidewalk") {
						p.z += 0.1;
						trilist_sidewalk.points.push_back(p);
					}
					
					// Add a line segment to our line list marker.
					// See http://wiki.ros.org/rviz/DisplayTypes/Marker#Line_List_.28LINE_LIST.3D5.29
					// We can do this because points in the libOpenDRIVE mesh alternate from the
					// left to right side, so that even indices are on one side and odds are on the other.
					if (idx > 1) { 
						Point a;
						a.x = pts[idx-2][0];
						a.y = pts[idx-2][1];

						Point b;
						b.x = pts[idx][0];
						b.y = pts[idx][1];

						line_list.points.push_back(a);
						line_list.points.push_back(b);
					}
				}
			}
		}
	}
	// Add each marker to our marker array.

	point_count = 	trilist_driving.points.size() +
					trilist_shoulder.points.size() +
					trilist_sidewalk.points.size() +
					line_list.points.size();
	lane_markers.markers.push_back(trilist_driving); // Triangles that form surfaces for e.g. roads
	lane_markers.markers.push_back(trilist_shoulder); 
	lane_markers.markers.push_back(trilist_sidewalk); // Triangles that form surfaces for e.g. roads
	lane_markers.markers.push_back(line_list); // Borders and other lines
}

void PathPublisherNode::publishMarkerArray() {
	RCLCPP_INFO_ONCE(get_logger(), "Publishing %i map markers with %i points. This will print once.", lane_markers.markers.size(), point_count);
	marker_pub->publish(lane_markers);
}