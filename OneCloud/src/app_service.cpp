/*
 * app_client.c
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"
#include "common/error.h"
#include "util/easywsclient.hpp"
#include "util/string_helper.h"
#include "util/script_helper.h"
#include "util/http_helper.h"
#include "util/log_helper.h"
#include "util/date_helper.h"
#include "watch_dog.h"

#include "app.h"

using easywsclient::WebSocket;
static WebSocket::pointer app_center_ws = NULL;


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

///////////////////////////////////////////////////////////
// function definition
///////////////////////////////////////////////////////////
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int app_event_machine();
int event_idle_process();
int event_setup_web_connection_process();
int event_release_web_connection_process();
int event_send_web_status_process();
int event_call_startup_process();
int event_call_shutdown_process();
int event_check_startup_process();
int event_check_shutdown_process();
int event_call_cabinet_query_process();
int event_call_server_query_process();
int event_send_web_heart_beat_process();
void timer_event_process();
void timer_event_watchdog_heartbeat();
void web_make_startup_status_parameters(OC_CMD_QUERY_STARTUP_RESP* resp, char* uuid,
		char* custome_args, char* json_params, int is_error);
void web_make_shutdown_status_parameters(OC_CMD_QUERY_SHUTDOWN_RESP* resp, char* uuid,
		char* custome_args, char* json_params, int is_error);
void web_make_cabinet_status_parameters(OC_CMD_QUERY_CABINET_RESP* query_cabinet,
		OC_CMD_QUERY_SERVER_RESP* query_server, char* custome_args, char* json_params);
void web_operation_error_response(char* uuid, char* poid, char* operation,
		enum error_code err_code);
void web_make_post_command_parameters(char* operation, char* uuid, char* custome_args,
		char* json_params);
int web_generate_command_uuid(char* out_uuid);

///////////////////////////////////////////////////////////
// Global variables definition
///////////////////////////////////////////////////////////
// Debug output level
static int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;

// APP service thread ID
pthread_t g_app_thread_id = 0;
int g_app_running = OC_FALSE;
int g_current_app_event = APP_EVENT_IDLE;
int g_next_app_event = APP_EVENT_IDLE;
pthread_t g_web_thread_id = 0;
int g_web_running = OC_FALSE;

// Web transmit buffer
uint8_t web_tx_buf[OC_WEB_PKG_MAX_LEN];
uint8_t web_rx_buf[OC_WEB_PKG_MAX_LEN];
uint8_t shell_invoke_buf[MAX_SCRIPT_SHELL_BUFFER];

// Timer thread
pthread_t g_timer_thread_id = 0;
int g_timer_running = OC_FALSE;

pthread_t g_monitor_ethernet_id = 0; //used to monitor ethernet

// Heart beat & watch dog
int g_app_heart_beat_counter = 0;
int g_watch_dog_counter = 0;
int g_startup_check_counter = -1;    // -1:disable, 0:check query, >0:counting
int g_shutdown_check_counter = -1;   // -1:disable, 0:check query, >0:counting

// APP command
OC_APP_OPERATION g_app_operation;
OC_APP_OPERATION g_recv_operation;
int g_cabinet_status = OC_CABINET_STATUS_STOPPED;

// Auth info
char g_poid[MAX_PARAM_VALUE_LEN];
char g_access_key[MAX_PARAM_VALUE_LEN];
char g_access_secret[MAX_PARAM_VALUE_LEN];
char g_busi_username[MAX_PARAM_VALUE_LEN];
char g_busi_password[MAX_PARAM_VALUE_LEN];
int g_enable_remote_operation = OC_TRUE;

int g_debug_control_flag = OC_FALSE;
int g_debug_count = 0;

#define OC_TRANSLATE_STATUS(a)  (a==OC_CABINET_STATUS_STOPPED ? "stopped" :\
		(a==OC_CABINET_STATUS_RUNNING ? "running":\
				(a==OC_CABINET_STATUS_STARTING ? "starting":\
						(a==OC_CABINET_STATUS_STOPPING ? "stopping":"unknown"))))

#define OC_TRANSLATE_SRV_TYPE(a)  (a==OC_SRV_TYPE_SERVER ? "server" :\
		(a==OC_SRV_TYPE_SWITCH ? "switch":\
				(a==OC_SRV_TYPE_FIREWALL ? "firewall":\
						(a==OC_SRV_TYPE_RAID ? "raid":\
								(a==OC_SRV_TYPE_ROUTER ? "router":\
										(a==OC_SRV_TYPE_UPS ? "ups":"unknown"))))))

void translate_status(int status_code, char* status_str) {
	if (status_code == OC_CABINET_STATUS_STOPPED)
		strcpy(status_str, "stopped");
	else if (status_code == OC_CABINET_STATUS_RUNNING)
		strcpy(status_str, "running");
	else if (status_code == OC_CABINET_STATUS_STARTING)
		strcpy(status_str, "starting");
	else if (status_code == OC_CABINET_STATUS_STOPPING)
		strcpy(status_str, "stopping");
	else
		strcpy(status_str, "unknown");
}

void translate_server_type(int type_code, char* type_str) {
	if (type_code == OC_SRV_TYPE_SERVER)
		strcpy(type_str, "server");
	else if (type_code == OC_SRV_TYPE_SWITCH)
		strcpy(type_str, "switch");
	else if (type_code == OC_SRV_TYPE_FIREWALL)
		strcpy(type_str, "firewall");
	else if (type_code == OC_SRV_TYPE_RAID)
		strcpy(type_str, "raid");
	else if (type_code == OC_SRV_TYPE_ROUTER)
		strcpy(type_str, "router");
	else if (type_code == OC_SRV_TYPE_UPS)
		strcpy(type_str, "ups");
	else
		strcpy(type_str, "unknown");
}

/**
 * Signal handler
 */
void sig_handler(const int sig) {
	// just for debug
//	if (g_debug_control_flag == OC_FALSE && g_debug_count == 0) {
//		g_debug_count++;
//		g_debug_control_flag = OC_TRUE;
//		return;
//	}
	// release sub thread
	g_app_running = OC_FALSE;
	g_timer_running = OC_FALSE;
	g_web_running = OC_FALSE;
	sleep(1);

	log_info_print(g_debug_verbose, "SIGINT handled.");
	log_info_print(g_debug_verbose, "Now stop APP service.");

	exit(EXIT_SUCCESS);
}

/*
 * Load configuration main daemon
 */
int load_config(char *config) {
	int i = 0;
	char temp_value[128];
	int temp_int = 0;
	int result = OC_FAILURE;
	int com_count = 0;

	// Load configuration from file
	result = load_config_from_file(config);
	if (result == OC_FAILURE) {
		log_error_print(g_debug_verbose, "load config file failure.[%s]", config);
		return result;
	}
	print_config_detail();

	g_config = (struct settings*) malloc(sizeof(struct settings));
	memset((void*) g_config, 0, sizeof(struct settings));

	g_config->role = role_app_service;

	temp_int = 0;
	get_parameter_int(SECTION_APP_SERVICE, "debug", &temp_int, OC_DEBUG_LEVEL_DISABLE);
	g_debug_verbose = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "listen_port", &temp_int,
	OC_MAIN_DEFAULT_PORT);
	g_config->main_listen_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_WATCHDOG, "listen_port", &temp_int,
	OC_WATCHDOG_DEFAULT_PORT);
	g_config->watchdog_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_APP_SERVICE, "listen_port", &temp_int,
	OC_APP_DEFAULT_PORT);
	g_config->app_port = temp_int;

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_APP_SERVICE, "app_center_ws_url", temp_value,
			"ws://192.168.0.52:8080/wsapi/powerone/operation.ws");
	strcpy(g_config->app_center_ws_url, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_APP_SERVICE, "app_post_result_url", temp_value,
			"http://192.168.0.52:8080/api/powerone/operation/results");
	strcpy(g_config->app_post_result_url, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_APP_SERVICE, "app_post_status_url", temp_value,
			"http://192.168.0.52:8080/api/powerone/status");
	strcpy(g_config->app_post_status_url, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_APP_SERVICE, "app_post_command_url", temp_value,
			"http://192.168.0.52:8100/api/powerone/operation/commands");
	strcpy(g_config->app_post_command_url, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_APP_SERVICE, "cabinet_auth_file", temp_value,
			"/opt/onecloud/config/cabinet.auth");
	strcpy(g_config->cabinet_auth_file, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_APP_SERVICE, "control_auth_file", temp_value,
			"/opt/onecloud/config/control.auth");
	strcpy(g_config->control_auth_file, temp_value);

	release_config_map();

	// auth info
	memset((void*) g_poid, 0, sizeof(g_poid));
	memset((void*) g_access_key, 0, sizeof(g_access_key));
	memset((void*) g_access_secret, 0, sizeof(g_access_secret));
	memset((void*) g_busi_username, 0, sizeof(g_busi_username));
	memset((void*) g_busi_password, 0, sizeof(g_busi_password));
	if (strlen(g_config->cabinet_auth_file) > 0) {
		load_cabinet_auth_from_file(g_config->cabinet_auth_file, g_poid, g_access_key,
				g_access_secret);
	}
	if (strlen(g_config->control_auth_file) > 0) {
		load_control_auth_from_file(g_config->control_auth_file, g_busi_username, g_busi_password,
				&g_enable_remote_operation);
	}
	g_config->remoteOperation = g_enable_remote_operation;

	log_debug_print(g_debug_verbose, "g_poid=[%s]", g_poid);
	log_debug_print(g_debug_verbose, "g_access_key=[%s]", g_access_key);
	log_debug_print(g_debug_verbose, "g_access_secret=[%s]", g_access_secret);
	log_debug_print(g_debug_verbose, "g_busi_username=[%s]", g_busi_username);
	log_debug_print(g_debug_verbose, "g_busi_password=[%s]", g_busi_password);
	log_debug_print(g_debug_verbose, "g_enable_remote_operation=[%d]", g_enable_remote_operation);

	return OC_SUCCESS;
}

