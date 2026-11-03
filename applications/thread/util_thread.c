/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-04-14     BZH1WX       the first version
 */
#include "util_thread.h"
#include "can.h"
#include <string.h>
#include <rthw.h>

static thread_entry_t thread_table[THREAD_MANAGER_MAX_COUNT];
static int thread_count = 0;

/* Nano 不支持动态内存：静态预分配线程控制块与栈区域 */
static struct rt_thread _thread_objs[THREAD_MANAGER_MAX_COUNT];
static rt_uint8_t       _thread_stacks[THREAD_MANAGER_MAX_COUNT][THREAD_MAX_STACK_SIZE];

/* 内部工具：添加条目（假定 name 不重复且有空间），失败返回 -1 成功返回索引 */
static int _add_thread_entry(rt_thread_t th, const char *name, void *stack_mem, rt_uint32_t stack_size)
{
    if (!th || !name) return -1;
    /* 查找第一个空槽 */
    for (int i = 0; i < THREAD_MANAGER_MAX_COUNT; i++)
    {
        if (thread_table[i].thread == RT_NULL)
        {
            memset(thread_table[i].name, 0, sizeof(thread_table[i].name));
            strncpy(thread_table[i].name, name, RT_NAME_MAX);
            thread_table[i].name[RT_NAME_MAX] = '\0';
            thread_table[i].thread = th;
            thread_table[i].stack = stack_mem;
            thread_table[i].stack_size = stack_size;
            if (i >= thread_count) thread_count = i + 1; /* 更新已用上限 */
            return i;
        }
    }
    return -1; /* 没空槽 */
}

/* 内部工具：按索引移除条目并压缩表，成功返回 0 失败返回 -1 */
static void _shrink_usage_bound(void)
{
    /* 回退 thread_count 到最后一个非空槽+1 */
    int last = -1;
    for (int i = 0; i < thread_count; i++)
    {
        if (thread_table[i].thread != RT_NULL) last = i;
    }
    thread_count = (last >= 0) ? (last + 1) : 0;
}

/* 内部工具：查找索引 */
static int _find_index(const char *name)
{
    if (!name) return -1;
    for (int i = 0; i < THREAD_MANAGER_MAX_COUNT; i++)
    {
        if (thread_table[i].thread && strncmp(thread_table[i].name, name, RT_NAME_MAX) == 0)
            return i;
    }
    return -1;
}

rt_thread_t find_thread(const char *name)
{
    int idx = _find_index(name);
    return (idx >= 0) ? thread_table[idx].thread : RT_NULL;
}

