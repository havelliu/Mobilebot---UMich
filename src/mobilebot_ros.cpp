/*******************************************************************************
 * mobilebot_ros.c
 *******************************************************************************/
 

//this lib was compiled for C so we need to be explicit
extern "C"
{
#include "rc_usefulincludes.h"
#include "roboticscape.h"
}


#include "mobilebot_config.h"
#include <iostream>
#include <ros/ros.h>
#include "std_msgs/String.h"
#include "geometry_msgs/Twist.h"
#include "geometry_msgs/Quaternion.h"
#include "sensor_msgs/Imu.h"
#include "mobilebot_msgs/MobileBotState.h"
#include "nav_msgs/Odometry.h"
#include <unistd.h>
#include <math.h>
#include <tf2_ros/transform_broadcaster.h>

float dutyL;  // left  motor PWM duty cycle
float dutyR;  // right motor PWM duty cycle

// global variables
ros::Publisher state_publisher;
ros::Publisher motor_publisher;
ros::Publisher odom_publisher;
/*******************************************************************************
 * Local Function declarations
 *******************************************************************************/
// IMU interrupt routine
void mobilebot_controller();

// threads
void* setpoint_manager(void* ptr);
void* printf_loop(void* ptr);

// regular functions
void on_pause_press();
void on_mode_release();
void RPYtoQuat(float roll, float pitch, float yaw, geometry_msgs::Quaternion& q);
int blink_green();
int blink_red();

/*******************************************************************************
 * Global Variables
 *******************************************************************************/
rc_imu_data_t imu_data;
float auto_linear_desired = 0.0;
float rc_linear_desired = 0.0;
float auto_angular_desired = 0.0;
float rc_angular_desired = 0.0;
float linear_desired = 0.0;
float angular_desired = 0.0;
int prevEncoderLeft = 0;
int currentEncoderLeft = 0;
int prevEncoderRight = 0;
int currentEncoderRight = 0;
float leftError = 0.0;
float rightError= 0.0;
float leftDistance = 0.0;
float rightDistance = 0.0;
float centerDistance = 0.0;
float x_pos = 0.0;
float y_pos = 0.0;
float x_pos_robot_frame = 0.0;
float y_pos_robot_frame = 0.0;
float angle = -3.141592/2;  //Angle of the robot with respect to the global frame
float currentTime = 0.0;
float pastTime = 0.0;
float turbo = 1.2;
float prevSpeedL = 0.0;
float prevSpeedR = 0.0;
float increment = 0.0;
float radius = 0.0;

rc_filter_t filter1 = rc_empty_filter();
rc_filter_t filter2 = rc_empty_filter();
rc_imu_data_t imu;
/*******************************************************************************
 * shutdown_signal_handler(int signo)
 *
 * catch Ctrl-C signal and change system state to EXITING
 * all threads should watch for get_state()==EXITING and shut down cleanly
 *******************************************************************************/
void ros_compatible_shutdown_signal_handler(int signo)
{
  if (signo == SIGINT)
    {
      rc_set_state(EXITING);
      ROS_INFO("\nReceived SIGINT Ctrl-C.");
      ros::shutdown();
    }
  else if (signo == SIGTERM)
    {
      rc_set_state(EXITING);
      ROS_INFO("Received SIGTERM.");
      ros::shutdown();
    }
}

void RC_CmdCallback(const geometry_msgs::Twist::ConstPtr& cmd_vel)
{
  rc_linear_desired = cmd_vel->linear.x;
  rc_angular_desired = cmd_vel->angular.z;

  return;
}

void auto_CmdCallBack(const geometry_msgs::Twist::ConstPtr& cmd_vel)
{
  auto_linear_desired = cmd_vel->linear.x;
  auto_angular_desired = cmd_vel->angular.z;

  return;
}

/*******************************************************************************
 * main()
 *
 * Initialize the filters, IMU, threads, & wait untill shut down
 *******************************************************************************/
