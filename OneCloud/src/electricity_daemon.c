/*
 * temperature_daemon.c
 *
 *  Created on: Mar 29, 2017
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
#include "util/dlt645_util.h"
#include "util/log_helper.h"
#include "util/date_helper.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#define RECV_BUF_SIZE 100

int serial_fd = 0;
int serial_status = 0; //0 no use;1 using
char device_name[32];
float kwh = 0;
float voltage = 0;
float current = 0;
float kw = 0;
uint32_t event = 0;
static void* thread_dlt645_routine(void*);
static void* thread_dlt645_ctrl(void * event);
void dlt645_kwh();
void dlt645_current();
void dlt645_voltage();
void dlt645_kw();
void dlt645_turnon();
void dlt645_turnoff();
int dlt645_send(DLT645_FRAME* in_frame, DLT645_FRAME** out_frame);

int init_serial(char* device);
int uart_send(int fd, char *data, int datalen);
int uart_recv(int fd, char *data, int datalen);
int uart_recv_block(int fd, char *data, int datalen);
void sig_handler(const int sig);

// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_electricity(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_electricity(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_heart_beat(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
void timer_event_process();
void timer_event_watchdog_heartbeat();

// Debug output level
int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;

// Current COM number
int g_com_num = 2;

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
			fprintf(stderr, "usage: gpio_daemon [configuration file]\n");
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

	log_info_print(g_debug_verbose, "dlt645_routine thread!");
	sprintf(device_name, "/dev/ttyS%d", g_com_num);
	log_info_print(g_debug_verbose, "Target device is %s.", device_name);
	/*Initialize serial*/
	int init_result = dlt645_init_serial(&serial_fd, (char*) device_name);
	if (init_result == -1) {
		log_info_print(g_debug_verbose, "Initialize %s failure.(return %d)", device_name,
				init_result);
		return EXIT_FAILURE;
	} else {
		log_info_print(g_debug_verbose, "Initialize %s success.", device_name);
	}
	/* handle SIGINT */
	signal(SIGINT, sig_handler);
	pthread_t thread_id_routine;
	if (pthread_create(&thread_id_routine, NULL, thread_dlt645_routine,
	NULL) == -1) {
		log_error_print(g_debug_verbose, " com temperature pthread_create error!");
		return EXIT_FAILURE;
	}
	pthread_detach(thread_id_routine);

	///////////////////////////////////////////////////////////
	// Timer service thread
	///////////////////////////////////////////////////////////
	g_timer_running = OC_TRUE;
	if (pthread_create(&g_timer_thread_id, NULL, thread_timer_handle, (void *) NULL) == -1) {
		log_error_print(g_debug_verbose, "Timer thread create error!");
		exit(EXIT_FAILURE);
	}
	pthread_detach(g_timer_thread_id);

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
			log_error_print(g_debug_verbose, "Error: Accept error!");
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
	case OC_REQ_CTRL_ELECTRICITY:
		result = main_busi_ctrl_electricity(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_ELECTRICITY:
		result = main_busi_query_electricity(req_buf, req_len, resp_buf, resp_len);
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
 * Temperature control process
 */
int main_busi_ctrl_electricity(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;
	uint32_t exec_result = 0;
	uint32_t error_no = 0;

	OC_CMD_CTRL_ELECTRICITY_REQ* request = (OC_CMD_CTRL_ELECTRICITY_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);
	if (request->event == 1 || request->event == 2) {
		log_debug_print(g_debug_verbose, "thread_dlt645_ctrl!");
		pthread_t thread_id_ctrl;
		if (pthread_create(&thread_id_ctrl, NULL, thread_dlt645_ctrl, (void *) (&(request->event)))
				== -1) {
			log_error_print(g_debug_verbose, "thread_dlt645_ctrl pthread_create error!");
			exec_result = 1;
		}
	} else {
		log_error_print(g_debug_verbose, "no meaning event[%d]!", request->event);
		exec_result = 1;
	}

	OC_CMD_CTRL_ELECTRICITY_RESP* response = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;
	result = generate_cmd_ctrl_electricity_resp(&response, exec_result, error_no);
	result = translate_cmd2buf_ctrl_electricity_resp(response, &busi_buf, &busi_buf_len);
	if (result == OC_SUCCESS) {
		result = generate_response_package(OC_REQ_CTRL_ELECTRICITY, busi_buf, busi_buf_len,
				&temp_buf, &temp_buf_len);
		if (result == OC_SUCCESS) {
			memcpy(resp_buf, temp_buf, temp_buf_len);
			*resp_len = temp_buf_len;
		}
	}
	if (response != NULL)
		free(response);
	if (busi_buf != NULL)
		free(busi_buf);
	if (temp_buf != NULL)
		free(temp_buf);

	return result;
}

/**
 * Temperature query process
 */
int main_busi_query_electricity(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;
	uint32_t exec_result = 0;
	uint32_t error_no = 0;

	OC_CMD_QUERY_ELECTRICITY_REQ* request = (OC_CMD_QUERY_ELECTRICITY_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);
	OC_CMD_QUERY_ELECTRICITY_RESP* response = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;
	result = generate_cmd_query_electricity_resp(&response, exec_result, error_no, kwh, voltage,
			current, kw);
	result = translate_cmd2buf_query_electricity_resp(response, &busi_buf, &busi_buf_len);
	if (result == OC_SUCCESS) {
		result = generate_response_package(OC_REQ_QUERY_ELECTRICITY, busi_buf, busi_buf_len,
				&temp_buf, &temp_buf_len);
		if (result == OC_SUCCESS) {
			memcpy(resp_buf, temp_buf, temp_buf_len);
			*resp_len = temp_buf_len;
		}
	}
	if (response != NULL)
		free(response);
	if (busi_buf != NULL)
		free(busi_buf);
	if (temp_buf != NULL)
		free(temp_buf);

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

//Initial serial device
int init_serial(char* device) {
	serial_fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (serial_fd < 0) {
		log_error_print(g_debug_verbose, "open error");
		return -1;
	}

	struct termios options;
	//获取与终端相关的参数
	tcgetattr(serial_fd, &options);
	//修改所获得的参数
	options.c_cflag |= (CLOCAL | CREAD);	//设置控制模式状态，本地连接，接收使能
	options.c_cflag &= ~CSIZE; 				//字符长度，设置数据位之前一定要屏掉这个位
	options.c_cflag &= ~CRTSCTS;			//无硬件流控
	options.c_cflag |= CS8; 				//8位数据长度
	options.c_cflag &= ~CSTOPB; 			//1位停止位
	options.c_iflag |= IGNPAR; 				//无奇偶检验位
	options.c_oflag = 0; 					//输出模式
	options.c_lflag = 0; 					//不激活终端模式
	cfsetospeed(&options, B2400); 		//设置波特率
	//cfsetospeed(&options, B9600); 		//设置波特率
	//cfsetospeed(&options, B115200); 		//设置波特率

	//设置新属性，TCSANOW：所有改变立即生效
	tcflush(serial_fd, TCIFLUSH); 			//溢出数据可以接收，但不读
	tcsetattr(serial_fd, TCSANOW, &options);

	return 0;
}

/**
 *Serial send data
 *@fd: COM file descriptor
 *@data: data will send
 *@datalen:data length
 */
int uart_send(int fd, char *data, int datalen) {
	int len = 0;
	len = write(fd, data, datalen);
	if (len == datalen) {	// write complete
		return len;
	} else {
		tcflush(fd, TCOFLUSH); //TCOFLUSH刷新写入的数据但不传送
		return -1;
	}

	return 0;
}

/**
 *Serial receive data
 *@fd: COM file descriptor
 *@data: data buffer for receive
 *@datalen:data buffer size
 */
int uart_recv(int fd, char *data, int datalen) {
	int len = 0, ret = 0;
	fd_set fs_read;
	struct timeval tv_timeout;

	FD_ZERO(&fs_read);
	FD_SET(fd, &fs_read);
	tv_timeout.tv_sec = (10 * 20 / 115200 + 1);
	tv_timeout.tv_usec = 0;

	ret = select(fd + 1, &fs_read, NULL, NULL, &tv_timeout);
	log_debug_print(g_debug_verbose, "ret = %d", ret);
	//如果返回0，代表在描述符状态改变前已超过timeout时间,错误返回-1

	if (FD_ISSET(fd, &fs_read)) {
		len = read(fd, data, datalen);
		log_debug_print(g_debug_verbose, "len = %d", len);
		return len;
	} else {
		perror("select");
		return -1;
	}

	return 0;
}

/**
 *Serial receive data
 *@fd: COM file descriptor
 *@data: data buffer for receive
 *@datalen:data buffer size
 */
int uart_recv_block(int fd, char *data, int datalen) {
	int len = 0, ret = 0;

	len = read(fd, data, datalen);
	log_info_print(g_debug_verbose, "recv data,len = %d", len);

	return len;
}

void sig_handler(const int sig) {
	g_timer_running = OC_FALSE;

	log_info_print(g_debug_verbose, "SIGINT handled.");
	log_info_print(g_debug_verbose, "Now stop COM receive demo.");
	if (serial_fd != 0)
		close(serial_fd);

	exit(EXIT_SUCCESS);
}

void dlt645_kwh() {
	DLT645_FRAME* in_frame = NULL;
	DLT645_FRAME* out_frame = NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//kwh
	if (dlt645_gen_cmd_query_kwh(&in_frame, address) == -1) {
		log_error_print(g_debug_verbose, "dlt645_gen_cmd_query_kwh() catch error!");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame, &out_frame);
	if (out_frame != NULL) {
		memset(temp, 0, sizeof(temp));
		memset(temp_len, 0, sizeof(temp_len));
		memset(data_buf, 0, sizeof(data_buf));
		sprintf(temp_len, "%02X", out_frame->data_len);
		for (i = atoi(temp_len) - 1; i > 3; i--) {
			sprintf(temp, "%02X", out_frame->data[i]);
			strcat(data_buf, temp);
		}
		kwh = atof(data_buf) / 100;
		log_debug_print(g_debug_verbose, "kwh =[%.2f]", kwh);
		dlt645_free_frame(out_frame);
	}
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
}
void dlt645_current() {
	DLT645_FRAME* in_frame = NULL;
	DLT645_FRAME* out_frame = NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

	//current
	if (dlt645_gen_cmd_query_current(&in_frame, address) == -1) {
		log_error_print(g_debug_verbose, "dlt645_gen_cmd_query_current() catch error!");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame, &out_frame);
	if (out_frame != NULL) {
		memset(temp, 0, sizeof(temp));
		memset(temp_len, 0, sizeof(temp_len));
		memset(data_buf, 0, sizeof(data_buf));
		sprintf(temp_len, "%02X", out_frame->data_len);
		for (i = atoi(temp_len) - 1; i > 3; i--) {
			sprintf(temp, "%02X", out_frame->data[i]);
			strcat(data_buf, temp);
		}
		current = atof(data_buf) / 1000;
		log_debug_print(g_debug_verbose, "current =[%.3f]", current);

		dlt645_free_frame(out_frame);
	}
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
}
void dlt645_voltage() {
	DLT645_FRAME* in_frame = NULL;
	DLT645_FRAME* out_frame = NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//voltage
	if (dlt645_gen_cmd_query_voltage(&in_frame, address) == -1) {
		log_error_print(g_debug_verbose, "dlt645_gen_cmd_query_voltage() catch error!");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame, &out_frame);
	if (out_frame != NULL) {
		memset(temp, 0, sizeof(temp));
		memset(temp_len, 0, sizeof(temp_len));
		memset(data_buf, 0, sizeof(data_buf));
		sprintf(temp_len, "%02X", out_frame->data_len);
		for (i = atoi(temp_len) - 1; i > 3; i--) {
			sprintf(temp, "%02X", out_frame->data[i]);
			strcat(data_buf, temp);
		}
		voltage = atof(data_buf) / 10;
		log_debug_print(g_debug_verbose, "voltage =[%.1f]", voltage);

		dlt645_free_frame(out_frame);
	}
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
}
void dlt645_kw() {
	DLT645_FRAME* in_frame = NULL;
	DLT645_FRAME* out_frame = NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//kw
	if (dlt645_gen_cmd_query_kw(&in_frame, address) == -1) {
		log_error_print(g_debug_verbose, "dlt645_gen_cmd_query_kw() catch error!");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame, &out_frame);
	if (out_frame != NULL) {
		memset(temp, 0, sizeof(temp));
		memset(temp_len, 0, sizeof(temp_len));
		memset(data_buf, 0, sizeof(data_buf));
		sprintf(temp_len, "%02X", out_frame->data_len);
		for (i = atoi(temp_len) - 1; i > 3; i--) {
			sprintf(temp, "%02X", out_frame->data[i]);
			strcat(data_buf, temp);
		}
		kw = atof(data_buf) / 10;
		log_debug_print(g_debug_verbose, "kw =[%.4f]", kw);

		dlt645_free_frame(out_frame);
	}
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
}
void dlt645_turnon() {
	DLT645_FRAME* in_frame = NULL;
	DLT645_FRAME* out_frame = NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//on
	if (dlt645_gen_cmd_control_turnOn(&in_frame, address) == -1) {
		log_error_print(g_debug_verbose, "dlt645_gen_cmd_control_turnOn() catch error!");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame, &out_frame);
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
	if (out_frame != NULL)
		dlt645_free_frame(out_frame);
}
void dlt645_turnoff() {
	DLT645_FRAME* in_frame = NULL;
	DLT645_FRAME* out_frame = NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//off
	if (dlt645_gen_cmd_control_turnOff(&in_frame, address) == -1) {
		log_error_print(g_debug_verbose, "dlt645_gen_cmd_control_turnOff() catch error!");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame, &out_frame);
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
	if (out_frame != NULL)
		dlt645_free_frame(out_frame);
}
int dlt645_send(DLT645_FRAME* in_frame, DLT645_FRAME** out_frame) {
	log_debug_print(g_debug_verbose, "dlt645_send serial_status=[%d]", serial_status);
	while (serial_status != 0) {
		usleep(100 * 1000);
	}
	serial_status = 1;
	int result = 0;
	unsigned char* bcd_buf = NULL;
	unsigned char buf_len = 0;
	result = dlt645_frame_to_bcd(in_frame, &bcd_buf, &buf_len);
	dlt645_print_frame(in_frame);
	dlt645_print_buffer(bcd_buf, buf_len);

	// send active code message
	//printf("Now , send 0xFE to %s (%d)...\n", device_name, 4);
	//unsigned char ef_buf[4] = {0xFE,0xFE,0xFE,0xFE};
	//uart_send(serial_fd, ef_buf, 4);
	//sleep(1);

	log_debug_print(g_debug_verbose, "Now , send data to %s (%d)...", device_name, buf_len);
	uart_send(serial_fd, (char*) bcd_buf, buf_len);

	log_debug_print(g_debug_verbose, "Now , %s receive data...", device_name);
	char buf_recv[RECV_BUF_SIZE];
	int recv_bytes = 0;
	int total_bytes = 0;
	int retry = 3;
	int command_valid = OC_FALSE;
	int start_offset = 0;
	int tmpcount = 0;
	while (retry > 0) {
		memset((void*) buf_recv, 0, RECV_BUF_SIZE);
		recv_bytes = 0;
		total_bytes = 0;
		command_valid = OC_FALSE;

		// try to receive a completed message
		while (1) {
//			recv_bytes = uart_recv_block(serial_fd, buf_recv + total_bytes,
//			RECV_BUF_SIZE - total_bytes);
			recv_bytes = uart_recv(serial_fd, buf_recv + total_bytes,
			RECV_BUF_SIZE - total_bytes);
			if (recv_bytes > 0) {
				total_bytes += recv_bytes;
				// check the receive data
//				for (tmpcount = 0; tmpcount < total_bytes; tmpcount++) {
//					char tmp;
//					tmp = buf_recv[tmpcount];
//					printf("%02X ", tmp);
//				}
				result = dlt645_check_command((unsigned char*) buf_recv, total_bytes,
						&start_offset);
				log_debug_print(g_debug_verbose, "check command (len=%d) return %d", total_bytes,
						result);
				if (result >= 0) {
					// Found the valid command
					command_valid = OC_TRUE;
					break;
				} else if (result == -1) {
					// invalid command
					break;
				} else if (result == -2) {
					//incomplete command, need receive data again
					continue;
				}
			} else {
				break;
			}
		}
		log_debug_print(g_debug_verbose, "uart total receive %d", total_bytes);

		if (command_valid == OC_TRUE)
			break;

		retry--;
	}

	// Print the response data
	if (command_valid == OC_TRUE) {
		log_debug_print(g_debug_verbose, "buf_recv len = %d, start_offset=%d", total_bytes,
				start_offset);
		result = dlt645_bcd_to_frame((unsigned char*) buf_recv + start_offset, out_frame);
		dlt645_print_frame(*out_frame);
	}

	// free memory
	if (bcd_buf != NULL)
		free(bcd_buf);

//	close(serial_fd);
	serial_status = 0;
	return result;
}
static void* thread_dlt645_routine(void*) {
	while (1) {
		log_debug_print(g_debug_verbose, "dlt645_kwh!");
		dlt645_kwh();
		sleep(1);

		log_debug_print(g_debug_verbose, "dlt645_current!");
		dlt645_current();
		sleep(1);

		log_debug_print(g_debug_verbose, "dlt645_voltage!");
		dlt645_voltage();
		sleep(1);

		log_debug_print(g_debug_verbose, "dlt645_kw!");
		dlt645_kw();
		sleep(20);
	}
	// Nerver run here
	close(serial_fd);
	exit(EXIT_SUCCESS);
}
static void* thread_dlt645_ctrl(void* event) {
	int tmp_event = *((int *) event);
	if (tmp_event == 1) {
		log_debug_print(g_debug_verbose, "dlt645_turnon!");
		dlt645_turnon();
	} else if (tmp_event == 2) {
		log_debug_print(g_debug_verbose, "dlt645_turnoff!");
		dlt645_turnoff();
	}
	pthread_exit(NULL);
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
	uint32_t device = device_proc_uart_com2;
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

	if(busi_server_buf != NULL)
		free(busi_server_buf);

	if(resp_pkg_server != NULL)
		free_network_package(resp_pkg_server);
}