/**
 * Business thread
 */
static void* thread_business_handle(void * sock_fd) {
	int fd = *((int *) sock_fd);
	int recv_bytes = 0;
	int send_bytes = 0;
	unsigned char data_recv[OC_PKG_MAX_LEN];
	unsigned char data_send[OC_PKG_MAX_LEN];
	int result = OC_SUCCESS;
	int n_send = 0;
	int offset = -1;

	memset(data_recv, 0, OC_PKG_MAX_LEN);
	memset(data_send, 0, OC_PKG_MAX_LEN);
	recv_bytes = net_read_package(fd, data_recv, OC_PKG_MAX_LEN, &offset);
	if (recv_bytes > 0) {
		result = main_business_process(data_recv + offset, recv_bytes, data_send, &send_bytes);
		if (result == OC_SUCCESS) {
			n_send = net_write_package(fd, data_send, send_bytes);
			if (n_send > 0) {
				// success
			} else {
				if (n_send == 0) {
					log_warn_print(g_debug_verbose, "Socket[%d] closed by client.", fd);
				} else {
					log_error_print(g_debug_verbose, "Socket[%d] write data error.", fd);
				}
			}
		} else {
			log_error_print(g_debug_verbose, "Business process failure.", fd);
		}
	} else {
		if (recv_bytes == 0) {
			log_warn_print(g_debug_verbose, "Socket[%d] closed by client.", fd);
		} else {
			log_error_print(g_debug_verbose, "Socket[%d] read data error.", fd);
		}
	}

	//Clear
	log_info_print(g_debug_verbose, "terminating current client_connection...");
	close(fd);            //close a file descriptor.
	pthread_exit(NULL);   //terminate calling thread!

	return NULL;
}

/**
 * Update APP event
 */
int update_app_event(int new_event) {
	// need add mutex
	g_current_app_event = new_event;

	return OC_SUCCESS;
}

/**
 * Set next APP event
 */
int set_next_app_event(int new_event) {
	// need add mutex

	int retry = 20;
	while (retry > 0) {
		if (g_next_app_event == APP_EVENT_IDLE) {
			g_next_app_event = new_event;
			break;
		}

		retry--;
		usleep(1000); //wait a little time (1ms)
	}

	return OC_SUCCESS;
}

/**
 * APP service thread
 */
void* thread_app_service_handle(void * param) {

	int next_event = APP_EVENT_IDLE;
	// Main event loop
	while (g_app_running) {

		next_event = app_event_machine();

		// 1st: do all step of current event chain
		if (next_event != APP_EVENT_IDLE) {
			update_app_event(next_event);
			continue;
		}

		// 2st: do the next event chain
		if (next_event == APP_EVENT_IDLE && g_next_app_event != APP_EVENT_IDLE) {
			update_app_event(g_next_app_event);
			g_next_app_event = APP_EVENT_IDLE;
			continue;
		}

		// 3st: idle event, sleep a while
		update_app_event(next_event);
		if (next_event == APP_EVENT_IDLE)
			usleep(1000 * 50);   //50ms
	}

	log_info_print(g_debug_verbose, "terminating APP service thread.");
	pthread_exit(NULL);   //terminate calling thread!

	return NULL;
}

/**
 * Validate the control command from WebSocket.
 */
int web_validate_control_command(char* uuid, char* poid, char* operation, char* password,
		char* time) {
	int result = OC_FAILURE;
	int read_result = OC_FAILURE;

	char busi_password[MAX_PARAM_VALUE_LEN];
	memset((void*) busi_password, 0, sizeof(busi_password));

	if (strlen(g_config->control_auth_file) > 0) {
		read_result = load_control_password_from_file(g_config->control_auth_file, busi_password);
	}

	if (read_result == OC_SUCCESS) {
		if (strcmp(password, busi_password) == 0)
			result = OC_SUCCESS;

		// update password in memory
		strcpy(g_busi_password, busi_password);
	}

	return result;
}

/**
 * WebSocket message handle
 */
void handle_web_message(const std::string & message) {
	printf(">>> %s\n", message.c_str());

	CONFIG_STR_PAIR* params = NULL;
	int param_num = 0;
	int result = OC_SUCCESS;
	int found = OC_FALSE;
	char uuid[40];
	char poid[40];
	char operation[40];
	char password[40];
	char time[40];
	int valid_success = OC_SUCCESS;
	int enable_operation = OC_FALSE;

	// message in JSON format, smallest length is 4 . eg "{x:y}"
	int len = message.length();
	if (len > 3) {
		result = simple_json_parse((char*) message.c_str(), len, &params, &param_num);
		memset((void*) (&g_recv_operation), 0, sizeof(OC_APP_OPERATION));

		if (result == OC_SUCCESS && param_num > 0) {
			memset((void*) uuid, 0, sizeof(uuid));
			memset((void*) poid, 0, sizeof(poid));
			memset((void*) operation, 0, sizeof(operation));
			memset((void*) password, 0, sizeof(password));
			memset((void*) time, 0, sizeof(time));
			found = str_get_json_field_value(params, param_num, APP_JSON_STR_UUID, uuid);
			if (found == OC_TRUE)
				found = str_get_json_field_value(params, param_num, APP_JSON_STR_POID, poid);
			if (found == OC_TRUE)
				found = str_get_json_field_value(params, param_num, APP_JSON_STR_OPERATION,
						operation);
			if (found == OC_TRUE)
				found = str_get_json_field_value(params, param_num, APP_JSON_STR_PASSWORD,
						password);
			if (found == OC_TRUE)
				found = str_get_json_field_value(params, param_num, APP_JSON_STR_TIME, time);

			// parameters invalid
			if (strlen(uuid) < 8 || strlen(poid) < 8 || strlen(operation) < 1
					|| strlen(password) < 1) {
				// send the error response
				web_operation_error_response(uuid, poid, operation, errcode_parameters_invalid);
				valid_success = OC_FAILURE;
			}

			// remote operation forbidden
			load_control_remoteOperation_from_file(g_config->control_auth_file, &enable_operation);
			g_enable_remote_operation = enable_operation;
			if (valid_success == OC_SUCCESS && g_enable_remote_operation == OC_FALSE) {
				// send the error response
				web_operation_error_response(uuid, poid, operation,
						errcode_remote_operation_forbidden);
				valid_success = OC_FAILURE;
			}

			// poid invalid
			if (valid_success == OC_SUCCESS && strcmp(g_poid, poid) != 0) {
				// send the error response
				web_operation_error_response(uuid, poid, operation, errcode_invalid_poid);
				valid_success = OC_FAILURE;
			}

			if (valid_success == OC_SUCCESS) {
				result = web_validate_control_command(uuid, poid, operation, password, time);
				if (result == OC_SUCCESS) {
					if (strlen(g_app_operation.operation) > 0 && g_app_operation.is_finish == 0) {
						// exist a operation not finish, send the error response
						web_operation_error_response(uuid, poid, operation,
								errcode_powerone_is_busy);
					} else {
						// do operation
						strcpy(g_recv_operation.uuid, uuid);
						strcpy(g_recv_operation.poid, poid);
						strcpy(g_recv_operation.operation, operation);
						strcpy(g_recv_operation.password, password);
						strcpy(g_recv_operation.time, time);

						if (strcmp(operation, "start") == 0) {
							update_app_event(APP_EVENT_CALL_STARTUP);
						} else if (strcmp(operation, "stop") == 0) {
							update_app_event(APP_EVENT_CALL_SHUTDOWN);
						} else if (strcmp(operation, "restart") == 0) {

						}
					}
				} else {
					// auth failed, send the error response
					web_operation_error_response(uuid, poid, operation, errcode_auth_failed);
					valid_success = OC_FAILURE;
				}
			}

		}
	}
	if (params != NULL)
		free(params);

//	if (message == "close") {
//		app_center_ws->close();
//	} else if (message == "startup") {
//		update_app_event(APP_EVENT_CALL_STARTUP);
//	} else if (message == "shutdown") {
//		update_app_event(APP_EVENT_CALL_SHUTDOWN);
//	}
}

