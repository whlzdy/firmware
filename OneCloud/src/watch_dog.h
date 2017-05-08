/*
 * watch_dog.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef WATCH_DOG_H_
#define WATCH_DOG_H_


#define WATCHDOG_MIN_ADDR 0x29D
#define WATCHDOG_MAX_ADDR 0x29E
#define WATCHDOG_MODE 0x29D
#define WATCHDOG_COUNTER 0x29E


#define WG_SHELL_RESTART_MAIN_DAEMON "/opt/onecloud/script/watchdog/restart_main_daemon.sh"
#define WG_SHELL_RESTART_APP_SERVICE "/opt/onecloud/script/watchdog/restart_app_service.sh"
#define WG_SHELL_RESTART_GPIO_DAEMON "/opt/onecloud/script/watchdog/restart_gpio_daemon.sh"
#define WG_SHELL_RESTART_SCRIPT_DAEMON "/opt/onecloud/script/watchdog/restart_script_daemon.sh"
#define WG_SHELL_RESTART_SIM_DAEMON "/opt/onecloud/script/watchdog/restart_sim_daemon.sh"
#define WG_SHELL_RESTART_TEMPERATURE_DAEMON "/opt/onecloud/script/watchdog/restart_temperature_daemon.sh"
#define WG_SHELL_RESTART_COM0_DAEMON "/opt/onecloud/script/watchdog/restart_com0_daemon.sh"
#define WG_SHELL_RESTART_COM1_DAEMON "/opt/onecloud/script/watchdog/restart_com1_daemon.sh"
#define WG_SHELL_RESTART_COM2_DAEMON "/opt/onecloud/script/watchdog/restart_com2_daemon.sh"
#define WG_SHELL_RESTART_COM3_DAEMON "/opt/onecloud/script/watchdog/restart_com3_daemon.sh"
#define WG_SHELL_RESTART_COM4_DAEMON "/opt/onecloud/script/watchdog/restart_com4_daemon.sh"
#define WG_SHELL_RESTART_COM5_DAEMON "/opt/onecloud/script/watchdog/restart_com5_daemon.sh"

#endif /* WATCH_DOG_H_ */
