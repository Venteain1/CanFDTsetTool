/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-14     BZH1WX       the first version
 */
#ifndef APPLICATIONS_THREAD_THREAD_H_
#define APPLICATIONS_THREAD_THREAD_H_

#include <rtthread.h>

/* 默认配置 */
#define THREAD_DEFAULT_STACK_SIZE 512
#define THREAD_DEFAULT_PRIORITY 25
#define THREAD_DEFAULT_TIMESLICE 20

#define THREAD_MAX_STACK_SIZE 2048

/* 线程管理最大数量，可以根据需要调整 */
#ifndef THREAD_MANAGER_MAX_COUNT
#define THREAD_MANAGER_MAX_COUNT 8
#endif

/* 线程管理条目 */
typedef struct thread_entry
{
	char        name[RT_NAME_MAX + 1]; /* 线程名 */
    rt_thread_t thread;                /* 线程对象指针 */
    void       *stack;                 /* 动态分配的栈指针 */
    rt_uint32_t stack_size;            /* 栈大小 */
} thread_entry_t;

/* 创建线程（Nano: 使用静态控制块+静态栈）：
 * 如果 priority/timeslice/stack_size 为 0 使用默认值；stack_size 不能超过 THREAD_MAX_STACK_SIZE
 * 成功返回 RT_EOK，失败返回负错误码 */
rt_err_t create_thread(void (*entry)(void *parameter),
                       const char *name,
                       rt_uint32_t stack_size,
                       rt_uint8_t priority,
                       rt_uint32_t timeslice);

/* 创建线程（带参数）：与 create_thread 相同但可传入用户 parameter 指针/数值
 * 成功返回 RT_EOK，失败返回负错误码 */
rt_err_t create_thread_param(void (*entry)(void *parameter),
                             void *parameter,
                             const char *name,
                             rt_uint32_t stack_size,
                             rt_uint8_t priority,
                             rt_uint32_t timeslice);

/* 查找线程 */
rt_thread_t find_thread(const char *name);

/* 删除线程（同时从管理结构移除），返回 RT_EOK 或错误码 */
rt_err_t delete_thread(const char *name);

/* 修改线程属性：可同时修改优先级与时间片
 * 若 priority 传入 0 表示保持原优先级不变；若 timeslice 传入 0 表示保持原时间片不变
 */
rt_err_t change_thread_attr(const char *name, rt_uint8_t priority, rt_uint32_t timeslice);

/* 重建线程：删除后按新参数创建，返回 create_thread 的结果 */
rt_err_t recreate_thread(void (*entry)(void *parameter),
                         const char *name,
                         rt_uint32_t stack_size,
                         rt_uint8_t priority,
                         rt_uint32_t timeslice);

/* 列出所有当前管理的线程数量 */
int list_managed_threads(thread_entry_t *out_array, int max_count);


#endif /* APPLICATIONS_THREAD_THREAD_H_ */
