#ifndef APPLICATIONS_USB_TINY_CDC_H_
#define APPLICATIONS_USB_TINY_CDC_H_
#include <stdio.h>
#include <stdbool.h> /* bool 恢复 */
#include <rtthread.h>
#include "stm32g4xx_hal.h"


void tinyusb_cdc_init(void);      /* 初始化 TinyUSB CDC */
int  tinyusb_cdc_write(const uint8_t *data, rt_size_t len); /* 写数据 */
bool tinyusb_cdc_ready(void);     /* DTR/连接状态 */
/* SLCAN 数据通道（CDC0）用函数：不依赖 DTR，只看 USB 是否连接 */
bool tinyusb_cdc_connected(void);                           /* CDC0 是否已枚举连接 */
int  tinyusb_cdc_write_force(const uint8_t *data, rt_size_t len); /* CDC0 写数据 */
int  tinyusb_cdc_read(uint8_t *data, rt_size_t len);        /* CDC0 读数据 */
void tinyusb_cdc_flush_input(void);                         /* 丢弃 CDC0 接收缓冲 */
/* 控制台usb传输用函数 */
int  tinyusb_cdc_console_write(const uint8_t *str, rt_size_t len); /* 控制台写数据 */
int  tinyusb_cdc_console_getchar(void); /* 控制台读字符 */
bool tinyusb_cdc_console_ready(void); /* 控制台DTR/连接状态 */

#endif /* APPLICATIONS_USB_TINY_CDC_H_ */
