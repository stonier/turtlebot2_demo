// Copyright 2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>
#include <cstdio>
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joy.hpp"
#include "geometry_msgs/msg/twist.hpp"

// this will only work on POSIX
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

// and this will only work on Linux
//#include <linux/input.h>
#include <linux/joystick.h>
// the equivalent for OS X seems to be IOKit/hid/IOHIDLib.h
// but it's completely different in basically every way possible

int main(int argc, char * argv[])
{
  const char *joy_path = "/dev/input/js0"; // parameterize someday
  int joy_fd = open(joy_path, O_RDONLY);
  if (joy_fd < 0) {
    printf("ahhhh couldn't open %s\n", joy_path);
    return 1;
  }
  else {
    printf("joy fd = %d\n", joy_fd);
  }

  rclcpp::init(argc, argv);
  auto node = rclcpp::node::Node::make_shared("joy_node");
  auto joy_pub = node->create_publisher<sensor_msgs::msg::Joy>(
                     "joy", rmw_qos_profile_sensor_data);
  auto cmd_vel_pub = node->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", rmw_qos_profile_default);
  auto cmd_vel_msg = std::make_shared<geometry_msgs::msg::Twist>();
  cmd_vel_msg->linear.x = 0.0;
  cmd_vel_msg->angular.z = 0.0;

  auto msg = std::make_shared<sensor_msgs::msg::Joy>();
  msg->header.stamp.sec = 0;
  msg->header.stamp.nanosec = 0;
  msg->header.frame_id = "joy";
  msg->axes.resize(2);
  msg->axes[0] = msg->axes[1] = 0;
  msg->buttons.resize(1);
  msg->buttons[0] = 0;

  //rclcpp::WallRate loop_rate(10);

  fd_set read_fds;
  struct timeval tv;

  while (rclcpp::ok()) {
    rclcpp::spin_some(node);
    while (true)  { // all good programs have this
      FD_ZERO(&read_fds);
      FD_SET(joy_fd, &read_fds);
      tv.tv_sec = 0;
      tv.tv_usec = 1000; // wait at most 1ms. this could be smarter
      int retval = select(joy_fd+1, &read_fds, NULL, NULL, &tv);
      if (retval == -1)
        perror("select()");
      else if (retval > 0)
      {
        //printf("select() returned nonzero\n");
        struct js_event e;
        int nread = read(joy_fd, &e, sizeof(e));
        if (nread != sizeof(struct js_event)) {
          printf("ahhhhh read %d bytes instead of %d\n",
                 nread, (int)sizeof(struct js_event));
          break;
        }
        //printf("js event type %d axis %d value %d\n",
        //       e.type, e.number, e.value);
        switch (e.type & 0x7f) {
          case JS_EVENT_BUTTON:
            if (e.number >= msg->buttons.size())
              msg->buttons.resize(e.number+1);
            msg->buttons[e.number] = e.value;
            break;
          case JS_EVENT_AXIS:
            if (e.number >= msg->axes.size())
              msg->axes.resize(e.number+1);
            msg->axes[e.number] = e.value / 32767.0;
            break;
          default:
            break;
        }
      }
      else
        break; // if we timed out, let ROS spin to do its stuff
    }
    bool deadman = msg->buttons[0] != 0;
    cmd_vel_msg->linear.x = deadman ? 0 : -msg->axes[1] * 0.5;
    cmd_vel_msg->angular.z = deadman ? 0 : -msg->axes[0] * 2.0;
    cmd_vel_pub->publish(cmd_vel_msg);

    printf("publishing: (%6.3f, %6.3f)\n", msg->axes[0], msg->axes[1]);
    joy_pub->publish(msg);
    //loop_rate.sleep();
  }
  return 0;
}

