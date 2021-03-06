/************************************************************
 * Name: obstacle_avoidance.cpp
 * Author: Alyssa Kubota, Sanmi Adeleye
 * Date: 02/18/2018
 *
 * Description: This program will subscribe to the /blobs topic,
 *        and use the blob information to find and go towards
 *        the blobs in a user-set order.
 ***********************************************************/

#include <kobuki_msgs/BumperEvent.h> 
#include <cmvision/Blob.h>
#include <cmvision/Blobs.h>
#include <geometry_msgs/Twist.h>
#include <ros/ros.h>
#include <cmvision/Blobs.h>
#include <stdio.h>
#include <vector>
#include <pcl_ros/point_cloud.h>
#include <pcl/point_types.h>
#include <time.h>
#include <math.h>
#include <sensor_msgs/Image.h> //added from follower
#include <depth_image_proc/depth_traits.h>  //added from follower

//#include <pluginlib/class_list_macros.h>
//#include <nodelet/nodelet.h>

//#include <visualization_msgs/Marker.h>
//#include <turtlebot_msgs/SetFollowState.h>

//#include "dynamic_reconfigure/server.h"
//#include "bot_follower/FollowerConfig.h"







ros::Publisher pub;

typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;

//uint16_t state=0;
double Center = 320;
std::vector<uint16_t> goal_xs;

//gcounter
int gcounter=0;
//int ginit2=0;
//int gfinal=100000;

// fucntional vars
double blob_x;
double obstaclex;
double obstaclez;


//states
int obstate=0;
int blobstate=0;
//int obsdetected=0;



/************************************************************
 * Function Name: blobsCallBack
 * Parameters: const cmvision::Blobs
 * Returns: void
 *
 * Description: This is the callback function of the /blobs topic
 ***********************************************************/

void blobsCallBack (const cmvision::Blobs& blobsIn) //this gets the centroid of the color blob corresponding to the goal.
{
if (blobsIn.blob_count > 0){

blobstate=1;    
int n=blobsIn.blob_count;
//std::cout << "Blobs!!" << std::endl;
//std::cout << n  << std::endl;

double goal_sum_x=0;
double goal_sum_y=0;


for (int i = 0; i < blobsIn.blob_count; i++){
	goal_sum_x += blobsIn.blobs[i].x;
	goal_sum_y += blobsIn.blobs[i].y;
}
goal_sum_x/=n;
goal_sum_y/=n;

//std::cout << "Blob centroid x" << std::endl;
//std::cout << goal_sum_x << std::endl;

//std::cout << "Blob centroid y" << std::endl;
//std::cout << goal_sum_y << std::endl;

    
blob_x=goal_sum_x; //setting glob var
	}
}



