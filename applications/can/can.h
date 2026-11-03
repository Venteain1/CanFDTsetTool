/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-03-04     BZH1WX       the first version
 */
#ifndef APPLICATIONS_CAN_H_
#define APPLICATIONS_CAN_H_
#include <rtthread.h>
#include <stdio.h>

void can_init();
void can_readmessage(void);
void can_sendmessage(uint32_t id, uint8_t *data, uint8_t len);
void can_poll_readmessage();

#endif /* APPLICATIONS_CAN_H_ */
