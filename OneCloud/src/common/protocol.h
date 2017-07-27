/*
 * protocol.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

/**
 * Message definition
 *  Network transmit package format:
 *    -------------------------------------------------------------------
 *    | start | magic   | command | data_len | raw data |  crc  |  end  |
 *    -------------------------------------------------------------------
 *    |4 Byte | 4 Byte  | 4 Byte  | 4 Byte   |  N Byte  | 4 Byte| 4 Byte|
 *
 *  raw data define base on the command
 */

/* Network package structure */
typedef struct oc_net_package{
	uint32_t prefix;			//start char(0xFAFAFAFA)  (4)
	uint32_t magic;				//magic char  (4)
	uint32_t command;			//command     (4)
	uint32_t length;			//length of data  (4)
	void *	 data;				//pointer of data (8)
	uint32_t crc;				//CRC code        (4)
	uint32_t suffix;			//end char(0xFBFBFBFB) (4)
} OC_NET_PACKAGE;
#define OC_PKG_PREFIX 	0xFAFAFAFA
#define OC_PKG_SUFFIX 	0xFBFBFBFB
#define OC_PKG_MAGIC 	0x00000000
#define OC_PKG_FRONT_PART_SIZE 16
#define OC_PKG_END_PART_SIZE 8
#define OC_PKG_MAX_LEN 1400
#define OC_PKG_MAX_DATA_LEN 1280
#define OC_WEB_PKG_MAX_LEN 64000

#define OC_RESPONSE_BIT 	0x00010000
// Command request define
#define OC_REQ_QUERY_CABINET_STATUS		0x00000001
#define OC_REQ_QUERY_SERVER_STATUS		0x00000002
#define OC_REQ_CTRL_STARTUP				0x00000003
#define OC_REQ_CTRL_SHUTDOWN			0x00000004
#define OC_REQ_QUERY_STARTUP			0x00000005
#define OC_REQ_QUERY_SHUTDOWN			0x00000006
#define OC_REQ_CTRL_GPIO				0x00000007
#define OC_REQ_QUERY_GPIO				0x00000008
#define OC_REQ_CTRL_COM					0x00000009
#define OC_REQ_QUERY_COM				0x0000000A
#define OC_REQ_CTRL_SIM					0x0000000B
#define OC_REQ_QUERY_SIM				0x0000000C
#define OC_REQ_CTRL_APP					0x0000000D
#define OC_REQ_QUERY_APP				0x0000000E
#define OC_REQ_HEART_BEAT				0x0000000F
#define OC_REQ_QUERY_TEMPERATURE		0x00000010
#define OC_REQ_CTRL_ELECTRICITY		    0x00000011
#define OC_REQ_QUERY_ELECTRICITY		0x00000012
#define OC_REQ_QUERY_VOICE				0x00000013
#define OC_REQ_QUERY_GPS				0x00000014
#define OC_REQ_QUERY_GPSUSB				0x00000015
#define OC_REQ_QUERY_LBS				0x00000016


/**
 *  Command definition : Cabinet query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    -----------------------------------------------------------------------------------------------------
 *    | command | status | timestamp |  kwh   | voltage | current | temperature | reserved 1 | reserved 2 |
 *    -----------------------------------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte | 4 Byte  | 4 Byte  |  4 Byte     | 4 Byte     | 4 Byte     |
 */

/* Request : Cabinet query*/
typedef struct oc_cmd_query_cabinet_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_CABINET_REQ;

/* Response : Cabinet query*/
typedef struct oc_cmd_query_cabinet_resp{
	uint32_t command;			//command
	uint32_t status;			//on:1, off:0, starting:2, stopping:3, error: 4
	time_t timestamp;			//current time
	time_t start_time;			//start time
	time_t stop_time;			//shutdown time
	float  	 kwh;
	float	 voltage;
	float	 current;
	float	 temperature;
	float	 watt;
	float	 voice_db;
	float    longitude;
	float    latitude;
	int      mcc;
	int      mnc;
	int      lac;
	int      ci;
	int	     reserved_1;
	float	 reserved_2;
} OC_CMD_QUERY_CABINET_RESP;


/**
 *  Command definition : Server query
 *  request format:
 *    --------------------------------------------
 *    | command |  type   | flag     | server id |
 *    --------------------------------------------
 *    |4 Byte   | 4 Byte  | 4 Byte   |  12 Byte  |
 *
 *  response format:
 *    ----------------------------------
 *    | command | number | server data |
 *    ----------------------------------
 *    |4 Byte   | 4 Byte |  N Byte     |
 */

/* Server status*/
typedef struct oc_server_status{
	uint32_t seq_id;			//command
	uint32_t type;				//0:server, 1:switch, 2:firewall, 3:raid, 4:router, 5:ups
	uint8_t	 name[32];			//Name of server
	uint8_t	 mac[32];			//MAC address
	uint8_t	 ip[32];			//IP address
	uint32_t status;			//1:running, 0:shutdown
} OC_SERVER_STATUS;

