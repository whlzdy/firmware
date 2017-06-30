/*
 * watch_dog.c
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
#include <pthread.h>
#include <sys/io.h>
#include <signal.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"
#include "watch_dog.h"
#include "util/script_helper.h"
#include "util/log_helper.h"
#include "util/date_helper.h"

// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
void timer_event_process();

// Debug output level
int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;
static int g_watch_dog_enable = OC_TRUE;

// Timer thread
pthread_t g_timer_thread_id = 0;
int g_timer_running = OC_FALSE;

// Heart beat & watch dog
int g_hard_watch_dog_counter = OC_WATCH_DOG_HARD_RESET_SEC;
int g_soft_main_proc_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_sim_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_gpio_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_script_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_app_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_temperature_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_voice_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_gps_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_gpsusb_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_lbs_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
int g_soft_com0_counter = -1;
int g_soft_com1_counter = -1;
int g_soft_com2_counter = -1; //electricity daemon
int g_soft_com3_counter = -1;
int g_soft_com4_counter = -1;
int g_soft_com5_counter = -1;

/**
 * Signal handler
 */
void sig_handler(const int sig) {
	// release hardware watch dog access
	ioperm(WATCHDOG_MIN_ADDR, WATCHDOG_MAX_ADDR, OC_FLAG_DISABLE);

	// release sub thread
	g_timer_running = OC_FALSE;
	sleep(1);

	log_info_print(g_debug_verbose, "SIGINT handled.");
	log_info_print(g_debug_verbose, "Now stop watch dog.");

	exit(EXIT_SUCCESS);
}

/**
 * Initialize hardware watch dog
 */
void init_hardware_watch_dog() {
	ioperm(WATCHDOG_MIN_ADDR, WATCHDOG_MAX_ADDR, OC_FLAG_ENABLE);

	// choice second mode
	outb(0x00, WATCHDOG_MODE);
	// after 60 second, reset
	outb(OC_WATCH_DOG_HARD_RESET_SEC, WATCHDOG_COUNTER);

	log_info_print(g_debug_verbose, "Initialize hardware watch dog.");
}

/**
 * Feed hardware watch dog
 */
