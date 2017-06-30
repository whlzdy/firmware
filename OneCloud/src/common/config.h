/*
 * config.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef CONFIG_H_
#define CONFIG_H_

////////////////////////////////////////////////////////////
// Common structure and function
////////////////////////////////////////////////////////////
#define MAX_FILE_PATH_LEN 128
#define MAX_CONSOLE_BUFFER 256
#define MAX_SCRIPT_SHELL_BUFFER 2048

// Daemon business role
enum daemon_business_role {
	role_main = 0,
	role_watchdog = 1,
	role_sim = 2,
	role_gpio = 3,
	role_script = 4,
	role_uart_com = 5,
	role_app_service = 6,
	role_temperature = 7,
	role_voice = 8,
	role_gps = 9,
	role_gpsusb = 10,
	role_lbs = 11
};

// Daemon business role
enum daemon_device_proc {
	device_proc_main = 0,
	device_proc_watchdog = 1,
	device_proc_sim = 2,
	device_proc_gpio = 3,
	device_proc_script = 4,
	device_proc_app_service = 6,
	device_proc_uart_com0 = 7,
	device_proc_uart_com1 = 8,
	device_proc_uart_com2 = 9,
	device_proc_uart_com3 = 10,
	device_proc_uart_com4 = 11,
	device_proc_uart_com5 = 12,
	device_temperature = 13,
	device_voice = 14,
	device_gps = 15,
	device_gpsusb = 16,
	device_lbs = 17
};


typedef struct uart_com_config {
	int iter_num;		// Iterate number
	int listen_port;	// COM daemon listen port
	int type;			// COM daemon business type
} UART_COM_CONFIG;
#define MAX_UART_COM_NUM 6

// Daemon settings
struct settings {
	int role;				// Daemon business role
	int main_listen_port;	// Main daemon listen port
	int watchdog_port;		// WatchDog listen port
	int sim_port;			// SIM daemon listen port
	int gpio_port;			// GPIO daemon listen port
	int script_port;		// Script daemon listen port
	int app_port;			// App service daemon listen port
	int temperature_port;	// Temperature daemon listen port
	int voice_port;	        // Voice daemon listen port
	int gps_port;	        // Gps daemon listen port
	int gpsusb_port;	    // Gpsusb daemon listen port
	int lbs_port;	    // Lbs daemon listen port
	int com_num;			// number of UART COM
	UART_COM_CONFIG com_config[MAX_UART_COM_NUM];
	char startup_ctrl_script[MAX_FILE_PATH_LEN];
	char startup_query_script[MAX_FILE_PATH_LEN];
	char shutdown_ctrl_script[MAX_FILE_PATH_LEN];
	char shutdown_query_script[MAX_FILE_PATH_LEN];
	char server_query_script[MAX_FILE_PATH_LEN];
	char exec_server_query_script[MAX_FILE_PATH_LEN];

	char app_center_ws_url[MAX_FILE_PATH_LEN];
	char app_post_result_url[MAX_FILE_PATH_LEN];
	char app_post_status_url[MAX_FILE_PATH_LEN];
	char app_post_command_url[MAX_FILE_PATH_LEN];

	char cabinet_auth_file[MAX_FILE_PATH_LEN];
	char control_auth_file[MAX_FILE_PATH_LEN];
	int  remoteOperation;	// enable remote operation or not

	char cabinet_status_file[MAX_FILE_PATH_LEN];

	int power_off_enable;	// enable power off when shutdown
	int power_off_force;	// enable force to power off when shutdown failed
};

////////////////////////////////////////////////////////////
// Configuration structure and function
////////////////////////////////////////////////////////////

#define MAX_PARAM_NAME_LEN 32
#define MAX_PARAM_VALUE_LEN 128
#define MAX_SECTION_ITEM_NUM 16
#define MAX_SECTION_NUM 20

#define SECTION_MAIN		"main"
#define SECTION_WATCHDOG	"watchdog"
#define SECTION_GPIO		"gpio"
#define SECTION_SIM			"sim"
#define SECTION_SCRIPT		"script"
#define SECTION_APP_SERVICE	"appservice"
#define SECTION_COM0		"com0"
#define SECTION_COM1		"com1"
#define SECTION_COM2		"com2"
#define SECTION_COM3		"com3"
#define SECTION_COM4		"com4"
#define SECTION_COM5		"com5"
#define SECTION_TEMPERATURE	"temperature"
#define SECTION_VOICE		"voice"
#define SECTION_GPS			"gps"
#define SECTION_GPSUSB		"gpsusb"
#define SECTION_LBS			"lbs"

#define STR_KEY_POID			"poid"
#define STR_KEY_ACCESS_KEY		"accessKey"
#define STR_KEY_ACCESS_SECRET	"accessSecret"
#define STR_KEY_USERNAME		"username"
#define STR_KEY_PASSWORD		"password"
#define STR_KEY_REMOTE_OPERATION		"remoteOperation"

#define STR_KEY_STATUS			"status"
#define STR_KEY_START_TIME		"start_time"
#define STR_KEY_STOP_TIME		"stop_time"

typedef struct config_string_pair {
	char name[MAX_PARAM_NAME_LEN];
	char value[MAX_PARAM_VALUE_LEN];
} CONFIG_STR_PAIR;

typedef struct config_section_group {
	char section_name[MAX_PARAM_NAME_LEN];
	int count;
	int max_size;
	CONFIG_STR_PAIR* param_list;
} CONFIG_SECTION_GROUP;

typedef struct config_map {
	int count;
	CONFIG_SECTION_GROUP* section_list[MAX_SECTION_NUM];
} CONFIG_MAP;

char* str_l_trim(char *buf);
char* str_r_trim(char *buf);
char* str_trim(char* buf);
void init_config_section_mem(CONFIG_SECTION_GROUP** section, char* section_name,
		int init_size);
void add_param_into_section(char* section_name, char* p_name, char* p_value);
void release_config_map();
int load_config_from_file(char* src_file);
int print_config_detail();
int get_parameter_str(char* section, char* param_name, char* out_value,
		char* default_value);
int get_parameter_int(char* section, char* param_name, int* out_value,
		int default_value);
/**
 * Load cabinet auth config
 */
int load_cabinet_auth_from_file(char* src_file, char* poid, char* access_key, char* access_secret);
/**
 * Load control auth config
 */
int load_control_auth_from_file(char* src_file, char* username, char* password, int* enable_remote_operation);
/**
 * Load control password
 */
int load_control_password_from_file(char* src_file, char* password);
/**
 * Load control remoteOperation flag
 */
int load_control_remoteOperation_from_file(char* src_file, int* enable_remote_operation);
/**
 * Load cabinet status data from file
 */
int load_cabinet_status_from_file(char* src_file, int* status, time_t* start_time, time_t* stop_time);
/**
 * Update cabinet status data into file
 */
int update_cabinet_status_into_file(char* src_file, int status, time_t start_time, time_t stop_time);
#endif /* CONFIG_H_ */
