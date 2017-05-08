/*
 * monitor_client.c
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

void print_json_cabinet_query(OC_CMD_QUERY_CABINET_RESP* query_cabinet);
void print_json_server_query(OC_CMD_QUERY_SERVER_RESP* query_server);

// Debug output level
int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;

void cmd_helper() {
	fprintf(stderr, "usage: monitor_client type=<cabinet|server> [server_id=<string>]\n");
}

#define QUERY_TYPE_CABINET	0
#define QUERY_TYPE_SERVER	1
#define QUERY_TYPE_NONE		99

/**
 * Cabinet monitor client
 */
int main(int argc, char **argv) {

	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* p = NULL;
	int monitor_type = QUERY_TYPE_CABINET; //0: cabinet, 1:server, other: not support
	uint8_t server_id[20];

	///////////////////////////////////////////////////////////
	// Console parameter process
	///////////////////////////////////////////////////////////
	if (argc < 2) {
		cmd_helper();
		exit(EXIT_FAILURE);
	}

	// Input parameter process
	server_id[0] = '\0';
	p = argv[1];
	sscanf(p, "%[^=]=%[^=]", param_name, param_value);

	if (strcmp("type", param_name) == 0) {
		if (strcmp("cabinet", param_value) == 0) {
			monitor_type = QUERY_TYPE_CABINET;
		} else if (strcmp("server", param_value) == 0) {
			monitor_type = QUERY_TYPE_SERVER;
			if (argc > 2) {
				p = argv[2];
				sscanf(p, "%[^=]=%[^=]", param_name, param_value);
				if (strcmp("server_id", param_name) == 0) {
					strcpy((char*) server_id, (const char*) param_value);
				}
			}
		} else {
			monitor_type = QUERY_TYPE_NONE;
			cmd_helper();
			exit(EXIT_FAILURE);
		}
	} else {
		cmd_helper();
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	// Business Communication
	///////////////////////////////////////////////////////////
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;
	int result = OC_SUCCESS;
	OC_NET_PACKAGE* resp_pkg = NULL;
	if (monitor_type == QUERY_TYPE_CABINET) {

		// Construct the request command
		OC_CMD_QUERY_CABINET_REQ * req_cabinet = NULL;
		result = generate_cmd_query_cabinet_req(&req_cabinet, 0);
		result = translate_cmd2buf_query_cabinet_req(req_cabinet, &busi_buf, &busi_buf_len);
		if (req_cabinet != NULL)
			free(req_cabinet);

		// Communication with server
		result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
		OC_REQ_QUERY_CABINET_STATUS, busi_buf, busi_buf_len, &resp_pkg);

		package_print_frame(resp_pkg);

		OC_CMD_QUERY_CABINET_RESP* query_cabinet = (OC_CMD_QUERY_CABINET_RESP*) resp_pkg->data;
		log_debug_print(g_debug_verbose, "kwh=%f", query_cabinet->kwh);
		print_json_cabinet_query(query_cabinet);

	} else if (monitor_type == QUERY_TYPE_SERVER) {

		// Construct the request command
		OC_CMD_QUERY_SERVER_REQ * req_server = NULL;
		uint32_t type = 0;
		if (strlen((const char*) server_id) > 0)
			type = 1;
		result = generate_cmd_query_server_req(&req_server, type, server_id, 0);
		result = translate_cmd2buf_query_server_req(req_server, &busi_buf, &busi_buf_len);
		if (req_server != NULL)
			free(req_server);

		// Communication with server
		result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
		OC_REQ_QUERY_SERVER_STATUS, busi_buf, busi_buf_len, &resp_pkg);

		package_print_frame(resp_pkg);

		OC_CMD_QUERY_SERVER_RESP* query_server = (OC_CMD_QUERY_SERVER_RESP*) resp_pkg->data;
		log_debug_print(g_debug_verbose, "server_num=%d ", query_server->server_num);
		print_json_server_query(query_server);

	}

	free(busi_buf);
	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return EXIT_SUCCESS;
}

/**
 * Print the cabinet status in JSON format.
 */
void print_json_cabinet_query(OC_CMD_QUERY_CABINET_RESP* query_cabinet) {
	if (query_cabinet == NULL) {
		printf("{}");
		return;
	}

	printf(
			"{\"timestamp\":\"%d\",\"status\":%d,\"kwh\":%.2f,\"voltage\":%.2f,\"current\":%.2f,\"temperature\":%.2f,\"watt\":%.2f,\"decibel\":%.2f}\n",
			query_cabinet->timestamp, query_cabinet->status, query_cabinet->kwh,
			query_cabinet->voltage, query_cabinet->current, query_cabinet->temperature,
			query_cabinet->watt, query_cabinet->voice_db);
}

/**
 * Print the server status in JSON format.
 */
void print_json_server_query(OC_CMD_QUERY_SERVER_RESP* query_server) {
	if (query_server == NULL) {
		printf("{}");
		return;
	}

	if (query_server->server_num == 0) {
		printf("{\"count\":0, \"data\":[]}");
		return;
	}
	char buffer[1024];
	char temp[128];
	int i = 0;
	memset((void*) buffer, 0, sizeof(buffer));
	memset((void*) temp, 0, sizeof(temp));

	OC_SERVER_STATUS* server_data = (OC_SERVER_STATUS*) (((char*)query_server) + sizeof(uint32_t) * 2);

	for (i = 0; i < query_server->server_num; i++) {
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\"type\":%d,\"name\":\"%s\",\"mac\":\"%s\",\"ip\":\"%s\",\"status\":%d",
				server_data[i].type, server_data[i].name,
				server_data[i].mac, server_data[i].ip,
				server_data[i].status);
		strcat(buffer, "{");
		strcat(buffer, temp);

		if ((i + 1) == query_server->server_num)
			strcat(buffer, "}");
		else
			strcat(buffer, "},");
	}
	printf("{\"count\":%d, \"data\":[%s]}", query_server->server_num, buffer);
}
