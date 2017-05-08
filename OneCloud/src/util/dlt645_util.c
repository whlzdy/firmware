/*
 * dlt645_util.c
 *
 *  Created on: Feb 27, 2017
 *      Author: clouder
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "common/onecloud.h"
#include "util/dlt645_util.h"
#include "util/log_helper.h"

// set serial speed
int dlt645_speed_arr[] = { B38400, B19200, B9600, B4800, B2400, B1200, B300,
B38400, B19200, B9600, B4800, B2400, B1200, B300, };
int dlt645_speed_name_arr[] = { 38400, 19200, 9600, 4800, 2400, 1200, 300, 38400, 19200, 9600, 4800,
		2400, 1200, 300, };

// Open serial device
int dlt645_open_serial_dev(char *dev_name) {
	int fd = open(dev_name, O_RDWR);
	if (-1 == fd) {
		fprintf(stderr, "Error: Can't Open Serial Port %s.\n", dev_name);
		return -1;
	}

	return fd;
}

// Set serial device speed
void dlt645_set_serial_speed(int fd, int speed) {
	int i = 0;
	int status = 0;
	struct termios option;

	tcgetattr(fd, &option);
	for (i = 0; i < sizeof(dlt645_speed_arr) / sizeof(int); i++) {
		if (speed == dlt645_speed_name_arr[i]) {
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&option, dlt645_speed_arr[i]);
			cfsetospeed(&option, dlt645_speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &option);
			if (status != 0) {
				perror("Error: when update serial speed, tcsetattr() error.\n");
				return;
			}
			tcflush(fd, TCIOFLUSH);
		}
	}
}

// set serial port data, stop and crc bit.
int dlt645_set_parity(int fd, int databits, int stopbits, int parity) {
	struct termios options;
	if (tcgetattr(fd, &options) != 0) {
		perror("Error: Get serial attribute error.\n");
		return (OC_FALSE);
	}
	options.c_cflag &= ~CSIZE;

	//  data bit
	switch (databits) {
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		fprintf(stderr, "Error: [set_parity] Unsupported data size\n");
		return (OC_FALSE);
	}

	switch (parity) {
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;		// Clear parity enable
		options.c_iflag &= ~INPCK;		// Enable parity checking
		options.c_lflag &= 0; // add comment it's very important to set this parameter.
		options.c_oflag &= ~OPOST; /*Output*/
		break;

	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);	// set odd parity
		options.c_iflag |= INPCK;				// disnable parity checking
		break;

	case 'e':
	case 'E':
		options.c_cflag |= PARENB;     // enable parity
		options.c_cflag &= ~PARODD;    // change into even
		options.c_iflag |= INPCK;      // disnable parity checking
		options.c_iflag |= IGNPAR;
		break;

	case 'S':
	case 's':  // as no parity
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;

	default:
		fprintf(stderr, "Error: [set_parity] Unsupported parity\n");
		return (OC_FALSE);
	}

	// set stop bit
	switch (stopbits) {
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		fprintf(stderr, "Error: [set_parity] Unsupported stop bits\n");
		return (OC_FALSE);
	}
	// Set input parity option
	if (parity != 'n')
		options.c_iflag |= INPCK;
	tcflush(fd, TCIFLUSH);
	options.c_cc[VTIME] = 5; 		// set time out for 5 X 100ms
	options.c_cc[VMIN] = 1;       	// update the options and do it now
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		perror("Error: [set_parity] Set serial attribute failed.\n");
		return (OC_FALSE);
	}
	return (OC_TRUE);
}

// Set the serial device with empty parameters
void dlt645_config_serial_empty(int fd) {
	struct termios oldtio, newtio;

	tcgetattr(fd, &oldtio);
	memset(&newtio, 0, sizeof(newtio));

	newtio.c_cflag = B2400 | CS8 | CLOCAL | CREAD;
	newtio.c_cflag &= ~CRTSCTS;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_iflag &= ~( IXON | IXOFF | IXANY);
	newtio.c_oflag = 0;

	tcflush(fd, TCIFLUSH);
	tcsetattr(fd, TCSANOW, &newtio);
}

//Initial serial device
int dlt645_init_serial(int* serial_fd, char* device) {
	int result = OC_SUCCESS;

	// open serial port
	int fd = dlt645_open_serial_dev(device);
	if (fd == -1) {
		return OC_FAILURE;
	}
	// set parameters
	dlt645_set_serial_speed(fd, 2400);
	dlt645_config_serial_empty(fd); // it's so important that we must face this.

	if (dlt645_set_parity(fd, 8, 1, 'E') == OC_FALSE) {
		printf("Error: set parity error\n");
		result = OC_FAILURE;
	} else {
		printf("Info: set serial ok!\n");
	}

	*serial_fd = fd;

	return result;
}

