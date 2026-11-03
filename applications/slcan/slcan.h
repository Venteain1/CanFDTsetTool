/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SLCAN (LAWICEL ASCII) + CAN FD 扩展，兼容 CANgaroo / Cangaroo
 */
#ifndef APPLICATIONS_SLCAN_SLCAN_H_
#define APPLICATIONS_SLCAN_SLCAN_H_

#include <stdint.h>
#include <rtthread.h>

/* 启动两路 SLCAN 解析/发送线程（CDC0<->CAN1，CDC1<->CAN2） */
void slcan_init(void);

/* 指定总线（0 = CAN1，1 = CAN2）的 SLCAN 通道是否已被上位机打开 */
rt_bool_t slcan_is_open(int bus);

/* 该路因 USB 发送缓冲不足而丢弃的帧数 */
uint32_t slcan_tx_drop(int bus);

/* 该路成功送往上位机的帧数 */
uint32_t slcan_tx_count(int bus);

#endif /* APPLICATIONS_SLCAN_SLCAN_H_ */
