/*
 * monitor_client.c
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
#include "util/cJSON.h"       //add refence cjson object 

void print_json_cabinet_query(OC_CMD_QUERY_CABINET_RESP* query_cabinet);
void print_json_server_query(OC_CMD_QUERY_SERVER_RESP* query_server);

// Debug output level
int g_debug_verbose = OC_DEBUG_LEVEL_ERROR;

// Configuration file name
char g_config_file[256];

// Global configuration
struct settings *g_config = NULL;

void cmd_helper() {
	fprintf(stderr, "usage: monitor_client type=<cabinet|server|cg|qg|qt|qe|ce|qv|qgps|qgpsusb|all> [server_id=<string>] [event=<int>]\n");
}

#define QUERY_TYPE_CABINET		0
#define QUERY_TYPE_SERVER		1
#define CTRL_TYPE_GPIO			2
#define QUERY_TYPE_GPIO			3
#define QUERY_TYPE_TEMPERATURE	4
#define QUERY_TYPE_ELECTRICITY	5
#define CTRL_TYPE_ELECTRICITY	6
#define QUERY_TYPE_VOICE		7
#define QUERY_TYPE_GPS			8
#define QUERY_TYPE_GPSUSB		9
#define QUERY_TYPE_LBS			10
#define QUERY_TYPE_ALL                11
#define QUERY_TYPE_NONE			99

/**
 * Cabinet monitor client
 */
