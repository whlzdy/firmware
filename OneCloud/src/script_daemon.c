/*
 * script_daemon.c
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
#include <signal.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"
#include "common/error.h"
#include "util/script_helper.h"
#include "util/log_helper.h"
#include "util/date_helper.h"

#include "script.h"

// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_server_status(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int script_shell_exec(unsigned char* script_file, unsigned char* result);
int str_parse_server_data(uint8_t* buffer, char* server_id, OC_SERVER_STATUS** server_data,
		uint32_t* server_num);
int analyze_startup_result(uint32_t step_no, int device_status, char* device_name,
		uint32_t* is_finish, uint32_t* error_no, uint8_t* message);
int analyze_shutdown_result(uint32_t step_no, int device_status, char* device_name,
		uint32_t* is_finish, uint32_t* error_no, uint8_t* message);

void timer_event_process();
void timer_event_watchdog_heartbeat();

// Debug output level
int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;

// Timer thread
pthread_t g_timer_thread_id = 0;
int g_timer_running = OC_FALSE;
int g_watch_dog_counter = 0;

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

	g_config->role = role_script;

	temp_int = 0;
	get_parameter_int(SECTION_SCRIPT, "debug", &temp_int, OC_DEBUG_LEVEL_DISABLE);
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
	get_parameter_int(SECTION_SCRIPT, "listen_port", &temp_int,
	OC_SCRIPT_DEFAULT_PORT);
	g_config->script_port = temp_int;

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_SCRIPT, "startup_ctrl_script", temp_value,
			"/opt/onecloud/script/startup.sh");
	strcpy(g_config->startup_ctrl_script, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_SCRIPT, "startup_query_script", temp_value,
			"/opt/onecloud/script/startup_check.sh");
	strcpy(g_config->startup_query_script, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_SCRIPT, "shutdown_ctrl_script", temp_value,
			"/opt/onecloud/script/shutdown.sh");
	strcpy(g_config->shutdown_ctrl_script, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_SCRIPT, "shutdown_query_script", temp_value,
			"/opt/onecloud/script/shutdown_check.sh");
	strcpy(g_config->shutdown_query_script, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_SCRIPT, "server_query_script", temp_value,
			"/opt/onecloud/script/server/server_check.sh");
	strcpy(g_config->server_query_script, temp_value);

	memset(temp_value, 0, sizeof(temp_value));
	get_parameter_str(SECTION_SCRIPT, "exec_server_query_script", temp_value,
			"/opt/onecloud/script/server/do_server_check.sh");
	strcpy(g_config->exec_server_query_script, temp_value);

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
 * Signal handler
 */
void sig_handler(const int sig) {
	// release sub thread
	g_timer_running = OC_FALSE;
	sleep(1);

	log_info_print(g_debug_verbose, "SIGINT handled.");
	log_info_print(g_debug_verbose, "Now stop script daemon.");

	exit(EXIT_SUCCESS);
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
			fprintf(stderr, "usage: script_daemon [configuration file]\n");
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
	s_addr_in.sin_port = htons(g_config->script_port);
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

	case OC_REQ_QUERY_SERVER_STATUS:
		result = main_busi_query_server_status(req_buf, req_len, resp_buf, resp_len);
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
 * Startup control process
 */
int main_busi_ctrl_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_CTRL_STARTUP_REQ* startup_ctrl_req = (OC_CMD_CTRL_STARTUP_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	///////////////////////////////////////////////////////////
	// Execute startup business logic
	///////////////////////////////////////////////////////////
	uint32_t exec_result = 0;
	uint32_t error_no = 0;
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);

	result = script_shell_exec((unsigned char*) (g_config->startup_ctrl_script), script_result);
	if (strncmp((const char*) script_result, "0", 1) == 0)
		exec_result = 0;
	else
		exec_result = 1;

	///////////////////////////////////////////////////////////
	// Construct response
	///////////////////////////////////////////////////////////
	OC_CMD_CTRL_STARTUP_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

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

	return result;
}

/**
 * Shutdown control process
 */
int main_busi_ctrl_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_CTRL_SHUTDOWN_REQ* shutdown_ctrl_req = (OC_CMD_CTRL_SHUTDOWN_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	///////////////////////////////////////////////////////////
	// Execute shutdown business logic
	///////////////////////////////////////////////////////////
	uint32_t exec_result = 0;
	uint32_t error_no = 0;
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);

	result = script_shell_exec((unsigned char*) (g_config->shutdown_ctrl_script), script_result);
	if (strncmp((const char*) script_result, "0", 1) == 0)
		exec_result = 0;
	else
		exec_result = 1;

	///////////////////////////////////////////////////////////
	// Construct response
	///////////////////////////////////////////////////////////
	OC_CMD_CTRL_SHUTDOWN_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

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

	return result;
}