void feed_hardware_watch_dog() {
	// after 60 second, reset
	outb(OC_WATCH_DOG_HARD_RESET_SEC, WATCHDOG_COUNTER);
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
		fprintf(stderr, "Error: load config file failure.[%s]\n", config);
		return result;
	}
	print_config_detail();

	g_config = (struct settings*) malloc(sizeof(struct settings));
	memset((void*) g_config, 0, sizeof(struct settings));

	g_config->role = role_watchdog;

	temp_int = 0;
	get_parameter_int(SECTION_WATCHDOG, "debug", &temp_int, OC_DEBUG_LEVEL_DISABLE);
	g_debug_verbose = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_WATCHDOG, "enable", &temp_int, OC_TRUE);
	g_watch_dog_enable = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "listen_port", &temp_int,
	OC_MAIN_DEFAULT_PORT);
	g_config->main_listen_port = temp_int;

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

	temp_int = 0;
	get_parameter_int(SECTION_VOICE, "listen_port", &temp_int,
	OC_VOICE_PORT);
	g_config->voice_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_GPS, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		temp_int = 0;
		get_parameter_int(SECTION_GPS, "listen_port", &temp_int,
		OC_GPS_PORT);
		g_config->gps_port = temp_int;
	}

	temp_int = 0;
	get_parameter_int(SECTION_GPSUSB, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		temp_int = 0;
		get_parameter_int(SECTION_GPSUSB, "listen_port", &temp_int,
		OC_GPSUSB_PORT);
		g_config->gpsusb_port = temp_int;
	}

	temp_int = 0;
	get_parameter_int(SECTION_LBS, "enable", &temp_int, OC_FALSE);
	if (temp_int == OC_TRUE) {
		temp_int = 0;
		get_parameter_int(SECTION_LBS, "listen_port", &temp_int,
		OC_LBS_PORT);
		g_config->lbs_port = temp_int;
	}

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
		g_soft_com0_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
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
		g_soft_com1_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
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
		g_soft_com2_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
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
		g_soft_com3_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
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
		g_soft_com4_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
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
		g_soft_com5_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		com_count++;
	}
	g_config->com_num = com_count;

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
			log_error_print(g_debug_verbose, "Socket[%d] closed by client.\n", fd);
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
			fprintf(stderr, "usage: watch_dog [configuration file]\n");
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
	s_addr_in.sin_port = htons(g_config->watchdog_port);
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

	// Initialize the hardware watch dog
	init_hardware_watch_dog();

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
		} else {
			pthread_detach(thread_id);
		}
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
 * Heart beat process
 */
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_HEART_BEAT_REQ* ctrl_heart_beat_req = (OC_CMD_HEART_BEAT_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	char time_temp_buf[32];
	helper_get_current_time_str(time_temp_buf, 32, NULL);

	uint32_t exec_result = 0;

	uint32_t device = ctrl_heart_beat_req->device;
	switch (device) {
	case device_proc_main:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of main_daemon", time_temp_buf);
		g_soft_main_proc_counter = (
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_main_proc_counter);
		break;

	case device_proc_watchdog:
		break;

	case device_proc_sim:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of sim_daemon", time_temp_buf);
		g_soft_sim_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_sim_counter;
		break;

	case device_proc_gpio:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of gpio_daemon", time_temp_buf);
		g_soft_gpio_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_gpio_counter;
		break;

	case device_proc_script:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of script_daemon", time_temp_buf);
		g_soft_script_counter =
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_script_counter;
		break;

	case device_proc_app_service:
		log_debug_print(g_debug_verbose, "[%s]eceive heart beat of app_service", time_temp_buf);
		g_soft_app_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_app_counter;
		break;

	case device_proc_uart_com0:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of COM0 daemon", time_temp_buf);
		g_soft_com0_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_com0_counter;
		break;

	case device_proc_uart_com1:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of COM1 daemon", time_temp_buf);
		g_soft_com1_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_com1_counter;
		break;

	case device_proc_uart_com2:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of COM2 daemon", time_temp_buf);
		g_soft_com2_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_com2_counter;
		break;

	case device_proc_uart_com3:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of COM3 daemon", time_temp_buf);
		g_soft_com3_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_com3_counter;
		break;

	case device_proc_uart_com4:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of COM4 daemon", time_temp_buf);
		g_soft_com4_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_com4_counter;
		break;

	case device_proc_uart_com5:
		log_debug_print(g_debug_verbose, "Receive heart beat of COM5 daemon", time_temp_buf);
		g_soft_com5_counter = ctrl_heart_beat_req->status == OC_HEALTH_OK ?
		OC_WATCH_DOG_SOFT_RESET_SEC :
																			g_soft_com5_counter;
		break;

	case device_temperature:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of temperature_daemon", time_temp_buf);
		g_soft_temperature_counter =
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_temperature_counter;
		break;

	case device_voice:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of voice_daemon", time_temp_buf);
		g_soft_voice_counter =
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_voice_counter;
		break;

	case device_gps:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of gps_daemon", time_temp_buf);
		g_soft_gps_counter =
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_gps_counter;
		break;

	case device_gpsusb:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of gpsusb_daemon", time_temp_buf);
		g_soft_gpsusb_counter =
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_gpsusb_counter;
		break;

	case device_lbs:
		log_debug_print(g_debug_verbose, "[%s]Receive heart beat of lbs_daemon", time_temp_buf);
		g_soft_lbs_counter =
				ctrl_heart_beat_req->status == OC_HEALTH_OK ?
				OC_WATCH_DOG_SOFT_RESET_SEC :
																g_soft_lbs_counter;
		break;

	default:
		break;
	}

	// Generate a response
	OC_CMD_HEART_BEAT_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

	result = generate_cmd_heart_beat_resp(&resp, exec_result);
	if (resp == NULL)
		result = OC_FAILURE;

	if (result == OC_SUCCESS) {
		result = translate_cmd2buf_heart_beat_resp(resp, &busi_buf, &busi_buf_len);
		if (result == OC_SUCCESS) {
			result = generate_response_package(OC_REQ_HEART_BEAT, busi_buf, busi_buf_len, &temp_buf,
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
 * Timer event process
 */
void timer_event_process() {

	int result = OC_SUCCESS;

	// Update counter
	if (g_hard_watch_dog_counter > 0)
		g_hard_watch_dog_counter--;
	if (g_soft_main_proc_counter > 0)
		g_soft_main_proc_counter--;
	if (g_soft_sim_counter > 0)
		g_soft_sim_counter--;
	if (g_soft_gpio_counter > 0)
		g_soft_gpio_counter--;
	if (g_soft_script_counter > 0)
		g_soft_script_counter--;
	if (g_soft_app_counter > 0)
		g_soft_app_counter--;
	if (g_soft_temperature_counter > 0)
		g_soft_temperature_counter--;
	if (g_soft_voice_counter > 0)
		g_soft_voice_counter--;
	if (g_soft_gps_counter > 0)
		g_soft_gps_counter--;
	if (g_soft_gpsusb_counter > 0)
		g_soft_gpsusb_counter--;
	if (g_soft_lbs_counter > 0)
		g_soft_lbs_counter--;
	if (g_soft_com0_counter > 0)
		g_soft_com0_counter--;
	if (g_soft_com1_counter > 0)
		g_soft_com1_counter--;
	if (g_soft_com2_counter > 0)
		g_soft_com2_counter--;
	if (g_soft_com3_counter > 0)
		g_soft_com3_counter--;
	if (g_soft_com4_counter > 0)
		g_soft_com4_counter--;
	if (g_soft_com5_counter > 0)
		g_soft_com5_counter--;


//	fprintf(stderr, "Debug: -------------------------\ng_hard_watch_dog_counter=%d, \ng_soft_main_proc_counter=%d, \ng_soft_sim_counter=%d, \ng_soft_gpio_counter=%d,\n g_soft_script_counter=%d,\n g_soft_app_counter=%d,\n g_soft_temperature_counter=%d,\n g_soft_com2_counter=%d\n-------------------------\n",
//			        g_hard_watch_dog_counter,
//			        g_soft_main_proc_counter,
//			        g_soft_sim_counter,
//			        g_soft_gpio_counter,
//			        g_soft_script_counter,
//			        g_soft_app_counter,
//			        g_soft_temperature_counter,
//			        g_soft_com2_counter);

	uint8_t script_result[MAX_CONSOLE_BUFFER];
	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);
	char time_temp_buf[32];

	if (g_watch_dog_enable == OC_TRUE) {
		if (g_soft_main_proc_counter == 0) {
			//Reset the Main daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the main_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_MAIN_DAEMON,
					script_result);
			g_soft_main_proc_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->sim_port > 0 && g_soft_sim_counter == 0) {
			//Reset the SIM daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the sim_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_SIM_DAEMON, script_result);
			g_soft_sim_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->gpio_port > 0 && g_soft_gpio_counter == 0) {
			//Reset the GPIO daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the gpio_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_GPIO_DAEMON,
					script_result);
			g_soft_gpio_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->script_port > 0 && g_soft_script_counter == 0) {
			//Reset the Script daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the script_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_SCRIPT_DAEMON,
					script_result);
			g_soft_script_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->app_port > 0 && g_soft_app_counter == 0) {
			//Reset the APP service daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the app_service", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_APP_SERVICE,
					script_result);
			g_soft_app_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->temperature_port > 0 && g_soft_temperature_counter == 0) {
			//Reset the Temperature daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the temperature_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_TEMPERATURE_DAEMON,
					script_result);
			g_soft_temperature_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->voice_port > 0 && g_soft_voice_counter == 0) {
			//Reset the Voice daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the voice_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_VOICE_DAEMON,
					script_result);
			g_soft_voice_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->gps_port > 0 && g_soft_gps_counter == 0) {
			//Reset the Voice daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the gps_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_GPS_DAEMON,
					script_result);
			g_soft_gps_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->gpsusb_port > 0 && g_soft_gpsusb_counter == 0) {
			//Reset the Voice daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the gpsusb_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_GPSUSB_DAEMON,
					script_result);
			g_soft_gpsusb_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->lbs_port > 0 && g_soft_lbs_counter == 0) {
			//Reset the Voice daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the lbs_daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_LBS_DAEMON,
					script_result);
			g_soft_lbs_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->com_config[0].listen_port > 0 && g_soft_com0_counter == 0) {
			//Reset the COM0 daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the COM0 daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_COM0_DAEMON,
					script_result);
			g_soft_com0_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->com_config[1].listen_port > 0 && g_soft_com1_counter == 0) {
			//Reset the COM1 daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the COM1 daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_COM1_DAEMON,
					script_result);
			g_soft_com1_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->com_config[2].listen_port > 0 && g_soft_com2_counter == 0) {
			//Reset the COM2 daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the COM2 daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_COM2_DAEMON,
					script_result);
			g_soft_com2_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->com_config[3].listen_port > 0 && g_soft_com3_counter == 0) {
			//Reset the COM3 daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the COM3 daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_COM3_DAEMON,
					script_result);
			g_soft_com3_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->com_config[4].listen_port > 0 && g_soft_com4_counter == 0) {
			//Reset the COM4 daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the COM4 daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_COM4_DAEMON,
					script_result);
			g_soft_com4_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		if (g_config->com_config[5].listen_port > 0 && g_soft_com5_counter == 0) {
			//Reset the COM5 daemon
			helper_get_current_time_str(time_temp_buf, 32, NULL);
			log_warn_print(g_debug_verbose, "[%s] now restart the COM5 daemon", time_temp_buf);

			result = script_shell_exec((unsigned char*) WG_SHELL_RESTART_COM5_DAEMON,
					script_result);
			g_soft_com5_counter = OC_WATCH_DOG_SOFT_RESET_SEC;
		}
		// Hardware watch dog reset
		if (g_soft_main_proc_counter > 0 && g_soft_sim_counter > 0 && g_soft_gpio_counter > 0
				&& g_soft_script_counter > 0 && g_soft_app_counter > 0
//				&& g_soft_temperature_counter > 0 && g_soft_voice_counter > 0 && g_soft_gps_counter > 0 && g_soft_com0_counter != 0
//				&& g_soft_com1_counter != 0 && g_soft_com2_counter != 0 && g_soft_com3_counter != 0
//				&& g_soft_com4_counter != 0 && g_soft_com5_counter != 0
				) {
			g_hard_watch_dog_counter = OC_WATCH_DOG_HARD_RESET_SEC;
			feed_hardware_watch_dog();
		}
	} else {
		feed_hardware_watch_dog();
	}
}