int main(int argc, char **argv) {

	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* p = NULL;
	int monitor_type = QUERY_TYPE_CABINET; //0: cabinet, 1:server, other: not support
	uint8_t server_id[20];
	uint32_t event = 0;

	///////////////////////////////////////////////////////////
	// Console parameter process
	///////////////////////////////////////////////////////////
	if (argc < 2) {
		cmd_helper();
		exit(EXIT_FAILURE);
	}

	// Input parameter process
	server_id[0] = '\0';
	p = argv[1];
	sscanf(p, "%[^=]=%[^=]", param_name, param_value);

	if (strcmp("type", param_name) == 0) {
		if (strcmp("cabinet", param_value) == 0) {
			monitor_type = QUERY_TYPE_CABINET;
		} else if (strcmp("server", param_value) == 0) {
			monitor_type = QUERY_TYPE_SERVER;
			if (argc > 2) {
				p = argv[2];
				sscanf(p, "%[^=]=%[^=]", param_name, param_value);
				if (strcmp("server_id", param_name) == 0) {
					strcpy((char*) server_id, (const char*) param_value);
				}
			}
		} else if (strcmp("cg", param_value) == 0) {
			monitor_type = CTRL_TYPE_GPIO;
			if (argc > 2) {
				p = argv[2];
				sscanf(p, "%[^=]=%[^=]", param_name, param_value);
				if (strcmp("event", param_name) == 0) {
					strcpy((char*) event, (const char*) param_value);
				}
			}
		} else if (strcmp("qg", param_value) == 0) {
			monitor_type = QUERY_TYPE_GPIO;
		} else if (strcmp("qt", param_value) == 0) {
			monitor_type = QUERY_TYPE_TEMPERATURE;
		} else if (strcmp("qe", param_value) == 0) {
			monitor_type = QUERY_TYPE_ELECTRICITY;
		} else if (strcmp("ce", param_value) == 0) {
			monitor_type = CTRL_TYPE_ELECTRICITY;
			if (argc > 2) {
				p = argv[2];
				sscanf(p, "%[^=]=%[^=]", param_name, param_value);
				if (strcmp("event", param_name) == 0) {
					strcpy((char*) event, (const char*) param_value);
				}
			}
		} else if (strcmp("qv", param_value) == 0) {
			monitor_type = QUERY_TYPE_VOICE;
		} else if (strcmp("qgps", param_value) == 0) {
			monitor_type = QUERY_TYPE_GPS;
		} else if (strcmp("qgpsusb", param_value) == 0) {
			monitor_type = QUERY_TYPE_GPSUSB;
		} else if (strcmp("qlbs", param_value) == 0) {
			monitor_type = QUERY_TYPE_LBS;
		} else if (strcmp("all", param_value) == 0) {
			monitor_type = QUERY_TYPE_ALL;
		}
		else {
			monitor_type = QUERY_TYPE_NONE;
			cmd_helper();
			exit(EXIT_FAILURE);
		}
	} else {
		cmd_helper();
		exit(EXIT_FAILURE);
	}

	///////////////////////////////////////////////////////////
	// Business Communication
	///////////////////////////////////////////////////////////
	uint8_t* requst_buf = NULL;
	uint8_t* busi_buf = NULL;
	int req_buf_len = 0;
	int busi_buf_len = 0;
	int result = OC_SUCCESS;
	OC_NET_PACKAGE* resp_pkg = NULL;
	if (monitor_type == QUERY_TYPE_CABINET) {

		// Construct the request command
		OC_CMD_QUERY_CABINET_REQ * req_cabinet = NULL;
		result = generate_cmd_query_cabinet_req(&req_cabinet, 0);
		result = translate_cmd2buf_query_cabinet_req(req_cabinet, &busi_buf, &busi_buf_len);
		if (req_cabinet != NULL)
			free(req_cabinet);

		// Communication with server
		result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
		OC_REQ_QUERY_CABINET_STATUS, busi_buf, busi_buf_len, &resp_pkg);

		package_print_frame(resp_pkg);

		OC_CMD_QUERY_CABINET_RESP* query_cabinet = (OC_CMD_QUERY_CABINET_RESP*) resp_pkg->data;
		log_debug_print(g_debug_verbose, "kwh=%f", query_cabinet->kwh);
		print_json_cabinet_query(query_cabinet);

	} else if (monitor_type == QUERY_TYPE_SERVER) {

		// Construct the request command
		OC_CMD_QUERY_SERVER_REQ * req_server = NULL;
		uint32_t type = 0;
		if (strlen((const char*) server_id) > 0)
			type = 1;
		result = generate_cmd_query_server_req(&req_server, type, server_id, 0);
		result = translate_cmd2buf_query_server_req(req_server, &busi_buf, &busi_buf_len);
		if (req_server != NULL)
			free(req_server);

		// Communication with server
		result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
		OC_REQ_QUERY_SERVER_STATUS, busi_buf, busi_buf_len, &resp_pkg);

		package_print_frame(resp_pkg);

		OC_CMD_QUERY_SERVER_RESP* query_server = (OC_CMD_QUERY_SERVER_RESP*) resp_pkg->data;
		log_debug_print(g_debug_verbose, "server_num=%d ", query_server->server_num);
		print_json_server_query(query_server);

		} else  if(monitor_type == QUERY_TYPE_ALL)
		{
			//s1:cabinet
			// Construct the request command
			OC_CMD_QUERY_CABINET_REQ * req_cabinet = NULL;
			result = generate_cmd_query_cabinet_req(&req_cabinet, 0);
			result = translate_cmd2buf_query_cabinet_req(req_cabinet, &busi_buf, &busi_buf_len);
			if (req_cabinet != NULL)
				free(req_cabinet);
			// Communication with server
			result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
			OC_REQ_QUERY_CABINET_STATUS, busi_buf, busi_buf_len, &resp_pkg);
			package_print_frame(resp_pkg);
			OC_CMD_QUERY_CABINET_RESP* query_cabinet = (OC_CMD_QUERY_CABINET_RESP*) resp_pkg->data;
			log_debug_print(g_debug_verbose, "kwh=%f", query_cabinet->kwh);
			//s2:server
			OC_CMD_QUERY_SERVER_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
			type = 1;
			result = generate_cmd_query_server_req(&req_server, type, server_id, 0);
			result = translate_cmd2buf_query_server_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
			free(req_server);
			// Communication with server
			result = net_business_communicate((uint8_t*) OC_LOCAL_IP, OC_MAIN_DEFAULT_PORT,
			OC_REQ_QUERY_SERVER_STATUS, busi_buf, busi_buf_len, &resp_pkg);
			package_print_frame(resp_pkg);
			OC_CMD_QUERY_SERVER_RESP* query_server = (OC_CMD_QUERY_SERVER_RESP*) resp_pkg->data;
			//s3:cjson serialzer
			cJSON *root,*devices,*servers,*item;
			char *out;
			int i;
			char tmp_str[32] = {0};
			root=cJSON_CreateObject();      // root object
			//add object to root
			//printf(
			//"{\"timestamp\":\"%d\",\"status\":%d,\"kwh\":%.2f,\"voltage\":%.2f,\"current\":%.2f,\"temperature\":%.2f,\"watt\":%.2f,\"decibel\":%.2f}\n",
			//query_cabinet->timestamp, query_cabinet->status, query_cabinet->kwh,
			//query_cabinet->voltage, query_cabinet->current, query_cabinet->temperature,
			//query_cabinet->watt, query_cabinet->voice_db);

			//sprintf(temp, "\"type\":%d,\"name\":\"%s\",\"mac\":\"%s\",\"ip\":\"%s\",\"status\":%d",
			//	server_data[i].type, server_data[i].name,
			//	server_data[i].mac, server_data[i].ip,
			//	server_data[i].status);
			cJSON_AddNumberToObject(root,"timestamp",query_cabinet->timestamp);    
			cJSON_AddNumberToObject(root,"status",query_cabinet->status);   
			memset(tmp_str,0,32);
			sprintf(tmp_str,"%.2f",query_cabinet->kwh);
			cJSON_AddStringToObject(root,"kwh",tmp_str);    
			memset(tmp_str,0,32);
			sprintf(tmp_str,"%.2f",query_cabinet->voltage);
			cJSON_AddStringToObject(root,"voltage",tmp_str);  
			memset(tmp_str,0,32);
			sprintf(tmp_str,"%.2f",query_cabinet->current);
			cJSON_AddStringToObject(root,"current",tmp_str);    
			memset(tmp_str,0,32);
			sprintf(tmp_str,"%.2f",query_cabinet->temperature);
			cJSON_AddStringToObject(root,"temperature",tmp_str);    
			memset(tmp_str,0,32);
			sprintf(tmp_str,"%.2f",query_cabinet->watt);
			cJSON_AddStringToObject(root,"watt",tmp_str);    
			memset(tmp_str,0,32);
			sprintf(tmp_str,"%.2f",query_cabinet->voice_db);
			cJSON_AddStringToObject(root,"decibel",tmp_str);   

			// gps
			if(query_cabinet->longitude != OC_GPS_COORD_UNDEF){
				// longitude
				memset(tmp_str,0,32);
				sprintf(tmp_str, "%.8f", query_cabinet->longitude);
				cJSON_AddStringToObject(root,"longitude",tmp_str);   
				// latitude
				memset(tmp_str,0,32);
				sprintf(tmp_str, "%.8f", query_cabinet->latitude);
				cJSON_AddStringToObject(root,"latitude",tmp_str); 
			}

			// lbs
			if(query_cabinet->lac != OC_LBS_BSTN_UNDEF){
				// mcc
				cJSON_AddNumberToObject(root,"mcc",query_cabinet->mcc);  
				// mnc
				cJSON_AddNumberToObject(root,"mnc",query_cabinet->mnc);  
				// lac
				cJSON_AddNumberToObject(root,"lac",query_cabinet->lac);   
				// ci
				cJSON_AddNumberToObject(root,"ci",query_cabinet->ci);   
			}
			// uptime
			int  cabinet_status = query_cabinet->status;
			memset( tmp_str, 0, sizeof(tmp_str));
			time_t now_time = time(NULL);
			long running_time = 0;
			if ((cabinet_status == OC_CABINET_STATUS_RUNNING
					|| cabinet_status == OC_CABINET_STATUS_STOPPING) && query_cabinet->start_time > 0)
				running_time = now_time - query_cabinet->start_time;
			cJSON_AddNumberToObject(root,"uptime",running_time);   
			char dev_status[8];
			if(cabinet_status == OC_CABINET_STATUS_RUNNING
				|| cabinet_status == OC_CABINET_STATUS_STOPPING || cabinet_status == OC_CABINET_STATUS_STARTING)
				strcpy(dev_status, "true");
			else
				strcpy(dev_status, "false");
			if(query_server->server_num > 0)
			{
				cJSON_AddItemToObject(root,"devices",devices=cJSON_CreateArray());    //servers array
				cJSON_AddStringToObject(item = cJSON_CreateObject() ,"name","storage");
				cJSON_AddStringToObject(item ,"type","raid");
				cJSON_AddStringToObject(item ,"working",dev_status);
				cJSON_AddItemToArray(devices,item);
				
				cJSON_AddStringToObject(item = cJSON_CreateObject() ,"name","firewall");
				cJSON_AddStringToObject(item ,"type","firewall");
				cJSON_AddStringToObject(item ,"working",dev_status);
				cJSON_AddItemToArray(devices,item);
				
				cJSON_AddStringToObject(item = cJSON_CreateObject() ,"name","switch");
				cJSON_AddStringToObject(item ,"type","switch");
				cJSON_AddStringToObject(item ,"working",dev_status);
				cJSON_AddItemToArray(devices,item);

				cJSON_AddStringToObject(item = cJSON_CreateObject() ,"name","servers");
				cJSON_AddStringToObject(item ,"type","server");	
				cJSON_AddItemToObject(item,"servers",servers=cJSON_CreateArray());    //servers array
				cJSON_AddItemToArray(devices,item);
				OC_SERVER_STATUS* server_data = (OC_SERVER_STATUS*) (((char*)query_server) + sizeof(uint32_t) * 2);
				//add sring msg to params
				for(i = 0;i < query_server->server_num;i++)
				{
					cJSON_AddNumberToObject(item = cJSON_CreateObject(),"type",server_data[i].type);
					memset(tmp_str,0,32);
					sprintf(tmp_str,"%s",server_data[i].name);
					log_debug_print(g_debug_verbose, "name=%s ", server_data[i].name);
					cJSON_AddStringToObject(item ,"name",tmp_str);
					memset(tmp_str,0,32);
					sprintf(tmp_str,"%s",server_data[i].mac);
					log_debug_print(g_debug_verbose, "mac=%s ", server_data[i].mac);
					cJSON_AddStringToObject(item ,"mac",tmp_str);
					memset(tmp_str,0,32);
					sprintf(tmp_str,"%s",server_data[i].ip);
					log_debug_print(g_debug_verbose, "ip=%s ", server_data[i].ip);
					cJSON_AddStringToObject(item ,"ip",tmp_str);
					log_debug_print(g_debug_verbose, "status=%d", server_data[i].status);
					cJSON_AddNumberToObject(item ,"status",server_data[i].status);
					cJSON_AddItemToArray(servers,item);
				}	
				
			}
			
			out=cJSON_Print(root);
			cJSON_Delete(root);
			printf("%s\n",out);
			free(out);
			
		}else if (monitor_type == CTRL_TYPE_GPIO) {

			// Construct the request command
			OC_CMD_CTRL_GPIO_REQ * req_cabinet = NULL;
			result = generate_cmd_ctrl_gpio_req(&req_cabinet, event,0);
			fprintf(stderr, "Info: event=%d \n", req_cabinet->event);
			result = translate_cmd2buf_ctrl_gpio_req(req_cabinet, &busi_buf, &busi_buf_len);
			if (req_cabinet != NULL)
				free(req_cabinet);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_GPIO_DEFAULT_PORT,
					OC_REQ_CTRL_GPIO, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_CTRL_GPIO_RESP* query_cabinet = (OC_CMD_CTRL_GPIO_RESP*) resp_pkg->data;
			fprintf(stderr, "Info: result=%d \n", query_cabinet->result);

		} else if (monitor_type == QUERY_TYPE_GPIO	) {

			// Construct the request command
			OC_CMD_QUERY_GPIO_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_gpio_req(&req_server, 0);
			result = translate_cmd2buf_query_gpio_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_GPIO_DEFAULT_PORT,
					OC_REQ_QUERY_GPIO, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_GPIO_RESP* query_server = (OC_CMD_QUERY_GPIO_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d io_status=%d io_status=%X \n", query_server->result, query_server->io_status, query_server->io_status);

		} else if (monitor_type == QUERY_TYPE_TEMPERATURE) {

			// Construct the request command
			OC_CMD_QUERY_TEMPERATURE_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_temperature_req(&req_server, 0);
			result = translate_cmd2buf_query_temperature_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_TEMPERATURE_PORT,
					OC_REQ_QUERY_TEMPERATURE, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_TEMPERATURE_RESP* query_server = (OC_CMD_QUERY_TEMPERATURE_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d t1=%.2f t2=%.2f \n", query_server->result, query_server->t1, query_server->t2);

		} else if (monitor_type == QUERY_TYPE_ELECTRICITY) {

			// Construct the request command
			OC_CMD_QUERY_ELECTRICITY_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_electricity_req(&req_server, 0);
			result = translate_cmd2buf_query_electricity_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", 17012,
					OC_REQ_QUERY_ELECTRICITY, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_ELECTRICITY_RESP* query_server = (OC_CMD_QUERY_ELECTRICITY_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d kwh=%.2f voltage=%.1f current=%.3f kw=%.4f \n", query_server->result, query_server->kwh, query_server->voltage, query_server->current, query_server->kw);

		} else if (monitor_type == CTRL_TYPE_ELECTRICITY) {

			// Construct the request command
			OC_CMD_CTRL_ELECTRICITY_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_ctrl_electricity_req(&req_server, event,0);
			result = translate_cmd2buf_ctrl_electricity_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", 17012,
					OC_REQ_CTRL_ELECTRICITY, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_CTRL_ELECTRICITY_RESP* query_server = (OC_CMD_CTRL_ELECTRICITY_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d  \n", query_server->result);

		} else if (monitor_type == QUERY_TYPE_VOICE) {

			// Construct the request command
			OC_CMD_QUERY_VOICE_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_voice_req(&req_server, 0);
			result = translate_cmd2buf_query_voice_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_VOICE_PORT,
					OC_REQ_QUERY_VOICE, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_VOICE_RESP* query_server = (OC_CMD_QUERY_VOICE_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d db=%.2f \n", query_server->result, query_server->db);

		} else if (monitor_type == QUERY_TYPE_GPS) {

			// Construct the request command
			OC_CMD_QUERY_GPS_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_gps_req(&req_server, 0);
			result = translate_cmd2buf_query_gps_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_GPS_PORT,
					OC_REQ_QUERY_GPS, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_GPS_RESP* query_server = (OC_CMD_QUERY_GPS_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d ew=%d longitude=%.2f ns=%d latitude=%.2f altitude=%.2f \n",
					query_server->result, query_server->ew, query_server->longitude, query_server->ns, query_server->latitude, query_server->altitude);

		} else if (monitor_type == QUERY_TYPE_GPSUSB) {

			// Construct the request command
			OC_CMD_QUERY_GPSUSB_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_gpsusb_req(&req_server, 0);
			result = translate_cmd2buf_query_gpsusb_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_GPSUSB_PORT,
					OC_REQ_QUERY_GPSUSB, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_GPSUSB_RESP* query_server = (OC_CMD_QUERY_GPSUSB_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d ew=%d longitude=%.2f ns=%d latitude=%.2f altitude=%.2f \n",
					query_server->result, query_server->ew, query_server->longitude, query_server->ns, query_server->latitude, query_server->altitude);

		} else if (monitor_type == QUERY_TYPE_LBS) {

			// Construct the request command
			OC_CMD_QUERY_LBS_REQ * req_server = NULL;
			uint32_t type = 0;
			if (strlen((const char*) server_id) > 0)
				type = 1;
			result = generate_cmd_query_lbs_req(&req_server, 0);
			result = translate_cmd2buf_query_lbs_req(req_server, &busi_buf, &busi_buf_len);
			if (req_server != NULL)
				free(req_server);

			// Communication with server
			result = net_business_communicate((uint8_t*) "127.0.0.1", OC_LBS_PORT,
					OC_REQ_QUERY_LBS, busi_buf, busi_buf_len, &resp_pkg);

			package_print_frame(resp_pkg);

			OC_CMD_QUERY_LBS_RESP* query_server = (OC_CMD_QUERY_LBS_RESP*) resp_pkg->data;
			fprintf(stderr, "Info:result=%d lac=%d ci=%d reserved_1=%d \n",
					query_server->result, query_server->lac, query_server->ci, query_server->reserved_1);

		}

	free(busi_buf);
	if (resp_pkg != NULL)
		free_network_package(resp_pkg);

	return EXIT_SUCCESS;
}

/**
 * Print the cabinet status in JSON format.
 */
void print_json_cabinet_query(OC_CMD_QUERY_CABINET_RESP* query_cabinet) {
	if (query_cabinet == NULL) {
		printf("{}");
		return;
	}

	printf(
			"{\"timestamp\":\"%d\",\"status\":%d,\"kwh\":%.2f,\"voltage\":%.2f,\"current\":%.2f,\"temperature\":%.2f,\"watt\":%.2f,\"decibel\":%.2f}\n",
			query_cabinet->timestamp, query_cabinet->status, query_cabinet->kwh,
			query_cabinet->voltage, query_cabinet->current, query_cabinet->temperature,
			query_cabinet->watt, query_cabinet->voice_db);
}

/**
 * Print the server status in JSON format.
 */
void print_json_server_query(OC_CMD_QUERY_SERVER_RESP* query_server) {
	if (query_server == NULL) {
		printf("{}");
		return;
	}

	if (query_server->server_num == 0) {
		printf("{\"count\":0, \"data\":[]}");
		return;
	}
	char buffer[1024];
	char temp[128];
	int i = 0;
	memset((void*) buffer, 0, sizeof(buffer));
	memset((void*) temp, 0, sizeof(temp));

	OC_SERVER_STATUS* server_data = (OC_SERVER_STATUS*) (((char*)query_server) + sizeof(uint32_t) * 2);

	for (i = 0; i < query_server->server_num; i++) {
		memset((void*) temp, 0, sizeof(temp));
		sprintf(temp, "\"type\":%d,\"name\":\"%s\",\"mac\":\"%s\",\"ip\":\"%s\",\"status\":%d",
				server_data[i].type, server_data[i].name,
				server_data[i].mac, server_data[i].ip,
				server_data[i].status);
		strcat(buffer, "{");
		strcat(buffer, temp);

		if ((i + 1) == query_server->server_num)
			strcat(buffer, "}");
		else
			strcat(buffer, "},");
	}
	printf("{\"count\":%d, \"data\":[%s]}", query_server->server_num, buffer);
}
