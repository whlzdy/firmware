/*
 * protocol.c
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#include<stdlib.h>
#include<pthread.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<stdio.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<string.h>
#include<unistd.h>
#include <pthread.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "util/log_helper.h"

///////////////////////////////////////////////////////////
// Network package function
///////////////////////////////////////////////////////////
/**
 * Generate request command package.
 */
int generate_request_package(uint32_t cmd, uint8_t* busi_buf, int in_buf_len, uint8_t** out_pkg_buf,
		int* out_pkg_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE pkg;

	pkg.prefix = OC_PKG_PREFIX;
	pkg.magic = OC_PKG_MAGIC;
	pkg.command = cmd;
	pkg.length = in_buf_len;
	pkg.suffix = OC_PKG_SUFFIX;
	pkg.data = busi_buf;
	pkg.crc = 0;

	result = translate_package_to_buffer(&pkg, out_pkg_buf, out_pkg_len);

	return result;
}

/**
 * Generate response command package.
 */
int generate_response_package(uint32_t cmd, uint8_t* busi_buf, int in_buf_len,
		uint8_t** out_pkg_buf, int* out_pkg_len) {
	int result = OC_SUCCESS;

	OC_NET_PACKAGE pkg;

	pkg.prefix = OC_PKG_PREFIX;
	pkg.magic = OC_PKG_MAGIC;
	pkg.command = cmd | OC_RESPONSE_BIT;
	pkg.length = in_buf_len;
	pkg.suffix = OC_PKG_SUFFIX;
	pkg.data = busi_buf;
	pkg.crc = 0;

	result = translate_package_to_buffer(&pkg, out_pkg_buf, out_pkg_len);

	return result;
}

/**
 * Translate command package to buffer.
 */
int translate_package_to_buffer(OC_NET_PACKAGE * pkg, uint8_t** out_buf, int* out_buf_len) {

	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_NET_PACKAGE) - sizeof(pkg->data) + pkg->length;

	uint8_t* buffer = (uint8_t*) malloc(buf_len);
	memset((void*) buffer, 0, buf_len);

	memcpy((void*) buffer, (void*) pkg, OC_PKG_FRONT_PART_SIZE);
	memcpy((void*) (buffer + OC_PKG_FRONT_PART_SIZE), pkg->data, pkg->length);
	memcpy((void*) (buffer + OC_PKG_FRONT_PART_SIZE + pkg->length),
			(void*) (((uint8_t*) pkg) + (sizeof(OC_NET_PACKAGE) - OC_PKG_END_PART_SIZE)),
			OC_PKG_END_PART_SIZE);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate command buffer to package structure .
 */
int translate_buffer_to_package(uint8_t* in_buf, int in_buf_len, OC_NET_PACKAGE** out_pkg) {

	if (in_buf == NULL || in_buf_len <= (OC_PKG_FRONT_PART_SIZE + OC_PKG_END_PART_SIZE))
		return OC_FAILURE;

	int result = OC_SUCCESS;
	uint32_t* pdata = NULL;

	OC_NET_PACKAGE* in_pkg = (OC_NET_PACKAGE*) in_buf;

	OC_NET_PACKAGE* package = (OC_NET_PACKAGE*) malloc(sizeof(OC_NET_PACKAGE));
	memset((void*) package, 0, sizeof(OC_NET_PACKAGE));

	memcpy((void*) package, (void*) in_pkg, OC_PKG_FRONT_PART_SIZE);

	if (in_pkg->length > 0 && in_pkg->length < OC_PKG_MAX_DATA_LEN) {
		package->data = malloc(in_pkg->length);
		memcpy((void*) package->data, (in_buf + OC_PKG_FRONT_PART_SIZE), in_pkg->length);
	}

	pdata = (uint32_t*) (in_buf + OC_PKG_FRONT_PART_SIZE + in_pkg->length);
	package->crc = *pdata;
	package->suffix = *(pdata + 1);

	*out_pkg = package;

	return result;
}

/**
 * Release network package memory
 */
void free_network_package(OC_NET_PACKAGE * pkg) {
	if (pkg != NULL) {
		if (pkg->data != NULL) {
			free(pkg->data);
			pkg->data = NULL;
		}
		free(pkg);
	}
}

/**
 * Validate the package buffer data
 */