/**
 * Web connection thread
 */
static void* thread_web_connection_handle(void * param) {

	int next_event = APP_EVENT_IDLE;
	int retry = 10;
	char ws_url[APP_WEB_URL_MAX_LEN];
	long timestamp = 0L;
	// Web thread loop
	while (g_web_running) {
		memset((void*) ws_url, 0, sizeof(ws_url));

		if (app_center_ws == NULL) {
			timestamp = time(NULL);
			make_sign_web_url(g_config->app_center_ws_url, timestamp * 1000, g_access_key,
					g_access_secret, ws_url);
			log_info_print(g_debug_verbose, "ws_url=[%s]", ws_url);

			app_center_ws = WebSocket::from_url(ws_url);
			if (app_center_ws == NULL)
				retry--;
			else {
				retry = 5;
				log_info_print(g_debug_verbose, "Connect to %s success.",
						g_config->app_center_ws_url);
			}
		}
		if (app_center_ws != NULL) {
			if (app_center_ws->getReadyState() == WebSocket::CLOSED) {
				delete app_center_ws;
				app_center_ws = NULL;
				continue;
			}
			// Send the message
			if (strlen((const char*) web_tx_buf) > 0) {
				std::string tx_message = (const char*) web_tx_buf;
				app_center_ws->send(tx_message);

				//clear the tx buffer
				memset((void*) web_tx_buf, 0, sizeof(web_tx_buf));
			}
			// Receive the message
			if (app_center_ws->getReadyState() != WebSocket::CLOSED) {
				app_center_ws->poll(10); // 10ms
				app_center_ws->dispatch(handle_web_message);
			}

		}
		if(retry <= 0) {
			sleep(5);
			retry = 5;
		}
	}

	log_info_print(g_debug_verbose, "terminating Web connection thread.");
	pthread_exit(NULL);   //terminate calling thread!

	return NULL;
}

/**
 * Timer thread
 */
static void* thread_timer_handle(void * param) {

	struct timeval delay_time;
	delay_time.tv_sec = OC_TIMER_DEFAULT_SEC;
	delay_time.tv_usec = 0;
	// timer thread loop
	while (g_timer_running) {
		//select(0, NULL, NULL, NULL, &delay_time);
		usleep(OC_TIMER_DEFAULT_SEC * 1000 * 1000);
		timer_event_process();
	}

	log_info_print(g_debug_verbose, "terminating Timer thread.");
	pthread_exit(NULL);

	return NULL;
}


/**
 * Monitor ethernet thread
 */
static void* monitor_ethernet_handle(void * param) {
	FILE * fp;
	char c;
	char ping_stream[512] = {0};
	char * str;
	struct timeval delay_time;
	delay_time.tv_sec = 30;
	delay_time.tv_usec = 0;
	// timer thread loop
	char time_temp_buf[32];
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	char * ping_cmd= "ping www.baidu.com -c 3 -W 3 -I enp1s0";
	int i;
	int result = OC_SUCCESS;
	while (g_timer_running) {
        	log_info_print(g_debug_verbose, "monitor ethernet handle is running begin...");
#if 1
		fp = popen(ping_cmd,"r");
        	//log_info_print(g_debug_verbose, "a");
                i=0;
		while((c=fgetc(fp)) != EOF)
		{
        		//log_info_print(g_debug_verbose, "b");
			ping_stream[i++] =c;
		}
        	//log_info_print(g_debug_verbose, "c");
		pclose(fp);
        	//log_info_print(g_debug_verbose, "d");
        	log_info_print(g_debug_verbose, "ping stream is %s",ping_stream);
		str = strstr(ping_stream,"100% packet loss");
		if(str != NULL)
		{
			log_info_print(g_debug_verbose, "route del....");
			system("route del default");
		}
		else
		{
			system("route add default gw 172.28.101.1 enp1s0");
		}
#endif
		select(0,NULL,NULL,NULL,&delay_time);
        	log_info_print(g_debug_verbose, "monitor ethernet handle is running end...");
	}

	log_info_print(g_debug_verbose, "terminating Timer thread.");
	pthread_exit(NULL);

	return NULL;
}

/**
 * Main daemon
 */