/* Request : Server query*/
typedef struct oc_cmd_query_server_req{
	uint32_t command;			//command
	uint32_t type;				//0:query all, 1:query by id
	uint32_t flag;			    //0:only query status, 1:get current and trigger a new status collect action.
	uint8_t	 server_id[20];		//server id
} OC_CMD_QUERY_SERVER_REQ;

/* Response : Server query*/
typedef struct oc_cmd_query_server_resp{
	uint32_t command;			//command
	uint32_t server_num;		//number of servers
	OC_SERVER_STATUS* server_data; //status data
} OC_CMD_QUERY_SERVER_RESP;


/**
 *  Command definition : Startup control
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    --------------------------------------------------------
 *    | command | result | timestamp | error no | reserved 1 |
 *    --------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte     |
 */

/* Request : Startup control*/
typedef struct oc_cmd_ctrl_startup_req{
	uint32_t command;			//command
	uint32_t flag;			    //default 0,  0:request from APP, 1:request from gpio
} OC_CMD_CTRL_STARTUP_REQ;

/* Response : Startup control*/
typedef struct oc_cmd_ctrl_startup_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	int		 reserved_1;
} OC_CMD_CTRL_STARTUP_RESP;


/**
 *  Command definition : Shutdown control
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    --------------------------------------------------------
 *    | command | result | timestamp | error no | reserved 1 |
 *    --------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte     |
 */

/* Request : Shutdown control*/
typedef struct oc_cmd_ctrl_shutdown_req{
	uint32_t command;			//command
	uint32_t flag;			    //default 0,  0:request from APP, 1:request from gpio
} OC_CMD_CTRL_SHUTDOWN_REQ;

/* Response : Shutdown control*/
typedef struct oc_cmd_ctrl_shutdown_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	int		 reserved_1;
} OC_CMD_CTRL_SHUTDOWN_RESP;


/**
 *  Command definition : Startup query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    ----------------------------------------------------------------------------------------
 *    | command | result | timestamp | error no | step no | is_finish | reserved 1 | message |
 *    ----------------------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  | 4 Byte    | 4 Byte     |128 Byte |
 */

/* Request : Startup query*/
typedef struct oc_cmd_query_startup_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_STARTUP_REQ;

/* Response : Startup query*/
typedef struct oc_cmd_query_startup_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	uint32_t step_no;			//step number
	uint32_t is_finish;			//0:not finish, 1:finish
	int		 reserved_1;
	uint8_t	 message[128];		//error message
} OC_CMD_QUERY_STARTUP_RESP;

/**
 *  Command definition : Shutdown query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    ----------------------------------------------------------------------------------------
 *    | command | result | timestamp | error no | step no | is_finish | reserved 1 | message |
 *    ----------------------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  |4 Byte     | 4 Byte     |128 Byte |
 */

/* Request : Shutdown query*/
typedef struct oc_cmd_query_shutdown_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_SHUTDOWN_REQ;

/* Response : Shutdown query*/
typedef struct oc_cmd_query_shutdown_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	uint32_t step_no;			//step number
	uint32_t is_finish;			//0:not finish, 1:finish
	int		 reserved_1;		//0: normal power off, 1:force power off
	uint8_t	 message[128];		//error message
} OC_CMD_QUERY_SHUTDOWN_RESP;


/**
 *  Command definition : GPIO control
 *  request format:
 *    -------------------------------
 *    | command |  event  |  flag   |
 *    -------------------------------
 *    |4 Byte   | 4 Byte  | 4 Byte  |
 *
 *  response format:
 *    --------------------------------------------------------------------
 *    | command | result | timestamp | IO status | error no | reserved 1 |
 *    --------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte    | 4 Byte   | 4 Byte     |
 */

/* Request : GPIO control*/
typedef struct oc_cmd_ctrl_gpio_req{
	uint32_t command;			//command
	uint32_t event;				//0:init, 1:startup, 2:shutdown, 3:catch error, 4:resolve error, 5:running, 6:stopped
	uint32_t flag;			    //reserved
	int		 reserved;
} OC_CMD_CTRL_GPIO_REQ;

/* Response : GPIO control*/
typedef struct oc_cmd_ctrl_gpio_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t io_status;			//32bit, one pin using 4bit
	uint32_t error_no;			//error code
	int		 reserved_1;
	int		 reserved_2;
} OC_CMD_CTRL_GPIO_RESP;

/**
 *  Command definition : GPIO query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    ---------------------------------------------------------------------------------------------------
 *    | command | timestamp | IO status |  P0   |  P1   |  P2   |  P3   |  P4   |  P5   |  P6   |  P7   |
 *    ---------------------------------------------------------------------------------------------------
 *    |4 Byte   | 8 Byte    |  4 Byte   | 4 Byte| 4 Byte| 4 Byte| 4 Byte| 4 Byte| 4 Byte| 4 Byte| 4 Byte|
 */

