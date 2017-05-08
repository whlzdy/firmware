/*
 * net.c
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

/**
 * Read network package data from Socket
 * return:
 *    data length, if got package success.
 *    0, if the connection closed.
 *    -1, if catch error.
 */
int net_read_package(int sockfd, unsigned char* buffer, int buf_len, int* start_pos) {
	if (sockfd < 1 || buffer == NULL)
		return -1;

	int result = -1;
	int total_byte = 0;
	int n_recv = 0;
	int retry = 3;
	int check = OC_SUCCESS;
	int offset = -1;

	while (total_byte < OC_PKG_FRONT_PART_SIZE && retry > 0) {
		n_recv = read(sockfd, buffer + total_byte, buf_len - total_byte);
		if (n_recv == 0) {
			fprintf(stderr, "Error: Maybe the client has closed.socket=[%d]\n", sockfd);
			result = 0;
			break;
		}
		if (n_recv < 0) {
			fprintf(stderr, "Error: read socket[%d] error\n", sockfd);
			result = -1;
			break;
		}
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "read socket[%d] %d bytes", sockfd, n_recv);

		total_byte += n_recv;
		retry--;
	}

	// check the package
	if (total_byte >= OC_PKG_FRONT_PART_SIZE) {

		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "read package data (socket[%d]).", sockfd);
		package_print_buffer(buffer, total_byte);

		check = check_package_buffer(buffer, total_byte, &offset);
		if (check == OC_SUCCESS) {
			log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "check package from socket[%d] success.",
					sockfd);
			*start_pos = offset;
			result = total_byte;
		} else {
			log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "check package from socket[%d] fail.", sockfd);
			if (offset > -1) {
				// found the start char, but package incomplete, read again
				n_recv = read(sockfd, buffer + total_byte, buf_len - total_byte);
				if (n_recv > 0) {
					total_byte += n_recv;
					check = check_package_buffer(buffer, total_byte, &offset);
					if (check == OC_SUCCESS) {
						*start_pos = offset;
						result = total_byte;
					} else {
						result = -1;
					}
				} else {
					result = -1;
				}
			} else {
				result = -1;
			}
		}
	} else {
		result = -1;
	}

	return result;
}

/**
 * Write network package data into Socket
 * return:
 *    data length, if send package success.
 *    0, if the connection closed.
 *    -1, if catch error.
 */
int net_write_package(int sockfd, unsigned char* buffer, int data_len) {
	if (sockfd < 1 || buffer == NULL)
		return -1;

	int result = -1;
	int total_byte = 0;
	int n_send = 0;
	int retry = 3;

	while (total_byte < data_len && retry > 0) {
		n_send = write(sockfd, buffer + total_byte, data_len - n_send);
		if (n_send == 0) {
			fprintf(stderr, "Error: Maybe the client has closed. socket=[%d]\n", sockfd);
			result = 0;
			break;
		}
		if (n_send < 0) {
			fprintf(stderr, "Error: write socket[%d] error\n", sockfd);
			result = -1;
			break;
		}
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "write socket[%d] %d bytes", sockfd, n_send);
		total_byte += n_send;
		retry--;
	}
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "write package data (socket[%d]).(%d/%d)", sockfd,
			total_byte, data_len);
	package_print_buffer(buffer, data_len);

	if (total_byte > 0)
		return total_byte;
	else
		return result;
}

