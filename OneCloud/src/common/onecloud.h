/*
 * onecloud.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef ONECLOUD_H_
#define ONECLOUD_H_

////////////////////////////////////////////////////////////
// structure and define
////////////////////////////////////////////////////////////
#define OC_TRUE		1
#define OC_FALSE	0

#define OC_SUCCESS	0
#define OC_FAILURE	-1

#define OC_DEBUG_LEVEL_DISABLE	0
#define OC_DEBUG_LEVEL_ERROR	1
#define OC_DEBUG_LEVEL_WARN		2
#define OC_DEBUG_LEVEL_INFO		3
#define OC_DEBUG_LEVEL_DEBUG	4

// modify on compile
#define OC_DEBUG_LEVEL_DEFAULT	5


#define OC_FLAG_ENABLE 1
#define OC_FLAG_DISABLE 0
#define OC_FLAG_UP 1
#define OC_FLAG_DOWN 0

#define OC_HEALTH_OK	0
#define OC_HEALTH_ERROR	1

#define OC_LOCAL_IP	"127.0.0.1"

#define OC_DEFAULT_CONFIG_FILE "./config/onecloud.ini"

#define OC_MAIN_DEFAULT_PORT		17001
#define OC_WATCHDOG_DEFAULT_PORT	17002
#define OC_GPIO_DEFAULT_PORT		17003
#define OC_SIM_DEFAULT_PORT			17004
#define OC_SCRIPT_DEFAULT_PORT		17005
#define OC_APP_DEFAULT_PORT			17006
#define OC_COM0_DEFAULT_PORT		17010
#define OC_COM1_DEFAULT_PORT		17011
#define OC_COM2_DEFAULT_PORT		17012
#define OC_COM3_DEFAULT_PORT		17013
#define OC_COM4_DEFAULT_PORT		17014
#define OC_COM5_DEFAULT_PORT		17015
#define OC_TEMPERATURE_PORT			17016
#define OC_VOICE_PORT				17017
#define OC_GPS_PORT					17018
#define OC_GPSUSB_PORT				17019
#define OC_LBS_PORT					17020

#define OC_MAX_CONN_LIMIT	10
#define OC_TIMER_DEFAULT_SEC	1
#define OC_SERVER_STATUS_CHECK_SEC	60
#define OC_CABINET_STATUS_CHECK_SEC	10
#define OC_APP_CONTROL_CHECK_SEC	10
#define OC_APP_HEART_BEAT_SEC	71
#define OC_WATCH_DOG_FEED_SEC	5
#define OC_WATCH_DOG_SOFT_RESET_SEC	60

// 60 = 0x3C
//#define OC_WATCH_DOG_HARD_RESET_SEC	0x3C
// 90 = 0x5A
//#define OC_WATCH_DOG_HARD_RESET_SEC	0x5A
// 120 = 0x78
#define OC_WATCH_DOG_HARD_RESET_SEC	0x78


#define OC_SRV_TYPE_SERVER		0
#define OC_SRV_TYPE_SWITCH		1
#define OC_SRV_TYPE_FIREWALL	2
#define OC_SRV_TYPE_RAID		3
#define OC_SRV_TYPE_ROUTER		4
#define OC_SRV_TYPE_UPS			5

#define OC_COM_TYPE_TEMPERATURE		1
#define OC_COM_TYPE_ELECTRICITY		2
#define OC_COM_TYPE_VOICE_MONITOR	3


#define OC_CABINET_STATUS_STOPPED	0
#define OC_CABINET_STATUS_RUNNING	1
#define OC_CABINET_STATUS_STARTING	2
#define OC_CABINET_STATUS_STOPPING	3



#define OC_STARTUP_TOTAL_STEP	4
#define OC_SHUTDOWN_TOTAL_STEP	4

#define OC_GPS_COORD_UNDEF 200.0F  // gps coordinate undefined
#define OC_LBS_BSTN_UNDEF 80000    // lbs base station undefined

#endif /* ONECLOUD_H_ */
