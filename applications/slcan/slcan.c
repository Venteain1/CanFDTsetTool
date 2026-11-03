/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SLCAN (LAWICEL ASCII) + CAN FD 扩展，供 CANgaroo / Cangaroo 等上位机直接驱动
 *
 * 支持的帧格式（与 CANgaroo SLCANDriver 一致）：
 *   t/T  标准/扩展经典数据帧
 *   r/R  标准/扩展远程帧
 *   d/D  标准/扩展 CAN FD 帧（不带 BRS）
 *   b/B  标准/扩展 CAN FD 帧（带 BRS）
 * 每帧以 '\r' 结束，数据为十六进制 ASCII；DLC 码 9~F 对应 12/16/20/24/32/48/64 字节。
 *
 * 支持的命令：
 *   C            关闭通道
 *   O            打开通道
 *   V            返回版本号
 *   S<x>         设置仲裁段波特率（单字符预设 0~9/A~D，或 6 位十六进制自定义）
 *   Y<x>         设置数据段波特率（单字符预设 1~5，或 6 位十六进制自定义）
 *   M0 / M1      正常模式 / 只听（Bus Monitoring）模式
 */

#include <rtthread.h>
#include <string.h>

#include "slcan.h"
#include "can.h"
#include "util_thread.h"
#include "usb_tiny_cdc.h"

#define DBG_TAG "SLCAN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define SLCAN_THREAD_STACK      1536
#define SLCAN_THREAD_PRIORITY   19
#define SLCAN_TIMESLICE         10
#define SLCAN_LINE_MAX          160

static char       s_line[SLCAN_LINE_MAX];
static rt_size_t  s_line_len;

static volatile rt_bool_t s_open = RT_FALSE;
static uint32_t   s_nominal_bps = 500000u;
static uint32_t   s_data_bps    = 2000000u;
static rt_bool_t  s_listen_only = RT_FALSE;

rt_bool_t slcan_is_open(void)
{
    return s_open;
}

/* ------------------------------------------------------------------ */
/* 输出                                                                */
/* ------------------------------------------------------------------ */

static void slcan_write(const void *data, rt_size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    int stall_ms = 0;

    while (len > 0 && stall_ms < 10)
    {
        int written;

        if (!tinyusb_cdc_connected()) return;

        written = tinyusb_cdc_write_force(p, len);
        if (written > 0)
        {
            p += written;
            len -= (rt_size_t)written;
            stall_ms = 0;
            continue;
        }
        rt_thread_mdelay(1);
        stall_ms++;
    }
}

static void slcan_reply_ok(void)
{
    slcan_write("\r", 1);
}

static void slcan_reply_err(void)
{
    slcan_write("\a", 1);
}

static void slcan_reply_version(void)
{
    slcan_write("V1013\r", 6);
}

/* ------------------------------------------------------------------ */
/* 波特率命令                                                          */
/* ------------------------------------------------------------------ */

static int slcan_hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static rt_bool_t slcan_lookup_bitrate(char code, rt_bool_t data_phase, uint32_t *bps)
{
    if (code >= 'a' && code <= 'z') code = (char)(code - 'a' + 'A');

    if (!data_phase)
    {
        switch (code)
        {
        case '0': *bps = 10000u;   return RT_TRUE;
        case '1': *bps = 20000u;   return RT_TRUE;
        case '2': *bps = 50000u;   return RT_TRUE;
        case '3': *bps = 100000u;  return RT_TRUE;
        case '4': *bps = 125000u;  return RT_TRUE;
        case '5': *bps = 250000u;  return RT_TRUE;
        case '6': *bps = 500000u;  return RT_TRUE;
        case '7': *bps = 800000u;  return RT_TRUE;
        case '8': *bps = 1000000u; return RT_TRUE;
        case '9': *bps = 83333u;   return RT_TRUE;
        case 'A': *bps = 75000u;   return RT_TRUE;
        case 'B': *bps = 62500u;   return RT_TRUE;
        case 'C': *bps = 33333u;   return RT_TRUE;
        case 'D': *bps = 5000u;    return RT_TRUE;
        default:  return RT_FALSE;
        }
    }

    switch (code)
    {
    case '1': *bps = 1000000u; return RT_TRUE;
    case '2': *bps = 2000000u; return RT_TRUE;
    case '3': *bps = 3000000u; return RT_TRUE;
    case '4': *bps = 4000000u; return RT_TRUE;
    case '5': *bps = 5000000u; return RT_TRUE;
    default:  return RT_FALSE;
    }
}

