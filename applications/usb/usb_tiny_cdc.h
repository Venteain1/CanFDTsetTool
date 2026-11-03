#ifndef APPLICATIONS_USB_TINY_CDC_H_
#define APPLICATIONS_USB_TINY_CDC_H_
#include <stdio.h>
#include <stdbool.h> /* bool 恢复 */
#include <rtthread.h>
#include "stm32g4xx_hal.h"

/* CDC 接口分配：
 *   CDC0 -> SLCAN / CAN1
 *   CDC1 -> SLCAN / CAN2
 *   CDC2 -> 控制台（finsh + 日志）
 */
#define TUSB_ITF_CAN1       0
#define TUSB_ITF_CAN2       1
#define TUSB_ITF_CONSOLE    2

void tinyusb_cdc_init(void);      /* 初始化 TinyUSB CDC */
int  tinyusb_cdc_write(const uint8_t *data, rt_size_t len); /* 写数据 */
bool tinyusb_cdc_ready(void);     /* DTR/连接状态 */

/* 通用接口：SLCAN 数据通道用，不依赖 DTR，只看 USB 是否连接 */
bool tinyusb_cdc_n_connected(uint8_t itf);
int  tinyusb_cdc_n_write(uint8_t itf, const uint8_t *data, rt_size_t len);
int  tinyusb_cdc_n_read(uint8_t itf, uint8_t *data, rt_size_t len);
void tinyusb_cdc_n_flush(uint8_t itf);

/* 批量发送：先 write_raw 多次，再统一 flush */
int  tinyusb_cdc_n_write_raw(uint8_t itf, const uint8_t *data, rt_size_t len);
void tinyusb_cdc_n_flush_tx(uint8_t itf);
uint32_t tinyusb_cdc_n_write_available(uint8_t itf);

/* CDC0 便捷封装 */
bool tinyusb_cdc_connected(void);
int  tinyusb_cdc_write_force(const uint8_t *data, rt_size_t len);
int  tinyusb_cdc_read(uint8_t *data, rt_size_t len);
void tinyusb_cdc_flush_input(void);

/* 控制台（CDC2） */
int  tinyusb_cdc_console_write(const uint8_t *str, rt_size_t len);
int  tinyusb_cdc_console_getchar(void);
bool tinyusb_cdc_console_ready(void);

#endif /* APPLICATIONS_USB_TINY_CDC_H_ */
