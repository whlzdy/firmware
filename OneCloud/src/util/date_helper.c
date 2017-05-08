/*
 * date_helper.c
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <string.h>


/**
 * Get current time
 */
void helper_get_current_time_str(char* out_buf, int out_buf_size, char* fmt) {
	char* format = fmt;

	if (format == NULL)
		format = "%Y-%m-%d_%H:%M:%S";

	memset(out_buf, 0, out_buf_size);
	time_t now = time(NULL);
	strftime(out_buf, 32, format, localtime(&now)); //format date and time.
}