void imagecb(const sensor_msgs::ImageConstPtr& depth_msg)
  {

	//local variables
  double min_y_ = 0.1; /**< The minimum y position of the points in the box. */
  double max_y_ = 0.5; /**< The maximum y position of the points in the box. */
  double min_x_ = -1; /**< The minimum x position of the points in the box. */
  double max_x_ = 1; /**< The maximum x position of the points in the box. */
  double max_z_ = 0.8; /**< The maximum z position of the points in the box. */
  double goal_z_ = 0.3; /**< The distance away from the robot to hold the centroid */
  double z_scale_ = 2.0; /**< The scaling factor for translational robot speed */
  double x_scale_ = 7.0; /**< The scaling factor for rotational robot speed */
  bool   enabled_ = true; /**< Enable/disable following; just prevents motor commands */



    // Precompute the sin function for each row and column
    uint32_t image_width = depth_msg->width;
    float x_radians_per_pixel = 60.0/57.0/image_width;
    float sin_pixel_x[image_width];
    for (int x = 0; x < image_width; ++x) {
      sin_pixel_x[x] = sin((x - image_width/ 2.0)  * x_radians_per_pixel);
    }

    uint32_t image_height = depth_msg->height;
    float y_radians_per_pixel = 45.0/57.0/image_width;
    float sin_pixel_y[image_height];
    for (int y = 0; y < image_height; ++y) {
      // Sign opposite x for y up values
      sin_pixel_y[y] = sin((image_height/ 2.0 - y)  * y_radians_per_pixel);
    }

    //X,Y,Z of the centroid
    float x = 0.0;
    float y = 0.0;
    float z = 1e6;
    //Number of points observed
    unsigned int n = 0;

    //Iterate through all the points in the region and find the average of the position
    const float* depth_row = reinterpret_cast<const float*>(&depth_msg->data[0]);
    int row_step = depth_msg->step / sizeof(float);
    for (int v = 0; v < (int)depth_msg->height; ++v, depth_row += row_step)
    {
     for (int u = 0; u < (int)depth_msg->width; ++u)
     {
       float depth = depth_image_proc::DepthTraits<float>::toMeters(depth_row[u]);
       if (!depth_image_proc::DepthTraits<float>::valid(depth) || depth > max_z_) continue;
       float y_val = sin_pixel_y[v] * depth;
       float x_val = sin_pixel_x[u] * depth;
       if ( y_val > min_y_ && y_val < max_y_ &&   //set min_ values
            x_val > min_x_ && x_val < max_x_)
       {
         x += x_val;
         y += y_val;
         z = std::min(z, depth); //approximate depth as forward.
         n++;
       }
     }
    }

    //If there are points, find the centroid and calculate the command goal.
    //If there are no points, simply publish a stop goal.
    if (n>4000)
    {
      x /= n;
      y /= n;        //At this point, x,y,z are the centroid coordinates
      if(z > max_z_){
        ROS_INFO_THROTTLE(1, "Centroid too far away %f, stopping the robot", z);}
       /* if (enabled_)
        {
          cmdpub_.publish(geometry_msgs::TwistPtr(new geometry_msgs::Twist()));//looks like twist() initializes all to 0.
        }
        return;
      }

      ROS_INFO_THROTTLE(1, "Centroid at %f %f %f with %d points", x, y, z, n);
      publishMarker(x, y, z); */
      
      if (enabled_)  //enabled
      {
	obstate=1;
	obstaclex= (z - goal_z_) * z_scale_;
	obstaclez=-x * x_scale_;
        //geometry_msgs::TwistPtr cmd(new geometry_msgs::Twist());  //set values of scales
	//cmd->linear.x = (z - goal_z_) * z_scale_;
        //cmd->angular.z = -x * x_scale_;
       // cmdpub_.publish(cmd);
      }
    }
   /* else
    {
      ROS_INFO_THROTTLE(1, "Not enough points(%d) detected, stopping the robot", n);
      publishMarker(x, y, z);

      if (enabled_)
      {
        cmdpub_.publish(geometry_msgs::TwistPtr(new geometry_msgs::Twist()));
      }
    }*/

  //  publishBbox();  // what happens if this is removed?
  }

























int main (int argc, char** argv)
{
  // Initialize ROS
  ros::init (argc, argv, "blob");
  ros::NodeHandle nh;
  //States variable 
  //state = 0;
  
  ros::Publisher cmdpub_ = nh.advertise<geometry_msgs::Twist> ("cmd_vel_mux/input/teleop", 10);

  //subscribe to /blobs topic 
  ros::Subscriber blobsSubscriber = nh.subscribe("/blobs", 10, blobsCallBack);
  ros::Subscriber sub_= nh.subscribe<sensor_msgs::Image>("camera/depth/image_rect", 1, imagecb ); // follower.cpp //&BotFollower::imagecb, this

  ros::Rate loop_rate(10);
  geometry_msgs::Twist t;

while(ros::ok()){

 if(obstate==0 and blobstate==0)// Cannot see obstacle any more
{

std::cout << "zero"<< std::endl;
std::cout << obstate<< std::endl;
std::cout << blobstate<< std::endl;
std::cout << gcounter<< std::endl;
if(gcounter>0)
{
gcounter--;
geometry_msgs::TwistPtr cmd(new geometry_msgs::Twist());
std::cout << "stop after obstacle"<< std::endl;
cmd->linear.x = 0.2;
cmd->angular.z = 0.0; // rotate until obstacle is in sight.
cmdpub_.publish(cmd);
}
else{
geometry_msgs::TwistPtr cmd(new geometry_msgs::Twist());
cmd->linear.x = 0;
cmd->angular.z = 0.5; // rotate until obstacle is in sight.
cmdpub_.publish(cmd);

}

} // the if block




 else if ((obstate==1 and blobstate==0) )
{
std::cout << gcounter<< std::endl;
if(gcounter<50)
{
gcounter+=1;
}
std::cout << "obstacle"<< std::endl;
std::cout << blobstate<< std::endl;
std::cout << obstate<< std::endl;

geometry_msgs::TwistPtr cmd(new geometry_msgs::Twist());
cmd->linear.x=0.0;
cmd->angular.z=0.5;
cmdpub_.publish(cmd);
} 


else if ((obstate==1 and blobstate ==1) or (obstate==0 and blobstate==1))
{	
	std::cout << "target"<< std::endl;
	std::cout << obstate<< std::endl;
	std::cout << blobstate<< std::endl;
	geometry_msgs::TwistPtr cmd(new geometry_msgs::Twist());
        cmd->linear.x = 0.2;
        cmd->angular.z = -(blob_x-350.0)*0.005;
        cmdpub_.publish(cmd);
}

    obstate=0;
    blobstate=0;
    // Spin
	ros::spinOnce();
	loop_rate.sleep();
  }

}

