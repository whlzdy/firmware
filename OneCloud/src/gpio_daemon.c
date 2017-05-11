/*
 * gpio_daemon.c
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
#include "util/date_helper.h"

#include "gpio.h"

// function definition
int main_business_process(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_unsupport_command(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len);
int main_busi_ctrl_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len);
int main_busi_query_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
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

	g_config->role = role_gpio;

	temp_int = 0;
	get_parameter_int(SECTION_GPIO, "debug", &temp_int, OC_DEBUG_LEVEL_DISABLE);
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
	get_parameter_int(SECTION_GPIO, "listen_port", &temp_int,
	OC_GPIO_DEFAULT_PORT);
	g_config->gpio_port = temp_int;

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
	s_addr_in.sin_port = htons(g_config->gpio_port);
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
	log_debug_print(g_debug_verbose, "gpio routine thread!");
	pthread_t thread_id_gpio;
	if (pthread_create(&thread_id_gpio, NULL, thread_gpio_routine_handle,
	NULL) == -1) {
		log_error_print(g_debug_verbose, "gpio routine pthread_create error!");
		return EXIT_FAILURE;
	}
	log_debug_print(g_debug_verbose, "gpio send thread!");
	pthread_t thread_id_send;
	if (pthread_create(&thread_id_send, NULL, thread_gpio_send_handle,
	NULL) == -1) {
		log_error_print(g_debug_verbose, "gpio send pthread_create error!");
		return EXIT_FAILURE;
	}
	log_debug_print(g_debug_verbose, "gpio query thread!");
	pthread_t thread_id_query;
	if (pthread_create(&thread_id_query, NULL, thread_gpio_query_handle,
	NULL) == -1) {
		log_error_print(g_debug_verbose, "gpio query pthread_create error!");
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

	///////////////////////////////////////////////////////////
	// Main service loop
	///////////////////////////////////////////////////////////
	while (1) {
		log_debug_print(g_debug_verbose, "waiting for new connection...");
		pthread_t thread_id;
		client_length = sizeof(s_addr_client);

		//Block here. Until server accpet a new connection.
		int sockfd = accept(sockfd_server, (struct sockaddr*) (&s_addr_client),
				(socklen_t *) (&client_length));
		if (sockfd == -1) {
			log_error_print(g_debug_verbose, "Accept error!");
			//ignore current socket ,continue while loop.
			continue;
		}
		log_debug_print(g_debug_verbose, "A new connection occurs!");
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
	case OC_REQ_CTRL_GPIO:
		result = main_busi_ctrl_gpio(req_buf, req_len, resp_buf, resp_len);
		break;

	case OC_REQ_QUERY_GPIO:
		result = main_busi_query_gpio(req_buf, req_len, resp_buf, resp_len);
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
 * GPIO control process
 */