// Translate ASCII data to HEX code
int dlt645_ascii2hex(unsigned char* src_buf, int src_buf_len, unsigned char* tgt_buf) {
	if (src_buf == NULL || tgt_buf == NULL || src_buf_len < 1)
		return -1;

	int i = 0;

	for (i = 0; i < src_buf_len; i++) {

	}

	return 0;
}

// Translate HEX code to ASCII data
int dlt645_hex2ascii(unsigned char* src_buf, int src_buf_len, unsigned char* tgt_buf) {
	return 0;
}

// Translate DL/T645 frame data to BCD code data
int dlt645_frame_to_bcd(DLT645_FRAME* frame, unsigned char** out_buf, unsigned char* out_len) {
	if (frame == NULL)
		return -1;

	// variables define
	unsigned char* buffer = NULL;
	unsigned char crc_code = 0x00;
	unsigned char buf_len = 0;
	unsigned char pre_len = 0;
	int i = 0;
	int crc_sum = 0;

	// allocate the memory
	buf_len = DLT645_FRAME_BEFORE_DATA_SIZE + frame->data_len + 2;
	buffer = (unsigned char*) malloc(buf_len);
	if (buffer == NULL)
		return -1;
	memset((void*) buffer, 0, buf_len);

	// ahead data buffer
	pre_len = DLT645_FRAME_BEFORE_DATA_SIZE;
	buffer[0] = frame->frame_start_ch1;
	for (i = 0; i < 6; i++) {
		buffer[i + 1] = frame->address[i];
	}
	buffer[7] = frame->frame_start_ch2;
	buffer[8] = frame->control;
	buffer[9] = frame->data_len;
	// !! notice: can not using memcpy
	//memcpy(buffer, (unsigned char*) frame, pre_len);
	//printf("buf_len=%d, pre_len=%d, data_len=%d\n", buf_len, pre_len, frame->data_len);

	// command data
	if (frame->data_len > 0) {
		for (i = 0; i < frame->data_len; i++) {
			buffer[pre_len + i] = frame->data[i] + 0x33;
		}
	}

	// calculate the CRC code
	for (i = 0; i < (buf_len - 2); i++) {
		crc_sum += buffer[i];
	}
	crc_code = crc_sum % 256;

	// suffix field
	buffer[buf_len - 2] = crc_code;
	buffer[buf_len - 1] = frame->frame_end_ch;

	frame->crc_code = crc_code;

	*out_buf = buffer;
	*out_len = buf_len;

	return 0;
}

// Translate BCD code data to DL/T645 frame data
int dlt645_bcd_to_frame(unsigned char* bcd_buf, DLT645_FRAME** out_frame) {
	if (bcd_buf == NULL)
		return -1;

	DLT645_FRAME* frame = NULL;
	unsigned char data_len = 0;
	unsigned char buf_len = 0;
	unsigned char pre_len = 0;
	int i = 0;

	// Allocate the frame memory
	frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	if (frame == NULL)
		return -1;
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	// copy the data
	pre_len = DLT645_FRAME_BEFORE_DATA_SIZE;
	// ahead data buffer
	//memcpy((unsigned char*)frame, bcd_buf, pre_len);
	frame->frame_start_ch1 = bcd_buf[0];
	for (i = 0; i < 6; i++) {
		frame->address[i] = bcd_buf[i + 1];
	}
	frame->frame_start_ch2 = bcd_buf[7];
	frame->control = bcd_buf[8];
	frame->data_len = bcd_buf[9];

	data_len = frame->data_len;
	buf_len = DLT645_FRAME_BEFORE_DATA_SIZE + frame->data_len + 2;

	// data
	if (data_len > 0) {
		frame->data = (unsigned char*) malloc(data_len);
		for (i = 0; i < data_len; i++) {
			frame->data[i] = bcd_buf[pre_len + i] - 0x33;
		}
	}

	// suffix field
	frame->crc_code = bcd_buf[buf_len - 2];
	frame->frame_end_ch = bcd_buf[buf_len - 1];

	*out_frame = frame;

	return 0;
}

/* Command validation
 * cmd_buf: Command buffer
 * input_len: Input buffer length.
 * return:  -2  incomplete command, may be need receive data from the channel.
 *          -1  Invalid command.
 *          0   check pass, command is completed.
 *          >0  exist a valid command, and some other data, return the length of command.
 */
