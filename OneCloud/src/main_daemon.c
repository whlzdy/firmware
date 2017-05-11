/*
 * main_daemon.c
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
#include <netdb.h>
#include <signal.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"
#include "common/error.h"
#include "util/log_helper.h"
#include "util/date_helper.h"

// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_cabinet_status(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_server_status(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_com(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_com(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_ctrl_sim(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_sim(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_ctrl_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_notify_gpio_action(uint32_t gpio_event);
void timer_event_process();
void timer_event_query_server_status();
void timer_event_query_cabinet_status();
void timer_event_watchdog_heartbeat();
int get_com_listen_port_by_type(int type, int offset);
int collect_cabinet_temperature(float* out_temperature);
int collect_cabinet_electricity(float* out_kwh, float* out_voltage, float* out_current,
		float* out_watt);
int collect_cabinet_voice_db(float* out_voice_db);
int call_cabinet_power_on();
int call_cabinet_power_off();

// Debug output level
int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];
int g_main_running = OC_FALSE;

// Global configuration
struct settings *g_config = NULL;

// Timer thread
pthread_t g_timer_thread_id = 0;
int g_timer_running = OC_FALSE;
int g_server_check_counter = -1;    // -1:disable, 0:check query, >0:counting
int g_cabinet_check_counter = -1;    // -1:disable, 0:check query, >0:counting
int g_manual_server_check = OC_TRUE;
int g_watch_dog_counter = 0;
int g_cabinet_runtime_status = OC_CABINET_STATUS_STOPPED;
int g_cabinet_runtime_error = OC_FALSE;
time_t g_cabinet_start_time = 0;
time_t g_cabinet_shutdown_time = 0;
/**
 * Signal handler
 */
void sig_handler(const int sig) {
	// release sub thread
	g_main_running = OC_FALSE;
	g_timer_running = OC_FALSE;
	sleep(1);

	if(g_config != NULL)
		free(g_config);

	log_info_print(g_debug_verbose, "SIGINT handled.");
	log_info_print(g_debug_verbose, "Now stop Main daemon.");

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

	g_config->role = role_main;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "debug", &temp_int, OC_DEBUG_LEVEL_DISABLE);
	g_debug_verbose = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "listen_port", &temp_int,
	OC_MAIN_DEFAULT_PORT);
	g_config->main_listen_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "power_off_enable", &temp_int, 1);
	g_config->power_off_enable = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "power_off_force", &temp_int, 1);
	g_config->power_off_force = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_WATCHDOG, "listen_port", &temp_int,
	OC_WATCHDOG_DEFAULT_PORT);
	g_config->watchdog_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_GPIO, "listen_port", &temp_int,
	OC_GPIO_DEFAULT_PORT);
	g_config->gpio_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_SIM, "listen_port", &temp_int,
	OC_SIM_DEFAULT_PORT);
	g_config->sim_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_SCRIPT, "listen_port", &temp_int,
	OC_SCRIPT_DEFAULT_PORT);
	g_config->script_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_APP_SERVICE, "listen_port", &temp_int,
	OC_APP_DEFAULT_PORT);
	g_config->app_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_TEMPERATURE, "listen_port", &temp_int,
	OC_TEMPERATURE_PORT);
	g_config->temperature_port = temp_int;

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_MAIN, "cabinet_status_file", temp_value,
			"/opt/onecloud/script/cabinet/cabinet_status.tmp");
	strcpy(g_config->cabinet_status_file, temp_value);

	temp_int = 0;
	get_parameter_int(SECTION_COM0, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		g_config->com_config[0].iter_num = 0;
		temp_int = 0;
		get_parameter_int(SECTION_COM0, "listen_port", &temp_int,
		OC_COM0_DEFAULT_PORT);
		g_config->com_config[0].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM0, "type", &temp_int, 0);
		g_config->com_config[0].type = temp_int;
		com_count++;
	}

	temp_int = 0;
	get_parameter_int(SECTION_COM1, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		g_config->com_config[1].iter_num = 1;
		temp_int = 0;
		get_parameter_int(SECTION_COM1, "listen_port", &temp_int,
		OC_COM1_DEFAULT_PORT);
		g_config->com_config[1].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM1, "type", &temp_int, 0);
		g_config->com_config[1].type = temp_int;
		com_count++;
	}

	temp_int = 0;
	get_parameter_int(SECTION_COM2, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		g_config->com_config[2].iter_num = 2;
		temp_int = 0;
		get_parameter_int(SECTION_COM2, "listen_port", &temp_int,
		OC_COM2_DEFAULT_PORT);
		g_config->com_config[2].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM2, "type", &temp_int, 0);
		g_config->com_config[2].type = temp_int;
		com_count++;
	}

	temp_int = 0;
	get_parameter_int(SECTION_COM3, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		g_config->com_config[3].iter_num = 3;
		temp_int = 0;
		get_parameter_int(SECTION_COM3, "listen_port", &temp_int,
		OC_COM3_DEFAULT_PORT);
		g_config->com_config[3].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM3, "type", &temp_int, 0);
		g_config->com_config[3].type = temp_int;
		com_count++;
	}

	temp_int = 0;
	get_parameter_int(SECTION_COM4, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		g_config->com_config[4].iter_num = 4;
		temp_int = 0;
		get_parameter_int(SECTION_COM4, "listen_port", &temp_int,
		OC_COM4_DEFAULT_PORT);
		g_config->com_config[4].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM4, "type", &temp_int, 0);
		g_config->com_config[4].type = temp_int;
		com_count++;
	}

	temp_int = 0;
	get_parameter_int(SECTION_COM5, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		g_config->com_config[5].iter_num = 5;
		temp_int = 0;
		get_parameter_int(SECTION_COM5, "listen_port", &temp_int,
		OC_COM5_DEFAULT_PORT);
		g_config->com_config[5].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM5, "type", &temp_int, 0);
		g_config->com_config[5].type = temp_int;
		com_count++;
	}

	g_config->com_num = com_count;

