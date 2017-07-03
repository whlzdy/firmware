/*
 * lbs_daemon.c
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
#include <math.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"
#include "util/script_helper.h"
#include "util/log_helper.h"
#include "util/date_helper.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#define RECV_BUF_SIZE 100

int serial_fd = 0;
char device_name[32];
uint32_t mcc = 0;
uint32_t mnc = 0;
uint32_t lac = OC_LBS_BSTN_UNDEF;
uint32_t ci = OC_LBS_BSTN_UNDEF;

int init_serial(char* device);
int uart_send(int fd, char *data, int datalen);
int uart_recv(int fd, char *data, int datalen);
void sig_handler(const int sig);
static void* thread_com_lbs_handle(void*);
int com_lbs_control_test(void * device_name);
int com_lbs_data_test(void * device_name);
// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_lbs(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_query_lbs(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
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

	g_config->role = role_lbs;

	temp_int = 0;
	get_parameter_int(SECTION_LBS, "debug", &temp_int, OC_DEBUG_LEVEL_DISABLE);
	g_debug_verbose = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_LBS, "enable", &temp_int, 0);
	if(temp_int == OC_FALSE){
		// disabled
		log_info_print(g_debug_verbose, "lbs_daemon is disabled, exit");
		exit(EXIT_SUCCESS);
	}

	temp_int = 0;
	get_parameter_int(SECTION_MAIN, "listen_port", &temp_int,
	OC_MAIN_DEFAULT_PORT);
	g_config->main_listen_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_WATCHDOG, "listen_port", &temp_int,
	OC_WATCHDOG_DEFAULT_PORT);
	g_config->watchdog_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_LBS, "listen_port", &temp_int,
	OC_LBS_PORT);
	g_config->lbs_port = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_LBS, "mcc", &temp_int,
	0);
	mcc = temp_int;

	temp_int = 0;
	get_parameter_int(SECTION_LBS, "mnc", &temp_int,
	0);
	mnc = temp_int;

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
			fprintf(stderr, "usage: lbs_daemon [configuration file]\n");
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
	s_addr_in.sin_port = htons(g_config->lbs_port);
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
	log_debug_print(g_debug_verbose, "com lbs thread!");
	pthread_t thread_id_lbs;
	if (pthread_create(&thread_id_lbs, NULL, thread_com_lbs_handle,
	NULL) == -1) {
		log_error_print(g_debug_verbose, "com lbs pthread_create error!");
		return EXIT_FAILURE;
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
	case OC_REQ_QUERY_LBS:
		result = main_busi_query_lbs(req_buf, req_len, resp_buf, resp_len);
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
 * Lbs control process
 */
int main_busi_ctrl_lbs(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;

	OC_NET_PACKAGE* request = (OC_NET_PACKAGE*) req_buf;

	return result;
}

/**
 * Lbs query process
 */