static void slcan_handle_rate(rt_bool_t data_phase)
{
    const char *arg = &s_line[1];
    rt_size_t   n = s_line_len > 0 ? s_line_len - 1 : 0;
    uint32_t    bps = 0;

    if (n == 1)
    {
        if (!slcan_lookup_bitrate(arg[0], data_phase, &bps))
        {
            slcan_reply_err();
            return;
        }
    }
    else if (n == 6)
    {
        for (rt_size_t i = 0; i < 6; i++)
        {
            int v = slcan_hex_nibble(arg[i]);
            if (v < 0)
            {
                slcan_reply_err();
                return;
            }
            bps = (bps << 4) | (uint32_t)v;
        }
    }
    else
    {
        slcan_reply_err();
        return;
    }

    if (bps < 5000u || bps > 8000000u)
    {
        slcan_reply_err();
        return;
    }

    if (data_phase) s_data_bps = bps;
    else            s_nominal_bps = bps;

    if (can1_set_timing(s_nominal_bps, s_data_bps, s_listen_only) != RT_EOK)
    {
        LOG_W("apply timing %u/%u failed", s_nominal_bps, s_data_bps);
        slcan_reply_err();
        return;
    }

    slcan_reply_ok();
}

static void slcan_handle_mode(void)
{
    rt_bool_t listen;

    if (s_line_len != 2 || (s_line[1] != '0' && s_line[1] != '1'))
    {
        slcan_reply_err();
        return;
    }

    listen = (s_line[1] == '1') ? RT_TRUE : RT_FALSE;
    if (listen != s_listen_only)
    {
        s_listen_only = listen;
        if (can1_set_timing(s_nominal_bps, s_data_bps, s_listen_only) != RT_EOK)
        {
            slcan_reply_err();
            return;
        }
    }
    slcan_reply_ok();
}

/* ------------------------------------------------------------------ */
/* 主机 -> CAN                                                         */
/* ------------------------------------------------------------------ */

static rt_err_t slcan_send_frame(char type)
{
    rt_bool_t ext = (type == 'T' || type == 'R' || type == 'D' || type == 'B');
    rt_bool_t rtr = (type == 'r' || type == 'R');
    rt_bool_t fd  = (type == 'd' || type == 'D' || type == 'b' || type == 'B');
    rt_bool_t brs = (type == 'b' || type == 'B');
    int id_len = ext ? 8 : 3;
    int pos = 1;
    uint32_t id = 0;
    int dlc;
    int len;
    can_user_msg_t msg;

    if ((int)s_line_len < pos + id_len + 1)
    {
        return -RT_EINVAL;
    }

    for (int i = 0; i < id_len; i++)
    {
        int v = slcan_hex_nibble(s_line[pos++]);
        if (v < 0) return -RT_EINVAL;
        id = (id << 4) | (uint32_t)v;
    }

    dlc = slcan_hex_nibble(s_line[pos++]);
    if (dlc < 0) return -RT_EINVAL;
    if (!fd && dlc > 8) return -RT_EINVAL;

    len = (int)can_dlc_to_len((uint8_t)dlc);

    /* 远程帧没有数据段，但保留 DLC 里的长度 */
    if (!rtr)
    {
        if ((int)s_line_len < pos + len * 2) return -RT_EINVAL;
    }

    if (id > (ext ? 0x1FFFFFFFu : 0x7FFu)) return -RT_EINVAL;

    memset(&msg, 0, sizeof(msg));
    msg.id       = id;
    msg.extended = ext ? 1 : 0;
    msg.remote   = rtr ? 1 : 0;
    msg.fd       = fd ? 1 : 0;
    msg.brs      = brs ? 1 : 0;
    msg.len      = (uint8_t)len;

    if (!rtr)
    {
        for (int i = 0; i < len; i++)
        {
            int hi = slcan_hex_nibble(s_line[pos++]);
            int lo = slcan_hex_nibble(s_line[pos++]);
            if (hi < 0 || lo < 0) return -RT_EINVAL;
            msg.data[i] = (uint8_t)((hi << 4) | lo);
        }
    }

    return can1_send_user_msg(&msg);
}

/* ------------------------------------------------------------------ */
/* CAN -> 主机                                                         */
/* ------------------------------------------------------------------ */