//	memset(temp_value, 0, sizeof(temp_value));
//	get_parameter_str("Main", "uuid", temp_value, "012345678900");
//	strcpy(g_config->uuid, temp_value);

	release_config_map();

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
			log_info_print(g_debug_verbose, "usage: main_daemon [configuration file]");
			exit(1);
		}
		strcpy(g_config_file, argv[1]);
	} else {
		strcpy(g_config_file, OC_DEFAULT_CONFIG_FILE);
	}

	///////////////////////////////////////////////////////////
	// Load configuration file
	///////////////////////////////////////////////////////////
	log_info_print(g_debug_verbose, "Info: using config file [%s]\n", g_config_file);
	if (load_config(g_config_file) == OC_FAILURE)
		exit(EXIT_FAILURE);

	/* handle SIGINT */
	signal(SIGINT, sig_handler);

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
	s_addr_in.sin_port = htons(g_config->main_listen_port);
	fd_temp = bind(sockfd_server, (struct sockaddr *) (&s_addr_in), sizeof(s_addr_in));
	if (fd_temp == -1) {
		log_error_print(g_debug_verbose, "Error: bind error!");
		exit(EXIT_FAILURE);
	}

	fd_temp = listen(sockfd_server, OC_MAX_CONN_LIMIT);
	if (fd_temp == -1) {
		log_error_print(g_debug_verbose, "Error: listen error!");
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	// Check modules, setup inter-connection
	///////////////////////////////////////////////////////////
	// Notify the GPIO daemon
	//main_notify_gpio_action(0);//0:init
	int status = 0;
	load_cabinet_status_from_file(g_config->cabinet_status_file, &status, &g_cabinet_start_time,
			&g_cabinet_shutdown_time);
	g_cabinet_runtime_status = (
			status == 1 ?
			OC_CABINET_STATUS_RUNNING :
							(status == 0 ? OC_CABINET_STATUS_STOPPED : g_cabinet_runtime_status));
	g_cabinet_runtime_error = (status == 2 ? OC_TRUE : OC_FALSE);

	///////////////////////////////////////////////////////////
	// Timer service thread
	///////////////////////////////////////////////////////////
	g_timer_running = OC_TRUE;
	if (pthread_create(&g_timer_thread_id, NULL, thread_timer_handle, (void *) NULL) == -1) {
		log_error_print(g_debug_verbose, "Timer thread create error!");
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	// Main service loop
	///////////////////////////////////////////////////////////
	g_main_running = OC_TRUE;
	int accept_try = 0;
	while (g_main_running) {
		log_info_print(g_debug_verbose, "waiting for new connection...");
		pthread_t thread_id;
		client_length = sizeof(s_addr_client);

		//Block here. Until server accpet a new connection.
		sockfd = accept(sockfd_server, (struct sockaddr*) (&s_addr_client),
				(socklen_t *) (&client_length));
		if (sockfd == -1) {
			log_error_print(g_debug_verbose, "Accept error!");
			//ignore current socket ,continue while loop.
			accept_try++;

			if (accept_try < 100)
				continue;
			else
				break;
		}
		log_info_print(g_debug_verbose, "A new connection occurs!");
		accept_try = 0;
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
	case OC_REQ_QUERY_CABINET_STATUS:
		result = main_busi_query_cabinet_status(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_SERVER_STATUS:
		result = main_busi_query_server_status(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_CTRL_STARTUP:
		result = main_busi_ctrl_startup(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_CTRL_SHUTDOWN:
		result = main_busi_ctrl_shutdown(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_STARTUP:
		result = main_busi_query_startup(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_SHUTDOWN:
		result = main_busi_query_shutdown(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_CTRL_GPIO:
		result = main_busi_ctrl_gpio(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_GPIO:
		result = main_busi_query_gpio(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_CTRL_COM:
		result = main_busi_ctrl_com(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_COM:
		result = main_busi_query_com(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_CTRL_SIM:
		result = main_busi_ctrl_sim(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_SIM:
		result = main_busi_query_sim(req_buf, req_len, resp_buf, resp_len);
		break;

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
 * Cabinet status query process
 */
int main_busi_query_cabinet_status(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_FAILURE;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_QUERY_CABINET_REQ* query_cabinet_req = (OC_CMD_QUERY_CABINET_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	OC_CMD_QUERY_CABINET_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

	uint32_t status =
			(g_cabinet_runtime_error == OC_TRUE ?
					4 :
					(g_cabinet_runtime_status == OC_CABINET_STATUS_RUNNING ?
							1 :
							(g_cabinet_runtime_status == OC_CABINET_STATUS_STARTING ?
									2 :
									(g_cabinet_runtime_status == OC_CABINET_STATUS_STOPPING ? 3 : 0))));

	// collect data
	float kwh = 123.0F;
	float voltage = 220.0F;
	float current = 2.5F;
	float temperature = 43.4F;
	float watt = 0.01F;
	float voice_db = 40.5F;
	time_t start_time = g_cabinet_start_time;
	collect_cabinet_temperature(&temperature);
	collect_cabinet_electricity(&kwh, &voltage, &current, &watt);
	collect_cabinet_voice_db(&voice_db);

	result = generate_cmd_query_cabinet_resp(&resp, status, kwh, voltage, current, temperature,
			watt, voice_db, start_time);
	if (resp == NULL)
		result = OC_FAILURE;

	if (result == OC_SUCCESS) {
		result = translate_cmd2buf_query_cabinet_resp(resp, &busi_buf, &busi_buf_len);
		if (result == OC_SUCCESS) {
			result = generate_response_package(OC_REQ_QUERY_CABINET_STATUS, busi_buf, busi_buf_len,
					&temp_buf, &temp_buf_len);
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
 * Server status query process
 */
int main_busi_query_server_status(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_QUERY_SERVER_REQ* query_server_req = (OC_CMD_QUERY_SERVER_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	//Route the request to the script daemon
	uint8_t* resp_buf_script = NULL;
	uint32_t resp_buf_script_len = 0;
	result = net_business_comm_route2((uint8_t*) OC_LOCAL_IP, g_config->script_port, req_buf,
			req_len, &resp_buf_script, &resp_buf_script_len);

	// Construct the response for invoker
	if (result == OC_SUCCESS) {
		// Analyze the script daemon response data

		memcpy(resp_buf, resp_buf_script, resp_buf_script_len);
		*resp_len = resp_buf_script_len;

	} else {
		// Do nothing, will close the socket
	}
	if(resp_buf_script != NULL)
		free(resp_buf_script);

	return result;
}

/**
 * Startup control process
 */
int main_busi_ctrl_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;
	int app_result = OC_SUCCESS;

	result = call_cabinet_power_on();

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_CTRL_STARTUP_REQ* ctrl_startup_req = (OC_CMD_CTRL_STARTUP_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	//Route the request to the script daemon
	uint8_t* resp_buf_script = NULL;
	uint32_t resp_buf_script_len = 0;
	result = net_business_comm_route2((uint8_t*) OC_LOCAL_IP, g_config->script_port, req_buf,
			req_len, &resp_buf_script, &resp_buf_script_len);

	// Construct the response for invoker
	if (result == OC_SUCCESS) {
		// Analyze the script daemon response data

		// Notify the GPIO daemon
		if (ctrl_startup_req->flag == 0)
			main_notify_gpio_action(1); //1:startup

		memcpy(resp_buf, resp_buf_script, resp_buf_script_len);
		*resp_len = resp_buf_script_len;

		g_cabinet_runtime_status = OC_CABINET_STATUS_STARTING;

		// notify the App center, send APP control
		if (ctrl_startup_req->flag == 1) {
			OC_CMD_CTRL_APP_REQ * req_app_ctrl = NULL;
			uint8_t* busi_buf = NULL;
			int busi_buf_len = 0;
			OC_NET_PACKAGE* resp_app_pkg = NULL;
			app_result = generate_cmd_app_control_req(&req_app_ctrl, 1); // startup operation
			app_result = translate_cmd2buf_app_control_req(req_app_ctrl, &busi_buf, &busi_buf_len);
			if (req_app_ctrl != NULL)
				free(req_app_ctrl);
			// Communication with server
			app_result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->app_port,
			OC_REQ_CTRL_APP, busi_buf, busi_buf_len, &resp_app_pkg);
			if (app_result != OC_SUCCESS) {
				log_warn_print(g_debug_verbose,
						"Notify app startup operation failed!net_business_communicate error=[%d]",
						result);
			}
			if (busi_buf != NULL)
				free(busi_buf);
			if (resp_app_pkg != NULL)
				free(resp_app_pkg);
		}
	} else {
		// Generate a error response
		OC_CMD_CTRL_STARTUP_RESP* resp = NULL;
		uint8_t* busi_buf = NULL;
		uint8_t* temp_buf = NULL;
		int busi_buf_len = 0;
		int temp_buf_len = 0;

		uint32_t exec_result = 1;
		uint32_t error_no = 1;
		result = generate_cmd_ctrl_startup_resp(&resp, exec_result, error_no);
		if (resp == NULL)
			result = OC_FAILURE;

		if (result == OC_SUCCESS) {
			result = translate_cmd2buf_ctrl_startup_resp(resp, &busi_buf, &busi_buf_len);
			if (result == OC_SUCCESS) {
				result = generate_response_package(OC_REQ_CTRL_STARTUP, busi_buf, busi_buf_len,
						&temp_buf, &temp_buf_len);
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
	}

	if(resp_buf_script!= NULL)
		free(resp_buf_script);

	return result;
}

/**
 * Shutdown control process
 */
int main_busi_ctrl_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;
	int app_result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_CTRL_SHUTDOWN_REQ* ctrl_shutdown_req = (OC_CMD_CTRL_SHUTDOWN_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	//Route the request to the script daemon
	uint8_t* resp_buf_script = NULL;
	uint32_t resp_buf_script_len = 0;
	result = net_business_comm_route2((uint8_t*) OC_LOCAL_IP, g_config->script_port, req_buf,
			req_len, &resp_buf_script, &resp_buf_script_len);

	// Construct the response for invoker
	if (result == OC_SUCCESS) {
		// Analyze the script daemon response data

		// Notify the GPIO daemon
		if (ctrl_shutdown_req->flag == 0)
			main_notify_gpio_action(2); //2:shutdown

		memcpy(resp_buf, resp_buf_script, resp_buf_script_len);
		*resp_len = resp_buf_script_len;

		g_cabinet_runtime_status = OC_CABINET_STATUS_STOPPING;

		// notify the App center, send APP control
		if (ctrl_shutdown_req->flag == 1) {
			OC_CMD_CTRL_APP_REQ * req_app_ctrl = NULL;
			uint8_t* busi_buf = NULL;
			int busi_buf_len = 0;
			OC_NET_PACKAGE* resp_app_pkg = NULL;
			app_result = generate_cmd_app_control_req(&req_app_ctrl, 2); // shutdown operation
			app_result = translate_cmd2buf_app_control_req(req_app_ctrl, &busi_buf, &busi_buf_len);
			if (req_app_ctrl != NULL)
				free(req_app_ctrl);
			// Communication with server
			app_result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->app_port,
			OC_REQ_CTRL_APP, busi_buf, busi_buf_len, &resp_app_pkg);
			if (app_result != OC_SUCCESS) {
				log_warn_print(g_debug_verbose,
						"Notify app shutdown operation failed!net_business_communicate error=[%d]",
						app_result);
			}
			if (busi_buf != NULL)
				free(busi_buf);
			if (resp_app_pkg != NULL)
				free(resp_app_pkg);
		}
	} else {
		// Generate a error response
		OC_CMD_CTRL_SHUTDOWN_RESP* resp = NULL;
		uint8_t* busi_buf = NULL;
		uint8_t* temp_buf = NULL;
		int busi_buf_len = 0;
		int temp_buf_len = 0;

		uint32_t exec_result = 1;
		uint32_t error_no = 1;
		result = generate_cmd_ctrl_shutdown_resp(&resp, exec_result, error_no);
		if (resp == NULL)
			result = OC_FAILURE;

		if (result == OC_SUCCESS) {
			result = translate_cmd2buf_ctrl_shutdown_resp(resp, &busi_buf, &busi_buf_len);
			if (result == OC_SUCCESS) {
				result = generate_response_package(OC_REQ_CTRL_SHUTDOWN, busi_buf, busi_buf_len,
						&temp_buf, &temp_buf_len);
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
	}

	if(resp_buf_script!= NULL)
		free(resp_buf_script);

	return result;
}

/**
 * Startup status query process
 */
int main_busi_query_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_QUERY_STARTUP_REQ* query_startup_req = (OC_CMD_QUERY_STARTUP_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	//Route the request to the script daemon
	uint8_t* resp_buf_script = NULL;
	uint32_t resp_buf_script_len = 0;
	result = net_business_comm_route2((uint8_t*) OC_LOCAL_IP, g_config->script_port, req_buf,
			req_len, &resp_buf_script, &resp_buf_script_len);

	// Construct the response for invoker
	if (result == OC_SUCCESS) {
		// Analyze the script daemon response data

		memcpy(resp_buf, resp_buf_script, resp_buf_script_len);
		*resp_len = resp_buf_script_len;

		// Notify the GPIO daemon
		OC_CMD_QUERY_STARTUP_RESP* resp_startup_query =
				(OC_CMD_QUERY_STARTUP_RESP*) (resp_buf_script + OC_PKG_FRONT_PART_SIZE);
		if (resp_startup_query->is_finish == OC_TRUE) {
			// when startup process finish
			if (resp_startup_query->step_no == OC_STARTUP_TOTAL_STEP) {
				if (resp_startup_query->error_no == error_no_error) {
					// normal completed
					main_notify_gpio_action(5); //3:catch error, 4:resolve error, 5:running, 6:stopped
					g_cabinet_runtime_status = OC_CABINET_STATUS_RUNNING;
					g_cabinet_start_time = time(NULL);
					main_notify_gpio_action(4); //4:resolve error, 5:running, 6:stopped
				} else {
					// catch error
					main_notify_gpio_action(3); //3:catch error, 4:resolve error, 5:running, 6:stopped
				}
			} else {
				// catch error
				main_notify_gpio_action(3); //3:catch error, 4:resolve error, 5:running, 6:stopped
			}
		}

	} else {
		// Do nothing, will close the socket
	}

	if(resp_buf_script!= NULL)
		free(resp_buf_script);

	return result;
}

/**
 * Shutdown status query process
 */
int main_busi_query_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_QUERY_SHUTDOWN_REQ* query_startup_req = (OC_CMD_QUERY_SHUTDOWN_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	//Route the request to the script daemon
	uint8_t* resp_buf_script = NULL;
	uint32_t resp_buf_script_len = 0;
	result = net_business_comm_route2((uint8_t*) OC_LOCAL_IP, g_config->script_port, req_buf,
			req_len, &resp_buf_script, &resp_buf_script_len);

	// Construct the response for invoker
	if (result == OC_SUCCESS) {
		// Analyze the script daemon response data

		//memcpy(resp_buf, resp_buf_script, resp_buf_script_len);
		//*resp_len = resp_buf_script_len;

		// Notify the GPIO daemon
		OC_CMD_QUERY_SHUTDOWN_RESP* resp_shutdown_query =
				(OC_CMD_QUERY_SHUTDOWN_RESP*) (resp_buf_script + OC_PKG_FRONT_PART_SIZE);
		if (resp_shutdown_query->is_finish == OC_TRUE) {
			// when shutdown process finish
			if (resp_shutdown_query->step_no == OC_SHUTDOWN_TOTAL_STEP) {
				if (resp_shutdown_query->error_no == error_no_error) {
					// normal completed
					main_notify_gpio_action(6); //3:catch error, 4:resolve error, 5:running, 6:stopped
					g_cabinet_runtime_status = OC_CABINET_STATUS_STOPPED;
					g_cabinet_shutdown_time = time(NULL);
					main_notify_gpio_action(4); //4:resolve error, 5:running, 6:stopped
				} else {
					// catch error
					main_notify_gpio_action(3); //3:catch error, 4:resolve error, 5:running, 6:stopped
				}
			}
			if (g_config->power_off_enable == OC_TRUE) {
				if (g_config->power_off_force == OC_TRUE) {
					// send power off signal
					call_cabinet_power_off();

					// clear the error
					resp_shutdown_query->error_no = error_no_error;
					resp_shutdown_query->result = 0;
					resp_shutdown_query->reserved_1 = 1; // force power off
				} else {
					if (resp_shutdown_query->error_no == error_no_error) {
						// send power off signal
						call_cabinet_power_off();

						// clear the error
						resp_shutdown_query->error_no = error_no_error;
						resp_shutdown_query->result = 0;
						resp_shutdown_query->reserved_1 = 0;	//normal power off
					} else {
						// do pass
					}
				}
			}
		}
		memcpy(resp_buf, resp_buf_script, resp_buf_script_len);
		*resp_len = resp_buf_script_len;

	} else {
		// Do nothing, will close the socket
	}

	if(resp_buf_script!= NULL)
		free(resp_buf_script);

	return result;

}

/**
 * GPIO control process
 */
int main_busi_ctrl_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * GPIO status query process
 */
int main_busi_query_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * UART COM control process
 */
int main_busi_ctrl_com(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

}

/**
 * UART COM status query process
 */
int main_busi_query_com(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * SIM control process
 */
int main_busi_ctrl_sim(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * SIM status query process
 */
int main_busi_query_sim(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * Phone APP client control process
 */
int main_busi_ctrl_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * Phone APP client query process
 */
int main_busi_query_app(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

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
 * Notify the GPIO daemon do event action.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int main_notify_gpio_action(uint32_t gpio_event) {
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_CTRL_GPIO_REQ * req_gpio_ctrl = NULL;
	result = generate_cmd_ctrl_gpio_req(&req_gpio_ctrl, gpio_event, 0);
	result = translate_cmd2buf_ctrl_gpio_req(req_gpio_ctrl, &busi_buf, &busi_buf_len);
	if (req_gpio_ctrl != NULL)
		free(req_gpio_ctrl);

	OC_NET_PACKAGE* resp_pkg = NULL;
	OC_CMD_CTRL_GPIO_RESP* resp_gpio_ctrl = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->gpio_port,
	OC_REQ_CTRL_GPIO, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS) {
		log_debug_print(g_debug_verbose, "Debug: net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_gpio_ctrl = (OC_CMD_CTRL_GPIO_RESP*) resp_pkg->data;
		if (resp_gpio_ctrl->result == 0) {
			log_info_print(g_debug_verbose, "GPIO event %d process normal.", gpio_event);
		} else {
			log_info_print(g_debug_verbose, "GPIO event %d process catch error.", gpio_event);
		}
	} else {
		log_error_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return result;

}

/**
 * Timer event process
 */
void timer_event_process() {

	// Update counter
	g_watch_dog_counter++;

	if (g_server_check_counter > 0)
		g_server_check_counter--;
	if (g_cabinet_check_counter > 0)
		g_cabinet_check_counter--;

	if (g_server_check_counter == 0 || g_manual_server_check == OC_TRUE) {
		//do server status check query
		timer_event_query_server_status();

		g_server_check_counter = OC_SERVER_STATUS_CHECK_SEC;

		if (g_manual_server_check == OC_TRUE)
			g_manual_server_check = OC_FALSE;
	}

	if (g_cabinet_check_counter == 0) {
		//do cabinet status check query
		timer_event_query_cabinet_status();

		g_cabinet_check_counter = OC_CABINET_STATUS_CHECK_SEC;
	}

	// Send heart beat to watch dog
	if (g_watch_dog_counter >= OC_WATCH_DOG_FEED_SEC) {
		// feed dog
		timer_event_watchdog_heartbeat();

		g_watch_dog_counter = 0;
	}
}

/**
 * Query all servers status in timer event.
 */
void timer_event_query_server_status() {
	int result = OC_SUCCESS;

	uint8_t* requst_buf = NULL;
	uint8_t* busi_cabinet_buf = NULL;
	uint8_t* busi_server_buf = NULL;
	int req_buf_len = 0;
	int cabinet_buf_len = 0;
	int server_buf_len = 0;
	int i = 0;

	OC_NET_PACKAGE* resp_pkg_server = NULL;
	OC_CMD_QUERY_SERVER_REQ * req_server = NULL;
	uint32_t type = 0;
	result = generate_cmd_query_server_req(&req_server, type, NULL, 1);
	result = translate_cmd2buf_query_server_req(req_server, &busi_server_buf, &server_buf_len);
	if (req_server != NULL)
		free(req_server);

	// Communication with server
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->script_port,
	OC_REQ_QUERY_SERVER_STATUS, busi_server_buf, server_buf_len, &resp_pkg_server);
	OC_CMD_QUERY_SERVER_RESP* query_server = NULL;

	int is_all_running = OC_TRUE;
	int is_all_stopped = OC_TRUE;
	int is_error = OC_FALSE;
	int status = (g_cabinet_runtime_status == OC_CABINET_STATUS_STOPPED ? 0 : 1);
	if(resp_pkg_server != NULL) {
		query_server = (OC_CMD_QUERY_SERVER_RESP*) resp_pkg_server->data;
		if (query_server->server_num > 0) {
			OC_SERVER_STATUS* server_data = (OC_SERVER_STATUS*) (((char*) query_server)
					+ sizeof(query_server->command) + sizeof(query_server->server_num));
			// all stopped check
			for (i = 0; i < query_server->server_num; i++) {
				if (server_data[i].status == 1) {
					is_all_stopped = OC_FALSE;
					break;
				}
			}
			// all running check
			for (i = 0; i < query_server->server_num; i++) {
				if (server_data[i].status == 0) {
					is_all_running = OC_FALSE;
					break;
				}
			}
			if (is_all_stopped == OC_TRUE) {
				// when do stop control, all stop is compeleted
				//if (g_cabinet_runtime_status == OC_CABINET_STATUS_STOPPING
				//		|| g_cabinet_runtime_status == OC_CABINET_STATUS_RUNNING)
				if (g_cabinet_runtime_status == OC_CABINET_STATUS_RUNNING)
					g_cabinet_runtime_status = OC_CABINET_STATUS_STOPPED;
			} else if (is_all_running == OC_TRUE) {
				// when box reboot
				if (g_cabinet_runtime_status == OC_CABINET_STATUS_STOPPED)
					g_cabinet_runtime_status = OC_CABINET_STATUS_RUNNING;
			} else {
				// when in running or stopped, some server still running, means catch an error
				if (g_cabinet_runtime_status == OC_CABINET_STATUS_STOPPED
						|| g_cabinet_runtime_status == OC_CABINET_STATUS_RUNNING)
					is_error = OC_TRUE;
			}

			// update the GPIO LED status
			if (is_error == OC_TRUE) {
				main_notify_gpio_action(3); //3:catch error, 4:resolve error, 5:running, 6:stopped
				g_cabinet_runtime_error = OC_TRUE;
				status = 2; //error
			} else {
				if (g_cabinet_runtime_status == OC_CABINET_STATUS_STOPPED
						&& g_cabinet_shutdown_time > g_cabinet_start_time) {
					main_notify_gpio_action(6); //3:catch error, 4:resolve error, 5:running, 6:stopped
					status = 0;  // stopped
				}
				if (g_cabinet_runtime_status == OC_CABINET_STATUS_RUNNING) {
					main_notify_gpio_action(5); //3:catch error, 4:resolve error, 5:running, 6:stopped
					status = 1;  // running
				}
				g_cabinet_runtime_error = OC_FALSE;
			}
			// save cabinet status into file
			update_cabinet_status_into_file(g_config->cabinet_status_file, status, g_cabinet_start_time,
					g_cabinet_shutdown_time);

		} else {
			log_warn_print(g_debug_verbose, "Query server status return empty server list.\n");
		}
	} else {
		log_warn_print(g_debug_verbose, "Query server status error,script daemon return NULL.\n");
	}

	if (busi_server_buf != NULL)
		free(busi_server_buf);

	if (resp_pkg_server != NULL)
		free_network_package(resp_pkg_server);
}

/**
 * Query cabinet status in timer event.
 */
void timer_event_query_cabinet_status() {
	// Get cabinet status

	// Collect electricity data

	// Collect temperature
}

void timer_event_watchdog_heartbeat() {
	int result = OC_SUCCESS;

	uint8_t* requst_buf = NULL;
	uint8_t* busi_server_buf = NULL;
	int server_buf_len = 0;

	OC_NET_PACKAGE* resp_pkg_server = NULL;
	OC_CMD_HEART_BEAT_REQ * req_heart_beat = NULL;
	uint32_t device = device_proc_main;
	uint32_t status = OC_HEALTH_OK;
	result = generate_cmd_heart_beat_req(&req_heart_beat, device, status, 0);
	result = translate_cmd2buf_heart_beat_req(req_heart_beat, &busi_server_buf, &server_buf_len);
	if (req_heart_beat != NULL)
		free(req_heart_beat);

	char time_temp_buf[32];
	helper_get_current_time_str(time_temp_buf, 32, NULL);

	// Communication with server
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->watchdog_port,
	OC_REQ_HEART_BEAT, busi_server_buf, server_buf_len, &resp_pkg_server);
	//OC_CMD_HEART_BEAT_RESP* resp_heart_beat = (OC_CMD_HEART_BEAT_RESP*) resp_pkg_server->data;
	if(result == OC_SUCCESS)
		log_debug_print(g_debug_verbose, "[%s] Send watchdog heart beat success", time_temp_buf);
	else
		log_debug_print(g_debug_verbose, "[%s] Send watchdog heart beat failure", time_temp_buf);

	if (busi_server_buf != NULL)
		free(busi_server_buf);

	if (resp_pkg_server != NULL)
		free_network_package(resp_pkg_server);
}
/**
 * Get listen port of COM by type
 */
int get_com_listen_port_by_type(int type, int offset) {
	if (g_config == NULL || g_config->com_num < 1)
		return 0;

	int i = 0;
	int offset_counter = 0;
	int listen_port = 0;

	for (i = 0; i < MAX_UART_COM_NUM; i++) {
		if (g_config->com_config[i].type > 0 && g_config->com_config[i].type == type) {
			if (offset_counter == offset) {
				listen_port = g_config->com_config[i].listen_port;
				break;
			}
			offset_counter++;
		}
	}

	return listen_port;
}

/**
 * Collect cabinet temperature.
 */
int collect_cabinet_temperature(float* out_temperature) {
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;

	OC_CMD_QUERY_TEMPERATURE_REQ * req_query_temperature = NULL;
	result = generate_cmd_query_temperature_req(&req_query_temperature, 0);
	result = translate_cmd2buf_query_temperature_req(req_query_temperature, &busi_buf,
			&busi_buf_len);
	if (req_query_temperature != NULL)
		free(req_query_temperature);

	OC_NET_PACKAGE* resp_pkg = NULL;
	OC_CMD_QUERY_TEMPERATURE_RESP* resp_query_temperature = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->temperature_port,
	OC_REQ_QUERY_TEMPERATURE, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS && resp_pkg != NULL) {
		log_debug_print(g_debug_verbose,
				"Query temperature, net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_query_temperature = (OC_CMD_QUERY_TEMPERATURE_RESP*) resp_pkg->data;
		*out_temperature = (resp_query_temperature->t1 + resp_query_temperature->t2) / 2;
	} else {
		log_debug_print(g_debug_verbose,
				"Query temperature, net_business_communicate return OC_FAILURE");
		*out_temperature = 0.0F;
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return result;
}

/**
 * Collect cabinet temperature.
 */
int collect_cabinet_electricity(float* out_kwh, float* out_voltage, float* out_current,
		float* out_watt) {
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;
	int listen_port = get_com_listen_port_by_type(OC_COM_TYPE_ELECTRICITY, 0);
	if (listen_port < 1) {
		log_error_print(g_debug_verbose, "Electricity daemon listen port error!");
		return OC_FAILURE;
	}

	OC_CMD_QUERY_ELECTRICITY_REQ * req_query_electricity = NULL;
	result = generate_cmd_query_electricity_req(&req_query_electricity, 0);
	result = translate_cmd2buf_query_electricity_req(req_query_electricity, &busi_buf,
			&busi_buf_len);
	if (req_query_electricity != NULL)
		free(req_query_electricity);

	OC_NET_PACKAGE* resp_pkg = NULL;

	OC_CMD_QUERY_ELECTRICITY_RESP* resp_query_electricity = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, listen_port,
	OC_REQ_QUERY_ELECTRICITY, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS && resp_pkg != NULL) {
		log_debug_print(g_debug_verbose,
				"Query electricity, net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_query_electricity = (OC_CMD_QUERY_ELECTRICITY_RESP*) resp_pkg->data;
		*out_kwh = resp_query_electricity->kwh;
		*out_voltage = resp_query_electricity->voltage;
		*out_current = resp_query_electricity->current;
		*out_watt = resp_query_electricity->kw;
	} else {
		log_debug_print(g_debug_verbose,
				"Query electricity, net_business_communicate return OC_FAILURE");
		*out_kwh = 0.0F;
		*out_voltage = 0.0F;
		*out_current = 0.0F;
		*out_watt = 0.0F;
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return result;
}

/**
 * Collect cabinet voice dB.
 */
int collect_cabinet_voice_db(float* out_voice_db) {
	int result = OC_SUCCESS;

	*out_voice_db = 40.0F;

	return result;
}

/**
 * Call cabinet Power On.
 */
int call_cabinet_power_on() {
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;
	int listen_port = get_com_listen_port_by_type(OC_COM_TYPE_ELECTRICITY, 0);
	if (listen_port < 1) {
		log_error_print(g_debug_verbose, "Electricity daemon listen port error!");
		return OC_FAILURE;
	}

	OC_CMD_CTRL_ELECTRICITY_REQ * req_ctrl_electricity = NULL;
	result = generate_cmd_ctrl_electricity_req(&req_ctrl_electricity, 1, 0); // turn on
	result = translate_cmd2buf_ctrl_electricity_req(req_ctrl_electricity, &busi_buf, &busi_buf_len);
	if (req_ctrl_electricity != NULL)
		free(req_ctrl_electricity);

	OC_NET_PACKAGE* resp_pkg = NULL;

	OC_CMD_CTRL_ELECTRICITY_RESP* resp_ctrl_electricity = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, listen_port,
	OC_REQ_CTRL_ELECTRICITY, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS && resp_pkg != NULL) {
		log_debug_print(g_debug_verbose, "Power On, net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_ctrl_electricity = (OC_CMD_CTRL_ELECTRICITY_RESP*) resp_pkg->data;

		if (resp_ctrl_electricity->result == 0) {
			log_info_print(g_debug_verbose, "Power On Success");
		} else {
			log_info_print(g_debug_verbose, "Power On Failure");
		}

	} else {
		log_debug_print(g_debug_verbose,
				"Debug: Power On, net_business_communicate return OC_FAILURE");
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return result;
}

/**
 * Call cabinet Power Off.
 */
int call_cabinet_power_off() {
	int result = OC_SUCCESS;

	// Construct the command
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;
	int listen_port = get_com_listen_port_by_type(OC_COM_TYPE_ELECTRICITY, 0);
	if (listen_port < 1) {
		log_error_print(g_debug_verbose, "Error: Electricity daemon listen port error!");
		return OC_FAILURE;
	}

	OC_CMD_CTRL_ELECTRICITY_REQ * req_ctrl_electricity = NULL;
	result = generate_cmd_ctrl_electricity_req(&req_ctrl_electricity, 2, 0); // turn off
	result = translate_cmd2buf_ctrl_electricity_req(req_ctrl_electricity, &busi_buf, &busi_buf_len);
	if (req_ctrl_electricity != NULL)
		free(req_ctrl_electricity);

	OC_NET_PACKAGE* resp_pkg = NULL;

	OC_CMD_CTRL_ELECTRICITY_RESP* resp_ctrl_electricity = NULL;
	result = net_business_communicate((uint8_t*) OC_LOCAL_IP, listen_port,
	OC_REQ_CTRL_ELECTRICITY, busi_buf, busi_buf_len, &resp_pkg);
	free(busi_buf);

	if (result == OC_SUCCESS && resp_pkg != NULL) {
		log_debug_print(g_debug_verbose, "Power Off, net_business_communicate return OC_SUCCESS");

		// notify the invoker
		resp_ctrl_electricity = (OC_CMD_CTRL_ELECTRICITY_RESP*) resp_pkg->data;

		if (resp_ctrl_electricity->result == 0) {
			log_info_print(g_debug_verbose, "Power Off Success");
		} else {
			log_info_print(g_debug_verbose, "Power Off Failure");
		}

	} else {
		log_debug_print(g_debug_verbose, "Power Off, net_business_communicate return OC_FAILURE");
	}

	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return result;
}