int main(int argc, char **argv) {
	///////////////////////////////////////////////////////////
	// Console parameter process
	///////////////////////////////////////////////////////////
	g_config_file[0] = '\0';
	if (argc > 1) {
		if (strcmp("help", argv[1]) == 0 || strcmp("-h", argv[1]) == 0
				|| strcmp("--help", argv[1]) == 0) {
			log_info_print(g_debug_verbose, "usage: app_service [configuration file]");
			exit(1);
		}
		strcpy(g_config_file, argv[1]);
	} else {
		strcpy(g_config_file, OC_DEFAULT_CONFIG_FILE);
	}

	///////////////////////////////////////////////////////////
	// Load configuration file
	///////////////////////////////////////////////////////////
	log_info_print(g_debug_verbose, "using config file [%s]", g_config_file);
	if (load_config(g_config_file) == OC_FAILURE)
		exit(EXIT_FAILURE);

	/* handle SIGINT */
	signal(SIGINT, sig_handler);

	///////////////////////////////////////////////////////////
	// Check modules, setup inter-connection
	///////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////
	// APP service thread
	///////////////////////////////////////////////////////////
	g_app_running = OC_TRUE;
	if (pthread_create(&g_app_thread_id, (const pthread_attr_t *) NULL, thread_app_service_handle,
			(void *) NULL) == -1) {
		log_error_print(g_debug_verbose, "APP thread create error!");
		exit(EXIT_FAILURE);
	}
	pthread_detach(g_app_thread_id);

	update_app_event(APP_EVENT_SETUP_WEB_CONN);

	memset((void*) (&g_app_operation), 0, sizeof(OC_APP_OPERATION));

	///////////////////////////////////////////////////////////
	// Startup server
	///////////////////////////////////////////////////////////
	int sockfd_server;
	int sockfd;
	int fd_temp;
	struct sockaddr_in s_addr_in;
	struct sockaddr_in s_addr_client;
	int client_length;

	sockfd_server = socket(AF_INET, SOCK_STREAM, 0);  //ipv4,TCP
	assert(sockfd_server != -1);

	//before bind(), set the attr of structure sockaddr.
	memset(&s_addr_in, 0, sizeof(s_addr_in));
	s_addr_in.sin_family = AF_INET;
	s_addr_in.sin_addr.s_addr = inet_addr("127.0.0.1");
	s_addr_in.sin_port = htons(g_config->app_port);
 	log_info_print(g_debug_verbose, "bind ...");
	fd_temp = bind(sockfd_server, (struct sockaddr *) (&s_addr_in), sizeof(s_addr_in));
	if (fd_temp == -1) {
		log_error_print(g_debug_verbose, "bind error!");
		exit(EXIT_FAILURE);
	}

	fd_temp = listen(sockfd_server, OC_MAX_CONN_LIMIT);
	if (fd_temp == -1) {
		log_error_print(g_debug_verbose, "listen error!");
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	// Timer service thread
	///////////////////////////////////////////////////////////
	g_timer_running = OC_TRUE;
	if (pthread_create(&g_timer_thread_id, NULL, thread_timer_handle, (void *) NULL) == -1) {
		log_error_print(g_debug_verbose, "Timer thread create error!");
		exit(EXIT_FAILURE);
	}
	pthread_detach(g_timer_thread_id);

#if 1
	///////////////////////////////////////////////////////////
	// thread monitor ethernet is 
	///////////////////////////////////////////////////////////
 	log_info_print(g_debug_verbose, "monitor ethernet is creating...");
	if (pthread_create(&g_monitor_ethernet_id, NULL, monitor_ethernet_handle,NULL) == -1) {
		log_error_print(g_debug_verbose, "monitor ethernet  thread create error!");
		exit(EXIT_FAILURE);
	}
	pthread_detach(g_monitor_ethernet_id);
#endif
	

	///////////////////////////////////////////////////////////
	// Main service loop
	///////////////////////////////////////////////////////////
	while (1) {
		log_info_print(g_debug_verbose, "waiting for new connection...");
		pthread_t thread_id;
		client_length = sizeof(s_addr_client);

		//Block here. Until server accpet a new connection.
		sockfd = accept(sockfd_server, (struct sockaddr*) (&s_addr_client),
				(socklen_t *) (&client_length));
		if (sockfd == -1) {
			log_error_print(g_debug_verbose, "Accept error!");
			//ignore current socket ,continue while loop.
			continue;
		}
		log_info_print(g_debug_verbose, "A new connection occurs!");
		if (pthread_create(&thread_id, NULL, thread_business_handle, (void *) (&sockfd)) != 0) {
			log_error_print(g_debug_verbose, "pthread_create error!");
			//break while loop
			break;
		}
		pthread_detach(thread_id);
	}

	//Clear
	int ret = shutdown(sockfd_server, SHUT_WR); //shut down the all or part of a full-duplex connection.
	assert(ret != -1);

	log_info_print(g_debug_verbose, "Server shuts down");
	return EXIT_SUCCESS;
}

/**
 * Main daemon business process.
 * Do some simple process, as a command router, but synchronize the sub-module.
 */
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	if (req_buf == NULL || req_len < 1 || resp_buf == NULL || resp_len == NULL)
		return OC_FAILURE;

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	int command = request->command;

	switch (command) {
	case OC_REQ_CTRL_APP:
		result = main_busi_ctrl_app(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_APP:
		result = main_busi_query_app(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_HEART_BEAT:
		result = main_busi_heart_beat(req_buf, req_len, resp_buf, resp_len);
		break;

	default:
		// bad command
		result = main_busi_unsupport_command(req_buf, req_len, resp_buf, resp_len);
		break;
	}

	return result;
}

/**
 * Unsupport command process.
 */
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * Phone APP client control process
 */
int main_busi_ctrl_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;
	int next_event = APP_EVENT_IDLE;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_CTRL_APP_REQ* ctrl_app_req = (OC_CMD_CTRL_APP_REQ*) (req_buf + OC_PKG_FRONT_PART_SIZE);

	char script_buffer[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) script_buffer, 0, MAX_SCRIPT_SHELL_BUFFER);
	char custome_args[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) custome_args, 0, MAX_SCRIPT_SHELL_BUFFER);
	char json_params[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) json_params, 0, MAX_SCRIPT_SHELL_BUFFER);
	char commandUuid[40];
	memset((void*) commandUuid, 0, sizeof(commandUuid));

	if (ctrl_app_req->operation == 1) {
		// startup operation command
		char* operation = "start";

		// generate the commandUuid
		web_generate_command_uuid(commandUuid);

		// post the command to App Center
		long timestamp = time(NULL);
		web_make_post_command_parameters(operation, commandUuid, custome_args, json_params);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_OPERATION_COMMAND,
				g_config->app_post_command_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);
		memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));
		strcpy((char*) shell_invoke_buf, script_buffer);
		event_send_web_status_process();

		// trigger the loop event
		strcpy(g_app_operation.uuid, commandUuid);
		strcpy(g_app_operation.poid, g_poid);
		strcpy(g_app_operation.operation, operation);
		g_app_operation.start_time = time(NULL);
		g_app_operation.is_finish = OC_FALSE;
		g_cabinet_status = OC_CABINET_STATUS_STARTING;
		g_startup_check_counter = OC_APP_CONTROL_CHECK_SEC;
		log_debug_print(g_debug_verbose, "Set g_startup_check_counter=%d", g_startup_check_counter);

		next_event = APP_EVENT_CHECK_STARTUP;

	} else if (ctrl_app_req->operation == 2) {
		// shutdown operation command
		char* operation = "stop";

		// generate the commandUuid
		web_generate_command_uuid(commandUuid);

		// post the command to App Center
		long timestamp = time(NULL);
		web_make_post_command_parameters(operation, commandUuid, custome_args, json_params);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_OPERATION_COMMAND,
				g_config->app_post_command_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);
		memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));
		strcpy((char*) shell_invoke_buf, script_buffer);
		event_send_web_status_process();

		// trigger the loop event
		strcpy(g_app_operation.uuid, commandUuid);
		strcpy(g_app_operation.poid, g_poid);
		strcpy(g_app_operation.operation, operation);
		g_app_operation.start_time = time(NULL);
		g_app_operation.is_finish = OC_FALSE;
		g_cabinet_status = OC_CABINET_STATUS_STOPPING;
		g_shutdown_check_counter = OC_APP_CONTROL_CHECK_SEC;
		log_debug_print(g_debug_verbose, "Set g_shutdown_check_counter=%d",
				g_shutdown_check_counter);

		next_event = APP_EVENT_CHECK_SHUTDOWN;

	} else if (ctrl_app_req->operation == 3) {
		// restart operation command

	}

	set_next_app_event(next_event);
	//update_app_event(next_event);

	// Generate a app control response
	OC_CMD_CTRL_APP_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

	uint32_t exec_result = 0;
	result = generate_cmd_app_control_resp(&resp, exec_result);
	if (resp == NULL)
		result = OC_FAILURE;

	if (result == OC_SUCCESS) {
		result = translate_cmd2buf_app_control_resp(resp, &busi_buf, &busi_buf_len);
		if (result == OC_SUCCESS) {
			result = generate_response_package(OC_REQ_CTRL_APP, busi_buf, busi_buf_len, &temp_buf,
					&temp_buf_len);
			if (result == OC_SUCCESS) {
				memcpy(resp_buf, temp_buf, temp_buf_len);
				*resp_len = temp_buf_len;
			}
		}
	}
	if (busi_buf != NULL)
		free(busi_buf);
	if (temp_buf != NULL)
		free(temp_buf);
	if (resp != NULL)
		free(resp);

	return result;
}

/**
 * Phone APP client query process
 */
int main_busi_query_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * Heart beat process
 */
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * APP event machine process
 */
int app_event_machine() {
	int next_event = APP_EVENT_IDLE;

	switch (g_current_app_event) {
	case APP_EVENT_IDLE:
		next_event = event_idle_process();
		break;

	case APP_EVENT_SETUP_WEB_CONN:
		next_event = event_setup_web_connection_process();
		break;

	case APP_EVENT_RELEASE_WEB_CONN:
		next_event = event_release_web_connection_process();
		break;

	case APP_EVENT_SEND_WEB_STATUS:
		next_event = event_send_web_status_process();
		break;

	case APP_EVENT_CALL_STARTUP:
		next_event = event_call_startup_process();
		break;

	case APP_EVENT_CALL_SHUTDOWN:
		next_event = event_call_shutdown_process();
		break;

	case APP_EVENT_CHECK_STARTUP:
		next_event = event_check_startup_process();
		break;

	case APP_EVENT_CHECK_SHUTDOWN:
		next_event = event_check_shutdown_process();
		break;

	case APP_EVENT_CALL_CABINET_QUERY:
		next_event = event_call_cabinet_query_process();
		break;

	case APP_EVENT_CALL_SERVER_QUERY:
		next_event = event_call_server_query_process();
		break;

	case APP_EVENT_SEND_WEB_HEART_BEAT:
		next_event = event_send_web_heart_beat_process();
		break;

	default:
		break;
	}

	return next_event;
}

