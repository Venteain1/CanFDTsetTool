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

/* 启动 SLCAN 解析线程（USB CDC0 作为 SLCAN 通道，CAN1 作为总线） */
void slcan_init(void);

/* 上位机是否已用 'O' 打开通道 */
rt_bool_t slcan_is_open(void);

/* 将一帧接收到的 CAN/CAN FD 报文按 SLCAN 格式发往主机，返回 1 表示已处理 */
int slcan_send_rx(const FDCAN_RxHeaderTypeDef *header, const uint8_t *data, uint8_t len);

#endif /* APPLICATIONS_SLCAN_SLCAN_H_ */