/* Request : GPIO query*/
typedef struct oc_cmd_query_gpio_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_GPIO_REQ;

/* Response : GPIO query*/
typedef struct oc_cmd_query_gpio_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t io_status;			//32bit, one pin using 4bit
	uint32_t p0_status;			//button 1 (Startup)
	uint32_t p1_status;			//button 2 (Shutdown)
	uint32_t p2_status;			//led 1 (green)
	uint32_t p3_status;			//led 2 (red)
	uint32_t p4_status;			//led 3 (yellow)
	uint32_t p5_status;			//not use
	uint32_t p6_status;			//not use
	uint32_t p7_status;			//not use
	int		 reserved_1;
} OC_CMD_QUERY_GPIO_RESP;


/**
 *  Command definition : Heart beat
 *  request format:
 *    --------------------------------------------------
 *    | command | device | status | flag   | timestamp |
 *    --------------------------------------------------
 *    |4 Byte   | 4 Byte | 4 Byte | 4 Byte | 8 Byte    |
 *
 *  response format:
 *    --------------------------------
 *    | command | result | timestamp |
 *    --------------------------------
 *    |4 Byte   | 4 Byte | 8 Byte    |
 */

/* Request : Heart beat*/
typedef struct oc_cmd_heart_beat_req{
	uint32_t command;			//command
	uint32_t device;			//which device send the message
	uint32_t status;			//status of device
	uint32_t flag;			    //reserved
	time_t timestamp;			//current time
} OC_CMD_HEART_BEAT_REQ;

/* Response : Heart beat*/
typedef struct oc_cmd_heart_beat_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
} OC_CMD_HEART_BEAT_RESP;



/**
 *  Command definition : Temperature query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    -----------------------------------------------------------------
 *    | command | result | timestamp | error no | t1      | t2        |
 *    -----------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  | 4 Byte    |
 */

/* Request : Temperature query*/
typedef struct oc_cmd_query_temperature_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_TEMPERATURE_REQ;

/* Response : Temperature query*/
typedef struct oc_cmd_query_temperature_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	float t1;			//t1
	float t2;			//t2
	int		 reserved;
} OC_CMD_QUERY_TEMPERATURE_RESP;



/**
 *  Command definition : Electricity ctrl
 *  request format:
 *    -------------------------------
 *    | command |  event  |  flag   |
 *    -------------------------------
 *    |4 Byte   | 4 Byte  |4 Byte   |
 *
 *  response format:
 *    -------------------------------------------
 *    | command | result | timestamp | error no |
 *    -------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   |
 */

/* Request : Electricity ctrl*/
typedef struct oc_cmd_ctrl_electricity_req{
	uint32_t command;			//command
	uint32_t event;				//1:turnon, 2:turnoff
	uint32_t flag;			    //reserved
	int		 reserved;
} OC_CMD_CTRL_ELECTRICITY_REQ;

/* Response : Electricity ctrl*/
typedef struct oc_cmd_ctrl_electricity_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	int		 reserved;
} OC_CMD_CTRL_ELECTRICITY_RESP;

/**
 *  Command definition : Electricity query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    ---------------------------------------------------------------------------
 *    | command | result | timestamp |  kwh   | voltage | current | kw          |
 *    ---------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte | 4 Byte  | 4 Byte  |  4 Byte     |
 */

/* Request : Electricity query*/
typedef struct oc_cmd_query_electricity_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_ELECTRICITY_REQ;

/* Response : Electricity query*/
typedef struct oc_cmd_query_electricity_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	float  	 kwh;               //6.2f
	float	 voltage;			//3.1f
	float	 current;			//3.3f
	float	 kw;				//2.4f
	int		 reserved;
} OC_CMD_QUERY_ELECTRICITY_RESP;


/**
 *  Command definition : APP control
 *  request format:
 *    -----------------------
 *    | command | operation |
 *    -----------------------
 *    |4 Byte   |  4 Byte   |
 *
 *  response format:
 *    ----------------------------------------------
 *    | command | result | reserved 1 | reserved 2 |
 *    ----------------------------------------------
 *    |4 Byte   | 4 Byte | 4 Byte     | 4 Byte     |
 */

/* Request : APP control*/
typedef struct oc_cmd_ctrl_app_req{
	uint32_t command;			//command
	uint32_t operation;			//0:init, 1:startup, 2:shutdown, 3:restart
} OC_CMD_CTRL_APP_REQ;

/* Response : GPIO control*/
typedef struct oc_cmd_ctrl_app_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	int		 reserved_1;
	int		 reserved_2;
} OC_CMD_CTRL_APP_RESP;


