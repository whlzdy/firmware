/*
 * log_helper.c
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
#include <stdarg.h>

#include "common/onecloud.h"
#include "common/config.h"
#include "common/protocol.h"
#include "common/net.h"

/**
 * Print log
 */
void log_printf(const char *fmt, ...) {
	static char sprint_buf[1024];
	va_list args;
	int n;
	va_start(args, fmt);
	n = vsprintf(sprint_buf, fmt, args);
	va_end(args);
	write(1, sprint_buf, n);
}

/**
 * Print error log
 * 	log_error_print(g_debug_verbose, "");
 *
 */
void log_error_print(int level, char* fmt, ...) {
	if (level >= OC_DEBUG_LEVEL_ERROR) {
		static char sprint_buf[1024];

		sprint_buf[0] = '\0';
		char* prefix = "Error: ";
		strcat(sprint_buf, prefix);

		va_list args;
		int n;
		va_start(args, fmt);
		n = vsprintf(sprint_buf + strlen(prefix), fmt, args);
		va_end(args);
		strcat(sprint_buf, "\n");

		write(1, sprint_buf, n + 8);
	}
}

/**
 * Print warn log
 * 	log_warn_print(g_debug_verbose, "");
 *
 */
void log_warn_print(int level, char* fmt, ...) {
	if (level >= OC_DEBUG_LEVEL_WARN) {
		static char sprint_buf[1024];

		sprint_buf[0] = '\0';
		char* prefix = "Warn: ";
		strcat(sprint_buf, prefix);

		va_list args;
		int n;
		va_start(args, fmt);
		n = vsprintf(sprint_buf + strlen(prefix), fmt, args);
		va_end(args);
		strcat(sprint_buf, "\n");

		write(1, sprint_buf, n + 7);
	}
}

/**
 * Print info log
 * 	log_info_print(g_debug_verbose, "");
 *
 */
void log_info_print(int level, char* fmt, ...) {
	if (level >= OC_DEBUG_LEVEL_INFO) {
		static char sprint_buf[1024];

		sprint_buf[0] = '\0';
		char* prefix = "Info: ";
		strcat(sprint_buf, prefix);

		va_list args;
		int n;
		va_start(args, fmt);
		n = vsprintf(sprint_buf + strlen(prefix), fmt, args);
		va_end(args);
		strcat(sprint_buf, "\n");

		write(1, sprint_buf, n + 7);
	}
}

/**
 * Print debug log
 * 	log_debug_print(g_debug_verbose, "");
 *
 */
void log_debug_print(int level, char* fmt, ...) {
	if (level >= OC_DEBUG_LEVEL_DEBUG) {
		static char sprint_buf[1024];

		sprint_buf[0] = '\0';
		char* prefix = "Debug: ";
		strcat(sprint_buf, prefix);

		va_list args;
		int n;
		va_start(args, fmt);
		n = vsprintf(sprint_buf + strlen(prefix), fmt, args);
		va_end(args);
		strcat(sprint_buf, "\n");

		write(1, sprint_buf, n + 8);
	}
}