int main(int argc, char** argv)
{
  // Announce this program to the ROS master as a "node" called "mobilebot_ros_node"
  ros::init(argc, argv, "mobilebot_ros_node");

  // Start the node resource managers (communication, time, etc)
  ros::start();
  ros::param::set("model", "burger");

  // Broadcast a simple log message
  ROS_INFO("File %s compiled on %s %s.",__FILE__, __DATE__, __TIME__);

  // Create nodehandle
  ros::NodeHandle mobilebot_node;

  // Advertise the topics this node will publish
 // state_publisher = mobilebot_node.advertise<mobilebot_msgs::MobileBotState>("mobilebot/state", 10);
  motor_publisher = mobilebot_node.advertise<geometry_msgs::Twist>("mobilebot/rc_cmd", 10);
  odom_publisher = mobilebot_node.advertise<nav_msgs::Odometry>("mobilebot/odom", 10);

  // subscribe the function CmdCallback to the topuc edumip/cmd
  ros::Subscriber sub_rc_cmd = mobilebot_node.subscribe("mobilebot/rc_cmd", 10, RC_CmdCallback); 
  ros::Subscriber auto_rc_cmd = mobilebot_node.subscribe("mobilebot/auto_cmd", 10, auto_CmdCallBack);

  if(rc_initialize()<0)
    {
      ROS_INFO("ERROR: failed to initialize cape.");
      return -1;
    }

  if(rc_initialize_dsm() == -1)
  {
	ROS_INFO("DSM not initialized, run binding routine");
  }
  rc_set_led(RED,1);
  rc_set_led(GREEN,0);
  rc_set_state(UNINITIALIZED);

  // set up button handlers
  rc_set_pause_pressed_func(&on_pause_press);
  rc_set_mode_released_func(&on_mode_release);

  // start printf_thread
  pthread_t  printf_thread;
  pthread_create(&printf_thread, NULL, printf_loop, (void*) NULL);

  // set up IMU configuration
  rc_imu_config_t imu_config = rc_default_imu_config();
  imu_config.dmp_sample_rate = SAMPLE_RATE_HZ;
  imu_config.orientation = ORIENTATION_Y_UP;

  // start imu
  if(rc_initialize_imu_dmp(&imu_data, imu_config)){
    ROS_INFO("ERROR: can't talk to IMU, all hope is lost\n");
    rc_blink_led(RED, 5, 5);
    return -1;
  }

  // overide the robotics cape default signal handleers with
  // one that is ros compatible
  signal(SIGINT,  ros_compatible_shutdown_signal_handler);
  signal(SIGTERM, ros_compatible_shutdown_signal_handler);

  // start balance stack to control setpoints
  pthread_t  setpoint_thread;
  pthread_create(&setpoint_thread, NULL, setpoint_manager, (void*)NULL);

  // this should be the last step in initialization
  // to make sure other setup functions don't interfere
  rc_set_imu_interrupt_func(&mobilebot_controller);

  //initialize encoders to 0
  rc_set_encoder_pos(ENCODER_CHANNEL_L,0);
  rc_set_encoder_pos(ENCODER_CHANNEL_R,0);

  rc_enable_saturation(&filter1, -1.0, 1.0);
  rc_enable_saturation(&filter2, -1.0, 1.0);

  if(rc_pid_filter(&filter1, 1.25, 0, .005, .04, .01))
  {
	ROS_INFO("FAILED TO MAKE MOTOR CONTROLLER");
  }
  if(rc_pid_filter(&filter2, 1.25, 0, .005, .04, .01))
  {
	ROS_INFO("FAILED TO MAKE MOTOR CONTROLLER");
  }

  // start in the RUNNING state, pressing the puase button will swap to
  // the PAUSED state then back again.
  ROS_INFO("\nMobilebot Initialized...\n");
  rc_set_state(RUNNING);

  // Process ROS callbacks until receiving a SIGINT (ctrl-c)
  ros::spin();

  // news
  ROS_INFO("Exiting!");

  // shut down the pthreads
  rc_set_state(EXITING);

  // Stop the ROS node's resources
  ros::shutdown();

  // cleanup
  rc_power_off_imu();
  rc_cleanup();
  return 0;
}

/*******************************************************************************
 * void* setpoint_manager(void* ptr)
 *
 * This thread is in charge of adjusting the controller setpoint based on user
 * inputs from dsm radio control. Also detects pickup to control arming the
 * controller.
 *******************************************************************************/
