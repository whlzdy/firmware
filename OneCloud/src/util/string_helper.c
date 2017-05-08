/*
 * string_helper.c
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

#include "common/onecloud.h"
#include "common/config.h"

char* str_l_trim(char *buf) {
	int len, i;
	char *tmp = NULL;
	len = strlen(buf);
	tmp = (char*) malloc(len);

	memset(tmp, 0x00, len);
	for (i = 0; i < len; i++) {
		if (buf[i] != ' ')
			break;
	}
	if (i < len) {
		strncpy(tmp, (buf + i), (len - i));
	}
	strncpy(buf, tmp, len);
	free(tmp);
	return buf;
}

char* str_r_trim(char *buf) {
	int len, i;
	char *tmp = NULL;
	len = strlen(buf);
	tmp = (char *) malloc(len);
	memset(tmp, 0x00, len);
	for (i = 0; i < len; i++) {
		if (buf[len - i - 1] != ' ')
			break;
	}
	if (i < len) {
		strncpy(tmp, buf, len - i);
	}
	strncpy(buf, tmp, len);
	free(tmp);
	return buf;
}

char* str_trim(char* buf) {
	if (buf == NULL)
		return NULL;
	return (str_r_trim(str_l_trim(buf)));
}

char* str_replace_json_double_quotes(char* buf) {
	int len, i;
	char *tmp = NULL;
	len = strlen(buf);
	tmp = (char *) malloc(len + 1);
	memset(tmp, 0x00, len + 1);
	int start_offset = -1;
	int end_offset = -1;

	// find the " forward
	for (i = 0; i < len; i++) {
		if (buf[i] == '\"') {
			start_offset = i + 1;
			break;
		}
	}
	// find the " backward
	for (i = len - 1; i > -1; i--) {
		if (buf[i] == '\"') {
			end_offset = i - 1;
			break;
		}
	}
	if (start_offset == end_offset) {
		// not found two "
		strcpy(tmp, buf);
	} else {
		strncpy(tmp, buf + start_offset, (end_offset - start_offset + 1));
	}

	tmp = str_trim(tmp);

	strncpy(buf, tmp, len);
	free(tmp);
	return buf;
}

/**
 * Simple JSON parse
 */
int simple_json_parse(char* json_buf, int json_len, CONFIG_STR_PAIR** out_pair, int* out_num) {
	if (json_buf == NULL || json_len < 2)
		return OC_FAILURE;

	int result = OC_SUCCESS;
	int i = 0, j = 0;

	int start_offset = -1;
	int end_offset = -1;

	// find the "{"
	for (i = 0; i < json_len; i++) {
		if (json_buf[i] == '\{') {
			start_offset = i + 1;
			break;
		}
	}
	// find the "}"
	for (i = json_len - 1; i > -1; i--) {
		if (json_buf[i] == '\}') {
			end_offset = i - 1;
			break;
		}
	}
	if (start_offset == -1 || end_offset == -1)
		return OC_FAILURE;

	int effect_len = end_offset - start_offset + 1;
	if (effect_len <= 0)
		return OC_SUCCESS;

	char* buffer = (char*) malloc(effect_len + 1);
	memset((void*) buffer, 0, effect_len + 1);
	strncpy(buffer, (const char*) (json_buf + start_offset), effect_len);

	// divide the string by ","
	int is_pass = OC_FALSE;
	int n_counter = 0;
	start_offset = 0;
	end_offset = -1;
	CONFIG_STR_PAIR temp_pair[100];
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_PARAM_VALUE_LEN];
	char temp_buf[160];
	for (i = 0; i < effect_len; i++) {
		if (buffer[i] == '\"') {
			is_pass = (is_pass == OC_TRUE ? OC_FALSE : OC_TRUE);
			continue;
		}
		if (buffer[i] == '\,' && is_pass == OC_FALSE) {
			end_offset = i - 1;
		}

		if (end_offset != -1) {
			memset(temp_buf, 0, sizeof(temp_buf));
			memset(param_name, 0, sizeof(param_name));
			memset(param_value, 0, sizeof(param_value));
			strncpy(temp_buf, (const char*) (buffer + start_offset),
					(end_offset - start_offset + 1));
			sscanf(temp_buf, "%[^:]:%[^:]", param_name, param_value);
			strcpy(temp_pair[n_counter].name, param_name);
			strcpy(temp_pair[n_counter].value, param_value);
			n_counter++;

			start_offset = i + 1;
			end_offset = -1;
		}
	}
	// process last part
	if (i == effect_len && end_offset == -1 && start_offset < (effect_len - 1)) {
		end_offset = effect_len - 1;
		memset(temp_buf, 0, sizeof(temp_buf));
		memset(param_name, 0, sizeof(param_name));
		memset(param_value, 0, sizeof(param_value));
		strncpy(temp_buf, (const char*) (buffer + start_offset), (end_offset - start_offset + 1));
		sscanf(temp_buf, "%[^:]:%[^:]", param_name, param_value);
		strcpy(temp_pair[n_counter].name, param_name);
		strcpy(temp_pair[n_counter].value, param_value);
		n_counter++;
	}

	if (n_counter > 0) {
		CONFIG_STR_PAIR* params = (CONFIG_STR_PAIR*) malloc(n_counter * sizeof(CONFIG_STR_PAIR));
		memset((void*) params, 0, n_counter * sizeof(CONFIG_STR_PAIR));

		for (i = 0; i < n_counter; i++) {
			str_replace_json_double_quotes(temp_pair[i].name);
			str_replace_json_double_quotes(temp_pair[i].value);
			strcpy(params[i].name, temp_pair[i].name);
			strcpy(params[i].value, temp_pair[i].value);
		}
		*out_pair = params;
		*out_num = n_counter;
	}

	if (buffer != NULL)
		free(buffer);

	return result;
}

/**
 * Get the JSON value
 * return:
 *      1  found the parameter by name
 *      0  not found.
 */
int str_get_json_field_value(CONFIG_STR_PAIR* params, int param_num, char* param_name,
		char* out_value) {
	if (params == NULL || param_num < 1)
		return OC_FALSE;

	int found = OC_FALSE;
	int i = 0;

	for (i = 0; i < param_num; i++) {
		if (strcmp(param_name, params[i].name) == 0) {
			found = OC_TRUE;
			strcpy(out_value, params[i].value);
		}
	}

	return found;
}
/*
 int main(int argc, char **argv) {
 //char* json = "{\"uuid\":\"111111 111\", \"poid\":\"aaaaaaa\",\"operation\":\"start\", \"password\":\"xxxxx\" ,\"time\":123456677}";
 char* json = "{xxxx:test}";
 int json_len = strlen((const char*)json) + 1;

 CONFIG_STR_PAIR* params = NULL;
 int param_num = 0;

 int result = simple_json_parse(json, json_len, &params, &param_num);

 printf("parameter num = %d\n", param_num);
 int i = 0;

 for(i=0; i<param_num; i++) {
 printf("%s : %s\n", params[i].name, params[i].value);
 }

 if(params != NULL)
 free(params);

 return EXIT_SUCCESS;
 }
 */
