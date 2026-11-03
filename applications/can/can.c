/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-03-05     BZH1WX       the first version
 * 2025-11-03     Venteain     增加 SLCAN 双通道与运行时波特率配置
 */

#include "stm32g4xx_hal.h"
#include "util_thread.h"
#include "can.h"
#include "slcan.h"

#define DBG_TAG "CAN"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include <finsh.h>

#ifndef CAN_RX_THREAD_BASE_NAME
#define CAN_RX_THREAD_BASE_NAME "can_rx"
#endif

/* FDCAN 内核时钟：PCLK1（drv_clk.c 中 APB1 不分频，170 MHz） */
#define FDCAN_KERNEL_CLK_HZ        170000000u

/* 两路总线作为 SLCAN 通道的默认速率，可被上位机的 S/Y 命令改写 */
#define CAN_DEFAULT_NOMINAL_BPS    500000u
#define CAN_DEFAULT_DATA_BPS       2000000u

#define CAN_BUS_COUNT              2

void can_rxthread_entry(void *parameter);
FDCAN_HandleTypeDef hfdcan1;
FDCAN_HandleTypeDef hfdcan2;

static FDCAN_HandleTypeDef *const can_handle[CAN_BUS_COUNT] = { &hfdcan1, &hfdcan2 };

typedef struct
{
    struct rt_mutex lock;
    rt_bool_t       lock_ready;
} can_bus_ctx_t;

static can_bus_ctx_t can_ctx[CAN_BUS_COUNT];

static const uint8_t _dlc_len_map[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64 };

/* ------------------------------------------------------------------ */
/* DLC 与字节数互转                                                    */
/* ------------------------------------------------------------------ */

uint8_t can_dlc_to_len(uint8_t dlc_code)
{
    return (dlc_code < 16) ? _dlc_len_map[dlc_code] : 0;
}

uint8_t can_len_to_dlc(uint8_t len)
{
    if (len <= 8) return len;

    switch (len)
    {
    case 12: return 9;
    case 16: return 10;
    case 20: return 11;
    case 24: return 12;
    case 32: return 13;
    case 48: return 14;
    case 64: return 15;
    default: return 0xFF;
    }
}

/* ------------------------------------------------------------------ */
/* 位时序计算                                                          */
/* ------------------------------------------------------------------ */

/* 在给定内核时钟下寻找最接近目标速率的 (prescaler, tseg1, tseg2, sjw)。
 * 采样点固定约 80%，误差超过 1% 视为无解。 */
static rt_bool_t fdcan_find_timing(uint32_t fclk, uint32_t bitrate, rt_bool_t data_phase,
                                   uint32_t *prescaler, uint32_t *tseg1, uint32_t *tseg2,
                                   uint32_t *sjw)
{
    const uint32_t p_max     = data_phase ? 32u : 512u;
    const uint32_t t1_max    = data_phase ? 32u : 256u;
    const uint32_t t2_max    = data_phase ? 16u : 128u;
    const uint32_t total_max = data_phase ? 49u : 385u;
    const uint32_t total_min = 8u;

    uint32_t best_ppm   = 0xFFFFFFFFu;
    uint32_t best_p     = 0;
    uint32_t best_t1    = 0;
    uint32_t best_t2    = 0;
    uint32_t best_total = 0;

    if (bitrate == 0u) return RT_FALSE;

    for (uint32_t p = 1u; p <= p_max; p++)
    {
        uint64_t denom = (uint64_t)p * bitrate;
        uint32_t total;
        uint32_t t2;
        uint32_t t1;
        uint64_t diff;
        uint32_t ppm;

        if (denom == 0u) continue;

        total = (uint32_t)(((uint64_t)fclk + denom / 2u) / denom);
        if (total < total_min || total > total_max) continue;

        t2 = total / 5u;                 /* 采样点 ≈ 80% */
        if (t2 < 2u) t2 = 2u;
        if (t2 > t2_max) t2 = t2_max;

        t1 = total - 1u - t2;
        if (t1 < 1u || t1 > t1_max) continue;

        {
            uint64_t want = denom * total;   /* bitrate * p * total */
            diff = (want > fclk) ? (want - fclk) : (fclk - want);
        }
        ppm = (uint32_t)((diff * 1000000ULL) / fclk);

        if (ppm > 10000u) continue;      /* 允许 1% 误差 */

        if (ppm < best_ppm || (ppm == best_ppm && total > best_total))
        {
            best_ppm   = ppm;
            best_p     = p;
            best_t1    = t1;
            best_t2    = t2;
            best_total = total;
        }
    }

    if (best_p == 0u) return RT_FALSE;

    *prescaler = best_p;
    *tseg1     = best_t1;
    *tseg2     = best_t2;
    *sjw       = (best_t2 > (data_phase ? 2u : 4u)) ? (data_phase ? 2u : 4u) : best_t2;

    return RT_TRUE;
}

