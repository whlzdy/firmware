/*
 * net.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef NET_H_
#define NET_H_

/**
 * Read network package data from Socket
 * return:
 *    data length, if got package success.
 *    0, if the connection closed.
 *    -1, if catch error.
 */
int net_read_package(int sockfd, unsigned char* buffer, int buf_len, int* start_pos);


/**
 * Write network package data into Socket
 * return:
 *    data length, if send package success.
 *    0, if the connection closed.
 *    -1, if catch error.
 */
int net_write_package(int sockfd, unsigned char* buffer, int data_len);

/**
 * Communicate with the server.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int net_business_communicate(uint8_t* ip, uint32_t port, uint32_t cmd, uint8_t* busi_buf,
		int in_buf_len, OC_NET_PACKAGE** resp_pkg);

/**
 * Route the request to the server, and get the response package data.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int net_business_comm_route(uint8_t* ip, uint32_t port, uint8_t* requst_buf, int req_buf_len,
		OC_NET_PACKAGE** resp_pkg);


/**
 * Route the request to the server, and get the response buffer data.
 * return:  OC_SUCCESS if success, else OC_FAILURE.
 */
int net_business_comm_route2(uint8_t* ip, uint32_t port, uint8_t* requst_buf, int req_buf_len,
		uint8_t** resp_buf, uint32_t* resp_len);
#endif /* NET_H_ */