int event_idle_process() {
	int next_event = APP_EVENT_IDLE;
	return next_event;
}

int event_setup_web_connection_process() {
	int next_event = APP_EVENT_IDLE;

	g_web_running = OC_TRUE;
	if (pthread_create(&g_web_thread_id, NULL, thread_web_connection_handle, (void *) NULL) == -1) {
		log_error_print(g_debug_verbose, "Web thread create error!");
	} else
		log_info_print(g_debug_verbose, "Web thread create success!");

	// wait 1s for setup websocket connection
	sleep(1);

	return next_event;
}

int event_release_web_connection_process() {
	int next_event = APP_EVENT_IDLE;

	return next_event;
}

int event_send_web_status_process() {
	int next_event = APP_EVENT_IDLE;

	int result = OC_SUCCESS;
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);

	if (strlen((const char*) shell_invoke_buf) > 0)
		result = script_shell_exec((unsigned char*) shell_invoke_buf, script_result);
	memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));

	log_debug_print(g_debug_verbose, "control status shell result = %s", script_result);

	return next_event;
}

int event_call_startup_process() {
	int next_event = APP_EVENT_IDLE;
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_CTRL_STARTUP_REQ * req_startup_ctrl = NULL;
	result = generate_cmd_ctrl_startup_req(&req_startup_ctrl, 0);
	result = translate_cmd2buf_ctrl_startup_req(req_startup_ctrl, &busi_buf, &busi_buf_len);
	if (req_startup_ctrl != NULL)
		free(req_startup_ctrl);

	OC_NET_PACKAGE* resp_pkg = NULL;
	OC_CMD_CTRL_STARTUP_RESP* resp_start_ctrl = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
	OC_REQ_CTRL_STARTUP, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		next_event = APP_EVENT_CHECK_STARTUP;
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_start_ctrl = (OC_CMD_CTRL_STARTUP_RESP*) resp_pkg->data;
		log_debug_print(g_debug_verbose, "resp_start_ctrl->result=%d", resp_start_ctrl->result);
		if (resp_start_ctrl->result == 0) {
			memcpy((void*) (&g_app_operation), (void*) (&g_recv_operation),
					sizeof(g_app_operation));
			g_app_operation.start_time = time(NULL);
			g_cabinet_status = OC_CABINET_STATUS_STARTING;
			g_startup_check_counter = OC_APP_CONTROL_CHECK_SEC;
			log_debug_print(g_debug_verbose, "Set g_startup_check_counter=%d",
					g_startup_check_counter);
			//strcpy((char*) web_tx_buf, "startup submit ok");
		} else {
			//strcpy((char*) web_tx_buf, "startup submit fail");
		}
	} else {
		next_event = APP_EVENT_IDLE;
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return next_event;
}

int event_call_shutdown_process() {
	int next_event = APP_EVENT_IDLE;
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_CTRL_SHUTDOWN_REQ * req_shutdown_ctrl = NULL;
	result = generate_cmd_ctrl_shutdown_req(&req_shutdown_ctrl, 0);
	result = translate_cmd2buf_ctrl_shutdown_req(req_shutdown_ctrl, &busi_buf, &busi_buf_len);
	if (req_shutdown_ctrl != NULL)
		free(req_shutdown_ctrl);

	OC_NET_PACKAGE* resp_pkg = NULL;
	OC_CMD_CTRL_SHUTDOWN_RESP* resp_shutdown_ctrl = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
	OC_REQ_CTRL_SHUTDOWN, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		next_event = APP_EVENT_CHECK_SHUTDOWN;
		// notify the invoker
		resp_shutdown_ctrl = (OC_CMD_CTRL_SHUTDOWN_RESP*) resp_pkg->data;
		log_debug_print(g_debug_verbose, "resp_start_ctrl->result=%d", resp_shutdown_ctrl->result);
		if (resp_shutdown_ctrl->result == 0) {
			memcpy((void*) (&g_app_operation), (void*) (&g_recv_operation),
					sizeof(g_app_operation));
			g_app_operation.start_time = time(NULL);
			g_cabinet_status = OC_CABINET_STATUS_STOPPING;
			g_shutdown_check_counter = OC_APP_CONTROL_CHECK_SEC;
			//strcpy((char*) web_tx_buf, "shutdown submit ok");
		} else {
			//strcpy((char*) web_tx_buf, "shutdown submit fail");
		}

	} else {
		next_event = APP_EVENT_IDLE;
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return next_event;
}

int event_check_startup_process() {
	int next_event = APP_EVENT_IDLE;
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_QUERY_STARTUP_REQ * req_startup_query = NULL;
	result = generate_cmd_query_startup_req(&req_startup_query, 0);
	result = translate_cmd2buf_query_startup_req(req_startup_query, &busi_buf, &busi_buf_len);
	if (req_startup_query != NULL)
		free(req_startup_query);

	OC_NET_PACKAGE* resp_pkg = NULL;
	OC_CMD_QUERY_STARTUP_RESP* resp_startup_query = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
	OC_REQ_QUERY_STARTUP, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		next_event = APP_EVENT_IDLE;
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_startup_query = (OC_CMD_QUERY_STARTUP_RESP*) resp_pkg->data;

		// update global status flag
		if (resp_startup_query->is_finish == OC_TRUE) {
			// finish the checking
			if (resp_startup_query->step_no == OC_STARTUP_TOTAL_STEP
					&& resp_startup_query->result == 0) {
				if (resp_startup_query->error_no == error_no_error) {
					//if (strcmp((const char*) resp_startup_query->message, "finish") == 0) {
					g_cabinet_status = OC_CABINET_STATUS_RUNNING;
				}
			}
			g_app_operation.is_finish = OC_TRUE;
			g_app_operation.end_time = time(NULL);

			// Send the heart beat to APP center in 5s
			if (g_app_heart_beat_counter > 0 && g_app_heart_beat_counter <= (OC_APP_HEART_BEAT_SEC - 5)) {
				g_next_app_event = APP_EVENT_SEND_WEB_HEART_BEAT;
			}
		}

		char script_buffer[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) script_buffer, 0, MAX_SCRIPT_SHELL_BUFFER);
		char custome_args[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) custome_args, 0, MAX_SCRIPT_SHELL_BUFFER);
		char json_params[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) json_params, 0, MAX_SCRIPT_SHELL_BUFFER);

		long timestamp = time(NULL);
		web_make_startup_status_parameters(resp_startup_query, g_app_operation.uuid, custome_args,
				json_params, OC_FALSE);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_CHECK_STATUS,
				g_config->app_post_result_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);

		memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));
		strcpy((char*) shell_invoke_buf, script_buffer);
		next_event = APP_EVENT_SEND_WEB_STATUS;

	} else {
		next_event = APP_EVENT_IDLE;
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return next_event;
}

int event_check_shutdown_process() {
	int next_event = APP_EVENT_IDLE;
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_QUERY_SHUTDOWN_REQ * req_shutdown_query = NULL;
	result = generate_cmd_query_shutdown_req(&req_shutdown_query, 0);
	result = translate_cmd2buf_query_shutdown_req(req_shutdown_query, &busi_buf, &busi_buf_len);
	if (req_shutdown_query != NULL)
		free(req_shutdown_query);

	OC_NET_PACKAGE* resp_pkg = NULL;
	OC_CMD_QUERY_SHUTDOWN_RESP* resp_shutdown_query = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
	OC_REQ_QUERY_SHUTDOWN, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		next_event = APP_EVENT_IDLE;
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_shutdown_query = (OC_CMD_QUERY_SHUTDOWN_RESP*) resp_pkg->data;

		// update global status flag
		if (resp_shutdown_query->is_finish == OC_TRUE) {
			if (resp_shutdown_query->step_no == OC_SHUTDOWN_TOTAL_STEP
					&& resp_shutdown_query->result == 0) {
				if (resp_shutdown_query->error_no == error_no_error) {
					//if (strcmp((const char*) resp_shutdown_query->message, "finish") == 0) {
					g_cabinet_status = OC_CABINET_STATUS_STOPPED;
				}
			}
			g_app_operation.is_finish = OC_TRUE;
			g_app_operation.end_time = time(NULL);

			// Send the heart beat to APP center in 5s
			if (g_app_heart_beat_counter > 0 && g_app_heart_beat_counter <= (OC_APP_HEART_BEAT_SEC - 5)) {
				g_next_app_event = APP_EVENT_SEND_WEB_HEART_BEAT;
			}
		}

		char script_buffer[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) script_buffer, 0, MAX_SCRIPT_SHELL_BUFFER);
		char custome_args[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) custome_args, 0, MAX_SCRIPT_SHELL_BUFFER);
		char json_params[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) json_params, 0, MAX_SCRIPT_SHELL_BUFFER);

		long timestamp = time(NULL);
		web_make_shutdown_status_parameters(resp_shutdown_query, g_app_operation.uuid, custome_args,
				json_params, OC_FALSE);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_CHECK_STATUS,
				g_config->app_post_result_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);

		memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));
		strcpy((char*) shell_invoke_buf, script_buffer);
		next_event = APP_EVENT_SEND_WEB_STATUS;

	} else {
		next_event = APP_EVENT_IDLE;
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return next_event;
}

