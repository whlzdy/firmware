/*
 * string_helper.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef STRING_HELPER_H_
#define STRING_HELPER_H_

char* str_l_trim(char *buf);
char* str_r_trim(char *buf);
char* str_trim(char* buf);

/**
 * Simple JSON parse
 */
int simple_json_parse(char* json_buf, int json_len, CONFIG_STR_PAIR** out_pair, int* out_num);

/**
 * Get the JSON value
 * return:
 *      1  found the parameter by name
 *      0  not found.
 */
int str_get_json_field_value(CONFIG_STR_PAIR* params, int param_num, char* param_name, char* out_value);

#endif /* STRING_HELPER_H_ */