/**
 *  Command definition : Voice query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    -----------------------------------------------------
 *    | command | result | timestamp | error no | db      |
 *    -----------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  |
 */

/* Request : Voice query*/
typedef struct oc_cmd_query_voice_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_VOICE_REQ;

/* Response : Voice query*/
typedef struct oc_cmd_query_voice_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	float db;			//db
} OC_CMD_QUERY_VOICE_RESP;


/**
 *  Command definition : Gps query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    -------------------------------------------------------------------------------------------------------
 *    | command | result | timestamp | error no | ew      |longitude| ns      | latitude|altitude |
 *    -------------------------------------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  | 4 Byte  | 4 Byte  | 4 Byte  | 4 Byte  |
 */

/* Request : Gps query*/
typedef struct oc_cmd_query_gps_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_GPS_REQ;

/* Response : Gps query*/
typedef struct oc_cmd_query_gps_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
    uint32_t ew;                //0: east, 1: west
	float longitude;			//longitude
    uint32_t ns;                //0: north, 1: south
	float latitude;				//latitude
	float altitude;				//altitude
} OC_CMD_QUERY_GPS_RESP;


/**
 *  Command definition : Gpsusb query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    -------------------------------------------------------------------------------------------------------
 *    | command | result | timestamp | error no | ew      |longitude| ns      | latitude|altitude |
 *    -------------------------------------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  | 4 Byte  | 4 Byte  | 4 Byte  | 4 Byte  |
 */

/* Request : Gpsusb query*/
typedef struct oc_cmd_query_gpsusb_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_GPSUSB_REQ;

/* Response : Gpsusb query*/
typedef struct oc_cmd_query_gpsusb_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
    uint32_t ew;                //0: east, 1: west
	float longitude;			//longitude
    uint32_t ns;                //0: north, 1: south
	float latitude;				//latitude
	float altitude;				//altitude
} OC_CMD_QUERY_GPSUSB_RESP;


/**
 *  Command definition : Lbs query
 *  request format:
 *    ---------------------
 *    | command |  flag   |
 *    ---------------------
 *    |4 Byte   | 4 Byte  |
 *
 *  response format:
 *    --------------------------------------------------------------------------
 *    | command | result | timestamp | error no | lac     | ci      |reserved_1|
 *    --------------------------------------------------------------------------
 *    |4 Byte   | 4 Byte |  8 Byte   | 4 Byte   | 4 Byte  | 4 Byte  | 4 Byte   |
 */

/* Request : Lbs query*/
typedef struct oc_cmd_query_lbs_req{
	uint32_t command;			//command
	uint32_t flag;			    //reserved
} OC_CMD_QUERY_LBS_REQ;

/* Response : Lbs query*/
typedef struct oc_cmd_query_lbs_resp{
	uint32_t command;			//command
	uint32_t result;			//0: completed normal, 1: catch error.
	time_t timestamp;			//current time
	uint32_t error_no;			//error code
	uint32_t mcc;               //mcc
	uint32_t mnc;               //mnc
    uint32_t lac;               //lac
    uint32_t ci;                //ci
	int		 reserved_1;
} OC_CMD_QUERY_LBS_RESP;



////////////////////////////////////////////////////////////
// Function definition
////////////////////////////////////////////////////////////

/**
 * Generate request command package.
 */
int generate_request_package(uint32_t cmd, uint8_t* busi_buf, int in_buf_len, uint8_t** out_pkg_buf, int* out_pkg_len);

/**
 * Generate response command package.
 */
int generate_response_package(uint32_t cmd, uint8_t* busi_buf, int in_buf_len, uint8_t** out_pkg_buf, int* out_pkg_len);

/**
 * Translate command package to buffer.
 */