int check_package_buffer(uint8_t* in_buf, int buf_len, int* start_pos) {
	int result = OC_SUCCESS;

	int current_pos = 0;
	int found_start = 0;
	int offset = 0;
	OC_NET_PACKAGE* pkg = NULL;

	// Find the start chars
	while (current_pos < buf_len) {
		pkg = (OC_NET_PACKAGE*) (in_buf + current_pos);
		if (pkg->prefix == OC_PKG_PREFIX) {
			found_start = 1;
			break;
		}
		current_pos++;
	}
	if (found_start == 0) {
		return OC_FAILURE;
	}

	pkg = (OC_NET_PACKAGE*) (in_buf + current_pos);
	*start_pos = current_pos;

	// Check the end chars
	offset = OC_PKG_FRONT_PART_SIZE + pkg->length;
	if (buf_len < (current_pos + offset + OC_PKG_END_PART_SIZE)) {
		fprintf(stderr, "Error: length error(buf_len=%d, pkg.length=%d) in check package\n",
				buf_len, pkg->length);
		return OC_FAILURE;
	}

	uint32_t* suffix = (uint32_t*) (in_buf + current_pos + offset + 4);
	if ((*suffix) == OC_PKG_SUFFIX) {
		result = OC_SUCCESS;
	} else {
		result = OC_FAILURE;
	}

	return result;
}

// Print the frame data for debug
void package_print_frame(OC_NET_PACKAGE* frame) {
	if (frame == NULL)
		return;

	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->prefix = %08X ", frame->prefix);
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->magic = %08X ", frame->magic);
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->command = %08X ", frame->command);
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->length = %08X ", frame->length);
	if (frame->length > 0) {
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->data = [");
		package_print_buffer((uint8_t*) frame->data, frame->length);
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "              ]");
	} else {
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->data = []");
	}
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->crc = %08X ", frame->crc);
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "frame->suffix = %08X ", frame->suffix);

}

// Print the buffer data for debug
void package_print_buffer(uint8_t* buffer, int buf_len) {
	if (buffer == NULL || buf_len < 1)
		return;

	int i = 0;
	char temp[4];

	unsigned char* data_buf = (unsigned char*) malloc(buf_len * 3 + 10);
	memset(data_buf, 0, buf_len * 3 + 10);

	for (i = 0; i < buf_len; i++) {
		if ((i + 1) % 16 == 0) {
			sprintf(temp, "%02X", buffer[i]);
			strcat((char*) data_buf, (const char*) temp);
			fprintf(stderr, "Debug:      [%s]\n", data_buf);
			memset(data_buf, 0, buf_len * 3 + 10);
		} else {
			if ((i + 1) % 8 == 0) {
				sprintf(temp, "%02X  ", buffer[i]);
			} else {
				sprintf(temp, "%02X ", buffer[i]);
			}
			strcat((char*) data_buf, (const char*) temp);
		}
	}
	if (i % 16 != 0) {
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "     [%s]", data_buf);
	}

	if (data_buf != NULL)
		free(data_buf);

}

///////////////////////////////////////////////////////////
// Command function: Cabinet query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Cabinet query.
 */