/* ------------------------------------------------------------------ */
/* 硬件配置                                                            */
/* ------------------------------------------------------------------ */

static void can_hw_defaults(FDCAN_HandleTypeDef *h)
{
    h->Init.ClockDivider        = FDCAN_CLOCK_DIV1;
    h->Init.FrameFormat         = FDCAN_FRAME_FD_BRS;
    h->Init.Mode                = FDCAN_MODE_NORMAL;
    h->Init.AutoRetransmission  = ENABLE;
    h->Init.TransmitPause       = ENABLE;
    h->Init.ProtocolException   = DISABLE;
    h->Init.StdFiltersNbr       = 28;
    h->Init.ExtFiltersNbr       = 8;
    h->Init.TxFifoQueueMode     = FDCAN_TX_FIFO_OPERATION;
}

static void can_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /**FDCAN1 GPIO Configuration
    PD0     ------> FDCAN1_RX
    PD1     ------> FDCAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /**FDCAN2 GPIO Configuration
     PB12     ------> FDCAN2_RX
     PB13     ------> FDCAN2_TX
     */
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* 标准帧 / 扩展帧全部接收，进 RX FIFO0 */
static rt_err_t can_config_filters(FDCAN_HandleTypeDef *hcan)
{
    FDCAN_FilterTypeDef filter = { 0 };

    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x000;
    filter.FilterID2    = 0x000;
    if (HAL_FDCAN_ConfigFilter(hcan, &filter) != HAL_OK)
    {
        LOG_E("std filter config failed");
        return -RT_ERROR;
    }

    filter.IdType       = FDCAN_EXTENDED_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0x00000000;
    filter.FilterID2    = 0x00000000;
    if (HAL_FDCAN_ConfigFilter(hcan, &filter) != HAL_OK)
    {
        LOG_E("ext filter config failed");
        return -RT_ERROR;
    }

    return RT_EOK;
}