/**
 * Communicate with the server.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int net_business_communicate(uint8_t* ip, uint32_t port, uint32_t cmd, uint8_t* busi_buf,
		int in_buf_len, OC_NET_PACKAGE** resp_pkg) {
	int result = OC_SUCCESS;

	// Setup connection
	int sockfd = 0;
	int tempfd;
	struct sockaddr_in s_addr_in;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);       //ipv4,TCP
	if (sockfd == -1) {
		fprintf(stderr, "Error: socket error!\n");
		return OC_FAILURE;
	}
	memset(&s_addr_in, 0, sizeof(s_addr_in));
	s_addr_in.sin_addr.s_addr = inet_addr((const char*) ip);
	s_addr_in.sin_family = AF_INET;
	s_addr_in.sin_port = htons(port);

	tempfd = connect(sockfd, (struct sockaddr *) (&s_addr_in), sizeof(s_addr_in));
	if (tempfd == -1) {
		fprintf(stderr, "Error: Connect error! [%s:%d]\n", ip, port);
		return OC_FAILURE;;
	}
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "Connect success! [%s:%d]", ip, port);

	//Communication with server
	int n_send = 0;
	int n_recv = 0;
	uint8_t* requst_buf = NULL;
	int req_buf_len = 0;

	// Construct the request and send to server
	result = generate_request_package(cmd, busi_buf, in_buf_len, &requst_buf, &req_buf_len);
	if (requst_buf != NULL && req_buf_len > OC_PKG_FRONT_PART_SIZE) {
		n_send = net_write_package(sockfd, requst_buf, req_buf_len);
	} else {
		fprintf(stderr, "Error: Generate request error! \n");
		result = OC_FAILURE;
	}
	free(requst_buf);

	// Receive the response data
	uint8_t data_recv[OC_PKG_MAX_LEN];
	memset(data_recv, 0, OC_PKG_MAX_LEN);
	int offset = -1;
	OC_NET_PACKAGE* resp = NULL;
	if (n_send > 0) {
		n_recv = net_read_package(sockfd, data_recv, OC_PKG_MAX_LEN, &offset);
		if (n_recv > 0) {
			result = translate_buffer_to_package(data_recv + offset, n_recv, &resp);
		} else {
			fprintf(stderr, "Error: Receive response error! socket=[%d] \n", sockfd);
			result = OC_FAILURE;
		}
	} else {
		fprintf(stderr, "Error: Send request to %s:%d error! \n", ip, port);
		result = OC_FAILURE;
	}

	if (result == OC_FAILURE) {
		if (resp != NULL) {
			free_network_package(resp);
			resp = NULL;
		}
	}
	*resp_pkg = resp;

	// Release TCP connection
	int ret = shutdown(sockfd, SHUT_WR);
	close(sockfd);

	return result;
}

/**
 * Route the request to the server, and get the response package data.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int net_business_comm_route(uint8_t* ip, uint32_t port, uint8_t* requst_buf, int req_buf_len,
		OC_NET_PACKAGE** resp_pkg) {
	int result = OC_SUCCESS;

	// Setup connection
	int sockfd = 0;
	int tempfd;
	struct sockaddr_in s_addr_in;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);       //ipv4,TCP
	if (sockfd == -1) {
		fprintf(stderr, "Error: socket error!\n");
		return OC_FAILURE;
	}
	memset(&s_addr_in, 0, sizeof(s_addr_in));
	s_addr_in.sin_addr.s_addr = inet_addr((const char*) ip);
	s_addr_in.sin_family = AF_INET;
	s_addr_in.sin_port = htons(port);

	tempfd = connect(sockfd, (struct sockaddr *) (&s_addr_in), sizeof(s_addr_in));
	if (tempfd == -1) {
		fprintf(stderr, "Error: Connect error! \n");
		return OC_FAILURE;;
	}
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "Connect success!");

	//Communication with server
	int n_send = 0;
	int n_recv = 0;

	// send data to server
	if (requst_buf != NULL && req_buf_len > OC_PKG_FRONT_PART_SIZE) {
		n_send = net_write_package(sockfd, requst_buf, req_buf_len);
	} else {
		fprintf(stderr, "Error: Input request data error! \n");
		result = OC_FAILURE;
	}

	// Receive the response data
	uint8_t data_recv[OC_PKG_MAX_LEN];
	memset(data_recv, 0, OC_PKG_MAX_LEN);
	int offset = -1;
	OC_NET_PACKAGE* resp = NULL;
	if (n_send > 0) {
		n_recv = net_read_package(sockfd, data_recv, OC_PKG_MAX_LEN, &offset);
		if (n_recv > 0) {
			result = translate_buffer_to_package(data_recv + offset, n_recv, &resp);
		} else {
			fprintf(stderr, "Error: Receive response error! \n");
			result = OC_FAILURE;
		}
	} else {
		fprintf(stderr, "Error: Send request to %s:%d error! \n", ip, port);
		result = OC_FAILURE;
	}

	*resp_pkg = resp;

	// Release TCP connection
	int ret = shutdown(sockfd, SHUT_WR);
	close(sockfd);

	return result;
}

/**
 * Route the request to the server, and get the response buffer data.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int net_business_comm_route2(uint8_t* ip, uint32_t port, uint8_t* requst_buf, int req_buf_len,
		uint8_t** resp_buf, uint32_t* resp_len) {
	int result = OC_SUCCESS;

	// Setup connection
	int sockfd = 0;
	int tempfd;
	struct sockaddr_in s_addr_in;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);       //ipv4,TCP
	if (sockfd == -1) {
		fprintf(stderr, "Error: socket error!\n");
		return OC_FAILURE;
	}
	memset(&s_addr_in, 0, sizeof(s_addr_in));
	s_addr_in.sin_addr.s_addr = inet_addr((const char*) ip);
	s_addr_in.sin_family = AF_INET;
	s_addr_in.sin_port = htons(port);

	tempfd = connect(sockfd, (struct sockaddr *) (&s_addr_in), sizeof(s_addr_in));
	if (tempfd == -1) {
		fprintf(stderr, "Error: Connect error! \n");
		return OC_FAILURE;;
	}
	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "Connect success!");

	//Communication with server
	int n_send = 0;
	int n_recv = 0;

	// send data to server
	if (requst_buf != NULL && req_buf_len > OC_PKG_FRONT_PART_SIZE) {
		n_send = net_write_package(sockfd, requst_buf, req_buf_len);
	} else {
		fprintf(stderr, "Error: Input request data error! \n");
		result = OC_FAILURE;
	}

	// Receive the response data
	uint8_t data_recv[OC_PKG_MAX_LEN];
	memset(data_recv, 0, OC_PKG_MAX_LEN);
	int offset = -1;
	uint8_t* resp_data = NULL;
	if (n_send > 0) {
		n_recv = net_read_package(sockfd, data_recv, OC_PKG_MAX_LEN, &offset);
		if (n_recv > 0) {
			resp_data = (uint8_t*) malloc(n_recv);
			memcpy((void*) resp_data, data_recv, n_recv);
		} else {
			fprintf(stderr, "Error: Receive response error! \n");
			result = OC_FAILURE;
		}
	} else {
		fprintf(stderr, "Error: Send request to %s:%d error! \n", ip, port);
		result = OC_FAILURE;
	}

	*resp_buf = resp_data;
	*resp_len = n_recv;

	// Release TCP connection
	int ret = shutdown(sockfd, SHUT_WR);
	close(sockfd);

	return result;
}
