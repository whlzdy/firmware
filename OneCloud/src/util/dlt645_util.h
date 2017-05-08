/*
 * dlt645_util.h
 *
 *  Created on: Feb 27, 2017
 *      Author: clouder
 */

#ifndef DLT645_UTIL_H_
#define DLT645_UTIL_H_

///////////////////////////////////////////////////////////////////
// Structure define
///////////////////////////////////////////////////////////////////
#define OC_TRUE		1
#define OC_FALSE	0
#define OC_SUCCESS	0
#define OC_FAILURE	-1

#define DLT645_AWARE_PREFIX 0xFE
#define DLT645_START_CHAR 0x68
#define DLT645_END_CHAR 0x16

#define DLT645_CTRL_CODE_READ 0x11
#define DLT645_CTRL_CODE_WRITE 0x14
#define DLT645_CTRL_CODE_ONOFF 0x1C

// DL/T645-2007 data structure
typedef struct dlt645_frame {
	unsigned char frame_start_ch1;
	unsigned char address[6];
	unsigned char frame_start_ch2;
	unsigned char control;
	unsigned char data_len;
	unsigned char *data;
	unsigned char crc_code;
	unsigned char frame_end_ch;
} DLT645_FRAME;
#define DLT645_FRAME_BEFORE_DATA_SIZE 10



///////////////////////////////////////////////////////////////////
// Function define
///////////////////////////////////////////////////////////////////

//Initial serial device
int dlt645_init_serial(int* serial_fd, char* device);

// Translate ASCII data to HEX code
int dlt645_ascii2hex(unsigned char* src_buf, int src_buf_len, unsigned char* tgt_buf);

// Translate HEX code to ASCII data
int dlt645_hex2ascii(unsigned char* src_buf, int src_buf_len, unsigned char* tgt_buf);

// Translate DL/T645 frame data to BCD code data
int dlt645_frame_to_bcd(DLT645_FRAME* frame, unsigned char** out_buf, unsigned char* out_len);

// Translate BCD code data to DL/T645 frame data
int dlt645_bcd_to_frame(unsigned char* bcd_buf, DLT645_FRAME** out_frame);

// Command validation
int dlt645_check_command(unsigned char* cmd_buf, unsigned char input_len, int* start_offset);

// Command generate : kwh electricity query
int dlt645_gen_cmd_query_kwh(DLT645_FRAME** out_frame, char* address);

// Command generate : current query
int dlt645_gen_cmd_query_current(DLT645_FRAME** out_frame, char* address);

// Command generate : Voltage query
int dlt645_gen_cmd_query_voltage(DLT645_FRAME** out_frame, char* address);

// Command generate : kw query
int dlt645_gen_cmd_query_kw(DLT645_FRAME** out_frame, char* address);

// Command generate : Turn on power
int dlt645_gen_cmd_control_turnOn(DLT645_FRAME** out_frame, char* address);

// Command generate : Turn off power
int dlt645_gen_cmd_control_turnOff(DLT645_FRAME** out_frame, char* address);

// Release the frame data memory
void dlt645_free_frame(DLT645_FRAME* frame);

// Print the frame data for debug
void dlt645_print_frame(DLT645_FRAME* frame);

// Print the buffer data for debug
void dlt645_print_buffer(unsigned char* buffer, unsigned char buf_len);

#endif /* DLT645_UTIL_H_ */
