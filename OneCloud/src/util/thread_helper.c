/*
 * thread_helper.c
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "util/thread_helper.h"
#include "util/log_helper.h"


/**
 * Create a custom thread.
 * return: 0, if success,
 *         -1, if error (errno is set)
 */
int create_custom_thread(pthread_t  *pthread_id, // Thread identifier
        	                    void       *(*func_entry)(void *),
        	                    void       *thread_param,  // Thread parameters
        	                    size_t      stack_sz)
{
	pthread_attr_t	attr;
	int	ret = 0;
	int	err_save;

	// Check the parameters
	if (!pthread_id) {
		fprintf(stderr, "NULL thread id\n");
		errno = EINVAL;
		return -1;
	}

	memset(&attr, 0, sizeof(attr));

	errno = pthread_attr_init(&attr);
	if (0 != errno)	{
		err_save = errno;
		fprintf(stderr, "Error: pthread_attr_init() failed (errno = %d)\n", errno);
		errno = err_save;
		return -1;
	}

	errno = pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	if (0 != errno) {
		err_save = errno;
		fprintf(stderr, "Error: pthread_attr_setscope() failed (errno = %d)\n", errno);
		errno = err_save;
		ret = -1;
		goto cust_thread_err;
	}

	errno = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (0 != errno) {
		err_save = errno;
		fprintf(stderr, "Error: pthread_attr_setdetachstate() failed (errno = %d)\n", errno);
		errno = err_save;
		ret = -1;
		goto cust_thread_err;
	}

	// Set the stack size
	errno = pthread_attr_setstacksize(&attr, stack_sz);
	if (0 != errno)	{
		err_save = errno;
		fprintf(stderr, "Error: Error %d on pthread_attr_setstacksize()\n", errno);
		errno = err_save;
		ret = -1;
		goto cust_thread_err;
	}

	// Thread creation
	errno = pthread_create(pthread_id, &attr, func_entry, thread_param);
	if (0 != errno)	{
		err_save = errno;
		fprintf(stderr, "Error: pthread_create() failed (errno = %m - %d)\n", errno);
		errno = err_save;
		ret = -1;
		goto cust_thread_err;
	}

	goto cust_thread_ok;

cust_thread_err:

cust_thread_ok:

	// The following calls will alter errno
	err_save = errno;

	errno = pthread_attr_destroy(&attr);
	if (0 != errno) {
		fprintf(stderr, "Error: pthread_attr_destroy() failed (errno = %d)\n", errno);
		ret = -1;
	}

	errno = err_save;

	return ret;
}