int main_busi_query_lbs(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;
	uint32_t exec_result = 0;
	uint32_t error_no = 0;

	OC_CMD_QUERY_LBS_REQ* request = (OC_CMD_QUERY_LBS_REQ*) (req_buf
			+ OC_PKG_FRONT_PART_SIZE);
	OC_CMD_QUERY_LBS_RESP* response = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;
	result = generate_cmd_query_lbs_resp(&response, exec_result, error_no, mcc, mnc, lac, ci, 0);
	result = translate_cmd2buf_query_lbs_resp(response, &busi_buf, &busi_buf_len);
	if (result == OC_SUCCESS) {
		result = generate_response_package(OC_REQ_QUERY_LBS, busi_buf, busi_buf_len,
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
		perror("open error");
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
	cfsetospeed(&options, B9600); 		//设置波特率
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
	tv_timeout.tv_sec = (10 * 20 / 115200 + 10);
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

void sig_handler(const int sig) {
	g_timer_running = OC_FALSE;

	log_info_print(g_debug_verbose, "SIGINT handled.\n");
	log_info_print(g_debug_verbose, "Now stop COM receive demo.");
	if (serial_fd != 0)
		close(serial_fd);

	exit(EXIT_SUCCESS);
}

static void* thread_com_lbs_handle(void*) {
	/* handle SIGINT */
	signal(SIGINT, sig_handler);

	char buf_recv[RECV_BUF_SIZE];
	int nRecv = 0;
	while (1) {
		memset(device_name, 0, sizeof(device_name));
		uint8_t script_result[MAX_CONSOLE_BUFFER];
		uint8_t tmp_result[MAX_CONSOLE_BUFFER];
		memset((void*) script_result, 0, MAX_CONSOLE_BUFFER);
		memset((void*) tmp_result, 0, MAX_CONSOLE_BUFFER);
		script_shell_exec((unsigned char*) "/opt/onecloud/script/lbs/getusbno.sh",
				script_result);
		log_debug_print(g_debug_verbose, "script_result [%s]", script_result);
		if (strlen((char*) script_result) < 2) {
			log_error_print(g_debug_verbose, "lbs control device not found!");
			exit(EXIT_FAILURE);
		}
		memcpy(tmp_result, script_result, strlen((char*) script_result) - 1);
		int control_found=0;
		int data_found=0;
		char *token0, *cur0 = (char*)tmp_result;
		char* const delim0 = " ";
		while (token0 = strsep(&cur0, delim0)) {
			log_debug_print(g_debug_verbose, "token0 [%s]", token0);
			if(com_lbs_control_test(token0)==0){
				log_debug_print(g_debug_verbose, "lbs control device found [%s]", token0);
				control_found=1;
				break;
			}
		}
		if(control_found != 1){
			log_error_print(g_debug_verbose, "lbs control device not found!");
			exit(EXIT_FAILURE);
		}
		strcpy(device_name, token0);
		log_debug_print(g_debug_verbose, "lbs device_name %s", device_name);
		while(1){
			if(com_lbs_data_test(device_name)!=0){
				log_error_print(g_debug_verbose, "lbs data error [%s]", device_name);
				break;
			}
			sleep(600);
			if(com_lbs_control_test(device_name)!=0){
				log_debug_print(g_debug_verbose, "lbs control device error [%s]", device_name);
					break;
			}
		}
	}

	// Nerver run here
	close(serial_fd);
	exit(EXIT_SUCCESS);
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
	uint32_t device = device_lbs;
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

int com_lbs_control_test(void* device_name) {
	if (init_serial((char*) device_name) != 0) {
		log_error_print(g_debug_verbose,
				"init_serial lbs control device error %s. ", device_name);
		return -1;
	}
    char buf_send[] = "AT+CREG=2\r\n";
    char buf_recv[RECV_BUF_SIZE];
	log_info_print(g_debug_verbose, "Open %s", device_name);
	log_info_print(g_debug_verbose, "send data[%s]", buf_send);
    uart_send(serial_fd, buf_send, 28);

	log_info_print(g_debug_verbose, "wait data...");
	int nRecv = 0;
	int retry = 3;
	while (retry > 0) {
		memset((void*) buf_recv, 0, RECV_BUF_SIZE);
		nRecv = uart_recv(serial_fd, buf_recv, RECV_BUF_SIZE);
		log_debug_print(g_debug_verbose, "uart receive [%s]", buf_recv);
		if(strstr(buf_recv,"OK")!=NULL){
			log_debug_print(g_debug_verbose, "uart receive OK!!");
			close(serial_fd);
			return 0;
		}
		retry--;
	}
	close(serial_fd);
	return -1;
}

int com_lbs_data_test(void* device_name) {
	if (init_serial((char*) device_name) != 0) {
		log_error_print(g_debug_verbose,
				"init_serial lbs data device error %s. ", device_name);
		return -1;
	}
    char buf_send[] = "AT+CREG?\r\n";
    char buf_recv[RECV_BUF_SIZE];
	log_info_print(g_debug_verbose, "send data[%s]", buf_send);
    uart_send(serial_fd, buf_send, 28);

	log_info_print(g_debug_verbose, "wait data...");
	int nRecv = 0;
	int retry = 3;
	while (retry > 0) {
		memset((void*) buf_recv, 0, RECV_BUF_SIZE);
		nRecv = uart_recv(serial_fd, buf_recv, RECV_BUF_SIZE);
		log_debug_print(g_debug_verbose, "uart receive [%s]", buf_recv);
		if(strstr(buf_recv,"+CREG:")!=NULL){
			//log_debug_print(g_debug_verbose, "uart receive +CREG:!!");
			char *token, *cur = buf_recv;
			char* const delim = ",";
			int count=0;
			while (token = strsep(&cur, delim)) {
				//log_debug_print(g_debug_verbose, "token [%s] count[%d]", token,count);
				if(count==2){
					lac=strtol(token,NULL,16);
				}
				if(count==3){
					ci=strtol(token,NULL,16);
				}
				count++;
			}
			log_debug_print(g_debug_verbose, "lac [%d] ci[%d]", lac,ci);

			close(serial_fd);
			return 0;
		}
		retry--;
	}
	close(serial_fd);
	return -1;
}