/* 创建线程 */
rt_err_t create_thread(void (*entry)(void *parameter),
                       const char *name,
                       rt_uint32_t stack_size,
                       rt_uint8_t priority,
                       rt_uint32_t timeslice)
{
    if (!entry || !name) return -RT_ERROR;
    if (_find_index(name) >= 0) return -RT_ERROR; /* 名称重复 */
    /* 判断是否有空槽 */
    int free_slot = -1;
    for (int i = 0; i < THREAD_MANAGER_MAX_COUNT; i++)
    {
        if (thread_table[i].thread == RT_NULL)
        {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) return -RT_EFULL;

    if (stack_size == 0) stack_size = THREAD_DEFAULT_STACK_SIZE;
    if (priority == 0) priority = THREAD_DEFAULT_PRIORITY;
    if (timeslice == 0) timeslice = THREAD_DEFAULT_TIMESLICE;

    /* 选择一个空槽位（与 thread_count 顺序一致） */
    struct rt_thread *thread_obj = &_thread_objs[free_slot];
    void *stack_mem = _thread_stacks[free_slot];
    if (stack_size > THREAD_MAX_STACK_SIZE) return -RT_ERROR;

    rt_err_t ret = rt_thread_init(thread_obj, name, entry, RT_NULL,
                                  stack_mem, stack_size, priority, timeslice);
    if (ret != RT_EOK) return ret;
    ret = rt_thread_startup(thread_obj);
    if (ret != RT_EOK) return ret;
    if (_add_thread_entry(thread_obj, name, stack_mem, stack_size) < 0)
    {
        rt_thread_detach(thread_obj);
        return -RT_ERROR;
    }
    return RT_EOK;
}

rt_err_t create_thread_param(void (*entry)(void *parameter),
                             void *parameter,
                             const char *name,
                             rt_uint32_t stack_size,
                             rt_uint8_t priority,
                             rt_uint32_t timeslice)
{
    if (!entry || !name) return -RT_ERROR;
    if (_find_index(name) >= 0) return -RT_ERROR; /* 名称重复 */
    int free_slot = -1;
    for (int i = 0; i < THREAD_MANAGER_MAX_COUNT; i++)
        if (thread_table[i].thread == RT_NULL) { free_slot = i; break; }
    if (free_slot < 0) return -RT_EFULL;

    if (stack_size == 0) stack_size = THREAD_DEFAULT_STACK_SIZE;
    if (priority == 0) priority = THREAD_DEFAULT_PRIORITY;
    if (timeslice == 0) timeslice = THREAD_DEFAULT_TIMESLICE;
    if (stack_size > THREAD_MAX_STACK_SIZE) return -RT_ERROR;

    struct rt_thread *thread_obj = &_thread_objs[free_slot];
    void *stack_mem = _thread_stacks[free_slot];
    rt_err_t ret = rt_thread_init(thread_obj, name, entry, parameter,
                                  stack_mem, stack_size, priority, timeslice);
    if (ret != RT_EOK) return ret;
    ret = rt_thread_startup(thread_obj);
    if (ret != RT_EOK) return ret;
    if (_add_thread_entry(thread_obj, name, stack_mem, stack_size) < 0)
    {
        rt_thread_detach(thread_obj);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/* 删除线程 */
rt_err_t delete_thread(const char *name)
{
    int idx = _find_index(name);
    if (idx < 0) return -RT_ERROR;
    rt_thread_t th = thread_table[idx].thread;
    if (!th) return -RT_ERROR;
    rt_err_t ret;

    ret = rt_thread_detach(th);
    /* Nano 模式下为静态内存，无需释放，标记指针空即可 */
    thread_table[idx].thread = RT_NULL;
    thread_table[idx].stack = RT_NULL;
    thread_table[idx].stack_size = 0;

    /* 不压缩，更新 usage bound */
    _shrink_usage_bound();
    return ret;
}

rt_err_t change_thread_attr(const char *name, rt_uint8_t priority, rt_uint32_t timeslice)
{
    int idx = _find_index(name);
    if (idx < 0) return -RT_ERROR;
    rt_thread_t th = thread_table[idx].thread;
    if (!th) return -RT_ERROR;

    /* 修改优先级：priority == 0 表示不改 */
    if (priority != 0)
    {
        rt_err_t ret = rt_thread_control(th, RT_THREAD_CTRL_CHANGE_PRIORITY, &priority);
        if (ret != RT_EOK) return ret;
    }

    /* 修改时间片：timeslice == 0 表示不改 */
    if (timeslice != 0)
    {
        rt_enter_critical();
        th->init_tick = timeslice;
        th->remaining_tick = timeslice;
        rt_exit_critical();
    }
    return RT_EOK;
}

rt_err_t recreate_thread(void (*entry)(void *parameter),
                         const char *name,
                         rt_uint32_t stack_size,
                         rt_uint8_t priority,
                         rt_uint32_t timeslice)
{
    delete_thread(name); /* 忽略错误，继续重建 */
    return create_thread(entry, name, stack_size, priority, timeslice);
}

int list_managed_threads(thread_entry_t *out_array, int max_count)
{
    if (!out_array || max_count <= 0) return 0;
    int written = 0;
    for (int i = 0; i < THREAD_MANAGER_MAX_COUNT && written < max_count; i++)
    {
        if (thread_table[i].thread)
        {
            out_array[written++] = thread_table[i];
        }
    }
    return written;
}