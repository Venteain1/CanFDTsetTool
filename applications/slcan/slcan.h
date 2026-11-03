/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SLCAN (LAWICEL ASCII) + CAN FD 扩展，兼容 CANgaroo / Cangaroo
 */
#ifndef APPLICATIONS_SLCAN_SLCAN_H_
#define APPLICATIONS_SLCAN_SLCAN_H_

#include <rtthread.h>
#include "stm32g4xx_hal.h"

/* 启动两路 SLCAN 解析线程（CDC0<->CAN1，CDC1<->CAN2） */
void slcan_init(void);

/* 指定总线（0 = CAN1，1 = CAN2）的 SLCAN 通道是否已被上位机打开 */
rt_bool_t slcan_is_open(int bus);

/* 将一帧接收到的 CAN/CAN FD 报文按 SLCAN 格式发往主机，返回 1 表示已处理 */
int slcan_send_rx(int bus, const FDCAN_RxHeaderTypeDef *header, const uint8_t *data, uint8_t len);

#endif /* APPLICATIONS_SLCAN_SLCAN_H_ */
