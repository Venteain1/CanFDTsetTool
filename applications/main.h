/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-14     BZH1WX       the first version
 */
#ifndef APPLICATIONS_MAIN_H_
#define APPLICATIONS_MAIN_H_

#include <rtthread.h>
#include "can.h"
#include "usb.h"
#include "slcan.h"
#include "util_thread.h"

typedef void (*start_func_t)(void);

static start_func_t functions[] = {
    can_init,
    usb_component_init,
    slcan_init,
    NULL
};
#endif /* APPLICATIONS_MAIN_H_ */
