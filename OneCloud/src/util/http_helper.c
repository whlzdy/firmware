/*
 * http_helper.c
 *
 *  Created on: Mar 17, 2017
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
#include "util/md5.h"
#include "util/log_helper.h"


void md5(unsigned char* in_buff, unsigned char* out_buff) {
	int i;
	unsigned char crypt[16];
	char temp[3];
	char buffer[33];
	MD5_CTX md5;
	MD5Init(&md5);
	MD5Update(&md5, in_buff, strlen( (const char*)in_buff));
	MD5Final(&md5, crypt);

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 16; i++) {
		sprintf(temp, "%02x", crypt[i]);
		strcat(buffer, temp);
	}
	memcpy(out_buff, buffer, sizeof(buffer));
}

/**
 * Construct a signed web URL
 */
int make_sign_web_url(char* original_url, long timestamp, char* access_key, char* access_secret, char* new_url)
{
	int result = OC_SUCCESS;

	char temp[256];
	char md5_buf[256];

	memset((void*)temp, 0, sizeof(temp));
	sprintf(temp,"%saccessKey%stime%ld%s",access_secret,access_key, timestamp,access_secret);

	memset(md5_buf, 0, sizeof(md5_buf));
	md5((unsigned char*)temp, (unsigned char*)md5_buf);

	memset((void*)temp, 0, sizeof(temp));
	sprintf(temp,"%s?accessKey=%s&time=%ld&sign=%s",original_url,access_key, timestamp,md5_buf);

	strcpy(new_url, (const char*)temp);
	return result;
}