/* 应用新的位时序（调用者需保证不并发访问该总线） */
static rt_err_t can_bus_apply(int bus, uint32_t nominal_bps, uint32_t data_bps, rt_bool_t listen_only)
{
    FDCAN_HandleTypeDef *h;
    uint32_t np, nt1, nt2, nsjw;
    uint32_t dp, dt1, dt2, dsjw;

    if (bus < 0 || bus >= CAN_BUS_COUNT) return -RT_EINVAL;
    h = can_handle[bus];

    if (!fdcan_find_timing(FDCAN_KERNEL_CLK_HZ, nominal_bps, RT_FALSE, &np, &nt1, &nt2, &nsjw))
    {
        LOG_E("CAN%d: no nominal timing for %u bps", bus + 1, nominal_bps);
        return -RT_EINVAL;
    }
    if (!fdcan_find_timing(FDCAN_KERNEL_CLK_HZ, data_bps, RT_TRUE, &dp, &dt1, &dt2, &dsjw))
    {
        LOG_E("CAN%d: no data timing for %u bps", bus + 1, data_bps);
        return -RT_EINVAL;
    }

    (void)HAL_FDCAN_Stop(h);

    h->Init.NominalPrescaler     = np;
    h->Init.NominalSyncJumpWidth = nsjw;
    h->Init.NominalTimeSeg1      = nt1;
    h->Init.NominalTimeSeg2      = nt2;
    h->Init.DataPrescaler        = dp;
    h->Init.DataSyncJumpWidth    = dsjw;
    h->Init.DataTimeSeg1         = dt1;
    h->Init.DataTimeSeg2         = dt2;
    h->Init.Mode = listen_only ? FDCAN_MODE_BUS_MONITORING : FDCAN_MODE_NORMAL;

    if (HAL_FDCAN_Init(h) != HAL_OK)
    {
        LOG_E("CAN%d init failed (nominal=%u data=%u)", bus + 1, nominal_bps, data_bps);
        return -RT_ERROR;
    }

    if (can_config_filters(h) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (HAL_FDCAN_Start(h) != HAL_OK)
    {
        LOG_E("CAN%d start failed", bus + 1);
        return -RT_ERROR;
    }

    LOG_I("CAN%d timing: %u/%u bps (p=%u/%u t1=%u/%u t2=%u/%u)%s",
          bus + 1, nominal_bps, data_bps, np, dp, nt1, dt1, nt2, dt2,
          listen_only ? " listen-only" : "");

    return RT_EOK;
}

rt_err_t can_bus_set_timing(int bus, uint32_t nominal_bps, uint32_t data_bps, rt_bool_t listen_only)
{
    rt_err_t ret;

    if (bus < 0 || bus >= CAN_BUS_COUNT) return -RT_EINVAL;
    if (!can_ctx[bus].lock_ready) return -RT_ERROR;

    rt_mutex_take(&can_ctx[bus].lock, RT_WAITING_FOREVER);
    ret = can_bus_apply(bus, nominal_bps, data_bps, listen_only);
    rt_mutex_release(&can_ctx[bus].lock);

    return ret;
}

/* ------------------------------------------------------------------ */
/* 发送                                                                */
/* ------------------------------------------------------------------ */

rt_err_t can_bus_send(int bus, const can_user_msg_t *msg)
{
    FDCAN_HandleTypeDef *h;
    FDCAN_TxHeaderTypeDef tx;
    uint8_t dlc;
    HAL_StatusTypeDef st;

    if (bus < 0 || bus >= CAN_BUS_COUNT) return -RT_EINVAL;
    if (msg == RT_NULL) return -RT_EINVAL;
    if (!msg->fd && msg->len > 8) return -RT_EINVAL;

    dlc = can_len_to_dlc(msg->len);
    if (dlc == 0xFF) return -RT_EINVAL;

    h = can_handle[bus];

    tx.Identifier          = msg->id;
    tx.IdType              = msg->extended ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    tx.TxFrameType         = msg->remote ? FDCAN_REMOTE_FRAME : FDCAN_DATA_FRAME;
    tx.DataLength          = ((uint32_t)dlc) << 16;
    tx.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx.BitRateSwitch       = msg->brs ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    tx.FDFormat            = msg->fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    tx.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx.MessageMarker       = 0;

    if (!can_ctx[bus].lock_ready) return -RT_ERROR;

    rt_mutex_take(&can_ctx[bus].lock, RT_WAITING_FOREVER);
    st = HAL_FDCAN_AddMessageToTxFifoQ(h, &tx, (uint8_t *)msg->data);
    rt_mutex_release(&can_ctx[bus].lock);

    return (st == HAL_OK) ? RT_EOK : -RT_EBUSY;
}

/* ------------------------------------------------------------------ */
/* 接收                                                                */
/* ------------------------------------------------------------------ */

static void can_log_rx_frame(int bus, const FDCAN_RxHeaderTypeDef *rxHeader,
                             const uint8_t *rxData, uint8_t data_len)
{
    LOG_D("CAN FD RX: CAN%d ID=0x%08X LEN=%d BRS=%d FDF=%d",
          bus + 1, (unsigned)rxHeader->Identifier, data_len,
          (rxHeader->BitRateSwitch == FDCAN_BRS_ON), (rxHeader->FDFormat == FDCAN_FD_CAN));

    if (data_len > 0)
    {
        char line_buf[96];
        int pos = 0;

        for (int i = 0; i < data_len; i++)
        {
            /* 每 16 字节换行 */
            if (i % 16 == 0)
            {
                if (pos > 0)
                {
                    line_buf[pos] = '\0';
                    LOG_D("%s", line_buf);
                    pos = 0;
                }
                pos += rt_snprintf(&line_buf[pos], sizeof(line_buf) - pos, "%02X:", i);
            }
            pos += rt_snprintf(&line_buf[pos], sizeof(line_buf) - pos, " %02X", rxData[i]);
            if (pos >= (int)sizeof(line_buf) - 5) /* 防止溢出强制输出 */
            {
                line_buf[pos] = '\0';
                LOG_D("%s", line_buf);
                pos = 0;
            }
        }
        if (pos > 0)
        {
            line_buf[pos] = '\0';
            LOG_D("%s", line_buf);
        }
    }
}

static void _can_poll_readmessage(int bus)
{
    FDCAN_HandleTypeDef *hcan = can_handle[bus];
    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[64];

    while (1)
    {
        uint32_t msg_count = HAL_FDCAN_GetRxFifoFillLevel(hcan, FDCAN_RX_FIFO0);

        if (msg_count > 0)
        {
            HAL_StatusTypeDef st;
            rt_bool_t locked = RT_FALSE;

            if (can_ctx[bus].lock_ready)
            {
                rt_mutex_take(&can_ctx[bus].lock, RT_WAITING_FOREVER);
                locked = RT_TRUE;
            }

            st = HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO0, &rxHeader, rxData);

            if (locked)
            {
                rt_mutex_release(&can_ctx[bus].lock);
            }

            if (st == HAL_OK)
            {
                uint8_t dlc_code = (uint8_t)((rxHeader.DataLength >> 16) & 0x0Fu);
                uint8_t data_len = can_dlc_to_len(dlc_code);

                if (slcan_is_open(bus))
                {
                    slcan_send_rx(bus, &rxHeader, rxData, data_len);
                }
                else
                {
                    can_log_rx_frame(bus, &rxHeader, rxData, data_len);
                }
            }
            else
            {
                LOG_E("CAN%d: failed to read message from FIFO0", bus + 1);
            }
        }
        else
        {
            rt_thread_mdelay(5);
        }

        rt_thread_yield();
    }
}