int slcan_send_rx(const FDCAN_RxHeaderTypeDef *header, const uint8_t *data, uint8_t len)
{
    char buf[SLCAN_LINE_MAX];
    rt_bool_t ext = (header->IdType == FDCAN_EXTENDED_ID);
    rt_bool_t rtr = (header->RxFrameType == FDCAN_REMOTE_FRAME);
    rt_bool_t fd  = (header->FDFormat == FDCAN_FD_CAN);
    rt_bool_t brs = (header->BitRateSwitch == FDCAN_BRS_ON);
    char type;
    int dlc;
    int n = 0;

    if (!s_open || len > 64) return 0;

    if (rtr)      type = ext ? 'R' : 'r';
    else if (fd)  type = brs ? (ext ? 'B' : 'b') : (ext ? 'D' : 'd');
    else          type = ext ? 'T' : 't';

    dlc = (int)can_len_to_dlc(len);
    if (dlc == 0xFF) return 0;

    buf[n++] = type;
    n += rt_snprintf(&buf[n], sizeof(buf) - n, ext ? "%08X" : "%03X", (unsigned)header->Identifier);
    n += rt_snprintf(&buf[n], sizeof(buf) - n, "%X", (unsigned)dlc);

    for (int i = 0; i < len && n < (int)sizeof(buf) - 3; i++)
    {
        uint8_t byte = rtr ? 0u : data[i]; /* 远程帧没有数据段，补 0 保持格式完整 */
        n += rt_snprintf(&buf[n], sizeof(buf) - n, "%02X", byte);
    }
    buf[n++] = '\r';

    slcan_write(buf, (rt_size_t)n);
    return 1;
}

/* ------------------------------------------------------------------ */
/* 命令行分发                                                          */
/* ------------------------------------------------------------------ */

static void slcan_handle_line(void)
{
    s_line[s_line_len] = '\0';

    switch (s_line[0])
    {
    case 'C':
        s_open = RT_FALSE;
        slcan_reply_ok();
        break;

    case 'O':
        tinyusb_cdc_flush_input();
        s_open = RT_TRUE;
        slcan_reply_ok();
        break;

    case 'V':
        slcan_reply_version();
        break;

    case 'S':
        slcan_handle_rate(RT_FALSE);
        break;

    case 'Y':
        slcan_handle_rate(RT_TRUE);
        break;

    case 'M':
        slcan_handle_mode();
        break;

    case 't': case 'T': case 'r': case 'R':
    case 'd': case 'D': case 'b': case 'B':
        if (slcan_send_frame(s_line[0]) == RT_EOK) slcan_reply_ok();
        else                                       slcan_reply_err();
        break;

    default:
        /* 未知命令保持静默，避免打乱上位机的响应匹配 */
        break;
    }
}

/* ------------------------------------------------------------------ */
/* 线程                                                                */
/* ------------------------------------------------------------------ */

static void slcan_thread_entry(void *parameter)
{
    uint8_t buf[64];

    (void)parameter;

    while (1)
    {
        int n;

        /* 主机拔掉/关闭串口后自动退出打开状态，CAN1 恢复本地日志输出 */
        if (s_open && !tinyusb_cdc_connected())
        {
            s_open = RT_FALSE;
            s_line_len = 0;
        }

        n = tinyusb_cdc_read(buf, sizeof(buf));

        for (int i = 0; i < n; i++)
        {
            char c = (char)buf[i];

            if (c == '\r' || c == '\n')
            {
                if (s_line_len > 0)
                {
                    slcan_handle_line();
                    s_line_len = 0;
                }
            }
            else if (c == '\b' || c == 0x7F)
            {
                if (s_line_len > 0) s_line_len--;
            }
            else if (s_line_len < sizeof(s_line) - 1)
            {
                s_line[s_line_len++] = c;
            }
            else
            {
                s_line_len = 0;
                slcan_reply_err();
            }
        }

        if (n <= 0)
        {
            rt_thread_mdelay(1);
        }
    }
}

void slcan_init(void)
{
    s_line_len = 0;
    s_open = RT_FALSE;

    if (create_thread_param(slcan_thread_entry, RT_NULL, "slcan",
                            SLCAN_THREAD_STACK, SLCAN_THREAD_PRIORITY,
                            SLCAN_TIMESLICE) != RT_EOK)
    {
        LOG_E("create slcan thread failed");
        return;
    }

    LOG_I("SLCAN-FD bridge ready on CAN1 (CANgaroo compatible)");
}
