/*
 * error.h
 *
 *  Created on: Mar 16, 2017
 *      Author: clouder
 */

#ifndef ERROR_H_
#define ERROR_H_

// Error Code for startup/shutdown operation
enum error_code {
    errcode_parameters_invalid = 400,
    errcode_auth_failed = 401,
    errcode_remote_operation_forbidden = 403,
    errcode_invalid_poid = 404,
    errcode_powerone_is_locked = 423,
    errcode_powerone_is_busy = 429,
    errcode_one_control_inner_error = 500,
    errcode_one_control_offline = 503,
    errcode_operation_timeout = 504,
    errcode_diskarray_startup_failed = 600,
    errcode_primary_server_startup_failed = 610,
    errcode_secondary_server_startup_failed = 611,
    errcode_cloud_platform_startup_failed = 620,
    errcode_primary_server_shutdown_failed = 710,
    errcode_secondary_server_shutdown_failed = 711,
    errcode_cloud_platform_shutdown_failed = 720,
    errcode_systemvm_shutdown_failed = 721
};


enum exec_message {
    exec_finish,
    exec_success,
    exec_failure,
    exec_initialize,
    exec_processing,
    exec_checking
};

enum error_message {
	error_no_error,
    error_start_diskarray_fail,
    error_start_diskarray_success,
    error_stop_diskarray_fail,
    error_stop_diskarray_success,
    error_start_primary_server_fail,
    error_start_primary_server_success,
    error_stop_primary_server_fail,
    error_stop_primary_server_success,
    error_start_compute_host_fail,
    error_start_compute_host_success,
    error_stop_compute_host_fail,
    error_stop_compute_host_success,
    error_start_cloud_platform_fail,
    error_start_cloud_platform_success,
    error_stop_cloud_platform_fail,
    error_stop_cloud_platform_success,
    error_start_application_fail,
    error_start_application_success,
    error_stop_application_fail,
    error_stop_application_success,

    error_stop_system_vm_fail,
    error_stop_system_vm_success,

    error_router_offline,
    error_switch_offline,
    error_firewall_offline,
    error_ups_offline
};

char *exec_msg_text(enum exec_message msg);
char *error_msg_text(enum error_message msg);

#endif /* ERROR_H_ */
