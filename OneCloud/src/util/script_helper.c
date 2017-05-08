/*
 * script_helper.c
 *
 *  Created on: Mar 13, 2017
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
#include "util/script_helper.h"
#include "util/log_helper.h"

/**
 * Execute shell script
 */
int script_shell_exec(unsigned char* script_file, unsigned char* result) {
	if (script_file == NULL || strlen((const char*) script_file) < 1 || result == NULL)
		return OC_FAILURE;

	FILE * fp = NULL;
	int ret = OC_SUCCESS;

	char buffer[MAX_CONSOLE_BUFFER];
	memset((void*) buffer, 0, MAX_CONSOLE_BUFFER);

	fp = popen((const char*) script_file, "r");
	fgets(buffer, sizeof(buffer), fp);
	pclose(fp);

	log_debug_print(OC_DEBUG_LEVEL_DEFAULT, "Execute script %s, return %s", script_file,
			buffer);

	if (strlen(buffer) > 0)
		strcpy((char*) result, (const char*) buffer);
	else
		ret = OC_FAILURE;

	return ret;
}
