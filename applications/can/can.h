/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-03-04     BZH1WX       the first version
 * 2025-11-03     Venteain     增加 SLCAN 发送接口与运行时位时序配置
 */
#ifndef APPLICATIONS_CAN_H_
#define APPLICATIONS_CAN_H_
#include <rtthread.h>
#include <stdio.h>

/* 一帧用户层报文：经典帧与 CAN FD 帧通用 */
typedef struct
{
    uint32_t id;        /* 11 位或 29 位标识符 */
    uint8_t  extended;  /* 1: 扩展帧 */
    uint8_t  remote;    /* 1: 远程帧 */
    uint8_t  fd;        /* 1: CAN FD 帧 */
    uint8_t  brs;       /* 1: 数据段切换波特率 */
    uint8_t  len;       /* 0~8 或 12/16/20/24/32/48/64 */
    uint8_t  data[64];
} can_user_msg_t;

void can_init(void);
void can_tx_test_message(void);

/* DLC 码（0~15）与字节数互转，can_len_to_dlc 对非法长度返回 0xFF */
uint8_t can_dlc_to_len(uint8_t dlc_code);
uint8_t can_len_to_dlc(uint8_t len);

/* bus: 0 = CAN1，1 = CAN2；在两路上发送一帧报文（SLCAN 通道） */
rt_err_t can_bus_send(int bus, const can_user_msg_t *msg);

/* 重新配置指定总线的仲裁段/数据段速率，listen_only 为 RT_TRUE 时进入只听模式 */
rt_err_t can_bus_set_timing(int bus, uint32_t nominal_bps, uint32_t data_bps, rt_bool_t listen_only);

#endif /* APPLICATIONS_CAN_H_ */