int event_call_cabinet_query_process() {
	int next_event = APP_EVENT_IDLE;
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_QUERY_CABINET_REQ * req_cabinet_query = NULL;
	result = generate_cmd_query_cabinet_req(&req_cabinet_query, 0);
	result = translate_cmd2buf_query_cabinet_req(req_cabinet_query, &busi_buf, &busi_buf_len);
	if (req_cabinet_query != NULL)
		free(req_cabinet_query);

	OC_NET_PACKAGE* resp_pkg = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
	OC_REQ_QUERY_CABINET_STATUS, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");
		// notify the invoker

	} else {
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
	}
	next_event = APP_EVENT_IDLE;

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return next_event;
}

int event_call_server_query_process() {
	int next_event = APP_EVENT_IDLE;
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_QUERY_SERVER_REQ * req_server_query = NULL;
	uint32_t type = 0;
	result = generate_cmd_query_server_req(&req_server_query, type, NULL, 0);
	result = translate_cmd2buf_query_server_req(req_server_query, &busi_buf, &busi_buf_len);
	if (req_server_query != NULL)
		free(req_server_query);

	OC_NET_PACKAGE* resp_pkg = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
	OC_REQ_QUERY_SERVER_STATUS, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");
		// notify the invoker

	} else {
		log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
	}
	next_event = APP_EVENT_IDLE;

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return next_event;
}

int event_send_web_heart_beat_process() {
	int next_event = APP_EVENT_IDLE;

	int result = OC_SUCCESS;
	uint8_t* busi_cabinet_buf = NULL;
	uint8_t* busi_server_buf = NULL;
	int req_buf_len = 0;
	int cabinet_buf_len = 0;
	int server_buf_len = 0;

	//////////////////////////////////////////////////////////
	// Collect cabinet status
	//////////////////////////////////////////////////////////
	OC_NET_PACKAGE* resp_pkg_cabinet = NULL;
	OC_CMD_QUERY_CABINET_REQ * req_cabinet = NULL;
	result = generate_cmd_query_cabinet_req(&req_cabinet, 0);
	result = translate_cmd2buf_query_cabinet_req(req_cabinet, &busi_cabinet_buf, &cabinet_buf_len);
	if (req_cabinet != NULL)
		free(req_cabinet);

	// Communication with server
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
	OC_REQ_QUERY_CABINET_STATUS, busi_cabinet_buf, cabinet_buf_len, &resp_pkg_cabinet);
	OC_CMD_QUERY_CABINET_RESP* query_cabinet = NULL;
	if(resp_pkg_cabinet != NULL)
		query_cabinet = (OC_CMD_QUERY_CABINET_RESP*) resp_pkg_cabinet->data;

	//////////////////////////////////////////////////////////
	// collect servers status
	//////////////////////////////////////////////////////////
	OC_NET_PACKAGE* resp_pkg_server = NULL;
	OC_CMD_QUERY_SERVER_REQ * req_server = NULL;
	uint32_t type = 0;
	result = generate_cmd_query_server_req(&req_server, type, NULL, 0);
	result = translate_cmd2buf_query_server_req(req_server, &busi_server_buf, &server_buf_len);
	if (req_server != NULL)
		free(req_server);

	// Communication with server
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
	OC_REQ_QUERY_SERVER_STATUS, busi_server_buf, server_buf_len, &resp_pkg_server);
	OC_CMD_QUERY_SERVER_RESP* query_server = NULL;
	if(resp_pkg_server != NULL)
		query_server = (OC_CMD_QUERY_SERVER_RESP*) resp_pkg_server->data;

	//////////////////////////////////////////////////////////
	// Construct the shell command text
	//////////////////////////////////////////////////////////
	if(query_cabinet != NULL && query_server != NULL) {
		char script_buffer[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) script_buffer, 0, MAX_SCRIPT_SHELL_BUFFER);
		char custome_args[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) custome_args, 0, MAX_SCRIPT_SHELL_BUFFER);
		char json_params[MAX_SCRIPT_SHELL_BUFFER];
		memset((void*) json_params, 0, MAX_SCRIPT_SHELL_BUFFER);

		long timestamp = time(NULL);
		g_cabinet_status = query_cabinet->status;
		web_make_cabinet_status_parameters(query_cabinet, query_server, custome_args, json_params);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_QUERY_STATUS,
				g_config->app_post_status_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);

		//////////////////////////////////////////////////////////
		// Send heart beat to APP center using script shell
		//////////////////////////////////////////////////////////
		uint8_t script_result[MAX_CONSOLE_BUFFER];
		memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);

		result = script_shell_exec((unsigned char*) script_buffer, script_result);

		log_debug_print(g_debug_verbose, "Heart beat status shell result = %s", script_result);
	} else {
		log_error_print(g_debug_verbose, "Query cabinet and servers status return null");
	}

	if(busi_cabinet_buf != NULL)
		free(busi_cabinet_buf);
	if(busi_server_buf != NULL)
		free(busi_server_buf);
	if(resp_pkg_cabinet != NULL)
		free_network_package(resp_pkg_cabinet);
	if(resp_pkg_server != NULL)
		free_network_package(resp_pkg_server);

	return next_event;
}

/**
 * Timer event process
 */
void timer_event_process() {

	// Update counter
	g_app_heart_beat_counter++;
	g_watch_dog_counter++;

	if (g_startup_check_counter > 0)
		g_startup_check_counter--;
	if (g_shutdown_check_counter > 0)
		g_shutdown_check_counter--;

//	log_debug_print(g_debug_verbose,
//			"Timer--- g_startup_check_counter=%d, g_shutdown_check_counter=%d",
//			g_startup_check_counter, g_shutdown_check_counter);
	int is_update_event = OC_FALSE;
	if (g_startup_check_counter == 0) {
		//do startup check query
		if (g_app_operation.is_finish == OC_TRUE /* && g_cabinet_status == OC_CABINET_STATUS_RUNNING */) {
			g_startup_check_counter = -1;
		} else {
			set_next_app_event(APP_EVENT_CHECK_STARTUP);
			g_startup_check_counter = OC_APP_CONTROL_CHECK_SEC;
			is_update_event = OC_TRUE;
		}
	}
	if (g_shutdown_check_counter == 0) {
		//do shutdown check query
		if (g_app_operation.is_finish == OC_TRUE /* && g_cabinet_status == OC_CABINET_STATUS_STOPPED */) {
			g_shutdown_check_counter = -1;
		} else {
			if (is_update_event == OC_FALSE) {
				set_next_app_event(APP_EVENT_CHECK_SHUTDOWN);
				g_shutdown_check_counter = OC_APP_CONTROL_CHECK_SEC;
				is_update_event = OC_TRUE;
			}
		}
	}

	// Send the heart beat to APP center
	if (g_app_heart_beat_counter >= OC_APP_HEART_BEAT_SEC) {
		if (is_update_event == OC_FALSE) {
			set_next_app_event(APP_EVENT_SEND_WEB_HEART_BEAT);
			g_app_heart_beat_counter = 0;
			is_update_event = OC_TRUE;
		}
		strcpy((char*) web_tx_buf, "0");
	}

	// Send heart beat to watch dog
	if (g_watch_dog_counter >= OC_WATCH_DOG_FEED_SEC) {
		// feed dog
		timer_event_watchdog_heartbeat();
		g_watch_dog_counter = 0;
	}
}


