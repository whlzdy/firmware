/*
 * http_sign.c
 *
 *  Created on: Mar 14, 2017
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

#include "common/config.h"
#include "common/onecloud.h"
#include "util/http_helper.h"

#define MAX_SIGN_PARAM_VALUE_LEN 1280
typedef struct sign_string_pair {
	char name[MAX_PARAM_NAME_LEN];
	char value[MAX_SIGN_PARAM_VALUE_LEN];
} SIGN_STR_PAIR;


void helper() {
	fprintf(stderr, "usage: http_sign access_secret param1=value1 param2=value2 ....\n");
}

int compare(const void *i, const void *j) {
	return strcmp(*(char **) i, *(char **) j);
}



/**
 * HTTP sign tool. only support simple parameter. not support array value.
 *
 * example: bin/http_sign pppppppp accessKey=HHHHHHHH xxx=yyy time=1489482980456
 *          1318a359d6674dfaab59ec7cbd627629
 *
 */
int main(int argc, char **argv) {

	if (argc > 1) {
		if (strcmp("help", argv[1]) == 0 || strcmp("-h", argv[1]) == 0
				|| strcmp("--help", argv[1]) == 0) {
			helper();
			exit(EXIT_FAILURE);
		}
	} else {
		helper();
		exit(EXIT_FAILURE);
	}

	int param_count = argc - 2;
	char* access_secret = argv[1];
	char* pair_data = NULL;
	int i = 0, j = 0;
	char param_name[MAX_PARAM_NAME_LEN];
	char param_value[MAX_SIGN_PARAM_VALUE_LEN];
	char temp[4000];
	char buffer[4000];

	SIGN_STR_PAIR* paramPair = (SIGN_STR_PAIR*) malloc(sizeof(SIGN_STR_PAIR) * param_count);
	memset((void*) paramPair, 0, sizeof(CONFIG_STR_PAIR) * param_count);
	char* sort_str[100];

	// initialize the parameter array
	for (i = 2; i < argc; i++) {
		memset(param_name, 0, sizeof(param_name));
		memset(param_value, 0, sizeof(param_value));
		pair_data = argv[i];
		sscanf(pair_data, "%[^=]=%[^=]", param_name, param_value);
		//printf("%s=%s\n", param_name, param_value);

		strcpy(paramPair[i - 2].name, param_name);
		strcpy(paramPair[i - 2].value, param_value);
		sort_str[i - 2] = paramPair[i - 2].name;
	}

	// sort the parameters ASC
	qsort(sort_str, param_count, sizeof(char *), compare);

	// construct the query string for sign
	memset(temp, 0, sizeof(temp));
	for (i = 0; i < param_count; i++) {
		for (j = 0; j < param_count; j++) {
			if (strcmp(sort_str[i], paramPair[j].name) == 0) {
				strcat(temp, paramPair[j].name);
				strcat(temp, paramPair[j].value);
				break;
			}
		}
		//printf("%s\n", sort_str[i]);
	}
	memset(buffer, 0, sizeof(buffer));

	sprintf(buffer, "%s%s%s", access_secret, temp, access_secret);
	//printf("%s\n", buffer);

	memset(temp, 0, sizeof(temp));
	md5((unsigned char*)buffer, (unsigned char*)temp);
	printf("%s\n", temp);

	if(paramPair != NULL)
		free(paramPair);

	return EXIT_SUCCESS;
}

