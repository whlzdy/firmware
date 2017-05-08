/*
 * http_helper.h
 *
 *  Created on: Mar 17, 2017
 *      Author: clouder
 */

#ifndef HTTP_HELPER_H_
#define HTTP_HELPER_H_

void md5(unsigned char* in_buff, unsigned char* out_buff);
/**
 * Construct a signed web URL
 */
int make_sign_web_url(char* original_url, long timestamp, char* access_key, char* access_secret, char* new_url);

#endif /* HTTP_HELPER_H_ */
