/*******************************************************************************
* mobilebot_config.h
*
* Contains the settings for configuration of mobilebot_ros.cpp
*******************************************************************************/

#ifndef MOBILEBOT_CONFIG
#define MOBILEBOT_CONFIG

#define SAMPLE_RATE_HZ 100	// main filter and control loop speed
#define DT 0.01			// 1/sample_rate

// Structural properties of mobilebot
#define CAPE_MOUNT_ANGLE		0.0
#define GEARBOX 				34.014
#define RPM				220
#define MAX_SPEED			3 //ft/s
#define MAX_ANGULAR_SPEED		4.23529 //rad/s
#define ENCODER_RES				48
#define ENC_COUNT_REV			1632.67
#define WHEEL_DIA			.260417
#define WHEEL_RADIUS_M			0.08
#define TRACK_WIDTH			.67708333 //track width in ft

// steering controller
#define D3_KP					1.0
#define D3_KI					0.3
#define D3_KD					0.05

// electrical hookups
#define MOTOR_CHANNEL_L			2
#define MOTOR_CHANNEL_R			1
#define MOTOR_POLARITY_L		1
#define MOTOR_POLARITY_R		1
#define ENCODER_CHANNEL_L		2
#define ENCODER_CHANNEL_R		1
#define ENCODER_POLARITY_L		1
#define ENCODER_POLARITY_R		1

// DSM channel config
#define DSM_DRIVE_POL			1
#define DSM_TURN_POL			-1
#define DSM_DRIVE_CH			3
#define DSM_TURN_CH				2
#define DSM_DEAD_ZONE			0.04

// Thread Loop Rates
#define SETPOINT_MANAGER_HZ		100
#define BATTERY_CHECK_HZ			5
#define IMU_PERIOD			.01
#define PRINTF_HZ			5

#endif	//BALANCE_CONFIG