void timer_event_watchdog_heartbeat() {
	int result = OC_SUCCESS;

	uint8_t* requst_buf = NULL;
	uint8_t* busi_server_buf = NULL;
	int server_buf_len = 0;

	OC_NET_PACKAGE* resp_pkg_server = NULL;
	OC_CMD_HEART_BEAT_REQ * req_heart_beat = NULL;
	uint32_t device = device_proc_app_service;
	uint32_t status = OC_HEALTH_OK;
	result = generate_cmd_heart_beat_req(&req_heart_beat, device, status, 0);
	result = translate_cmd2buf_heart_beat_req(req_heart_beat, &busi_server_buf, &server_buf_len);
	if (req_heart_beat != NULL)
		free(req_heart_beat);

	char time_temp_buf[32];
	helper_get_current_time_str(time_temp_buf, 32, NULL);

	// Communication with server
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_WATCHDOG_DEFAULT_PORT,
			OC_REQ_HEART_BEAT, busi_server_buf, server_buf_len, &resp_pkg_server);
	//OC_CMD_HEART_BEAT_RESP* resp_heart_beat = (OC_CMD_HEART_BEAT_RESP*) resp_pkg_server->data;
	if(result == OC_SUCCESS)
		log_debug_print(g_debug_verbose, "[%s] Send watchdog heart beat success", time_temp_buf);
	else
		log_debug_print(g_debug_verbose, "[%s] Send watchdog heart beat failure", time_temp_buf);

	if(busi_server_buf != NULL)
		free(busi_server_buf);

	if(resp_pkg_server != NULL)
		free_network_package(resp_pkg_server);
}
/**
 * Construct the parameters for web response of startup operation.
 */
void web_make_startup_status_parameters(OC_CMD_QUERY_STARTUP_RESP* resp, char* uuid,
		char* custome_args, char* json_params, int is_error) {
	char custome[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) custome, 0, MAX_SCRIPT_SHELL_BUFFER);
	char json[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) json, 0, MAX_SCRIPT_SHELL_BUFFER);

	char temp[160];

	// commandUuid
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "commandUuid=%s:", uuid);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"commandUuid\":\"%s\",", uuid);
	strcat(json, temp);

	// totalStage
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "totalStage=%d:", OC_STARTUP_TOTAL_STEP);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"totalStage\":%d,", OC_STARTUP_TOTAL_STEP);
	strcat(json, temp);

	// currentStage
	if(is_error == OC_TRUE) {
		// error response
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "currentStage=%d:", 0);
		strcat(custome, temp);

		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\"currentStage\":%d,", 0);
		strcat(json, temp);
	} else {
		// normal response
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "currentStage=%d:", resp->step_no > 0 ? resp->step_no : 1);
		strcat(custome, temp);

		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\"currentStage\":%d,", resp->step_no > 0 ? resp->step_no : 1);
		strcat(json, temp);

	}
	// finished
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "finished=%s:", (resp->is_finish == OC_TRUE ? "true" : "false"));
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"finished\":%s,", (resp->is_finish == OC_TRUE ? "true" : "false"));
	strcat(json, temp);

	// successful
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "successful=%s:",
			((resp->result == 0 && resp->error_no == error_no_error) ? "true" : "false"));
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"successful\":%s,",
			((resp->result == 0 && resp->error_no == error_no_error) ? "true" : "false"));
	strcat(json, temp);

	// errorCode
	char errorCode[4];
	memset((void*) errorCode, 0, sizeof(errorCode));
	if (resp->error_no > 0)
		sprintf(errorCode, "%d", resp->error_no);
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "errorCode=%s:", errorCode);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"errorCode\":%s,", errorCode);
	strcat(json, temp);

	// message
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "message=%s", resp->message);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"message\":\"%s\"", resp->message);
	strcat(json, temp);

	strcpy(custome_args, custome);
	strcpy(json_params, json);
}

/**
 * Construct the parameters for web response of shutdown operation.
 */
void web_make_shutdown_status_parameters(OC_CMD_QUERY_SHUTDOWN_RESP* resp, char* uuid,
		char* custome_args, char* json_params, int is_error) {
	char custome[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) custome, 0, MAX_SCRIPT_SHELL_BUFFER);
	char json[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) json, 0, MAX_SCRIPT_SHELL_BUFFER);

	char temp[160];

	// commandUuid
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "commandUuid=%s:", uuid);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"commandUuid\":\"%s\",", uuid);
	strcat(json, temp);

	// totalStage
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "totalStage=%d:", OC_SHUTDOWN_TOTAL_STEP);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"totalStage\":%d,", OC_SHUTDOWN_TOTAL_STEP);
	strcat(json, temp);

	// currentStage
	if(is_error == OC_TRUE) {
		// error response
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "currentStage=%d:", 0);
		strcat(custome, temp);

		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\"currentStage\":%d,", 0);
		strcat(json, temp);
	} else {
		// normal response
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "currentStage=%d:", resp->step_no > 0 ? resp->step_no : 1);
		strcat(custome, temp);

		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\"currentStage\":%d,", resp->step_no > 0 ? resp->step_no : 1);
		strcat(json, temp);
	}

	// finished
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "finished=%s:", (resp->is_finish == OC_TRUE ? "true" : "false"));
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"finished\":%s,", (resp->is_finish == OC_TRUE ? "true" : "false"));
	strcat(json, temp);

	// successful
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "successful=%s:",
			((resp->result == 0 && resp->error_no == error_no_error) ? "true" : "false"));
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"successful\":%s,",
			((resp->result == 0 && resp->error_no == error_no_error) ? "true" : "false"));
	strcat(json, temp);

	// errorCode
	char errorCode[4];
	memset((void*) errorCode, 0, sizeof(errorCode));
	if (resp->error_no > 0)
		sprintf(errorCode, "%d", resp->error_no);
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "errorCode=%s:", errorCode);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"errorCode\":%s,", errorCode);
	strcat(json, temp);

	// message
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "message=%s", resp->message);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"message\":\"%s\"", resp->message);
	strcat(json, temp);

	strcpy(custome_args, custome);
	strcpy(json_params, json);
}

/**
 * Construct the parameters for web heart beat post.
 */
void web_make_cabinet_status_parameters(OC_CMD_QUERY_CABINET_RESP* query_cabinet,
		OC_CMD_QUERY_SERVER_RESP* query_server, char* custome_args, char* json_params) {
	char custome[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) custome, 0, MAX_SCRIPT_SHELL_BUFFER);
	char json[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) json, 0, MAX_SCRIPT_SHELL_BUFFER);

	char temp[1024];
	int i = 0;

	// poid
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "poid=%s:", g_poid);
	strcat(custome, temp);

//	memset((void*) temp, 0, sizeof(temp));
//	sprintf(temp, "\"poid\":\"%s\",", g_poid);
//	strcat(json, temp);

	// status
//	char temp_status[20];
//	memset((void*) temp_status, 0, sizeof(temp_status));
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "status=%s:", OC_TRANSLATE_STATUS(g_cabinet_status));
	strcat(custome, temp);

//	memset((void*) temp, 0, sizeof(temp));
//	sprintf(temp, "\"status\":%s,", OC_TRANSLATE_STATUS(g_cabinet_status));
//	strcat(json, temp);

	// temperature
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "temperature=%.2f:", query_cabinet->temperature);
	strcat(custome, temp);

//	memset((void*) temp, 0, sizeof(temp));
//	sprintf(temp, "\"temperature\":%.2f,", query_cabinet->temperature);
//	strcat(json, temp);

	// ampere
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "ampere=%.2f:", query_cabinet->current);
	strcat(custome, temp);

//	memset((void*) temp, 0, sizeof(temp));
//	sprintf(temp, "\"ampere\":%.2f,", query_cabinet->current);
//	strcat(json, temp);

	// watt
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "watt=%.2f:", query_cabinet->watt);
	strcat(custome, temp);