void* setpoint_manager(void* ptr)
{
  rc_usleep(2500000);
  rc_set_state(RUNNING);
  rc_set_led(RED,0);
  rc_set_led(GREEN,1);

  while(rc_get_state()!=EXITING)
    {
      // sleep at beginning of loop so we can use the 'continue' statement
      rc_usleep(1000000/SETPOINT_MANAGER_HZ);
      // nothing to do if paused, go back to beginning of loop
      if(rc_get_state() != RUNNING) continue;

      //if(rc_get_dsm_ch_normalized(6) > 0)
      //{
      //  control = 0;
      //}
      //else
      //{
      //  control = 1;
      //}
      geometry_msgs::Twist msg;

      msg.linear.x = rc_get_dsm_ch_normalized(3)*MAX_SPEED;
      msg.angular.z = rc_get_dsm_ch_normalized(4)*MAX_ANGULAR_SPEED;

      motor_publisher.publish(msg);
    }

  pthread_exit(NULL);
}

/*******************************************************************************
 * void mobilebot_controller()
 *
 * discrete-time controller operated off IMU interrupt
 * Called at SAMPLE_RATE_HZ
 *******************************************************************************/
void mobilebot_controller()
{
  /*************************************************************
   * check for various exit conditions AFTER state estimate
   ***************************************************************/

 if(rc_get_state()==EXITING){
    rc_disable_motors();
    return;
  }

  prevEncoderLeft = currentEncoderLeft;
  currentEncoderLeft = rc_get_encoder_pos(ENCODER_CHANNEL_L);
  prevEncoderRight = currentEncoderRight;
  currentEncoderRight = -rc_get_encoder_pos(ENCODER_CHANNEL_R);
  prevSpeedL = dutyL;
  prevSpeedR = dutyR;

  leftDistance = (currentEncoderLeft - prevEncoderLeft)*WHEEL_DIA*PI/ENC_COUNT_REV;
  rightDistance = (currentEncoderRight - prevEncoderRight)*WHEEL_DIA*PI/ENC_COUNT_REV;
  centerDistance = (leftDistance + rightDistance)/2;
  

  leftError = leftDistance/IMU_PERIOD;
  rightError = rightDistance/IMU_PERIOD;

  if(rc_get_dsm_ch_normalized(6) > 0.0)
  {
	linear_desired = rc_linear_desired;
	angular_desired = rc_angular_desired;
  }
  else
  {
	linear_desired = auto_linear_desired;
	angular_desired = auto_angular_desired;
  }

  if(rc_get_dsm_ch_normalized(5) > 0.0)
  {
	turbo = 1.6;
  }
  else
  {
	turbo = 1.2;
  }

  linear_desired = linear_desired*turbo;

  if(fabs(angular_desired) < .1)
  {
    leftError = (linear_desired - leftError)/MAX_SPEED;
    rightError = (linear_desired - rightError)/MAX_SPEED;
  }
  else
  {

      leftError = (linear_desired/MAX_SPEED - angular_desired/MAX_ANGULAR_SPEED) - leftError/MAX_SPEED;
      rightError = (linear_desired/MAX_SPEED + angular_desired/MAX_ANGULAR_SPEED) - rightError/MAX_SPEED;
  }

  dutyL = rc_march_filter(&filter1, leftError);
  dutyR = rc_march_filter(&filter2, rightError);

  increment = (rightDistance - leftDistance)/TRACK_WIDTH;

  if(angle >= 2*3.141592)
  {
	angle = angle - 2*3.141592;
  }
  if(angle <= -2*3.141592)
  {
	angle = angle + 2*3.141592;
  }

  x_pos_robot_frame = -centerDistance*sin(increment);
  y_pos_robot_frame = centerDistance*cos(increment);

  x_pos += x_pos_robot_frame*sin(angle) + y_pos_robot_frame*cos(angle);
  y_pos += y_pos_robot_frame*sin(angle) - x_pos_robot_frame*cos(angle);

  angle += increment;

  geometry_msgs::Quaternion q;

  RPYtoQuat(0, 0, angle, q);

  //create Transform that send messages to tf
  static tf2_ros::TransformBroadcaster odom_broadcaster;

  //Transform Code
  geometry_msgs::TransformStamped odom_trans;
  odom_trans.header.stamp = ros::Time::now();
  odom_trans.header.frame_id = "/odom";
  odom_trans.child_frame_id = "/mobilebot";

  odom_trans.transform.translation.x = x_pos*.3048;
  odom_trans.transform.translation.y = y_pos*.3048;
  odom_trans.transform.rotation.x = q.x;
  odom_trans.transform.rotation.y = q.y;
  odom_trans.transform.rotation.z = q.z;
  odom_trans.transform.rotation.w = q.w;

  odom_broadcaster.sendTransform(odom_trans);

  nav_msgs::Odometry odom;
  odom.header.stamp = ros::Time::now();
  odom.header.frame_id = "/odom";
  odom.child_frame_id = "/mobilebot";
  odom.pose.pose.position.x = x_pos*.3048;
  odom.pose.pose.position.y = y_pos*.3048;
  odom.pose.pose.orientation = q;
  odom.twist.twist.linear.x = (leftDistance + rightDistance)/2/IMU_PERIOD*.3048;
  odom.twist.twist.angular.z = increment/IMU_PERIOD;

  odom_publisher.publish(odom);

  if(fabs(dutyL - prevSpeedL) > .2)
  {
	dutyL = dutyL - (dutyL - prevSpeedL)*.9;
  }
  if(fabs(dutyR - prevSpeedR) > .2)
  {
	dutyR = dutyR - (dutyR - prevSpeedR)*.9;
  }
  
  rc_set_motor(MOTOR_CHANNEL_L, MOTOR_POLARITY_L * dutyL);
  rc_set_motor(MOTOR_CHANNEL_R, MOTOR_POLARITY_R * dutyR);

  return;
}


