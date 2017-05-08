/*
 * error.c
 *
 *  Created on: Mar 16, 2017
 *      Author: clouder
 */

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/error.h"

static char* const executemssage[] = { "finish",
                                       "success",
                                       "failure",
                                       "initialize",
                                       "processing",
                                       "checking" };
char *exec_msg_text(enum exec_message msg) {
    return executemssage[msg];
}


static char* const errormssage[] = { "no_error",
									   "start_disk_array_fail",
                                       "start_disk_array_success",
                                       "stop_disk_array_fail",
                                       "stop_disk_array_success",

                                       "start_primary_server_fail",
                                       "start_primary_server_success",
                                       "stop_primary_server_fail",
                                       "stop_primary_server_success",

                                       "start_compute_host_fail",
                                       "start_compute_host_success",
                                       "stop_compute_host_fail",
                                       "stop_compute_host_success",

                                       "start_cloud_platform_fail",
                                       "start_cloud_platform_success",
                                       "stop_cloud_platform_fail",
                                       "stop_cloud_platform_success",

                                       "start_application_fail",
                                       "start_application_success",
                                       "stop_application_fail",
                                       "stop_application_success",

                                       "stop_system_vm_fail",
                                       "stop_system_vm_success",

                                       "router_offline",
                                       "switch_offline",
                                       "firewall_offline",
                                       "ups_offline"
};
char *error_msg_text(enum error_message msg) {
    return errormssage[msg];
}