/**
 * Startup status query process
 */
int main_busi_query_startup(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_QUERY_STARTUP_REQ* startup_query_req = (OC_CMD_QUERY_STARTUP_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	///////////////////////////////////////////////////////////
	// Execute startup status query business logic
	///////////////////////////////////////////////////////////
	uint32_t exec_result = 0;
	uint32_t error_no = 0;
	uint32_t step_no = 0;
	uint32_t is_finish = 0;
	uint32_t temp = 0;
	uint8_t message[128];
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	long timestamp = 0;
	int device_status = -1;
	char device_name[MAX_PARAM_NAME_LEN];

	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);
	memset((void*) message, 0, sizeof(message));

	result = script_shell_exec((unsigned char*) (g_config->startup_query_script), script_result);
	if (strncmp((const char*) script_result, "1", 1) == 0)
		exec_result = 1;
	else {
		exec_result = 0;
		if (strlen((const char*) script_result) > 0) {
			memset(device_name, 0, sizeof(device_name));
			sscanf((const char*) script_result, "%d %d %d %d %s", &temp, &timestamp, &step_no,
					&device_status, device_name);
		}
	}

	if (exec_result == 0) {
		result = analyze_startup_result(step_no, device_status, device_name, &is_finish, &error_no,
				message);
	}

	///////////////////////////////////////////////////////////
	// Construct response
	///////////////////////////////////////////////////////////
	OC_CMD_QUERY_STARTUP_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

	result = generate_cmd_query_startup_resp(&resp, exec_result, error_no, step_no, is_finish,
			message);
	if (resp == NULL)
		result = OC_FAILURE;

	if (result == OC_SUCCESS) {
		result = translate_cmd2buf_query_startup_resp(resp, &busi_buf, &busi_buf_len);
		if (result == OC_SUCCESS) {
			result = generate_response_package(OC_REQ_QUERY_STARTUP, busi_buf, busi_buf_len,
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
 * Shutdown status query process
 */
int main_busi_query_shutdown(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;
	OC_CMD_QUERY_SHUTDOWN_REQ* shutdown_query_req = (OC_CMD_QUERY_SHUTDOWN_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);

	///////////////////////////////////////////////////////////
	// Execute startup status query business logic
	///////////////////////////////////////////////////////////
	uint32_t exec_result = 0;
	uint32_t error_no = 0;
	uint32_t step_no = 0;
	uint32_t is_finish = 0;
	uint32_t temp = 0;
	uint8_t message[128];
	uint8_t script_result[MAX_CONSOLE_BUFFER];
	long timestamp = 0;
	int device_status = -1;
	char device_name[MAX_PARAM_NAME_LEN];

	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);
	memset((void*) message, 0, sizeof(message));

	result = script_shell_exec((unsigned char*) (g_config->shutdown_query_script), script_result);
	if (strncmp((const char*) script_result, "1", 1) == 0)
		exec_result = 1;
	else {
		exec_result = 0;
		if (strlen((const char*) script_result) > 0) {
			memset(device_name, 0, sizeof(device_name));
			sscanf((const char*) script_result, "%d %d %d %d %s", &temp, &timestamp, &step_no,
					&device_status, device_name);
		}
	}
	if (exec_result == 0) {
		result = analyze_shutdown_result(step_no, device_status, device_name, &is_finish, &error_no,
				message);
	}

	///////////////////////////////////////////////////////////
	// Construct response
	///////////////////////////////////////////////////////////
	OC_CMD_QUERY_SHUTDOWN_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

	result = generate_cmd_query_shutdown_resp(&resp, exec_result, error_no, step_no, is_finish,
			message);
	if (resp == NULL)
		result = OC_FAILURE;

	if (result == OC_SUCCESS) {
		result = translate_cmd2buf_query_shutdown_resp(resp, &busi_buf, &busi_buf_len);
		if (result == OC_SUCCESS) {
			result = generate_response_package(OC_REQ_QUERY_SHUTDOWN, busi_buf, busi_buf_len,
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

	///////////////////////////////////////////////////////////
	// Execute server status query business logic
	///////////////////////////////////////////////////////////
	uint32_t server_num = 0;
	OC_SERVER_STATUS* server_data = NULL;

	uint8_t script_result[MAX_CONSOLE_BUFFER];
	memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);

	result = script_shell_exec((unsigned char*) (g_config->server_query_script), script_result);
	if (strncmp((const char*) script_result, "0", 1) == 0) {
		// Execute fail
		server_num = 0;
	} else {
		// Parse server data
		char* server_id = NULL;
		if (strlen((const char*)(query_server_req->server_id)) > 0)
			server_id = (char*)query_server_req->server_id;

		result = str_parse_server_data(script_result, server_id, &server_data, &server_num);
		if (result == OC_FAILURE) {
			log_error_print(g_debug_verbose, "server status data parse error!");
		}
	}
	// if need submit a collect action
	if (query_server_req->flag == 1) {
		result = script_shell_exec((unsigned char*) (g_config->exec_server_query_script),
				script_result);
		if (strncmp((const char*) script_result, "0", 1) == 0) {
			log_info_print(g_debug_verbose, "Execute server status collect success!");
		} else {
			if (result == OC_FAILURE) {
				log_error_print(g_debug_verbose, "Execute server status collect error!");
			}
		}

	}

	///////////////////////////////////////////////////////////
	// Construct response
	///////////////////////////////////////////////////////////
	OC_CMD_QUERY_SERVER_RESP* resp = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;

	result = generate_cmd_query_server_resp(&resp, server_num, server_data);
	if (resp == NULL)
		result = OC_FAILURE;

	if (result == OC_SUCCESS) {
		result = translate_cmd2buf_query_server_resp(resp, &busi_buf, &busi_buf_len);
		if (result == OC_SUCCESS) {
			result = generate_response_package(OC_REQ_QUERY_SERVER_STATUS, busi_buf, busi_buf_len,
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
		free_cmd_query_server_resp(resp);
	if (server_data != NULL)
		free(server_data);

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
 * Parse server's status data from a text string.
 */
int str_parse_server_data(uint8_t* buffer, char* server_id, OC_SERVER_STATUS** server_data,
		uint32_t* server_num) {
	int result = OC_SUCCESS;

	// divide the string by ","
	int i = 0;
	int effect_len = strlen((const char*) buffer);
	int n_counter = 0;
	int start_offset = 0;
	int end_offset = -1;
	OC_SERVER_STATUS temp_status[20];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_ip[MAX_PARAM_NAME_LEN];
	int param_type = 0;
	int param_status = 0;
	int ret_num = 0;
	int temp_counter = 0;

	char temp_buf[160];
	for (i = 0; i < effect_len; i++) {
		if (buffer[i] == '\,') {
			end_offset = i - 1;
		}

		if (end_offset != -1) {
			memset(temp_buf, 0, sizeof(temp_buf));
			memset(param_name, 0, sizeof(param_name));
			memset(param_ip, 0, sizeof(param_ip));
			param_type = 0;
			param_status = 0;

			strncpy(temp_buf, (const char*) (buffer + start_offset),
					(end_offset - start_offset + 1));
			sscanf(temp_buf, "%s %d %d %s", param_name, &param_type, &param_status, param_ip);
			strcpy((char*) (temp_status[n_counter].name), param_name);
			strcpy((char*) (temp_status[n_counter].ip), param_ip);
			temp_status[n_counter].type = param_type;
			temp_status[n_counter].status = param_status;
			temp_status[n_counter].seq_id = n_counter + 1;

			n_counter++;

			start_offset = i + 1;
			end_offset = -1;
		}
	}
	// process last part
	if (i == effect_len && end_offset == -1 && start_offset < (effect_len - 1)) {
		end_offset = effect_len - 1;
		memset(temp_buf, 0, sizeof(temp_buf));
		memset(param_name, 0, sizeof(param_name));
		memset(param_ip, 0, sizeof(param_ip));
		param_type = 0;
		param_status = 0;
		strncpy(temp_buf, (const char*) (buffer + start_offset), (end_offset - start_offset + 1));
		sscanf(temp_buf, "%s %d %d %s", param_name, &param_type, &param_status, param_ip);
		strcpy((char*) (temp_status[n_counter].name), param_name);
		strcpy((char*) (temp_status[n_counter].ip), param_ip);
		temp_status[n_counter].type = param_type;
		temp_status[n_counter].status = param_status;
		temp_status[n_counter].seq_id = n_counter + 1;
		n_counter++;
	}

	if (n_counter > 0) {
		ret_num = 0;
		if (server_id != NULL) {
			// calculate the return number
			for (i = 0; i < n_counter; i++) {
				// server_id filter
				if (strcmp((const char*) temp_status[i].name, (const char*) server_id) == 0)
					ret_num++;
			}
		} else {
			ret_num = n_counter;
		}

		if (ret_num > 0) {
			OC_SERVER_STATUS* params = (OC_SERVER_STATUS*) malloc(
					ret_num * sizeof(OC_SERVER_STATUS));
			memset((void*) params, 0, ret_num * sizeof(OC_SERVER_STATUS));

			temp_counter = 0;
			for (i = 0; i < n_counter; i++) {
				if (server_id != NULL) {
					// server_id filter
					if (strcmp((const char*) temp_status[i].name, (const char*) server_id) != 0)
						continue;
				}
				strcpy((char*) (params[temp_counter].name), (const char*) (temp_status[i].name));
				strcpy((char*) (params[temp_counter].ip), (const char*) (temp_status[i].ip));
				params[temp_counter].type = temp_status[i].type;
				params[temp_counter].status = temp_status[i].status;
				params[temp_counter].seq_id = temp_status[i].seq_id;
				temp_counter ++;
			}
			*server_data = params;
		}
		*server_num = ret_num;
	}

	return result;
}

/**
 * Analyze startup result
 */
int analyze_startup_result(uint32_t step_no, int device_status, char* device_name,
		uint32_t* is_finish, uint32_t* error_no, uint8_t* message) {
	int result = OC_SUCCESS;
	switch (step_no) {
	case 1:	// check disk array
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_CHECKING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_checking));
			break;
		case OC_SH_DEV_STATUS_PASS:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_success));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_diskarray_startup_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_start_diskarray_fail));
			break;
		}
		break;

	case 2:	// startup primary server
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STARTING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_RUNNING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_success));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_primary_server_startup_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_start_primary_server_fail));
			break;
		}
		break;

	case 3: // startup compute host
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STARTING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_RUNNING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_success));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_secondary_server_startup_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_start_compute_host_fail));
			break;
		}
		break;

	case 4:	// startup cloud platform
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STARTING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_RUNNING:
			*is_finish = OC_TRUE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_finish));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_cloud_platform_startup_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_start_cloud_platform_fail));
			break;
		case OC_SH_DEV_STATUS_NOT_READY:
			*is_finish = OC_TRUE;
			*error_no = errcode_secondary_server_startup_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_start_compute_host_fail));
			break;
		}
		break;

	}

	return result;
}

