/*
 * gpio.h
 *
 *  Created on: Mar 2, 2017
 *      Author: clouder
 */

#ifndef GPIO_H_
#define GPIO_H_

#include <sys/io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#define FLAG_ENABLE 1
#define FLAG_DISABLE 0
#define FLAG_UP 1
#define FLAG_DOWN 0

#define GPIO_MIN_ADDR 0x298
#define GPIO_MAX_ADDR 0x29A
#define GPIO_GROUP_SELECT 0x298
#define GPIO_IO_DIRECTION 0x299
#define GPIO_DATA 0x29A

#define GPIO_BIT_P0 0x01
#define GPIO_BIT_P1 0x02
#define GPIO_BIT_P2 0x04
#define GPIO_BIT_P3 0x08
#define GPIO_BIT_P4 0x10
#define GPIO_BIT_P5 0x20
#define GPIO_BIT_P6 0x40
#define GPIO_BIT_P7 0x80


#define GPIO_FLAG_IO_ALL_OUTPUT 0x00
#define GPIO_FLAG_IO_ALL_INPUT 0xFF

// P0&P1 as input (0000 0011)
#define GPIO_FLAG_IO_CONFIG 0x03

// button press time span 100ms
#define BTN_PRESS_TIME 100*20
// button press time release 500ms
#define BTN_RELEASE_DELAY_TIME 500*20

// led flash default timestamp 500ms
#define LED_FLASH_TIME_DEFAULT 500
// flash status retain time
#define LED_FLASH_RETAIN_TIME  500000

// timestamp for main loop, 5ms
#define GPIO_MAIN_TIMESTAMP 10

typedef struct led_status_control {
	int status;    // 0: off, 1:on, 2:flash
	int save_status; // 0:off, 1:on
	int flash_status; // 0:off, 1:on  (using at flash)
	int flash_span_time;
	int flash_retain_time;
	int flash_retain_counter;
	int flash_on_counter;
	int flash_off_counter;
} LED_STATUS;

typedef struct{
  int  flag; // 0:no message, 1:receive, 2:send
  uint32_t event;				//0:init, 1:startup, 2:shutdown, 3:catch error, 4:resolve error, 5:running, 6:stopped
  int  query; // 0:no query, 1:need query
}CONTROL_MESSAGE;

void init_led_status();
unsigned char led_status_update_process(LED_STATUS* dev_led, unsigned char oldData, unsigned char gpio_bit);
void set_led_flash(LED_STATUS* dev_led);
static void* thread_gpio_routine_handle(void*);
void control_led_status(uint32_t event);
static void* thread_gpio_send_handle(void*);
static void* thread_gpio_query_handle(void*);

uint32_t button_0;// 0: off, 1:on
uint32_t button_1;
LED_STATUS dev_led_1;
LED_STATUS dev_led_2;
LED_STATUS dev_led_3;
LED_STATUS dev_led_4;
CONTROL_MESSAGE receive_message;
CONTROL_MESSAGE send_message;
uint32_t io_status;			//32bit, one pin using 4bit

#endif /* GPIO_H_ */