int main_busi_ctrl_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf, int* resp_len) {

	int result = OC_SUCCESS;
	uint32_t exec_result = 0;
	uint32_t error_no = 0;

	OC_CMD_CTRL_GPIO_REQ* request = (OC_CMD_CTRL_GPIO_REQ*) (req_buf + OC_PKG_FRONT_PART_SIZE);
	log_debug_print(g_debug_verbose, "request->event=[%d]", request->event);
	if (request->event >= 0 && request->event <= 6) {
		receive_message.flag = 1;
		receive_message.event = request->event;
	} else {
		receive_message.flag = 1;
		receive_message.event = 3;
		exec_result = OC_FAILURE;
	}
	//usleep(1000 * GPIO_MAIN_TIMESTAMP);
	OC_CMD_CTRL_GPIO_RESP* response = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;
	result = generate_cmd_ctrl_gpio_resp(&response, exec_result, io_status, error_no);
	result = translate_cmd2buf_ctrl_gpio_resp(response, &busi_buf, &busi_buf_len);
	if (result == OC_SUCCESS) {
		result = generate_response_package(OC_REQ_CTRL_GPIO, busi_buf, busi_buf_len, &temp_buf,
				&temp_buf_len);
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
 * GPIO status query process
 */
int main_busi_query_gpio(unsigned char* req_buf, int req_len, unsigned char* resp_buf,
		int* resp_len) {

	int result = OC_SUCCESS;
	uint32_t exec_result = 0;

	OC_CMD_QUERY_GPIO_REQ* request = (OC_CMD_QUERY_GPIO_REQ*) (req_buf + OC_PKG_FRONT_PART_SIZE);
	OC_CMD_QUERY_GPIO_RESP* response = NULL;
	uint8_t* busi_buf = NULL;
	uint8_t* temp_buf = NULL;
	int busi_buf_len = 0;
	int temp_buf_len = 0;
	result = generate_cmd_query_gpio_resp(&response, exec_result, io_status, button_0, button_1,
			dev_led_1.status, dev_led_2.status, dev_led_3.status, dev_led_4.status, 0, 0);
	result = translate_cmd2buf_query_gpio_resp(response, &busi_buf, &busi_buf_len);
	if (result == OC_SUCCESS) {
		result = generate_response_package(OC_REQ_QUERY_GPIO, busi_buf, busi_buf_len, &temp_buf,
				&temp_buf_len);
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

void sig_handler(const int sig) {
	g_timer_running = OC_FALSE;

	// release GPIO control
	ioperm(GPIO_MIN_ADDR, GPIO_MAX_ADDR, FLAG_DISABLE);

	log_info_print(g_debug_verbose, "SIGINT handled.");
	log_info_print(g_debug_verbose, "Now stop GPIO demo.");

	exit(EXIT_SUCCESS);
}

static void* thread_gpio_routine_handle(void*) {
	/* handle SIGINT */
	signal(SIGINT, sig_handler);

	// get GPIO control
	ioperm(GPIO_MIN_ADDR, GPIO_MAX_ADDR, FLAG_ENABLE);

	// GPIO main setting
	outb(0x04, GPIO_GROUP_SELECT);
	outb(GPIO_FLAG_IO_CONFIG, GPIO_IO_DIRECTION);

	// LED initialize
	init_led_status();

	unsigned char ioData = 0x00;
	unsigned char tempData = 0x00;
	unsigned char newIoData = 0x00;
	int p0_low_counter = 0;
	int p0_low_release_delay = 0;
	int p1_low_counter = 0;
	int p1_low_release_delay = 0;
	int btn_press_time = BTN_PRESS_TIME / GPIO_MAIN_TIMESTAMP;
	int btn_release_delay_time = BTN_RELEASE_DELAY_TIME / GPIO_MAIN_TIMESTAMP;
	button_0 = 0;
	button_1 = 0;
	control_led_status(0);
	memset(&receive_message, 0x00, sizeof(CONTROL_MESSAGE));
	memset(&send_message, 0x00, sizeof(CONTROL_MESSAGE));
	// main loop
	while (1) {

		// get status data from GPIO
		ioData = inb(GPIO_DATA);

		// event detect
		tempData = ioData & GPIO_BIT_P0;
		p0_low_counter = (tempData > 0 ? 0 : (p0_low_counter + 1));
		button_0 = (tempData > 0 ? 0 : 1);
		tempData = ioData & GPIO_BIT_P1;
		p1_low_counter = (tempData > 0 ? 0 : (p1_low_counter + 1));
		button_1 = (tempData > 0 ? 0 : 1);

		////////////////////////////////////////////////////////////////////////
		// GPIO business process
		////////////////////////////////////////////////////////////////////////
		// Button 0 (P0)
		if (p0_low_counter > btn_press_time && p0_low_release_delay == 0) {
			p0_low_release_delay = btn_release_delay_time;

			// button 0 (P0) click event process
			log_debug_print(g_debug_verbose, "Button 0 click.");
			// add custom process here
			memset(&send_message, 0x00, sizeof(CONTROL_MESSAGE));
			send_message.flag = 2;
			send_message.event = 1; //start
			control_led_status(1);
		}
		// Button 2 (P1)
		if (p1_low_counter > btn_press_time && p1_low_release_delay == 0) {
			p1_low_release_delay = btn_release_delay_time;

			// button 1 (P1) click event process
			log_debug_print(g_debug_verbose, "Button 1 click.");
			// add custom process here
			memset(&send_message, 0x00, sizeof(CONTROL_MESSAGE));
			send_message.flag = 2;
			send_message.event = 2; //shutdown
			control_led_status(2);
		}
		if (receive_message.flag == 1) {
			control_led_status(receive_message.event);
			receive_message.flag = 0;
		}
		//continue flash
		if (dev_led_1.status == 2 && dev_led_1.flash_retain_counter == 1) {
			set_led_flash(&dev_led_1);
		}
		if (dev_led_2.status == 2 && dev_led_2.flash_retain_counter == 1) {
			set_led_flash(&dev_led_2);
		}

		newIoData = ioData;

		// LED 1 (P2)
		tempData = ioData & GPIO_BIT_P2;
		tempData = led_status_update_process(&dev_led_1, tempData, GPIO_BIT_P2);
		newIoData = tempData | ((~GPIO_BIT_P2) & newIoData);

		// LED 2 (P3)
		tempData = ioData & GPIO_BIT_P3;
		tempData = led_status_update_process(&dev_led_2, tempData, GPIO_BIT_P3);
		newIoData = tempData | ((~GPIO_BIT_P3) & newIoData);

		// LED 3 (P4)
		tempData = ioData & GPIO_BIT_P4;
		tempData = led_status_update_process(&dev_led_3, tempData, GPIO_BIT_P4);
		newIoData = tempData | ((~GPIO_BIT_P4) & newIoData);

		// LED 4 (P6)
		tempData = ioData & GPIO_BIT_P6;
		tempData = led_status_update_process(&dev_led_4, tempData, GPIO_BIT_P6);
		newIoData = tempData | ((~GPIO_BIT_P6) & newIoData);

		// Update LED status
		outb(newIoData, GPIO_DATA);

		////////////////////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////////////

		// delay counter
		p0_low_release_delay = (p0_low_release_delay > 0 ? (p0_low_release_delay - 1) : 0);
		p1_low_release_delay = (p1_low_release_delay > 0 ? (p1_low_release_delay - 1) : 0);

		//sleep 1msxn,
		usleep(1000 * GPIO_MAIN_TIMESTAMP);
	}

	// never go here
	ioperm(GPIO_MIN_ADDR, GPIO_MAX_ADDR, FLAG_DISABLE);
	return NULL;
}

// LED light status initialize.(must invoke after GPIO direction setting)
void init_led_status() {
	unsigned char ioData = 0x00;
	ioData = inb(GPIO_DATA);

	// clear output bit
	ioData = ioData & (GPIO_BIT_P0 | GPIO_BIT_P1); // ioData & 0x03

	// LED 1
	dev_led_1.status = 1;
	dev_led_1.flash_span_time = LED_FLASH_TIME_DEFAULT;
	dev_led_1.flash_retain_time = LED_FLASH_RETAIN_TIME;
	dev_led_1.flash_retain_counter = 0;
	dev_led_1.flash_off_counter = 0;
	dev_led_1.flash_on_counter = 0;
	dev_led_1.save_status = dev_led_1.status;

	// LED 2
	dev_led_2.status = 1;
	dev_led_2.flash_span_time = LED_FLASH_TIME_DEFAULT;
	dev_led_2.flash_retain_time = LED_FLASH_RETAIN_TIME;
	dev_led_2.flash_retain_counter = 0;
	dev_led_2.flash_off_counter = 0;
	dev_led_2.flash_on_counter = 0;
	dev_led_2.save_status = dev_led_2.status;

	// LED 3
	dev_led_3.status = 1;
	dev_led_3.flash_span_time = LED_FLASH_TIME_DEFAULT;
	dev_led_3.flash_retain_time = LED_FLASH_RETAIN_TIME;
	dev_led_3.flash_retain_counter = 0;
	dev_led_3.flash_off_counter = 0;
	dev_led_3.flash_on_counter = 0;
	dev_led_3.save_status = dev_led_3.status;

	// LED 4
	dev_led_4.status = 1;
	dev_led_4.flash_span_time = LED_FLASH_TIME_DEFAULT;
	dev_led_4.flash_retain_time = LED_FLASH_RETAIN_TIME;
	dev_led_4.flash_retain_counter = 0;
	dev_led_4.flash_off_counter = 0;
	dev_led_4.flash_on_counter = 0;
	dev_led_4.save_status = dev_led_4.status;

	// Now just support on or off
	if (dev_led_1.status == 1)
		ioData = ioData | GPIO_BIT_P2;
	if (dev_led_2.status == 1)
		ioData = ioData | GPIO_BIT_P3;
	if (dev_led_3.status == 1)
		ioData = ioData | GPIO_BIT_P4;
	if (dev_led_4.status == 1)
		ioData = ioData | GPIO_BIT_P6;

	// output data
	outb(ioData, GPIO_DATA);
}

// Update LED GPIO bit data.
unsigned char led_status_update_process(LED_STATUS* dev_led, unsigned char oldData,
		unsigned char gpio_bit) {
	unsigned char newData = oldData;
	if (dev_led->status == 0) {
		// off
		newData = 0x00;
	} else if (dev_led->status == 1) {
		// on
		newData = gpio_bit;
	} else if (dev_led->status == 2) {
		// flash
		dev_led->flash_retain_counter--;
		if (dev_led->flash_retain_counter > 0) {
			// continue flash
			if (oldData == gpio_bit) {
				// current is on
				dev_led->flash_on_counter--;
				if (dev_led->flash_on_counter == 0) {
					newData = 0x00; // turn off
					dev_led->flash_off_counter = dev_led->flash_span_time / GPIO_MAIN_TIMESTAMP;
				}
			} else {
				// current is off
				dev_led->flash_off_counter--;
				if (dev_led->flash_off_counter == 0) {
					newData = gpio_bit; // turn on
					dev_led->flash_on_counter = dev_led->flash_span_time / GPIO_MAIN_TIMESTAMP;
				}
			}
		} else {
			// stop flash
			dev_led->status = dev_led->save_status;
			newData = (dev_led->status == 0 ? 0x00 : gpio_bit);
			dev_led->flash_retain_counter = 0;
			dev_led->flash_off_counter = 0;
			dev_led->flash_on_counter = 0;
		}
	}

	return newData;

}

void set_led_flash(LED_STATUS* dev_led) {
	dev_led->save_status = dev_led->status;
	dev_led->flash_retain_counter = dev_led->flash_retain_time / GPIO_MAIN_TIMESTAMP;
	dev_led->flash_off_counter = dev_led->flash_span_time / GPIO_MAIN_TIMESTAMP;
	dev_led->flash_on_counter = dev_led->flash_span_time / GPIO_MAIN_TIMESTAMP;
	dev_led->status = 2;
}
//0:init, 1:startup, 2:shutdown, 3:catch error, 4:resolve error, 5:running, 6:stopped
void control_led_status(uint32_t event) {
	switch (event) {
	case 0:
		dev_led_1.status = 0; //green off
		dev_led_2.status = 1; //red on
		dev_led_3.status = 0; //orange off
		break;
	case 1:
		set_led_flash(&dev_led_1); //green flash
		dev_led_2.status = 1; //red on
		dev_led_3.status = 0; //orange off
		break;
	case 2:
		dev_led_1.status = 1; //green on
		set_led_flash(&dev_led_2); //red flash
		dev_led_3.status = 0; //orange off
		break;
	case 3:
		dev_led_3.status = 1; //orange on
		break;
	case 4:
		dev_led_3.status = 0; //orange off
		break;
	case 5:
		dev_led_1.status = 1; //green on
		dev_led_2.status = 0; //red off
		dev_led_3.status = 0; //orange off
		break;
	case 6:
		dev_led_1.status = 0; //green off
		dev_led_2.status = 1; //red on
		dev_led_3.status = 0; //orange off
		break;
	default:
		printf("no meaning %d\n", event);
		break;
	}
	io_status = button_0 + (button_1 << 4) + (dev_led_1.status << 8) + (dev_led_2.status << 12)
			+ (dev_led_3.status << 16);
	log_debug_print(g_debug_verbose, "io_status=[%d][%X]", io_status, io_status);
	log_debug_print(g_debug_verbose, "event=[%d]", event);
}

static void* thread_gpio_send_handle(void*) {
	while (1) {
		if (send_message.flag != 2) {
			usleep(100 * 1000);
			continue;
		}
		send_message.flag = 0;
		uint8_t* requst_buf = NULL;
		uint8_t* busi_buf = NULL;
		int req_buf_len = 0;
		int busi_buf_len = 0;
		int result = OC_SUCCESS;
		OC_NET_PACKAGE* resp_pkg = NULL;
		// Construct the request command

		if (send_message.event == 1) {
			OC_CMD_CTRL_STARTUP_REQ * req_startup = NULL;
			result = generate_cmd_ctrl_startup_req(&req_startup, 1);
			result = translate_cmd2buf_ctrl_startup_req(req_startup, &busi_buf, &busi_buf_len);
			if (req_startup != NULL)
				free(req_startup);
			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_MAIN_DEFAULT_PORT,
			OC_REQ_CTRL_STARTUP, busi_buf, busi_buf_len, &resp_pkg);
			if (result != OC_SUCCESS) {
				log_debug_print(g_debug_verbose, "net_business_communicate error=[%d]", result);
				control_led_status(3);
				continue;
			}
			package_print_frame(resp_pkg);

			OC_CMD_CTRL_STARTUP_RESP* ctrl_gpio = (OC_CMD_CTRL_STARTUP_RESP*) resp_pkg->data;
			log_debug_print(g_debug_verbose, "Info: result=%d", ctrl_gpio->result);
			if (ctrl_gpio->result != 0) {
				receive_message.flag = 1;
				receive_message.event = 3; //3:catch error
			} else {
				send_message.query = 1;
			}
		} else if (send_message.event == 2) {
			OC_CMD_CTRL_SHUTDOWN_REQ * req_shutdown = NULL;
			result = generate_cmd_ctrl_shutdown_req(&req_shutdown, 1);
			result = translate_cmd2buf_ctrl_shutdown_req(req_shutdown, &busi_buf, &busi_buf_len);
			if (req_shutdown != NULL)
				free(req_shutdown);
			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_MAIN_DEFAULT_PORT,
			OC_REQ_CTRL_SHUTDOWN, busi_buf, busi_buf_len, &resp_pkg);
			if (result != OC_SUCCESS) {
				log_debug_print(g_debug_verbose, "net_business_communicate error=[%d]", result);
				control_led_status(3);
				continue;
			}
			package_print_frame(resp_pkg);

			OC_CMD_CTRL_SHUTDOWN_RESP* ctrl_gpio = (OC_CMD_CTRL_SHUTDOWN_RESP*) resp_pkg->data;
			log_debug_print(g_debug_verbose, "Info: result=%d ", ctrl_gpio->result);
			if (ctrl_gpio->result != 0) {
				receive_message.flag = 1;
				receive_message.event = 3; //3:catch error
			} else {
				send_message.query = 1;
			}
		} else {
			log_debug_print(g_debug_verbose, "no meaning send_message.event=[%d]",
					send_message.event);
			exit(EXIT_FAILURE);
		}

		if (busi_buf != NULL)
			free(busi_buf);
		if (resp_pkg != NULL)
			free_network_package(resp_pkg);

	}
	return NULL;
}

static void* thread_gpio_query_handle(void*) {
	while (1) {
		if (send_message.query != 1) {
			sleep(5);
			continue;
		}
		uint8_t* requst_buf = NULL;
		uint8_t* busi_buf = NULL;
		int req_buf_len = 0;
		int busi_buf_len = 0;
		int result = OC_SUCCESS;
		OC_NET_PACKAGE* resp_pkg = NULL;
		// Construct the request command

		if (send_message.event == 1) {
			OC_CMD_QUERY_STARTUP_REQ * req_startup_query = NULL;
			result = generate_cmd_query_startup_req(&req_startup_query, 0);
			result = translate_cmd2buf_query_startup_req(req_startup_query, &busi_buf,
					&busi_buf_len);
			if (req_startup_query != NULL)
				free(req_startup_query);

			OC_NET_PACKAGE* resp_pkg = NULL;
			OC_CMD_QUERY_STARTUP_RESP* resp_startup_query = NULL;
			result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
			OC_REQ_QUERY_STARTUP, busi_buf, busi_buf_len, &resp_pkg);
			free(busi_buf);

			if (result == OC_SUCCESS) {
				log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");

				resp_startup_query = (OC_CMD_QUERY_STARTUP_RESP*) resp_pkg->data;

				if (resp_startup_query->step_no == OC_STARTUP_TOTAL_STEP
						&& resp_startup_query->result == 0) {
					if (strcmp((const char*) resp_startup_query->message, "finish") == 0) {
						receive_message.flag = 1;
						receive_message.event = 5; //5:running
						send_message.query = 0;
					}
				}
			} else {
				log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
				receive_message.flag = 1;
				receive_message.event = 3; //3:catch error
			}
		} else if (send_message.event == 2) {
			OC_CMD_QUERY_SHUTDOWN_REQ * req_shutdown_query = NULL;
			result = generate_cmd_query_shutdown_req(&req_shutdown_query, 0);
			result = translate_cmd2buf_query_shutdown_req(req_shutdown_query, &busi_buf,
					&busi_buf_len);
			if (req_shutdown_query != NULL)
				free(req_shutdown_query);

			OC_NET_PACKAGE* resp_pkg = NULL;
			OC_CMD_QUERY_SHUTDOWN_RESP* resp_shutdown_query = NULL;
			result = net_business_communicate((uint8_t*) OC_LOCAL_IP, g_config->main_listen_port,
			OC_REQ_QUERY_SHUTDOWN, busi_buf, busi_buf_len, &resp_pkg);
			free(busi_buf);

			if (result == OC_SUCCESS) {
				log_debug_print(g_debug_verbose, "net_business_communicate return OC_SUCCESS");

				resp_shutdown_query = (OC_CMD_QUERY_SHUTDOWN_RESP*) resp_pkg->data;

				if (resp_shutdown_query->step_no == OC_SHUTDOWN_TOTAL_STEP
						&& resp_shutdown_query->result == 0) {
					if (strcmp((const char*) resp_shutdown_query->message, "finish") == 0) {
						receive_message.flag = 1;
						receive_message.event = 6; //6:stopped
						send_message.query = 0;
					}
				}
			} else {
				log_debug_print(g_debug_verbose, "net_business_communicate return OC_FAILURE");
				receive_message.flag = 1;
				receive_message.event = 3; //3:catch error
			}
		} else {
			log_debug_print(g_debug_verbose, "no meaning send_message.event=[%d]",
					send_message.event);
			exit(EXIT_FAILURE);
		}
		if (resp_pkg != NULL)
			free_network_package(resp_pkg);

		sleep(5);

	}
	return NULL;
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
	uint32_t device = device_proc_gpio;
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
