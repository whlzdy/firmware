/*
 * thread_helper.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef THREAD_HELPER_H_
#define THREAD_HELPER_H_

#include <stdint.h>

/**
 * Create a custom thread.
 * return: 0, if success,
 *         -1, if error (errno is set)
 */
int create_custom_thread(pthread_t  *pthread_id, // Thread identifier
        	                    void       *(*func_entry)(void *),
        	                    void       *thread_param,  // Thread parameters
        	                    size_t      stack_sz);


#endif /* THREAD_HELPER_H_ */