//	memset((void*) temp, 0, sizeof(temp));
//	sprintf(temp, "\"watt\":%.2f,", query_cabinet->watt);
//	strcat(json, temp);

	// kwh
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "kwh=%.2f:", query_cabinet->kwh);
	strcat(custome, temp);

	// voice_db
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "decibel=%.2f:", query_cabinet->voice_db);
	strcat(custome, temp);

	// gps
	if(query_cabinet->longitude != OC_GPS_COORD_UNDEF){
		// longitude
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "longitude=%.8f:", query_cabinet->longitude);
		strcat(custome, temp);

		// latitude
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "latitude=%.8f:", query_cabinet->latitude);
		strcat(custome, temp);
	}

	// lbs
	if(query_cabinet->lac != OC_LBS_BSTN_UNDEF){
		// mcc
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "mcc=%d:", query_cabinet->mcc);
		strcat(custome, temp);

		// mnc
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "mnc=%d:", query_cabinet->mnc);
		strcat(custome, temp);

		// lac
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "lac=%d:", query_cabinet->lac);
		strcat(custome, temp);

		// ci
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "ci=%d:", query_cabinet->ci);
		strcat(custome, temp);
	}


	// uptime
	memset((void*) temp, 0, sizeof(temp));
	time_t now_time = time(NULL);
	long running_time = 0;
	if ((g_cabinet_status == OC_CABINET_STATUS_RUNNING
			|| g_cabinet_status == OC_CABINET_STATUS_STOPPING) && query_cabinet->start_time > 0)
		running_time = now_time - query_cabinet->start_time;
	sprintf(temp, "uptime=%ld:", running_time);
	strcat(custome, temp);

	// door1
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "door1=%d:", query_cabinet->door1);
	strcat(custome, temp);
	// door2
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "door2=%d", query_cabinet->door2);
	strcat(custome, temp);


	//devices
	if (query_server->server_num > 0) {
		OC_SERVER_STATUS* server_data = (OC_SERVER_STATUS*) (((char*) query_server)
				+ sizeof(query_server->command) + sizeof(query_server->server_num));
		strcat(json, "\"[");

		char dev_status[8];
		if(g_cabinet_status == OC_CABINET_STATUS_RUNNING
				|| g_cabinet_status == OC_CABINET_STATUS_STOPPING || g_cabinet_status == OC_CABINET_STATUS_STARTING)
			strcpy(dev_status, "true");
		else
			strcpy(dev_status, "false");

		//raid
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "{\\\"name\\\":\\\"%s\\\",", "storage");
		strcat(json, temp);
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\\\"type\\\":\\\"%s\\\",", "raid");
		strcat(json, temp);
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\\\"working\\\":%s},",dev_status);
		strcat(json, temp);

		// firewall
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "{\\\"name\\\":\\\"%s\\\",", "firewall");
		strcat(json, temp);
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\\\"type\\\":\\\"%s\\\",", "firewall");
		strcat(json, temp);
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\\\"working\\\":%s},",dev_status);
		strcat(json, temp);

		// switch
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "{\\\"name\\\":\\\"%s\\\",", "switch");
		strcat(json, temp);
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\\\"type\\\":\\\"%s\\\",", "switch");
		strcat(json, temp);
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\\\"working\\\":%s},",dev_status);
		strcat(json, temp);

		// servers
		for (i = 0; i < query_server->server_num; i++) {
			// name
//			memset((void*) temp, 0, sizeof(temp));
//			sprintf(temp, "devices[%d].name=%s:", i, server_data[i].name);
//			strcat(custome, temp);

			memset((void*) temp, 0, sizeof(temp));
			sprintf(temp, "{\\\"name\\\":\\\"%s\\\",", server_data[i].name);
			strcat(json, temp);

			// type
//			memset((void*) temp, 0, sizeof(temp));
//			sprintf(temp, "devices[%d].type=%s:", i, OC_TRANSLATE_SRV_TYPE(server_data[i].type));
//			strcat(custome, temp);

			memset((void*) temp, 0, sizeof(temp));
			sprintf(temp, "\\\"type\\\":\\\"%s\\\",", OC_TRANSLATE_SRV_TYPE(server_data[i].type));
			strcat(json, temp);

			// working

			if (i == query_server->server_num - 1) {
//				memset((void*) temp, 0, sizeof(temp));
//				sprintf(temp, "devices[%d].working=%s", i,
//						server_data[i].status == 1 ? "true" : "false");
//				strcat(custome, temp);

				memset((void*) temp, 0, sizeof(temp));
				sprintf(temp, "\\\"working\\\":%s}", server_data[i].status == 1 ? "true" : "false");
				strcat(json, temp);
			} else {
//				memset((void*) temp, 0, sizeof(temp));
//				sprintf(temp, "devices[%d].working=%s:", i,
//						server_data[i].status == 1 ? "true" : "false");
//				strcat(custome, temp);

				memset((void*) temp, 0, sizeof(temp));
				sprintf(temp, "\\\"working\\\":%s},",
						server_data[i].status == 1 ? "true" : "false");
				strcat(json, temp);
			}
		}
		strcat(json, "]\"");

	} else {
		// devices
		//strcat(custome, "devices=");
		strcat(json, "\"[]\"");
	}

	strcpy(custome_args, custome);
	strcpy(json_params, json);
}

/**
 * Construct the parameters for post operation command.
 */
void web_make_post_command_parameters(char* operation, char* uuid, char* custome_args,
		char* json_params) {
	char custome[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) custome, 0, MAX_SCRIPT_SHELL_BUFFER);
	char json[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) json, 0, MAX_SCRIPT_SHELL_BUFFER);

	char temp[160];

	// commandUuid
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "commandUuid=%s:", uuid);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"commandUuid\":\"%s\",", uuid);
	strcat(json, temp);

	// poid
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "poid=%s:", g_poid);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"poid\":\"%s\",", g_poid);
	strcat(json, temp);

	// operation
	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "operation=%s", operation);
	strcat(custome, temp);

	memset((void*) temp, 0, sizeof(temp));
	sprintf(temp, "\"operation\":\"%s\"", operation);
	strcat(json, temp);

	strcpy(custome_args, custome);
	strcpy(json_params, json);
}

/**
 * Send the web operation error response tho APP Center.
 */
void web_operation_error_response(char* uuid, char* poid, char* operation,
		enum error_code err_code) {

	char script_buffer[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) script_buffer, 0, MAX_SCRIPT_SHELL_BUFFER);
	char custome_args[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) custome_args, 0, MAX_SCRIPT_SHELL_BUFFER);
	char json_params[MAX_SCRIPT_SHELL_BUFFER];
	memset((void*) json_params, 0, MAX_SCRIPT_SHELL_BUFFER);

	long timestamp = time(NULL);

	if (strcmp(operation, "start")) {
		OC_CMD_QUERY_STARTUP_RESP resp_startup_query;
		memset((void*) &resp_startup_query, 0, sizeof(resp_startup_query));

		resp_startup_query.error_no = err_code;
		resp_startup_query.is_finish = OC_TRUE;
		resp_startup_query.result = 1;
		resp_startup_query.step_no = 0;

		web_make_startup_status_parameters(&resp_startup_query, uuid, custome_args, json_params, OC_TRUE);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_CHECK_STATUS,
				g_config->app_post_result_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);

		memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));
		strcpy((char*) shell_invoke_buf, script_buffer);

		event_send_web_status_process();
	} else if (strcmp(operation, "stop")) {
		OC_CMD_QUERY_SHUTDOWN_RESP resp_shutdown_query;
		memset((void*) &resp_shutdown_query, 0, sizeof(resp_shutdown_query));

		resp_shutdown_query.error_no = err_code;
		resp_shutdown_query.is_finish = OC_TRUE;
		resp_shutdown_query.result = 1;
		resp_shutdown_query.step_no = 0;

		web_make_shutdown_status_parameters(&resp_shutdown_query, uuid, custome_args, json_params, OC_TRUE);

		sprintf(script_buffer, "%s %s %s %s %ld %s %s", APP_SHELL_POST_CHECK_STATUS,
				g_config->app_post_result_url, g_access_key, g_access_secret, timestamp * 1000,
				custome_args, json_params);

		memset((void*) shell_invoke_buf, 0, sizeof(shell_invoke_buf));
		strcpy((char*) shell_invoke_buf, script_buffer);

		event_send_web_status_process();
	} else if (strcmp(operation, "restart")) {

	}

}

/**
 * Generate a command UUID for web app.
 */
int web_generate_command_uuid(char* out_uuid) {
	int result = OC_SUCCESS;
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);

	char* uuidgen = "uuidgen";
	result = script_shell_exec((unsigned char*) uuidgen, script_result);

	log_debug_print(g_debug_verbose, "generate a new uuid, result = %s", script_result);

	strncpy(out_uuid, (const char*) script_result, 36);

	return result;
}