/* ------------------------------------------------------------------ */
/* 测试发送命令（CAN2）                                                */
/* ------------------------------------------------------------------ */

void can_tx_test_message(void)
{
    /* 发送一个测试 CAN FD 报文（标准 ID 0x123，64 字节自增数据） */
    FDCAN_TxHeaderTypeDef txHeader;
    txHeader.Identifier          = 0x123;               /* 标准 ID */
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_64;  /* 64 字节 */
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_ON;        /* FD + BRS */
    txHeader.FDFormat            = FDCAN_FD_CAN;        /* 使用 FD 格式 */
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    uint8_t data[64];
    for (int i = 0; i < 64; i++)
    {
        data[i] = (uint8_t)i;
    }

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan2, &txHeader, data) != HAL_OK)
    {
        LOG_E("CAN TX test message failed");
        return;
    }
    LOG_D("CAN TX test message sent ID=0x%03X, LEN=64", (unsigned)txHeader.Identifier);
}

MSH_CMD_EXPORT_ALIAS(can_tx_test_message, can_tx64, send a 64-byte FD CAN test frame on FDCAN2);

/* ------------------------------------------------------------------ */
/* 初始化                                                              */
/* ------------------------------------------------------------------ */

void can_init(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

    /** Initializes the peripherals clocks
     */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        LOG_E("can clock config failed");
    }

    /* Peripheral clock enable */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    can_gpio_init();

    hfdcan1.Instance = FDCAN1;
    hfdcan2.Instance = FDCAN2;

    for (int bus = 0; bus < CAN_BUS_COUNT; bus++)
    {
        char name[RT_NAME_MAX + 1];

        rt_snprintf(name, sizeof(name), "can%d", bus + 1);
        rt_mutex_init(&can_ctx[bus].lock, name, RT_IPC_FLAG_PRIO);
        can_ctx[bus].lock_ready = RT_TRUE;

        can_hw_defaults(can_handle[bus]);
        if (can_bus_apply(bus, CAN_DEFAULT_NOMINAL_BPS, CAN_DEFAULT_DATA_BPS, RT_FALSE) != RT_EOK)
        {
            LOG_E("CAN%d init failed", bus + 1);
        }
    }

    // create CAN1 & CAN2 receive threads (parameter 1 / 2)
    if (create_thread_param(can_rxthread_entry, (void *)1, CAN_RX_THREAD_BASE_NAME "1", 1536, 20, 10) != RT_EOK)
        LOG_E("create can1 rx thread failed");
    if (create_thread_param(can_rxthread_entry, (void *)2, CAN_RX_THREAD_BASE_NAME "2", 1536, 20, 10) != RT_EOK)
        LOG_E("create can2 rx thread failed");

    LOG_I("CAN bus init done");
}

void can_rxthread_entry(void *parameter)
{
    int bus = ((uintptr_t)parameter == 2) ? 1 : 0;

    while (1)
    {
        _can_poll_readmessage(bus);
    }
}