int dlt645_check_command(unsigned char* cmd_buf, unsigned char input_len, int* start_offset) {
	if (cmd_buf == NULL || input_len < 1)
		return -1;

	unsigned char data_len = 0;
	unsigned char buf_len = 0;
	unsigned char pre_len = 0;
	unsigned char crc_code = 0x00;
	int i = 0;
	int crc_sum = 0;
	int start_index = -1;

	// length check
	pre_len = DLT645_FRAME_BEFORE_DATA_SIZE;
	if (pre_len >= input_len)
		return -2;

	// find the start char
	for (i = 0; i < input_len; i++) {
		if (cmd_buf[i] == DLT645_START_CHAR) {
			start_index = i;
			break;
		}
	}
	if (start_index == -1) {
		// invalid command, may need a retry receive data
		return -1;
	}

	data_len = cmd_buf[start_index + 9];
	buf_len = DLT645_FRAME_BEFORE_DATA_SIZE + data_len + 2;
	if (buf_len > (input_len - start_index))
		return -2;

	// flag char check
	if (cmd_buf[start_index] != DLT645_START_CHAR || cmd_buf[start_index + 7] != DLT645_START_CHAR
			|| cmd_buf[start_index + buf_len - 1] != DLT645_END_CHAR)
		return -1;

	// calculate the CRC code
	for (i = 0; i < (buf_len - 2); i++) {
		crc_sum += cmd_buf[start_index + i];
	}
	crc_code = crc_sum % 256;
	// CRC code check
	if (crc_code != cmd_buf[start_index + buf_len - 2])
		return -1;

	*start_offset = start_index;

	if ((start_index + buf_len) == input_len)
		return 0;
	else
		return buf_len;
}

/* Command generate : kwh electricity query.
 * out_frame: Pointer for return frame data.
 * address:  Address of RS485 device.
 * return: 0 if success, -1 if failure.
 */
int dlt645_gen_cmd_query_kwh(DLT645_FRAME** out_frame, char* address) {
	unsigned char data_len = 0;

	DLT645_FRAME* frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	frame->frame_start_ch1 = DLT645_START_CHAR;
	frame->frame_start_ch2 = DLT645_START_CHAR;

	if (address != NULL) {
		memcpy(frame->address, address, sizeof(frame->address));
	}
	frame->control = DLT645_CTRL_CODE_READ;

	data_len = 0x04;
	unsigned char* data = (unsigned char*) malloc(data_len);
//	data[0] = 0x09;
//	data[1] = 0x00;
//	data[2] = 0x01;
//	data[3] = 0x00;
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x01;
	data[3] = 0x00;
	frame->data = data;
	frame->data_len = data_len;

	frame->crc_code = 0x00;   //update at translate to BCD data
	frame->frame_end_ch = DLT645_END_CHAR;

	*out_frame = frame;

	return 0;
}

// Command generate : current query
int dlt645_gen_cmd_query_current(DLT645_FRAME** out_frame, char* address) {
	unsigned char data_len = 0;

	DLT645_FRAME* frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	frame->frame_start_ch1 = DLT645_START_CHAR;
	frame->frame_start_ch2 = DLT645_START_CHAR;

	if (address != NULL) {
		memcpy(frame->address, address, sizeof(frame->address));
	}
	frame->control = DLT645_CTRL_CODE_READ;

	data_len = 0x04;
	unsigned char* data = (unsigned char*) malloc(data_len);
	data[0] = 0x00;
	data[1] = 0x01;
	data[2] = 0x02;
	data[3] = 0x02;
	frame->data = data;
	frame->data_len = data_len;

	frame->crc_code = 0x00;   //update at translate to BCD data
	frame->frame_end_ch = DLT645_END_CHAR;

	*out_frame = frame;

	return 0;
}

// Command generate : Voltage query
int dlt645_gen_cmd_query_voltage(DLT645_FRAME** out_frame, char* address) {
	unsigned char data_len = 0;

	DLT645_FRAME* frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	frame->frame_start_ch1 = DLT645_START_CHAR;
	frame->frame_start_ch2 = DLT645_START_CHAR;

	if (address != NULL) {
		memcpy(frame->address, address, sizeof(frame->address));
	}
	frame->control = DLT645_CTRL_CODE_READ;

	data_len = 0x04;
	unsigned char* data = (unsigned char*) malloc(data_len);
	data[0] = 0x00;
	data[1] = 0x01;
	data[2] = 0x01;
	data[3] = 0x02;
	frame->data = data;
	frame->data_len = data_len;

	frame->crc_code = 0x00;   //update at translate to BCD data
	frame->frame_end_ch = DLT645_END_CHAR;

	*out_frame = frame;

	return 0;
}