/**
 * Analyze shutdown result
 */
int analyze_shutdown_result(uint32_t step_no, int device_status, char* device_name,
		uint32_t* is_finish, uint32_t* error_no, uint8_t* message) {
	int result = OC_SUCCESS;
	switch (step_no) {
	case 1:	// Shutdown the application
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STOPPING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_STOPPED:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_success));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_cloud_platform_shutdown_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_stop_application_fail));
			break;
		}
		break;

	case 2:	// Shutdown the system VM
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STOPPING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_STOPPED:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_success));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_systemvm_shutdown_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_stop_system_vm_fail));
			break;
		}
		break;

	case 3: // Shutdown the phycial compute host
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STOPPING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_STOPPED:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_success));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_secondary_server_shutdown_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_stop_compute_host_fail));
			break;
		}
		break;

	case 4:	// Shutdown the Primary server
		switch (device_status) {
		case OC_SH_DEV_STATUS_INIT:
		case OC_SH_DEV_STATUS_STOPPING:
			*is_finish = OC_FALSE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_processing));
			break;
		case OC_SH_DEV_STATUS_STOPPED:
			*is_finish = OC_TRUE;
			*error_no = error_no_error;
			strcpy((char*) message, (const char*) exec_msg_text(exec_finish));
			break;
		case OC_SH_DEV_STATUS_FAIL:
			*is_finish = OC_TRUE;
			*error_no = errcode_primary_server_shutdown_failed;
			strcpy((char*) message, (const char*) error_msg_text(error_stop_primary_server_fail));
			break;
		}
		break;

	}

	return result;
}

/**
 * Timer event process
 */
void timer_event_process() {

	// Update counter
	g_watch_dog_counter++;

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
	uint32_t device = device_proc_script;
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
