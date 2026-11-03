/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-08     RT-Thread    first version
 */
#include "main.h"
#include <rtdbg.h>
#define DBG_TAG "MAIN"
#define DBG_LVL DBG_LOG

void run_on_start();

int main(void)
{
    run_on_start();

    LOG_I("Already running...");

    while(1){
        rt_thread_mdelay(1000);
    }

    return RT_EOK;
}

void run_on_start(){
    for (int i = 0; functions[i] != NULL; i++) {
        functions[i]();
    }
}
