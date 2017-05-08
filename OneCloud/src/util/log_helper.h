/*
 * log_helper.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef LOG_HELPER_H_
#define LOG_HELPER_H_


void log_error_print(int level, char* fmt, ...);

void log_warn_print(int level, char* fmt, ...);

void log_info_print(int level, char* fmt, ...);

void log_debug_print(int level, char* fmt, ...);

#endif /* LOG_HELPER_H_ */
