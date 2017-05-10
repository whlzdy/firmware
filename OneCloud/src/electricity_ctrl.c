/*
 * electricity_ctrl.c
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

#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#define RECV_BUF_SIZE 100

int serial_fd = 0;
char device_name[32];
void dlt645_turnon();
void dlt645_turnoff();
int dlt645_send(DLT645_FRAME* in_frame,DLT645_FRAME** out_frame);

int init_serial(char* device);
int uart_send(int fd, char *data, int datalen);
int uart_recv(int fd, char *data, int datalen);
int uart_recv_block(int fd, char *data, int datalen);
void sig_handler(const int sig);


/**
 * Main daemon
 */
int main(int argc, char **argv) {
	/////////////////////////////////////////////////////
	// Parameter process
	/////////////////////////////////////////////////////
	if (argc < 3) {
		printf("Usage: electricity_ctrl <dev_name> <on|off> \n");
		exit(EXIT_SUCCESS);
	}
	char tmp_name[10];
	char action[10];
	memset(device_name,0x00,sizeof(device_name));
	memset(tmp_name,0x00,sizeof(tmp_name));
	memset(action,0x00,sizeof(action));
	sscanf(argv[1], "%s", &tmp_name);
	sprintf(device_name, "/dev/%s", tmp_name);
	sscanf(argv[2], "%s", &action);
	/////////////////////////////////////////////////////
	// Open serial port
	/////////////////////////////////////////////////////
	printf("Target device is %s.\n", device_name);
	/*Initialize serial*/
	int init_result = dlt645_init_serial(&serial_fd, (char*) device_name);
	if (init_result == -1) {
		printf("Initialize %s failure.(return %d)", device_name,
				init_result);
		return EXIT_FAILURE;
	} else {
		printf("Initialize %s success.", device_name);
	}

	/* handle SIGINT */
	signal(SIGINT, sig_handler);

    if(strcmp(action,"on")==0){
		printf("dlt645_turnon!\n");
		dlt645_turnon();
    }else if(strcmp(action,"off")==0){
		printf("dlt645_turnoff!\n");
		dlt645_turnoff();
    }else{
    	printf("no meaning %s !\n",argv[2]);
    }
    close(serial_fd);

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
	printf("ret = %d\n", ret);
	//如果返回0，代表在描述符状态改变前已超过timeout时间,错误返回-1

	if (FD_ISSET(fd, &fs_read)) {
		len = read(fd, data, datalen);
		printf("len = %d\n", len);
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
	printf("Info: recv data,len = %d\n", len);

	return len;
}

void sig_handler(const int sig)
{
	printf("SIGINT handled.\n");
	printf("Info: Now stop COM receive demo.\n");
	if(serial_fd != 0)
		close(serial_fd);

	exit(EXIT_SUCCESS);
}

void dlt645_turnon(){
	DLT645_FRAME* in_frame=NULL;
	DLT645_FRAME* out_frame=NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//on
	if (dlt645_gen_cmd_control_turnOn(&in_frame, address) == -1) {
		printf("Error: dlt645_gen_cmd_control_turnOn() catch error!\n");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame,&out_frame);
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
	if (out_frame != NULL)
		dlt645_free_frame(out_frame);
}
void dlt645_turnoff(){
	DLT645_FRAME* in_frame=NULL;
	DLT645_FRAME* out_frame=NULL;
	char temp[10];
	char temp_len[10];
	char data_buf[100];
	int i;
	char address[6] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
	//off
	if (dlt645_gen_cmd_control_turnOff(&in_frame, address) == -1) {
		printf("Error: dlt645_gen_cmd_control_turnOff() catch error!\n");
		exit(EXIT_FAILURE);
	}
	dlt645_send(in_frame,&out_frame);
	if (in_frame != NULL)
		dlt645_free_frame(in_frame);
	if (out_frame != NULL)
		dlt645_free_frame(out_frame);
}
int dlt645_send(DLT645_FRAME* in_frame,DLT645_FRAME** out_frame){
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

	printf("Now , send data to %s (%d)...\n", device_name, buf_len);
	uart_send(serial_fd, (char*)bcd_buf, buf_len);

	printf("Now , %s receive data...\n", device_name);
	char buf_recv[RECV_BUF_SIZE];
	int recv_bytes = 0;
	int total_bytes = 0;
	int retry = 3;
	int command_valid = OC_FALSE;
	int start_offset = 0;
	int tmpcount=0;
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
				for(tmpcount=0;tmpcount<total_bytes;tmpcount++){
					char tmp;
					tmp=buf_recv[tmpcount];
					printf("%02X ",tmp);
				}
				printf("\n");
				result = dlt645_check_command((unsigned char*)buf_recv, total_bytes,
						&start_offset);
				printf("Info: check command (len=%d) return %d\n", total_bytes,
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
		printf("uart total receive %d\n", total_bytes);

		if (command_valid == OC_TRUE)
			break;

		retry--;
	}

	// Print the response data
	if (command_valid == OC_TRUE) {
		printf("Info: buf_recv len = %d, start_offset=%d\n", total_bytes,
				start_offset);
		result = dlt645_bcd_to_frame((unsigned char*)buf_recv + start_offset, out_frame);
		dlt645_print_frame(*out_frame);
	}

	// free memory
	if (bcd_buf != NULL)
		free(bcd_buf);

//	close(serial_fd);
	return result;
}