/*******************************************************************************
 * printf_loop()
 *
 * prints diagnostics to console
 * this only gets started if executing from terminal
 *******************************************************************************/
void* printf_loop(void* ptr)
{
  rc_state_t last_rc_state, new_rc_state; // keep track of last state
  mobilebot_msgs::MobileBotState  mobilebot_state;

  new_rc_state = rc_get_state();

  while(rc_get_state()!=EXITING)
    {
      last_rc_state = new_rc_state;
      new_rc_state = rc_get_state();
      // publish the state
      //state_publisher.publish(mobilebot_state);


      rc_usleep(1000000 / PRINTF_HZ);

    }

  pthread_exit(NULL);
  //return NULL;
}

/*******************************************************************************
 * void on_pause_press()
 *
 *******************************************************************************/
void on_pause_press()
{
	const int samples = 100;
	const int us_wait = 2000000;

	switch(rc_get_state())
	{
		case EXITING:
		return;

		case RUNNING:
		rc_set_state(PAUSED);
		rc_set_led(RED,1);
		rc_set_led(GREEN,0);
		break;

		case PAUSED:
		rc_set_state(RUNNING);
		rc_set_led(GREEN,1);
		rc_set_led(RED,0);
		break;

		default:
		break;
	}

	//while(i < samples)
	//{
	//	rc_usleep(us_wait/samples);

	//	if(rc_get_pause_button() == RELEASED)
	//	{
	//		return;
	//	}
	//	++i;
	//}

	ROS_INFO("long press detected, shuting down/n");

	rc_blink_led(RED,5,2);
	rc_set_state(EXITING);
	return;
}

/*******************************************************************************
 * void on_mode_release()
 * toggle between position and angle modes if MiP is paused
 *******************************************************************************/
void on_mode_release()
{
	rc_blink_led(GREEN,5,1);
	return;
}

void RPYtoQuat(float roll, float pitch, float yaw, geometry_msgs::Quaternion& q)
{
	double cy = cos(yaw*.5);
	double sy = sin(yaw*.5);
	double cr = cos(roll*.5);
	double sr = sin(roll*.5);
	double cp = cos(pitch*.5);
	double sp = sin(pitch*.5);

	q.w = cy*cr*cp + sy*sr*sp;
	q.x = cy*sr*cp - sy*cr*sp;
	q.y = cy*cr*sp + sy*sr*cp;
	q.z = sy*cr*cp - cy*sr*sp;
}
