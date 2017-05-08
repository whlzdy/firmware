/*
 * config.c
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sysexits.h>
#include <stddef.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "util/string_helper.h"
#include "util/log_helper.h"

static CONFIG_MAP* conf_map = NULL;

void init_config_section_mem(CONFIG_SECTION_GROUP** section, char* section_name, int init_size) {
	CONFIG_SECTION_GROUP* sec = (CONFIG_SECTION_GROUP*) malloc(sizeof(CONFIG_SECTION_GROUP));
	memset(sec, 0, sizeof(CONFIG_SECTION_GROUP));
	strcpy(sec->section_name, section_name);
	sec->count = 0;
	sec->param_list = (CONFIG_STR_PAIR*) calloc(init_size, sizeof(CONFIG_STR_PAIR));
	sec->max_size = init_size;

	*section = sec;
}

void add_param_into_section(char* section_name, char* p_name, char* p_value) {
	if (conf_map == NULL || conf_map->count == 0)
		return;

	int i = 0;

	// find section
	CONFIG_SECTION_GROUP* sec = NULL;
	for (i = 0; i < conf_map->count; i++) {
		if (strcmp(section_name, conf_map->section_list[i]->section_name) == 0) {
			sec = conf_map->section_list[i];
			break;
		}
	}
	if (sec != NULL) {
		if (sec->count == 0) {
			strcpy(sec->param_list[0].name, p_name);
			strcpy(sec->param_list[0].value, p_value);
			sec->count = 1;
		} else {
			for (i = 0; i < sec->count; i++) {
				if (strcmp(sec->param_list[i].name, p_name) == 0) {
					strcpy(sec->param_list[i].value, p_value);
					break;
				}
			}
			if (i == sec->count) {
				if (sec->count == sec->max_size) {
					printf("Error: The too much parameter in section[%s].\n", section_name);
					return;
				}
				// not found exist same parameter
				strcpy(sec->param_list[sec->count].name, p_name);
				strcpy(sec->param_list[sec->count].value, p_value);
				sec->count++;
			}
		}
	}
}

void release_config_map() {
	int i = 0, j = 0;
	CONFIG_SECTION_GROUP* section = NULL;
	if (conf_map != NULL) {
		for (i = 0; i < MAX_SECTION_NUM; i++) {
			section = conf_map->section_list[i];
			if (section != NULL) {
				free(section->param_list);
				section->param_list = NULL;
				free(section);
				conf_map->section_list[i] = NULL;
			}
		}
		free(conf_map);
		conf_map = NULL;
	}
}

int load_config_from_file(char* src_file) {
	int ret = OC_FAILURE;

	char line_buf[256];
	char curr_section[MAX_PARAM_NAME_LEN];
	char temp_1[MAX_PARAM_NAME_LEN];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* line_data = NULL;
	int len = 0;
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: Open config file %s failed.\n", src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	if (conf_map == NULL) {
		conf_map = (CONFIG_MAP*) malloc(sizeof(CONFIG_MAP));
		memset(conf_map, 0, sizeof(CONFIG_MAP));

		CONFIG_SECTION_GROUP* section = NULL;
		init_config_section_mem(&section, SECTION_MAIN, MAX_SECTION_ITEM_NUM);
		conf_map->section_list[0] = section;
		conf_map->count++;
	}
	strcpy(curr_section, SECTION_MAIN);

	// read line data
	ret = OC_SUCCESS;
	memset(line_buf, 0, sizeof(line_buf));
	while (fgets(line_buf, 256, f) != NULL) {
		len = strlen(line_buf);
		if (len < 3) {
			continue;
		}
		if (line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';
		if (strlen(line_buf) < 3) {
			continue;
		}

		line_data = str_trim(line_buf);
		len = strlen(line_data);
		if (len < 3) {
			continue;
		}

		// find section
		if (line_data[0] == '[' && line_data[len - 1] == ']') {
			memset(temp_1, 0, sizeof(temp_1));
			strncpy(temp_1, line_data + 1, len - 2);
			if (strcmp(curr_section, temp_1) != 0) {
				if (conf_map->count == (MAX_SECTION_NUM)) {
					fprintf(stderr, "Error: The config file too much section.\n");
					ret = OC_FAILURE;
					break;
				}
				CONFIG_SECTION_GROUP* section = NULL;
				init_config_section_mem(&section, temp_1, MAX_SECTION_ITEM_NUM);
				conf_map->section_list[conf_map->count] = section;
				conf_map->count++;
				strcpy(curr_section, temp_1);
			}
			continue;
		}

		//split parameter and value
		if (strchr(line_data, '=') == NULL) {
			// not found "="
			fprintf(stderr, "Error: Config content error.[%].\n", line_data);
			ret = OC_FAILURE;
			break;
		}

		sscanf(line_data, "%[^=]=%[^=]", param_name, param_value);
		//param_name = str_r_trim((char*)param_name);
		//param_value = str_l_trim((char*)param_value);

		add_param_into_section(curr_section, param_name, param_value);
	}

	// close the file
	fclose(f);

	if (ret == OC_FAILURE)
		release_config_map();

	return ret;
}

int print_config_detail() {
	int ret = -1;
	if (conf_map == NULL || conf_map->count == 0) {
		fprintf(stderr, "Error: config map is null or empty");
		return -1;
	}
	char print_buf[256];
	int i = 0;
	int j = 0;

	// open the file
	for (i = 0; i < conf_map->count; i++) {
		CONFIG_SECTION_GROUP* section = conf_map->section_list[i];
		sprintf(print_buf, "[%s]", section->section_name);
		log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "%s", print_buf);
		for (j = 0; j < section->count; j++) {
			sprintf(print_buf, "%s=%s", section->param_list[j].name, section->param_list[j].value);
			log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "%s", print_buf);
		}
	}
	ret = 0;

	return ret;
}

int get_parameter_str(char* section, char* param_name, char* out_value, char* default_value) {
	if (conf_map == NULL || conf_map->count == 0) {
		strcpy(out_value, default_value);
		fprintf(stderr, "Error: config map is null or empty");
		return -1;
	}
	short found = 0;
	int i = 0;

	// find section
	CONFIG_SECTION_GROUP* sec = NULL;
	for (i = 0; i < conf_map->count; i++) {
		if (strcmp(section, conf_map->section_list[i]->section_name) == 0) {
			sec = conf_map->section_list[i];
			break;
		}
	}
	if (sec != NULL) {
		if (sec->count > 0) {
			for (i = 0; i < sec->count; i++) {
				if (strcmp(sec->param_list[i].name, param_name) == 0) {
					strcpy(out_value, sec->param_list[i].value);
					found = 1;
					break;
				}
			}
		}
	}

	if (found == 0) {
		strcpy(out_value, default_value);
	}
	return 0;
}

int get_parameter_int(char* section, char* param_name, int* out_value, int default_value) {
	if (conf_map == NULL || conf_map->count == 0) {
		*out_value = default_value;
		fprintf(stderr, "Error: config map is null or empty");
		return -1;
	}

	char param_value[MAX_PARAM_VALUE_LEN];
	int ret = get_parameter_str(section, param_name, param_value, " ");
	if (ret < 0) {
		*out_value = default_value;
		return ret;
	} else {
		*out_value = atoi(param_value);
		return 0;
	}
}

/**
 * Load cabinet auth config
 */