// Command generate : kw query
int dlt645_gen_cmd_query_kw(DLT645_FRAME** out_frame, char* address) {
	unsigned char data_len = 0;

	DLT645_FRAME* frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	frame->frame_start_ch1 = DLT645_START_CHAR;
	frame->frame_start_ch2 = DLT645_START_CHAR;

	if (address != NULL) {
		memcpy(frame->address, address, sizeof(frame->address));
	}
	frame->control = DLT645_CTRL_CODE_READ;

	data_len = 0x04;
	unsigned char* data = (unsigned char*) malloc(data_len);
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x03;
	data[3] = 0x02;
	frame->data = data;
	frame->data_len = data_len;

	frame->crc_code = 0x00;   //update at translate to BCD data
	frame->frame_end_ch = DLT645_END_CHAR;

	*out_frame = frame;

	return 0;
}

// Command generate : Turn on power
int dlt645_gen_cmd_control_turnOn(DLT645_FRAME** out_frame, char* address) {
	unsigned char data_len = 0;

	DLT645_FRAME* frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	frame->frame_start_ch1 = DLT645_START_CHAR;
	frame->frame_start_ch2 = DLT645_START_CHAR;

	if (address != NULL) {
		memcpy(frame->address, address, sizeof(frame->address));
	}
	frame->control = DLT645_CTRL_CODE_ONOFF;

	data_len = 0x09;
	unsigned char* data = (unsigned char*) malloc(data_len);
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	data[4] = 0x87 - 0x33;
	data[5] = 0x86 - 0x33;
	data[6] = 0x74 - 0x33;
	data[7] = 0x78 - 0x33;
	data[8] = 0x1B;
	frame->data = data;
	frame->data_len = data_len;

	frame->crc_code = 0x00;   //update at translate to BCD data
	frame->frame_end_ch = DLT645_END_CHAR;

	*out_frame = frame;

	return 0;
}

// Command generate : Turn off power
int dlt645_gen_cmd_control_turnOff(DLT645_FRAME** out_frame, char* address) {
	unsigned char data_len = 0;

	DLT645_FRAME* frame = (DLT645_FRAME*) malloc(sizeof(DLT645_FRAME));
	memset((void*) frame, 0, sizeof(DLT645_FRAME));

	frame->frame_start_ch1 = DLT645_START_CHAR;
	frame->frame_start_ch2 = DLT645_START_CHAR;

	if (address != NULL) {
		memcpy(frame->address, address, sizeof(frame->address));
	}
	frame->control = DLT645_CTRL_CODE_ONOFF;

	data_len = 0x09;
	unsigned char* data = (unsigned char*) malloc(data_len);
	data[0] = 0x00;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	data[4] = 0x87 - 0x33;
	data[5] = 0x86 - 0x33;
	data[6] = 0x74 - 0x33;
	data[7] = 0x78 - 0x33;
	//data[8] = 0x1A;
	data[8] = 0x4D - 0x33;
	frame->data = data;
	frame->data_len = data_len;

	frame->crc_code = 0x00;   //update at translate to BCD data
	frame->frame_end_ch = DLT645_END_CHAR;

	*out_frame = frame;

	return 0;
}

// Release the frame data memory
void dlt645_free_frame(DLT645_FRAME* frame) {
	if (frame == NULL)
		return;

	if (frame->data != NULL) {
		free(frame->data);
		frame->data = NULL;
	}

	free(frame);
}

// Print the frame data for debug
void dlt645_print_frame(DLT645_FRAME* frame) {
	if (frame == NULL)
		return;

	int i = 0;
	char temp[4];

	unsigned char* data_buf = (unsigned char*) malloc(frame->data_len * 3 + 1);
	memset(data_buf, 0, frame->data_len * 3 + 1);

	if (frame->data_len > 0) {
		temp[0] = '\0';
		for (i = 0; i < frame->data_len; i++) {
			sprintf(temp, "%02X ", frame->data[i]);
			strcat((char*) data_buf, (const char*) temp);
		}
	}

	log_debug_print(OC_DEBUG_LEVEL_DEFAULT,
			"[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %s %02X %02X]",
			frame->frame_start_ch1, frame->address[0], frame->address[1], frame->address[2],
			frame->address[3], frame->address[4], frame->address[5], frame->frame_start_ch2,
			frame->control, frame->data_len, data_buf, frame->crc_code, frame->frame_end_ch);

	if (data_buf != NULL)
		free(data_buf);
}

// Print the buffer data for debug
void dlt645_print_buffer(unsigned char* buffer, unsigned char buf_len) {
	if (buffer == NULL || buf_len < 1)
		return;

	int i = 0;
	char temp[4];

	unsigned char* data_buf = (unsigned char*) malloc(buf_len * 3 + 1);
	memset(data_buf, 0, buf_len * 3 + 1);

	for (i = 0; i < buf_len; i++) {
		sprintf(temp, "%02X ", buffer[i]);
		strcat((char*) data_buf, (const char*) temp);
	}

	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "[%s]\n", data_buf);

	if (data_buf != NULL)
		free(data_buf);

}