int translate_package_to_buffer(OC_NET_PACKAGE * pkg, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate command buffer to package structure .
 */
int translate_buffer_to_package(uint8_t* in_buf, int in_buf_len, OC_NET_PACKAGE** out_pkg);

/**
 * Release network package memory
 */
void free_network_package(OC_NET_PACKAGE * pkg);

/**
 * Validate the package buffer data
 */
int check_package_buffer(uint8_t* in_buf, int buf_len, int* start_pos);

// Print the frame data for debug
void package_print_frame(OC_NET_PACKAGE* frame);
// Print the buffer data for debug
void package_print_buffer(uint8_t* buffer, int buf_len);



///////////////////////////////////////////////////////////
// Command function: Cabinet query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Cabinet query.
 */
int generate_cmd_query_cabinet_req(OC_CMD_QUERY_CABINET_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Cabinet query.
 */
int generate_cmd_query_cabinet_resp(OC_CMD_QUERY_CABINET_RESP ** out_resp,
		uint32_t status, float kwh, float voltage, float current,
		float temperature, float watt, float voice_db, float longitude, float latitude,
		int mcc, int mnc, int lac, int ci, time_t start_time);

/**
 * Translate request command to buffer: Cabinet query.
 */
int translate_cmd2buf_query_cabinet_req(OC_CMD_QUERY_CABINET_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Cabinet query.
 */
int translate_cmd2buf_query_cabinet_resp(OC_CMD_QUERY_CABINET_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Cabinet query.
 */
int translate_buf2cmd_query_cabinet_req(uint8_t* in_buf, OC_CMD_QUERY_CABINET_REQ** out_req);

/**
 * Translate buffer to response command: Cabinet query.
 */
int translate_buf2cmd_query_cabinet_resp(uint8_t* in_buf, OC_CMD_QUERY_CABINET_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: Server query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Server query.
 */
int generate_cmd_query_server_req(OC_CMD_QUERY_SERVER_REQ ** out_req, uint32_t type, uint8_t* server_id, uint32_t flag);

/**
 * Generate response command: Server query.
 */
int generate_cmd_query_server_resp(OC_CMD_QUERY_SERVER_RESP ** out_resp, uint32_t server_num, OC_SERVER_STATUS* server_data);

/**
 * Translate request command to buffer: Server query.
 */
int translate_cmd2buf_query_server_req(OC_CMD_QUERY_SERVER_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Server query.
 */
int translate_cmd2buf_query_server_resp(OC_CMD_QUERY_SERVER_RESP* resp,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Server query.
 */
int translate_buf2cmd_query_server_req(uint8_t* in_buf,	OC_CMD_QUERY_SERVER_REQ** out_req);

/**
 * Translate buffer to response command: Server query.
 */
int translate_buf2cmd_query_server_resp(uint8_t* in_buf, OC_CMD_QUERY_SERVER_RESP ** out_resp);

/**
 * Release command memory: Response of query server
 */
void free_cmd_query_server_resp(OC_CMD_QUERY_SERVER_RESP * resp);



///////////////////////////////////////////////////////////
// Command function: Startup control
///////////////////////////////////////////////////////////

/**
 * Generate request command: Startup control.
 */
int generate_cmd_ctrl_startup_req(OC_CMD_CTRL_STARTUP_REQ ** out_req, uint32_t flag);
/**
 * Generate response command: Startup control.
 */
int generate_cmd_ctrl_startup_resp(OC_CMD_CTRL_STARTUP_RESP ** out_resp, uint32_t exec_result, uint32_t error_no);

/**
 * Translate request command to buffer: Startup control.
 */
int translate_cmd2buf_ctrl_startup_req(OC_CMD_CTRL_STARTUP_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Startup control.
 */
int translate_cmd2buf_ctrl_startup_resp(OC_CMD_CTRL_STARTUP_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Startup control.
 */
int translate_buf2cmd_ctrl_startup_req(uint8_t* in_buf,	OC_CMD_CTRL_STARTUP_REQ** out_req);
/**
 * Translate buffer to response command: Startup control.
 */
int translate_buf2cmd_ctrl_startup_resp(uint8_t* in_buf, OC_CMD_CTRL_STARTUP_RESP ** out_resp);

///////////////////////////////////////////////////////////
// Command function: Shutdown control
///////////////////////////////////////////////////////////

/**
 * Generate request command: Shutdown control.
 */
int generate_cmd_ctrl_shutdown_req(OC_CMD_CTRL_SHUTDOWN_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Shutdown control.
 */
int generate_cmd_ctrl_shutdown_resp(OC_CMD_CTRL_SHUTDOWN_RESP ** out_resp, uint32_t exec_result, uint32_t error_no);

/**
 * Translate request command to buffer: Shutdown control.
 */
int translate_cmd2buf_ctrl_shutdown_req(OC_CMD_CTRL_SHUTDOWN_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Shutdown control.
 */
int translate_cmd2buf_ctrl_shutdown_resp(OC_CMD_CTRL_SHUTDOWN_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Shutdown control.
 */
int translate_buf2cmd_ctrl_shutdown_req(uint8_t* in_buf, OC_CMD_CTRL_SHUTDOWN_REQ** out_req);

/**
 * Translate buffer to response command: Shutdown control.
 */
int translate_buf2cmd_ctrl_shutdown_resp(uint8_t* in_buf, OC_CMD_CTRL_SHUTDOWN_RESP ** out_resp);

///////////////////////////////////////////////////////////
// Command function: Startup query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Startup query.
 */
int generate_cmd_query_startup_req(OC_CMD_QUERY_STARTUP_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Startup query.
 */
int generate_cmd_query_startup_resp(OC_CMD_QUERY_STARTUP_RESP ** out_resp,
		uint32_t exec_result, uint32_t error_no, uint32_t step_no, uint32_t is_finish, uint8_t* message);
/**
 * Translate request command to buffer: Startup query.
 */
int translate_cmd2buf_query_startup_req(OC_CMD_QUERY_STARTUP_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Startup query.
 */
int translate_cmd2buf_query_startup_resp(OC_CMD_QUERY_STARTUP_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Startup query.
 */
int translate_buf2cmd_query_startup_req(uint8_t* in_buf, OC_CMD_QUERY_STARTUP_REQ** out_req);

/**
 * Translate buffer to response command: Startup query.
 */
int translate_buf2cmd_query_startup_resp(uint8_t* in_buf, OC_CMD_QUERY_STARTUP_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: Shutdown query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Shutdown query.
 */
int generate_cmd_query_shutdown_req(OC_CMD_QUERY_SHUTDOWN_REQ ** out_req, uint32_t flag);

/**
 * Generate response command: Shutdown query.
 */
int generate_cmd_query_shutdown_resp(OC_CMD_QUERY_SHUTDOWN_RESP ** out_resp,
		uint32_t exec_result, uint32_t error_no, uint32_t step_no, uint32_t is_finish, uint8_t* message);

/**
 * Translate request command to buffer: Shutdown query.
 */
int translate_cmd2buf_query_shutdown_req(OC_CMD_QUERY_SHUTDOWN_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Shutdown query.
 */
int translate_cmd2buf_query_shutdown_resp(OC_CMD_QUERY_SHUTDOWN_RESP* resp,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Shutdown query.
 */
int translate_buf2cmd_query_shutdown_req(uint8_t* in_buf, OC_CMD_QUERY_SHUTDOWN_REQ** out_req);

/**
 * Translate buffer to response command: Shutdown query.
 */
int translate_buf2cmd_query_shutdown_resp(uint8_t* in_buf, OC_CMD_QUERY_SHUTDOWN_RESP ** out_resp);


///////////////////////////////////////////////////////////
// Command function: GPIO control
///////////////////////////////////////////////////////////

/**
 * Generate request command: GPIO control.
 */
int generate_cmd_ctrl_gpio_req(OC_CMD_CTRL_GPIO_REQ ** out_req,	uint32_t event, uint32_t flag);

/**
 * Generate response command: GPIO control.
 */
int generate_cmd_ctrl_gpio_resp(OC_CMD_CTRL_GPIO_RESP ** out_resp, uint32_t exec_result, uint32_t io_status, uint32_t error_no);

/**
 * Translate request command to buffer: GPIO control.
 */
int translate_cmd2buf_ctrl_gpio_req(OC_CMD_CTRL_GPIO_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: GPIO control.
 */
int translate_cmd2buf_ctrl_gpio_resp(OC_CMD_CTRL_GPIO_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: GPIO control.
 */
int translate_buf2cmd_ctrl_gpio_req(uint8_t* in_buf, OC_CMD_CTRL_GPIO_REQ** out_req);

/**
 * Translate buffer to response command: GPIO control.
 */
int translate_buf2cmd_ctrl_gpio_resp(uint8_t* in_buf, OC_CMD_CTRL_GPIO_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: GPIO query
///////////////////////////////////////////////////////////

/**
 * Generate request command: GPIO query.
 */
int generate_cmd_query_gpio_req(OC_CMD_QUERY_GPIO_REQ ** out_req, uint32_t flag);

/**
 * Generate response command: GPIO query.
 */
int generate_cmd_query_gpio_resp(OC_CMD_QUERY_GPIO_RESP ** out_resp,
		uint32_t exec_result, uint32_t io_status, uint32_t p0_status,
		uint32_t p1_status, uint32_t p2_status, uint32_t p3_status,
		uint32_t p4_status, uint32_t p5_status, uint32_t p6_status,
		uint32_t p7_status);

/**
 * Translate request command to buffer: GPIO query.
 */
int translate_cmd2buf_query_gpio_req(OC_CMD_QUERY_GPIO_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: GPIO query.
 */
int translate_cmd2buf_query_gpio_resp(OC_CMD_QUERY_GPIO_RESP* resp,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: GPIO query.
 */
int translate_buf2cmd_query_gpio_req(uint8_t* in_buf, OC_CMD_QUERY_GPIO_REQ** out_req);
/**
 * Translate buffer to response command: GPIO query.
 */
int translate_buf2cmd_query_gpio_resp(uint8_t* in_buf, OC_CMD_QUERY_GPIO_RESP ** out_resp);




///////////////////////////////////////////////////////////
// Command function: Heart beat
///////////////////////////////////////////////////////////

/**
 * Generate request command: Heart beat.
 */
int generate_cmd_heart_beat_req(OC_CMD_HEART_BEAT_REQ ** out_req, uint32_t device, uint32_t status,uint32_t flag);
/**
 * Generate response command: Heart beat.
 */
int generate_cmd_heart_beat_resp(OC_CMD_HEART_BEAT_RESP ** out_resp, uint32_t exec_result);

/**
 * Translate request command to buffer: Heart beat.
 */
int translate_cmd2buf_heart_beat_req(OC_CMD_HEART_BEAT_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Heart beat.
 */
int translate_cmd2buf_heart_beat_resp(OC_CMD_HEART_BEAT_RESP* resp,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Heart beat.
 */
int translate_buf2cmd_heart_beat_req(uint8_t* in_buf, OC_CMD_HEART_BEAT_REQ** out_req);

/**
 * Translate buffer to response command: Heart beat.
 */
int translate_buf2cmd_heart_beat_resp(uint8_t* in_buf, OC_CMD_HEART_BEAT_RESP ** out_resp);


///////////////////////////////////////////////////////////
// Command function: Temperature query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Temperature query.
 */
int generate_cmd_query_temperature_req(OC_CMD_QUERY_TEMPERATURE_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Temperature query.
 */
int generate_cmd_query_temperature_resp(OC_CMD_QUERY_TEMPERATURE_RESP ** out_resp, uint32_t exec_result, uint32_t error_no, float t1, float t2);

/**
 * Translate request command to buffer: Temperature query.
 */
int translate_cmd2buf_query_temperature_req(OC_CMD_QUERY_TEMPERATURE_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Temperature query.
 */
int translate_cmd2buf_query_temperature_resp(OC_CMD_QUERY_TEMPERATURE_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Temperature query.
 */
int translate_buf2cmd_query_temperature_req(uint8_t* in_buf, OC_CMD_QUERY_TEMPERATURE_REQ** out_req);

/**
 * Translate buffer to response command: Temperature query.
 */
int translate_buf2cmd_query_temperature_resp(uint8_t* in_buf, OC_CMD_QUERY_TEMPERATURE_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: Electricity control
///////////////////////////////////////////////////////////

/**
 * Generate request command: Electricity control.
 */
int generate_cmd_ctrl_electricity_req(OC_CMD_CTRL_ELECTRICITY_REQ ** out_req,	uint32_t event, uint32_t flag);

/**
 * Generate response command: Electricity control.
 */
int generate_cmd_ctrl_electricity_resp(OC_CMD_CTRL_ELECTRICITY_RESP ** out_resp, uint32_t exec_result, uint32_t error_no);

/**
 * Translate request command to buffer: Electricity control.
 */
int translate_cmd2buf_ctrl_electricity_req(OC_CMD_CTRL_ELECTRICITY_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Electricity control.
 */
int translate_cmd2buf_ctrl_electricity_resp(OC_CMD_CTRL_ELECTRICITY_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Electricity control.
 */
int translate_buf2cmd_ctrl_electricity_req(uint8_t* in_buf, OC_CMD_CTRL_ELECTRICITY_REQ** out_req);

/**
 * Translate buffer to response command: Electricity control.
 */
int translate_buf2cmd_ctrl_electricity_resp(uint8_t* in_buf, OC_CMD_CTRL_ELECTRICITY_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: Electricity query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Electricity query.
 */
int generate_cmd_query_electricity_req(OC_CMD_QUERY_ELECTRICITY_REQ ** out_req, uint32_t flag);

/**
 * Generate response command: Electricity query.
 */
int generate_cmd_query_electricity_resp(OC_CMD_QUERY_ELECTRICITY_RESP ** out_resp,
		uint32_t exec_result, uint32_t error_no, float kwh, float voltage, float current, float kw);

/**
 * Translate request command to buffer: Electricity query.
 */
int translate_cmd2buf_query_electricity_req(OC_CMD_QUERY_ELECTRICITY_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Electricity query.
 */
int translate_cmd2buf_query_electricity_resp(OC_CMD_QUERY_ELECTRICITY_RESP* resp,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Electricity query.
 */
int translate_buf2cmd_query_electricity_req(uint8_t* in_buf, OC_CMD_QUERY_ELECTRICITY_REQ** out_req);
/**
 * Translate buffer to response command: Electricity query.
 */
int translate_buf2cmd_query_electricity_resp(uint8_t* in_buf, OC_CMD_QUERY_ELECTRICITY_RESP ** out_resp);




///////////////////////////////////////////////////////////
// Command function: APP control
///////////////////////////////////////////////////////////

/**
 * Generate request command: APP control.
 */
int generate_cmd_app_control_req(OC_CMD_CTRL_APP_REQ ** out_req, uint32_t operation);

/**
 * Generate response command: APP control.
 */
int generate_cmd_app_control_resp(OC_CMD_CTRL_APP_RESP ** out_resp,	uint32_t exec_result);

/**
 * Translate request command to buffer: APP control.
 */
int translate_cmd2buf_app_control_req(OC_CMD_CTRL_APP_REQ * req, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: APP control.
 */
int translate_cmd2buf_app_control_resp(OC_CMD_CTRL_APP_RESP* resp,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: APP control.
 */
int translate_buf2cmd_app_control_req(uint8_t* in_buf, OC_CMD_CTRL_APP_REQ** out_req);
/**
 * Translate buffer to response command: APP control.
 */
int translate_buf2cmd_app_control_resp(uint8_t* in_buf, OC_CMD_CTRL_APP_RESP ** out_resp);




///////////////////////////////////////////////////////////
// Command function: Voice query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Voice query.
 */
int generate_cmd_query_voice_req(OC_CMD_QUERY_VOICE_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Voice query.
 */
int generate_cmd_query_voice_resp(OC_CMD_QUERY_VOICE_RESP ** out_resp, uint32_t exec_result, uint32_t error_no, float db);

/**
 * Translate request command to buffer: Voice query.
 */
int translate_cmd2buf_query_voice_req(OC_CMD_QUERY_VOICE_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Voice query.
 */
int translate_cmd2buf_query_voice_resp(OC_CMD_QUERY_VOICE_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Voice query.
 */
int translate_buf2cmd_query_voice_req(uint8_t* in_buf, OC_CMD_QUERY_VOICE_REQ** out_req);

/**
 * Translate buffer to response command: Voice query.
 */
int translate_buf2cmd_query_voice_resp(uint8_t* in_buf, OC_CMD_QUERY_VOICE_RESP ** out_resp);




///////////////////////////////////////////////////////////
// Command function: Gps query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Gps query.
 */
int generate_cmd_query_gps_req(OC_CMD_QUERY_GPS_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Gps query.
 */
int generate_cmd_query_gps_resp(OC_CMD_QUERY_GPS_RESP ** out_resp, uint32_t exec_result, uint32_t error_no, uint32_t ew, float longitude, uint32_t ns, float latitude, float altitude);

/**
 * Translate request command to buffer: Gps query.
 */
int translate_cmd2buf_query_gps_req(OC_CMD_QUERY_GPS_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Gps query.
 */
int translate_cmd2buf_query_gps_resp(OC_CMD_QUERY_GPS_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Gps query.
 */
int translate_buf2cmd_query_gps_req(uint8_t* in_buf, OC_CMD_QUERY_GPS_REQ** out_req);

/**
 * Translate buffer to response command: Gps query.
 */
int translate_buf2cmd_query_gps_resp(uint8_t* in_buf, OC_CMD_QUERY_GPS_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: Gpsusb query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Gpsusb query.
 */
int generate_cmd_query_gpsusb_req(OC_CMD_QUERY_GPSUSB_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Gpsusb query.
 */
int generate_cmd_query_gpsusb_resp(OC_CMD_QUERY_GPSUSB_RESP ** out_resp, uint32_t exec_result, uint32_t error_no, uint32_t ew, float longitude, uint32_t ns, float latitude, float altitude);

/**
 * Translate request command to buffer: Gpsusb query.
 */
int translate_cmd2buf_query_gpsusb_req(OC_CMD_QUERY_GPSUSB_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Gpsusb query.
 */
int translate_cmd2buf_query_gpsusb_resp(OC_CMD_QUERY_GPSUSB_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Gpsusb query.
 */
int translate_buf2cmd_query_gpsusb_req(uint8_t* in_buf, OC_CMD_QUERY_GPSUSB_REQ** out_req);

/**
 * Translate buffer to response command: Gpsusb query.
 */
int translate_buf2cmd_query_gpsusb_resp(uint8_t* in_buf, OC_CMD_QUERY_GPSUSB_RESP ** out_resp);



///////////////////////////////////////////////////////////
// Command function: Lbs query
///////////////////////////////////////////////////////////

/**
 * Generate request command: Lbs query.
 */
int generate_cmd_query_lbs_req(OC_CMD_QUERY_LBS_REQ ** out_req,	uint32_t flag);

/**
 * Generate response command: Lbs query.
 */
int generate_cmd_query_lbs_resp(OC_CMD_QUERY_LBS_RESP ** out_resp, uint32_t exec_result, uint32_t error_no, uint32_t mcc, uint32_t mnc, uint32_t lac, uint32_t ci, uint32_t reserved_1);

/**
 * Translate request command to buffer: Lbs query.
 */
int translate_cmd2buf_query_lbs_req(OC_CMD_QUERY_LBS_REQ * req,	uint8_t** out_buf, int* out_buf_len);

/**
 * Translate response command to buffer: Lbs query.
 */
int translate_cmd2buf_query_lbs_resp(OC_CMD_QUERY_LBS_RESP* resp, uint8_t** out_buf, int* out_buf_len);

/**
 * Translate buffer to request command: Lbs query.
 */
int translate_buf2cmd_query_lbs_req(uint8_t* in_buf, OC_CMD_QUERY_LBS_REQ** out_req);

/**
 * Translate buffer to response command: Lbs query.
 */
int translate_buf2cmd_query_lbs_resp(uint8_t* in_buf, OC_CMD_QUERY_LBS_RESP ** out_resp);



#endif /* PROTOCOL_H_ */