int load_cabinet_auth_from_file(char* src_file, char* poid, char* access_key, char* access_secret) {
	int ret = OC_FAILURE;

	char line_buf[256];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* line_data = NULL;
	int len = 0;
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: Open cabinet auth file %s failed.\n", src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	// read line data
	ret = OC_SUCCESS;
	memset(line_buf, 0, sizeof(line_buf));
	while (fgets(line_buf, 256, f) != NULL) {
		len = strlen(line_buf);
		if (len < 3) {
			continue;
		}
		if (line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';
		if (strlen(line_buf) < 3) {
			continue;
		}

		line_data = str_trim(line_buf);
		len = strlen(line_data);
		if (len < 3) {
			continue;
		}

		//split parameter and value
		if (strchr(line_data, '=') == NULL) {
			// not found "="
			fprintf(stderr, "Error: Config content error.[%].\n", line_data);
			ret = OC_FAILURE;
			break;
		}

		sscanf(line_data, "%[^=]=%[^=]", param_name, param_value);

		if (strcmp(param_name, STR_KEY_POID) == 0) {
			if (strlen((const char*) param_value) > 0)
				strcpy(poid, param_value);
		} else if (strcmp(param_name, STR_KEY_ACCESS_KEY) == 0) {
			if (strlen((const char*) param_value) > 0)
				strcpy(access_key, param_value);
		} else if (strcmp(param_name, STR_KEY_ACCESS_SECRET) == 0) {
			if (strlen((const char*) param_value) > 0)
				strcpy(access_secret, param_value);
		}
	}

	// close the file
	fclose(f);

	return ret;
}

/**
 * Load control auth config
 */
int load_control_auth_from_file(char* src_file, char* username, char* password,
		int* enable_remote_operation) {
	int ret = OC_FAILURE;

	char line_buf[256];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* line_data = NULL;
	int len = 0;
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: Open control auth file %s failed.\n", src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	// read line data
	ret = OC_SUCCESS;
	memset(line_buf, 0, sizeof(line_buf));
	while (fgets(line_buf, 256, f) != NULL) {
		len = strlen(line_buf);
		if (len < 3) {
			continue;
		}
		if (line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';
		if (strlen(line_buf) < 3) {
			continue;
		}

		line_data = str_trim(line_buf);
		len = strlen(line_data);
		if (len < 3) {
			continue;
		}

		//split parameter and value
		if (strchr(line_data, '=') == NULL) {
			// not found "="
			fprintf(stderr, "Error: Config content error.[%].\n", line_data);
			ret = OC_FAILURE;
			break;
		}

		sscanf(line_data, "%[^=]=%[^=]", param_name, param_value);

		if (strcmp(param_name, STR_KEY_USERNAME) == 0) {
			if (strlen((const char*) param_value) > 0)
				strcpy(username, param_value);
		} else if (strcmp(param_name, STR_KEY_PASSWORD) == 0) {
			if (strlen((const char*) param_value) > 0)
				strcpy(password, param_value);
		} else if (strcmp(param_name, STR_KEY_REMOTE_OPERATION) == 0) {
			if (strlen((const char*) param_value) > 0) {
				if (strcmp(param_value, "true") == 0)
					*enable_remote_operation = OC_TRUE;
				else
					*enable_remote_operation = OC_FALSE;
			}
		}
	}

	// close the file
	fclose(f);

	return ret;
}

/**
 * Load cabinet status data from file
 */
int load_cabinet_status_from_file(char* src_file, int* status, time_t* start_time,
		time_t* stop_time) {
	int ret = OC_FAILURE;

	char line_buf[256];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* line_data = NULL;
	int len = 0;
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: Open cabinet status file %s failed.\n", src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	// read line data
	ret = OC_SUCCESS;
	memset(line_buf, 0, sizeof(line_buf));
	while (fgets(line_buf, 256, f) != NULL) {
		len = strlen(line_buf);
		if (len < 3) {
			continue;
		}
		if (line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';
		if (strlen(line_buf) < 3) {
			continue;
		}

		line_data = str_trim(line_buf);
		len = strlen(line_data);
		if (len < 3) {
			continue;
		}

		//split parameter and value
		if (strchr(line_data, '=') == NULL) {
			// not found "="
			fprintf(stderr, "Error: Config content error.[%].\n", line_data);
			ret = OC_FAILURE;
			break;
		}

		sscanf(line_data, "%[^=]=%[^=]", param_name, param_value);

		if (strcmp(param_name, STR_KEY_STATUS) == 0) {
			if (strlen((const char*) param_value) > 0)
				*status = atoi((const char*) param_value);
		} else if (strcmp(param_name, STR_KEY_START_TIME) == 0) {
			if (strlen((const char*) param_value) > 0)
				*start_time = atol((const char*) param_value);
		} else if (strcmp(param_name, STR_KEY_STOP_TIME) == 0) {
			if (strlen((const char*) param_value) > 0)
				*stop_time = atol((const char*) param_value);
		}
	}

	// close the file
	fclose(f);

	return ret;
}

/**
 * Update cabinet status data into file
 */
int update_cabinet_status_into_file(char* src_file, int status, time_t start_time, time_t stop_time) {
	int ret = OC_FAILURE;

	int old_status = -1;
	time_t old_start_time = 0;
	time_t old_stop_time = 0;

	ret = load_cabinet_status_from_file(src_file, &old_status, &old_start_time, &old_stop_time);
	if (ret == OC_FAILURE)
		return ret;

	if (old_status == status && old_start_time == start_time && old_stop_time == stop_time)
		return OC_SUCCESS;

	char line_buf[256];
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "w");
	if (f == NULL) {
		fprintf(stderr, "Error: Open cabinet status file %s failed.\n", src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	memset(line_buf, 0, sizeof(line_buf));
	sprintf(line_buf, "%s=%d\n", STR_KEY_STATUS, status);
	fwrite(line_buf, 1, strlen((const char*) line_buf), f);

	memset(line_buf, 0, sizeof(line_buf));
	sprintf(line_buf, "%s=%ld\n", STR_KEY_START_TIME, start_time);
	fwrite(line_buf, 1, strlen((const char*) line_buf), f);

	memset(line_buf, 0, sizeof(line_buf));
	sprintf(line_buf, "%s=%ld\n", STR_KEY_STOP_TIME, stop_time);
	fwrite(line_buf, 1, strlen((const char*) line_buf), f);

	ret = OC_SUCCESS;

	// close the file
	fclose(f);

	return ret;
}

/**
 * Load control password
 */
int load_control_password_from_file(char* src_file, char* password) {
	int ret = OC_FAILURE;

	char line_buf[256];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* line_data = NULL;
	int len = 0;
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: Open control auth file %s failed when load control password.\n",
				src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	// read line data
	ret = OC_SUCCESS;
	memset(line_buf, 0, sizeof(line_buf));
	while (fgets(line_buf, 256, f) != NULL) {
		len = strlen(line_buf);
		if (len < 3) {
			continue;
		}
		if (line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';
		if (strlen(line_buf) < 3) {
			continue;
		}

		line_data = str_trim(line_buf);
		len = strlen(line_data);
		if (len < 3) {
			continue;
		}

		//split parameter and value
		if (strchr(line_data, '=') == NULL) {
			// not found "="
			fprintf(stderr, "Error: Config content error.[%].\n", line_data);
			ret = OC_FAILURE;
			break;
		}

		sscanf(line_data, "%[^=]=%[^=]", param_name, param_value);

		if (strcmp(param_name, STR_KEY_PASSWORD) == 0) {
			if (strlen((const char*) param_value) > 0)
				strcpy(password, param_value);
			break;
		}
	}

	// close the file
	fclose(f);

	return ret;
}

/**
 * Load control remoteOperation flag
 */
int load_control_remoteOperation_from_file(char* src_file, int* enable_remote_operation) {
	int ret = OC_FAILURE;

	char line_buf[256];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char* line_data = NULL;
	int len = 0;
	FILE *f = NULL;

	// open the file
	f = fopen(src_file, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: Open control auth file %s failed when load control password.\n",
				src_file);
		return ret;
	}
	fseek(f, 0, SEEK_SET);

	// read line data
	ret = OC_SUCCESS;
	memset(line_buf, 0, sizeof(line_buf));
	while (fgets(line_buf, 256, f) != NULL) {
		len = strlen(line_buf);
		if (len < 3) {
			continue;
		}
		if (line_buf[len - 1] == '\n')
			line_buf[len - 1] = '\0';
		if (strlen(line_buf) < 3) {
			continue;
		}

		line_data = str_trim(line_buf);
		len = strlen(line_data);
		if (len < 3) {
			continue;
		}

		//split parameter and value
		if (strchr(line_data, '=') == NULL) {
			// not found "="
			fprintf(stderr, "Error: Config content error.[%].\n", line_data);
			ret = OC_FAILURE;
			break;
		}

		sscanf(line_data, "%[^=]=%[^=]", param_name, param_value);

		if (strcmp(param_name, STR_KEY_REMOTE_OPERATION) == 0) {
			if (strlen((const char*) param_value) > 0) {
				if (strcmp(param_value, "true") == 0)
					*enable_remote_operation = OC_TRUE;
				else
					*enable_remote_operation = OC_FALSE;
			} else {
				*enable_remote_operation = OC_FALSE;
			}
			break;
		}

	}
	// close the file
	fclose(f);

	return ret;

}
