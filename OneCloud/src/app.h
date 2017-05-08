/*
 * app.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef APP_H_
#define APP_H_


#define APP_EVENT_IDLE					0x0001
#define APP_EVENT_SETUP_WEB_CONN		0x0002
#define APP_EVENT_RELEASE_WEB_CONN		0x0003
#define APP_EVENT_SEND_WEB_STATUS		0x0004
#define APP_EVENT_CALL_STARTUP			0x0005
#define APP_EVENT_CALL_SHUTDOWN			0x0006
#define APP_EVENT_CHECK_STARTUP			0x0007
#define APP_EVENT_CHECK_SHUTDOWN		0x0008
#define APP_EVENT_CALL_CABINET_QUERY	0x0009
#define APP_EVENT_CALL_SERVER_QUERY		0x000A
#define APP_EVENT_SEND_WEB_HEART_BEAT	0x000B

#define APP_WEB_URL_MAX_LEN	512


#define APP_JSON_STR_UUID "uuid"
#define APP_JSON_STR_POID "poid"
#define APP_JSON_STR_OPERATION "operation"
#define APP_JSON_STR_PASSWORD "password"
#define APP_JSON_STR_TIME "time"
#define APP_JSON_STR_STATUS "status"


#define APP_SHELL_POST_CHECK_STATUS "/opt/onecloud/script/app/post_control_status.sh"
#define APP_SHELL_POST_QUERY_STATUS "/opt/onecloud/script/app/post_query_status.sh"
#define APP_SHELL_POST_OPERATION_COMMAND "/opt/onecloud/script/app/post_command_operation.sh"

/* Operation from APP*/
typedef struct oc_app_operation{
	char uuid[40];
	char poid[40];
	char operation[40];
	char password[40];
	char time[40];
	int  is_finish;
	time_t start_time;
	time_t end_time;
} OC_APP_OPERATION;



#endif /* APP_H_ */
