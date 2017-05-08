/*
 * com_daemon.c
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

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"
#include "util/log_helper.h"

// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_com(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_com(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);

// Debug output level
static int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;

// Current COM number
int g_com_num = 0;

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
		log_error_print(g_debug_verbose, "load config file failure.[%s]\n", config);
		return result;
	}
	print_config_detail();

	g_config = (struct settings*) malloc(sizeof(struct settings));
	memset((void*) g_config, 0, sizeof(struct settings));

	g_config->role = role_uart_com;

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "listen_port", &temp_int,
	OC_MAIN_DEFAULT_PORT);
	g_config->main_listen_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_WATCHDOG, "listen_port", &temp_int,
	OC_WATCHDOG_DEFAULT_PORT);
	g_config->watchdog_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_COM0, "enable", &temp_int, OC_FALSE);
	if (g_com_num == 0 && temp_int == OC_TRUE) {
		g_config->com_config[0].iter_num = 0;
		temp_int = 0;
		get_parameter_int(SECTION_COM0, "debug", &temp_int,
		OC_DEBUG_LEVEL_DISABLE);
		g_debug_verbose = temp_int;
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
	if (g_com_num == 1 && temp_int == OC_TRUE) {
		g_config->com_config[1].iter_num = 1;
		temp_int = 0;
		get_parameter_int(SECTION_COM1, "debug", &temp_int,
		OC_DEBUG_LEVEL_DISABLE);
		g_debug_verbose = temp_int;
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
	if (g_com_num == 2 && temp_int == OC_TRUE) {
		g_config->com_config[2].iter_num = 2;
		temp_int = 0;
		get_parameter_int(SECTION_COM2, "debug", &temp_int,
		OC_DEBUG_LEVEL_DISABLE);
		g_debug_verbose = temp_int;
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
	if (g_com_num == 3 && temp_int == OC_TRUE) {
		g_config->com_config[3].iter_num = 3;
		temp_int = 0;
		get_parameter_int(SECTION_COM3, "debug", &temp_int,
		OC_DEBUG_LEVEL_DISABLE);
		g_debug_verbose = temp_int;
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
	if (g_com_num == 4 && temp_int == OC_TRUE) {
		g_config->com_config[4].iter_num = 4;
		temp_int = 0;
		get_parameter_int(SECTION_COM4, "debug", &temp_int,
		OC_DEBUG_LEVEL_DISABLE);
		g_debug_verbose = temp_int;
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
	if (g_com_num == 5 && temp_int == OC_TRUE) {
		g_config->com_config[5].iter_num = 5;
		temp_int = 0;
		get_parameter_int(SECTION_COM5, "debug", &temp_int,
		OC_DEBUG_LEVEL_DISABLE);
		g_debug_verbose = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM5, "listen_port", &temp_int,
		OC_COM5_DEFAULT_PORT);
		g_config->com_config[5].listen_port = temp_int;
		temp_int = 0;
		get_parameter_int(SECTION_COM5, "type", &temp_int, 0);
		g_config->com_config[5].type = temp_int;
		com_count++;
	}

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
					log_warn_print(g_debug_verbose, "Warn: Socket[%d] closed by client.", fd);
				} else {
					log_error_print(g_debug_verbose, "Socket[%d] write data error.", fd);
				}
			}
		} else {
			log_error_print(g_debug_verbose, "Business process failure.", fd);
		}
	} else {
		if (recv_bytes == 0) {
			log_warn_print(g_debug_verbose, "Warn: Socket[%d] closed by client.", fd);
		} else {
			log_error_print(g_debug_verbose, "Error: Socket[%d] read data error.", fd);
		}
	}

	//Clear
	log_debug_print(g_debug_verbose, "terminating current client_connection...");
	close(fd);            //close a file descriptor.
	pthread_exit(NULL);   //terminate calling thread!

	return NULL;
}

void print_help() {
	fprintf(stderr, "usage: com_daemon <com_num> [configuration file]\n");
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
			print_help();
			exit(EXIT_FAILURE);
		}
		g_com_num = atoi(argv[1]);
		if (argc > 2) {
			strcpy(g_config_file, argv[1]);
		} else {
			strcpy(g_config_file, OC_DEFAULT_CONFIG_FILE);
		}
	} else {
		print_help();
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	// Load configuration file
	///////////////////////////////////////////////////////////
	log_info_print(g_debug_verbose, "using config file [%s]", g_config_file);
	if (load_config(g_config_file) == OC_FAILURE)
		exit(EXIT_FAILURE);

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
	s_addr_in.sin_port = htons(g_config->com_config[g_com_num].listen_port);
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
	case OC_REQ_CTRL_COM:
		result = main_busi_ctrl_com(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_COM:
		result = main_busi_query_com(req_buf, req_len, resp_buf, resp_len);
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
 * Heart beat process
 */
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