int generate_cmd_query_cabinet_req(OC_CMD_QUERY_CABINET_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_CABINET_REQ* req = (OC_CMD_QUERY_CABINET_REQ*) malloc(
			sizeof(OC_CMD_QUERY_CABINET_REQ));
	req->command = OC_REQ_QUERY_CABINET_STATUS;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Cabinet query.
 */
int generate_cmd_query_cabinet_resp(OC_CMD_QUERY_CABINET_RESP ** out_resp, uint32_t status,
		float kwh, float voltage, float current, float temperature, float watt, float voice_db,
		time_t start_time) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_CABINET_RESP* resp = (OC_CMD_QUERY_CABINET_RESP*) malloc(
			sizeof(OC_CMD_QUERY_CABINET_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_CABINET_RESP));

	resp->command = OC_REQ_QUERY_CABINET_STATUS | OC_RESPONSE_BIT;
	resp->status = status;
	resp->timestamp = time(NULL);
	resp->kwh = kwh;
	resp->voltage = voltage;
	resp->current = current;
	resp->temperature = temperature;
	resp->watt = watt;
	resp->voice_db = voice_db;
	resp->start_time = start_time;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Cabinet query.
 */
int translate_cmd2buf_query_cabinet_req(OC_CMD_QUERY_CABINET_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_CABINET_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);
	//memset((void*)buffer, 0, sizeof(buf_len));

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Cabinet query.
 */
int translate_cmd2buf_query_cabinet_resp(OC_CMD_QUERY_CABINET_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_CABINET_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);
	//memset((void*)buffer, 0, sizeof(buf_len));

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Cabinet query.
 */
int translate_buf2cmd_query_cabinet_req(uint8_t* in_buf, OC_CMD_QUERY_CABINET_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_CABINET_REQ);
	OC_CMD_QUERY_CABINET_REQ* req = (OC_CMD_QUERY_CABINET_REQ*) malloc(buf_len);
	//memset((void*)buffer, 0, sizeof(buf_len));

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Cabinet query.
 */
int translate_buf2cmd_query_cabinet_resp(uint8_t* in_buf, OC_CMD_QUERY_CABINET_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_CABINET_RESP);
	OC_CMD_QUERY_CABINET_RESP* resp = (OC_CMD_QUERY_CABINET_RESP*) malloc(buf_len);
	//memset((void*)buffer, 0, sizeof(buf_len));

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Server query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Server query.
 */
int generate_cmd_query_server_req(OC_CMD_QUERY_SERVER_REQ ** out_req, uint32_t type,
		uint8_t* server_id, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_SERVER_REQ* req = (OC_CMD_QUERY_SERVER_REQ*) malloc(
			sizeof(OC_CMD_QUERY_SERVER_REQ));
	memset((void*) req, 0, sizeof(OC_CMD_QUERY_SERVER_REQ));

	req->command = OC_REQ_QUERY_SERVER_STATUS;
	req->type = type;
	if (type == 1) {
		strcpy((char*) req->server_id, (const char*) server_id);
	}
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Server query.
 */
int generate_cmd_query_server_resp(OC_CMD_QUERY_SERVER_RESP ** out_resp, uint32_t server_num,
		OC_SERVER_STATUS* server_data) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_SERVER_RESP* resp = (OC_CMD_QUERY_SERVER_RESP*) malloc(
			sizeof(OC_CMD_QUERY_SERVER_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_SERVER_RESP));

	resp->command = OC_REQ_QUERY_SERVER_STATUS | OC_RESPONSE_BIT;
	resp->server_num = server_num;
	if (server_data != NULL) {
		resp->server_data = (OC_SERVER_STATUS*) malloc(server_num * sizeof(OC_SERVER_STATUS));
		memcpy((void*) resp->server_data, (void*) server_data,
				server_num * sizeof(OC_SERVER_STATUS));
	}
	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Server query.
 */
int translate_cmd2buf_query_server_req(OC_CMD_QUERY_SERVER_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_SERVER_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);
	//memset((void*)buffer, 0, sizeof(buf_len));

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Server query.
 */
int translate_cmd2buf_query_server_resp(OC_CMD_QUERY_SERVER_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int prefix_len = sizeof(OC_CMD_QUERY_SERVER_RESP) - sizeof(void*);
	int buf_len = prefix_len + resp->server_num * sizeof(OC_SERVER_STATUS);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);
	memset((void*) buffer, 0, sizeof(buf_len));

	memcpy((void*) buffer, (void*) resp, prefix_len);
	if (resp->server_num > 0) {
		memcpy((void*) (buffer + prefix_len), (void*) (resp->server_data),
				resp->server_num * sizeof(OC_SERVER_STATUS));
	}

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Server query.
 */
int translate_buf2cmd_query_server_req(uint8_t* in_buf, OC_CMD_QUERY_SERVER_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_SERVER_REQ);
	OC_CMD_QUERY_SERVER_REQ* req = (OC_CMD_QUERY_SERVER_REQ*) malloc(buf_len);
	//memset((void*)buffer, 0, sizeof(buf_len));

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Server query.
 */
int translate_buf2cmd_query_server_resp(uint8_t* in_buf, OC_CMD_QUERY_SERVER_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int prefix_len = sizeof(OC_CMD_QUERY_SERVER_RESP) - sizeof(void*);
	OC_CMD_QUERY_SERVER_RESP* resp = (OC_CMD_QUERY_SERVER_RESP*) malloc(
			sizeof(OC_CMD_QUERY_SERVER_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_SERVER_RESP));

	memcpy((void*) resp, (void*) in_buf, prefix_len);

	int data_len = resp->server_num * sizeof(OC_SERVER_STATUS);
	if (data_len > 0) {
		resp->server_data = (OC_SERVER_STATUS*) malloc(data_len);
		memcpy((void*) resp->server_data, (void*) (in_buf + prefix_len), data_len);
	}

	*out_resp = resp;

	return result;
}
/**
 * Release command memory: Response of query server
 */
void free_cmd_query_server_resp(OC_CMD_QUERY_SERVER_RESP * resp) {
	if (resp != NULL) {
		if (resp->server_data != NULL) {
			free(resp->server_data);
			resp->server_data = NULL;
		}
		free(resp);
	}
}

///////////////////////////////////////////////////////////
// Command function: Startup control
///////////////////////////////////////////////////////////

/**
 * Generate request command: Startup control.
 */
int generate_cmd_ctrl_startup_req(OC_CMD_CTRL_STARTUP_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_STARTUP_REQ* req = (OC_CMD_CTRL_STARTUP_REQ*) malloc(
			sizeof(OC_CMD_CTRL_STARTUP_REQ));
	req->command = OC_REQ_CTRL_STARTUP;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Startup control.
 */
int generate_cmd_ctrl_startup_resp(OC_CMD_CTRL_STARTUP_RESP ** out_resp, uint32_t exec_result,
		uint32_t error_no) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_STARTUP_RESP* resp = (OC_CMD_CTRL_STARTUP_RESP*) malloc(
			sizeof(OC_CMD_CTRL_STARTUP_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_CTRL_STARTUP_RESP));

	resp->command = OC_REQ_CTRL_STARTUP | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Startup control.
 */
int translate_cmd2buf_ctrl_startup_req(OC_CMD_CTRL_STARTUP_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_STARTUP_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Startup control.
 */
int translate_cmd2buf_ctrl_startup_resp(OC_CMD_CTRL_STARTUP_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_STARTUP_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Startup control.
 */
int translate_buf2cmd_ctrl_startup_req(uint8_t* in_buf, OC_CMD_CTRL_STARTUP_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_STARTUP_REQ);
	OC_CMD_CTRL_STARTUP_REQ* req = (OC_CMD_CTRL_STARTUP_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Startup control.
 */
int translate_buf2cmd_ctrl_startup_resp(uint8_t* in_buf, OC_CMD_CTRL_STARTUP_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_STARTUP_RESP);
	OC_CMD_CTRL_STARTUP_RESP* resp = (OC_CMD_CTRL_STARTUP_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Shutdown control
///////////////////////////////////////////////////////////

/**
 * Generate request command: Shutdown control.
 */
int generate_cmd_ctrl_shutdown_req(OC_CMD_CTRL_SHUTDOWN_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_SHUTDOWN_REQ* req = (OC_CMD_CTRL_SHUTDOWN_REQ*) malloc(
			sizeof(OC_CMD_CTRL_SHUTDOWN_REQ));
	req->command = OC_REQ_CTRL_SHUTDOWN;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Shutdown control.
 */
int generate_cmd_ctrl_shutdown_resp(OC_CMD_CTRL_SHUTDOWN_RESP ** out_resp, uint32_t exec_result,
		uint32_t error_no) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_SHUTDOWN_RESP* resp = (OC_CMD_CTRL_SHUTDOWN_RESP*) malloc(
			sizeof(OC_CMD_CTRL_SHUTDOWN_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_CTRL_SHUTDOWN_RESP));

	resp->command = OC_REQ_CTRL_SHUTDOWN | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Shutdown control.
 */
int translate_cmd2buf_ctrl_shutdown_req(OC_CMD_CTRL_SHUTDOWN_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_SHUTDOWN_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Shutdown control.
 */
int translate_cmd2buf_ctrl_shutdown_resp(OC_CMD_CTRL_SHUTDOWN_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_SHUTDOWN_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Shutdown control.
 */
int translate_buf2cmd_ctrl_shutdown_req(uint8_t* in_buf, OC_CMD_CTRL_SHUTDOWN_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_SHUTDOWN_REQ);
	OC_CMD_CTRL_SHUTDOWN_REQ* req = (OC_CMD_CTRL_SHUTDOWN_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Shutdown control.
 */
int translate_buf2cmd_ctrl_shutdown_resp(uint8_t* in_buf, OC_CMD_CTRL_SHUTDOWN_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_SHUTDOWN_RESP);
	OC_CMD_CTRL_SHUTDOWN_RESP* resp = (OC_CMD_CTRL_SHUTDOWN_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Startup query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Startup query.
 */
int generate_cmd_query_startup_req(OC_CMD_QUERY_STARTUP_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_STARTUP_REQ* req = (OC_CMD_QUERY_STARTUP_REQ*) malloc(
			sizeof(OC_CMD_QUERY_STARTUP_REQ));
	req->command = OC_REQ_QUERY_STARTUP;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Startup query.
 */
int generate_cmd_query_startup_resp(OC_CMD_QUERY_STARTUP_RESP ** out_resp, uint32_t exec_result,
		uint32_t error_no, uint32_t step_no, uint32_t is_finish, uint8_t* message) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_STARTUP_RESP* resp = (OC_CMD_QUERY_STARTUP_RESP*) malloc(
			sizeof(OC_CMD_QUERY_STARTUP_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_STARTUP_RESP));

	resp->command = OC_REQ_QUERY_STARTUP | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;
	resp->step_no = step_no;
	resp->is_finish = is_finish;
	if (message != NULL)
		strcpy((char*) (resp->message), (const char*) message);

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Startup query.
 */
int translate_cmd2buf_query_startup_req(OC_CMD_QUERY_STARTUP_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_STARTUP_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Startup query.
 */
int translate_cmd2buf_query_startup_resp(OC_CMD_QUERY_STARTUP_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_STARTUP_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Startup query.
 */
int translate_buf2cmd_query_startup_req(uint8_t* in_buf, OC_CMD_QUERY_STARTUP_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_STARTUP_REQ);
	OC_CMD_QUERY_STARTUP_REQ* req = (OC_CMD_QUERY_STARTUP_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Startup query.
 */
int translate_buf2cmd_query_startup_resp(uint8_t* in_buf, OC_CMD_QUERY_STARTUP_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_STARTUP_RESP);
	OC_CMD_QUERY_STARTUP_RESP* resp = (OC_CMD_QUERY_STARTUP_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Shutdown query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Shutdown query.
 */
int generate_cmd_query_shutdown_req(OC_CMD_QUERY_SHUTDOWN_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_SHUTDOWN_REQ* req = (OC_CMD_QUERY_SHUTDOWN_REQ*) malloc(
			sizeof(OC_CMD_QUERY_SHUTDOWN_REQ));
	req->command = OC_REQ_QUERY_SHUTDOWN;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Shutdown query.
 */
int generate_cmd_query_shutdown_resp(OC_CMD_QUERY_SHUTDOWN_RESP ** out_resp, uint32_t exec_result,
		uint32_t error_no, uint32_t step_no, uint32_t is_finish, uint8_t* message) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_SHUTDOWN_RESP* resp = (OC_CMD_QUERY_SHUTDOWN_RESP*) malloc(
			sizeof(OC_CMD_QUERY_SHUTDOWN_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_SHUTDOWN_RESP));

	resp->command = OC_REQ_QUERY_SHUTDOWN | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;
	resp->step_no = step_no;
	resp->is_finish = is_finish;
	if (message != NULL)
		strcpy((char*) (resp->message), (const char*) message);

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Shutdown query.
 */
int translate_cmd2buf_query_shutdown_req(OC_CMD_QUERY_SHUTDOWN_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_SHUTDOWN_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Shutdown query.
 */
int translate_cmd2buf_query_shutdown_resp(OC_CMD_QUERY_SHUTDOWN_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_SHUTDOWN_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Shutdown query.
 */
int translate_buf2cmd_query_shutdown_req(uint8_t* in_buf, OC_CMD_QUERY_SHUTDOWN_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_SHUTDOWN_REQ);
	OC_CMD_QUERY_SHUTDOWN_REQ* req = (OC_CMD_QUERY_SHUTDOWN_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Shutdown query.
 */
int translate_buf2cmd_query_shutdown_resp(uint8_t* in_buf, OC_CMD_QUERY_SHUTDOWN_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_SHUTDOWN_RESP);
	OC_CMD_QUERY_SHUTDOWN_RESP* resp = (OC_CMD_QUERY_SHUTDOWN_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: GPIO control
///////////////////////////////////////////////////////////

/**
 * Generate request command: GPIO control.
 */
int generate_cmd_ctrl_gpio_req(OC_CMD_CTRL_GPIO_REQ ** out_req, uint32_t event, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_GPIO_REQ* req = (OC_CMD_CTRL_GPIO_REQ*) malloc(sizeof(OC_CMD_CTRL_GPIO_REQ));
	req->command = OC_REQ_CTRL_GPIO;
	req->event = event;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: GPIO control.
 */
int generate_cmd_ctrl_gpio_resp(OC_CMD_CTRL_GPIO_RESP ** out_resp, uint32_t exec_result,
		uint32_t io_status, uint32_t error_no) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_GPIO_RESP* resp = (OC_CMD_CTRL_GPIO_RESP*) malloc(sizeof(OC_CMD_CTRL_GPIO_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_CTRL_GPIO_RESP));

	resp->command = OC_REQ_CTRL_GPIO | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->io_status = io_status;
	resp->error_no = error_no;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: GPIO control.
 */
int translate_cmd2buf_ctrl_gpio_req(OC_CMD_CTRL_GPIO_REQ * req, uint8_t** out_buf, int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_GPIO_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: GPIO control.
 */
int translate_cmd2buf_ctrl_gpio_resp(OC_CMD_CTRL_GPIO_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_GPIO_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: GPIO control.
 */
int translate_buf2cmd_ctrl_gpio_req(uint8_t* in_buf, OC_CMD_CTRL_GPIO_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_GPIO_REQ);
	OC_CMD_CTRL_GPIO_REQ* req = (OC_CMD_CTRL_GPIO_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: GPIO control.
 */
int translate_buf2cmd_ctrl_gpio_resp(uint8_t* in_buf, OC_CMD_CTRL_GPIO_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_GPIO_RESP);
	OC_CMD_CTRL_GPIO_RESP* resp = (OC_CMD_CTRL_GPIO_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: GPIO query
///////////////////////////////////////////////////////////

/**
 * Generate request command: GPIO query.
 */
int generate_cmd_query_gpio_req(OC_CMD_QUERY_GPIO_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_GPIO_REQ* req = (OC_CMD_QUERY_GPIO_REQ*) malloc(sizeof(OC_CMD_QUERY_GPIO_REQ));
	req->command = OC_REQ_QUERY_GPIO;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: GPIO query.
 */
int generate_cmd_query_gpio_resp(OC_CMD_QUERY_GPIO_RESP ** out_resp, uint32_t exec_result,
		uint32_t io_status, uint32_t p0_status, uint32_t p1_status, uint32_t p2_status,
		uint32_t p3_status, uint32_t p4_status, uint32_t p5_status, uint32_t p6_status,
		uint32_t p7_status) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_GPIO_RESP* resp = (OC_CMD_QUERY_GPIO_RESP*) malloc(sizeof(OC_CMD_QUERY_GPIO_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_GPIO_RESP));

	resp->command = OC_REQ_QUERY_GPIO | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->io_status = io_status;
	resp->p0_status = p0_status;
	resp->p1_status = p1_status;
	resp->p2_status = p2_status;
	resp->p3_status = p3_status;
	resp->p4_status = p4_status;
	resp->p5_status = p5_status;
	resp->p6_status = p6_status;
	resp->p7_status = p7_status;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: GPIO query.
 */
int translate_cmd2buf_query_gpio_req(OC_CMD_QUERY_GPIO_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_GPIO_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: GPIO query.
 */
int translate_cmd2buf_query_gpio_resp(OC_CMD_QUERY_GPIO_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_GPIO_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: GPIO query.
 */
int translate_buf2cmd_query_gpio_req(uint8_t* in_buf, OC_CMD_QUERY_GPIO_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_GPIO_REQ);
	OC_CMD_QUERY_GPIO_REQ* req = (OC_CMD_QUERY_GPIO_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: GPIO query.
 */
int translate_buf2cmd_query_gpio_resp(uint8_t* in_buf, OC_CMD_QUERY_GPIO_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_GPIO_RESP);
	OC_CMD_QUERY_GPIO_RESP* resp = (OC_CMD_QUERY_GPIO_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Heart beat
///////////////////////////////////////////////////////////

/**
 * Generate request command: Heart beat.
 */
int generate_cmd_heart_beat_req(OC_CMD_HEART_BEAT_REQ ** out_req, uint32_t device, uint32_t status,
		uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_HEART_BEAT_REQ* req = (OC_CMD_HEART_BEAT_REQ*) malloc(sizeof(OC_CMD_HEART_BEAT_REQ));
	req->command = OC_REQ_HEART_BEAT;
	req->timestamp = time(NULL);
	req->device = device;
	req->status = status;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Heart beat.
 */
int generate_cmd_heart_beat_resp(OC_CMD_HEART_BEAT_RESP ** out_resp, uint32_t exec_result) {
	int result = OC_SUCCESS;

	OC_CMD_HEART_BEAT_RESP* resp = (OC_CMD_HEART_BEAT_RESP*) malloc(sizeof(OC_CMD_HEART_BEAT_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_HEART_BEAT_RESP));

	resp->command = OC_REQ_HEART_BEAT | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Heart beat.
 */
int translate_cmd2buf_heart_beat_req(OC_CMD_HEART_BEAT_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_HEART_BEAT_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Heart beat.
 */
int translate_cmd2buf_heart_beat_resp(OC_CMD_HEART_BEAT_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_HEART_BEAT_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Heart beat.
 */
int translate_buf2cmd_heart_beat_req(uint8_t* in_buf, OC_CMD_HEART_BEAT_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_HEART_BEAT_REQ);
	OC_CMD_HEART_BEAT_REQ* req = (OC_CMD_HEART_BEAT_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Heart beat.
 */
int translate_buf2cmd_heart_beat_resp(uint8_t* in_buf, OC_CMD_HEART_BEAT_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_HEART_BEAT_RESP);
	OC_CMD_HEART_BEAT_RESP* resp = (OC_CMD_HEART_BEAT_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Temperature query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Temperature query.
 */
int generate_cmd_query_temperature_req(OC_CMD_QUERY_TEMPERATURE_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_TEMPERATURE_REQ* req = (OC_CMD_QUERY_TEMPERATURE_REQ*) malloc(
			sizeof(OC_CMD_QUERY_TEMPERATURE_REQ));
	req->command = OC_REQ_QUERY_TEMPERATURE;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Temperature query.
 */
int generate_cmd_query_temperature_resp(OC_CMD_QUERY_TEMPERATURE_RESP ** out_resp,
		uint32_t exec_result, uint32_t error_no, float t1, float t2) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_TEMPERATURE_RESP* resp = (OC_CMD_QUERY_TEMPERATURE_RESP*) malloc(
			sizeof(OC_CMD_QUERY_TEMPERATURE_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_TEMPERATURE_RESP));

	resp->command = OC_REQ_QUERY_TEMPERATURE | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;
	resp->t1 = t1;
	resp->t2 = t2;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Temperature query.
 */
int translate_cmd2buf_query_temperature_req(OC_CMD_QUERY_TEMPERATURE_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_TEMPERATURE_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Temperature query.
 */
int translate_cmd2buf_query_temperature_resp(OC_CMD_QUERY_TEMPERATURE_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_TEMPERATURE_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Temperature query.
 */
int translate_buf2cmd_query_temperature_req(uint8_t* in_buf, OC_CMD_QUERY_TEMPERATURE_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_TEMPERATURE_REQ);
	OC_CMD_QUERY_TEMPERATURE_REQ* req = (OC_CMD_QUERY_TEMPERATURE_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Temperature query.
 */
int translate_buf2cmd_query_temperature_resp(uint8_t* in_buf,
		OC_CMD_QUERY_TEMPERATURE_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_TEMPERATURE_RESP);
	OC_CMD_QUERY_TEMPERATURE_RESP* resp = (OC_CMD_QUERY_TEMPERATURE_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Electricity control
///////////////////////////////////////////////////////////

/**
 * Generate request command: Electricity control.
 */
int generate_cmd_ctrl_electricity_req(OC_CMD_CTRL_ELECTRICITY_REQ ** out_req, uint32_t event,
		uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_ELECTRICITY_REQ* req = (OC_CMD_CTRL_ELECTRICITY_REQ*) malloc(
			sizeof(OC_CMD_CTRL_ELECTRICITY_REQ));
	req->command = OC_REQ_CTRL_ELECTRICITY;
	req->event = event;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Electricity control.
 */
int generate_cmd_ctrl_electricity_resp(OC_CMD_CTRL_ELECTRICITY_RESP ** out_resp,
		uint32_t exec_result, uint32_t error_no) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_ELECTRICITY_RESP* resp = (OC_CMD_CTRL_ELECTRICITY_RESP*) malloc(
			sizeof(OC_CMD_CTRL_ELECTRICITY_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_CTRL_ELECTRICITY_RESP));

	resp->command = OC_REQ_CTRL_ELECTRICITY | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Electricity control.
 */
int translate_cmd2buf_ctrl_electricity_req(OC_CMD_CTRL_ELECTRICITY_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_ELECTRICITY_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Electricity control.
 */
int translate_cmd2buf_ctrl_electricity_resp(OC_CMD_CTRL_ELECTRICITY_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_ELECTRICITY_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Electricity control.
 */
int translate_buf2cmd_ctrl_electricity_req(uint8_t* in_buf, OC_CMD_CTRL_ELECTRICITY_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_ELECTRICITY_REQ);
	OC_CMD_CTRL_ELECTRICITY_REQ* req = (OC_CMD_CTRL_ELECTRICITY_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Electricity control.
 */
int translate_buf2cmd_ctrl_electricity_resp(uint8_t* in_buf,
		OC_CMD_CTRL_ELECTRICITY_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_ELECTRICITY_RESP);
	OC_CMD_CTRL_ELECTRICITY_RESP* resp = (OC_CMD_CTRL_ELECTRICITY_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: Electricity query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Electricity query.
 */
int generate_cmd_query_electricity_req(OC_CMD_QUERY_ELECTRICITY_REQ ** out_req, uint32_t flag) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_ELECTRICITY_REQ* req = (OC_CMD_QUERY_ELECTRICITY_REQ*) malloc(
			sizeof(OC_CMD_QUERY_ELECTRICITY_REQ));
	req->command = OC_REQ_QUERY_ELECTRICITY;
	req->flag = flag;

	*out_req = req;

	return result;
}

/**
 * Generate response command: Electricity query.
 */
int generate_cmd_query_electricity_resp(OC_CMD_QUERY_ELECTRICITY_RESP ** out_resp,
		uint32_t exec_result, uint32_t error_no, float kwh, float voltage, float current, float kw) {
	int result = OC_SUCCESS;

	OC_CMD_QUERY_ELECTRICITY_RESP* resp = (OC_CMD_QUERY_ELECTRICITY_RESP*) malloc(
			sizeof(OC_CMD_QUERY_ELECTRICITY_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_QUERY_ELECTRICITY_RESP));

	resp->command = OC_REQ_QUERY_ELECTRICITY | OC_RESPONSE_BIT;
	resp->result = exec_result;
	resp->timestamp = time(NULL);
	resp->error_no = error_no;
	resp->kwh = kwh;
	resp->voltage = voltage;
	resp->current = current;
	resp->kw = kw;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: Electricity query.
 */
int translate_cmd2buf_query_electricity_req(OC_CMD_QUERY_ELECTRICITY_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_ELECTRICITY_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate response command to buffer: Electricity query.
 */
int translate_cmd2buf_query_electricity_resp(OC_CMD_QUERY_ELECTRICITY_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_ELECTRICITY_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: Electricity query.
 */
int translate_buf2cmd_query_electricity_req(uint8_t* in_buf, OC_CMD_QUERY_ELECTRICITY_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_ELECTRICITY_REQ);
	OC_CMD_QUERY_ELECTRICITY_REQ* req = (OC_CMD_QUERY_ELECTRICITY_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: Electricity query.
 */
int translate_buf2cmd_query_electricity_resp(uint8_t* in_buf,
		OC_CMD_QUERY_ELECTRICITY_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_QUERY_ELECTRICITY_RESP);
	OC_CMD_QUERY_ELECTRICITY_RESP* resp = (OC_CMD_QUERY_ELECTRICITY_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}

///////////////////////////////////////////////////////////
// Command function: APP control
///////////////////////////////////////////////////////////

/**
 * Generate request command: APP control.
 */
int generate_cmd_app_control_req(OC_CMD_CTRL_APP_REQ ** out_req, uint32_t operation) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_APP_REQ* req = (OC_CMD_CTRL_APP_REQ*) malloc(sizeof(OC_CMD_CTRL_APP_REQ));
	req->command = OC_REQ_CTRL_APP;
	req->operation = operation;

	*out_req = req;

	return result;
}

/**
 * Generate response command: APP control.
 */
int generate_cmd_app_control_resp(OC_CMD_CTRL_APP_RESP ** out_resp, uint32_t exec_result) {
	int result = OC_SUCCESS;

	OC_CMD_CTRL_APP_RESP* resp = (OC_CMD_CTRL_APP_RESP*) malloc(sizeof(OC_CMD_CTRL_APP_RESP));
	memset((void*) resp, 0, sizeof(OC_CMD_CTRL_APP_RESP));

	resp->command = OC_REQ_CTRL_APP | OC_RESPONSE_BIT;
	resp->result = exec_result;

	*out_resp = resp;

	return result;
}

/**
 * Translate request command to buffer: APP control.
 */
int translate_cmd2buf_app_control_req(OC_CMD_CTRL_APP_REQ * req, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_APP_REQ);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) req, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;

}

/**
 * Translate response command to buffer: APP control.
 */
int translate_cmd2buf_app_control_resp(OC_CMD_CTRL_APP_RESP* resp, uint8_t** out_buf,
		int* out_buf_len) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_APP_RESP);
	uint8_t* buffer = (uint8_t*) malloc(buf_len);

	memcpy((void*) buffer, (void*) resp, buf_len);

	*out_buf = buffer;
	*out_buf_len = buf_len;

	return result;
}

/**
 * Translate buffer to request command: APP control.
 */
int translate_buf2cmd_app_control_req(uint8_t* in_buf, OC_CMD_CTRL_APP_REQ** out_req) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_APP_REQ);
	OC_CMD_CTRL_APP_REQ* req = (OC_CMD_CTRL_APP_REQ*) malloc(buf_len);

	memcpy((void*) req, (void*) in_buf, buf_len);

	*out_req = req;

	return result;
}

/**
 * Translate buffer to response command: APP control.
 */
int translate_buf2cmd_app_control_resp(uint8_t* in_buf, OC_CMD_CTRL_APP_RESP ** out_resp) {
	int result = OC_SUCCESS;

	int buf_len = sizeof(OC_CMD_CTRL_APP_RESP);
	OC_CMD_CTRL_APP_RESP* resp = (OC_CMD_CTRL_APP_RESP*) malloc(buf_len);

	memcpy((void*) resp, (void*) in_buf, buf_len);

	*out_resp = resp;

	return result;
}
