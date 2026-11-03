/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-03-05     BZH1WX       the first version
 */

#include "stm32g4xx_hal.h"
#include "util_thread.h"
#include "can.h"
#define DBG_TAG "CAN"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include <finsh.h>

#ifndef CAN_RX_THREAD_BASE_NAME
#define CAN_RX_THREAD_BASE_NAME "can_rx"
#endif

void can_rxthread_entry(void *parameter);
FDCAN_HandleTypeDef hfdcan1;
FDCAN_HandleTypeDef hfdcan2;

void can_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };
    RCC_PeriphCLKInitTypeDef PeriphClkInit = { 0 };

    /** Initializes the peripherals clocks
     */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
    PeriphClkInit.FdcanClockSelection = RCC_FDCANCLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        LOG_E("can init faild");
    }

    /* Peripheral clock enable */
    __HAL_RCC_FDCAN_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**FDCAN1 GPIO Configuration
    PD0     ------> FDCAN1_RX
    PD1     ------> FDCAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /**FDCAN2 GPIO Configuration
     PB12     ------> FDCAN2_RX
     PB13     ------> FDCAN2_TX
     */
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF9_FDCAN2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    hfdcan1.Instance = FDCAN1;
    hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
    hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan1.Init.AutoRetransmission = ENABLE;
    hfdcan1.Init.TransmitPause = ENABLE;
    hfdcan1.Init.ProtocolException = DISABLE;
    hfdcan1.Init.NominalPrescaler = 17;
    hfdcan1.Init.NominalSyncJumpWidth = 4;
    hfdcan1.Init.NominalTimeSeg1 = 15;
    hfdcan1.Init.NominalTimeSeg2 = 4;
    hfdcan1.Init.DataPrescaler = 5;
    hfdcan1.Init.DataSyncJumpWidth = 2;
    hfdcan1.Init.DataTimeSeg1 = 7;
    hfdcan1.Init.DataTimeSeg2 = 2;
    hfdcan1.Init.StdFiltersNbr = 28;
    hfdcan1.Init.ExtFiltersNbr = 8;
    hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;

    if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
    {
        LOG_E("can init faild");
    }

    /* FDCAN2 初始化 */
    hfdcan2.Instance = FDCAN2;
    hfdcan2.Init.ClockDivider = FDCAN_CLOCK_DIV1;
    hfdcan2.Init.FrameFormat = FDCAN_FRAME_FD_BRS;
    hfdcan2.Init.Mode = FDCAN_MODE_NORMAL;
    hfdcan2.Init.AutoRetransmission = ENABLE;
    hfdcan2.Init.TransmitPause = ENABLE;
    hfdcan2.Init.ProtocolException = DISABLE;
    hfdcan2.Init.NominalPrescaler = 17;
    hfdcan2.Init.NominalSyncJumpWidth = 4;
    hfdcan2.Init.NominalTimeSeg1 = 15;
    hfdcan2.Init.NominalTimeSeg2 = 4;
    hfdcan2.Init.DataPrescaler = 5;
    hfdcan2.Init.DataSyncJumpWidth = 2;
    hfdcan2.Init.DataTimeSeg1 = 7;
    hfdcan2.Init.DataTimeSeg2 = 2;
    hfdcan2.Init.StdFiltersNbr = 28;
    hfdcan2.Init.ExtFiltersNbr = 8;
    hfdcan2.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
    if (HAL_FDCAN_Init(&hfdcan2) != HAL_OK)
    {
        LOG_E("can2 init faild");
    }

    FDCAN_FilterTypeDef sFilterConfigStandard;
    sFilterConfigStandard.IdType = FDCAN_STANDARD_ID;
    sFilterConfigStandard.FilterIndex = 0;
    sFilterConfigStandard.FilterType = FDCAN_FILTER_MASK;
    sFilterConfigStandard.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfigStandard.FilterID1 = 0x000;
    sFilterConfigStandard.FilterID2 = 0x000;
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfigStandard) != HAL_OK)
    {
        LOG_E("Filter configuration failed");
    }

    /* FDCAN2 过滤器：标准 ID 全接收 */
    FDCAN_FilterTypeDef sFilterConfigStandard2 = sFilterConfigStandard;
    if (HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfigStandard2) != HAL_OK)
    {
        LOG_E("Filter2 configuration failed");
    }

    FDCAN_FilterTypeDef sFilterConfigExtended;
    sFilterConfigExtended.IdType = FDCAN_EXTENDED_ID;
    sFilterConfigExtended.FilterIndex = 0;
    sFilterConfigExtended.FilterType = FDCAN_FILTER_MASK;
    sFilterConfigExtended.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfigExtended.FilterID1 = 0x00000000;
    sFilterConfigExtended.FilterID2 = 0x00000000;
    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfigExtended) != HAL_OK){
        LOG_E("Filter configuration failed");
    }

    /* FDCAN2 扩展 ID 全接收 */
    FDCAN_FilterTypeDef sFilterConfigExtended2 = sFilterConfigExtended;
    if (HAL_FDCAN_ConfigFilter(&hfdcan2, &sFilterConfigExtended2) != HAL_OK){
        LOG_E("Filter2 configuration failed");
    }

    rt_thread_mdelay(10);

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
    {
        LOG_E("can1 init faild");
    }
    if (HAL_FDCAN_Start(&hfdcan2) != HAL_OK)
    {
        LOG_E("can2 init faild");
    }

    // create CAN1 & CAN2 receive threads (parameter 1 / 2)
    if (create_thread_param(can_rxthread_entry, (void*)1, CAN_RX_THREAD_BASE_NAME "1", 1024, 20, 10) != RT_EOK)
        LOG_E("create can1 rx thread failed");
    if (create_thread_param(can_rxthread_entry, (void*)2, CAN_RX_THREAD_BASE_NAME "2", 1024, 20, 10) != RT_EOK)
        LOG_E("create can2 rx thread failed");

    LOG_I("");
}

static void _can_poll_readmessage(FDCAN_HandleTypeDef *hcan)
{
    FDCAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[64];

    /* DLC 转长度映射表 (0..15) */
    static const uint8_t _dlc_len_map[16] = { 0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64 };

    while (1) {
    uint32_t msg_count = HAL_FDCAN_GetRxFifoFillLevel(hcan, FDCAN_RX_FIFO0);

        if (msg_count > 0) {
            if (HAL_FDCAN_GetRxMessage(hcan, FDCAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
            {
                uint8_t dlc_code = (rxHeader.DataLength >> 16) & 0x0F;
                uint8_t data_len = (dlc_code < 16) ? _dlc_len_map[dlc_code] : 0;
                LOG_D("CAN FD RX: CAN%c ID=0x%08X LEN=%d BRS=%d FDF=%d", (hcan==&hfdcan1?'1':'2'), rxHeader.Identifier, data_len, 
                      (rxHeader.BitRateSwitch == FDCAN_BRS_ON), (rxHeader.FDFormat == FDCAN_FD_CAN));
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
            else {
                LOG_E("Failed to read message from FIFO0");
            }
        }
        else {
            rt_thread_mdelay(5);
        }
        rt_thread_yield();
    }
}

void can_tx_test_message()
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
    LOG_D("CAN TX test message sent ID=0x%03X, LEN=64", txHeader.Identifier);
}

MSH_CMD_EXPORT_ALIAS(can_tx_test_message, can_tx64, send a 64-byte FD CAN test frame on FDCAN2);

void can_rxthread_entry(void *parameter)
{
    uintptr_t sel = (uintptr_t)parameter; /* 1 => CAN1, 2 => CAN2 */
    FDCAN_HandleTypeDef *h = (sel == 2) ? &hfdcan2 : &hfdcan1;
    while (1)
    {
        _can_poll_readmessage(h);
    }
}

//void can_sendmessage(uint32_t id, uint8_t* data, uint8_t len)
//{
//    CAN_TxHeaderTypeDef txHeader;
//
//    txHeader.StdId = id;
//    txHeader.IDE = CAN_ID_STD;
//    txHeader.RTR = CAN_RTR_DATA;
//    txHeader.DLC = len;
//    txHeader.TransmitGlobalTime = DISABLE;
//
//    uint32_t txMaibox;
//    uint8_t FreeTxNum = 0;
//
//    FreeTxNum = HAL_CAN_GetTxMailboxesFreeLevel(&hcan1);
//    while (0 == FreeTxNum)
//    {
//        FreeTxNum = HAL_CAN_GetTxMailboxesFreeLevel(&hcan1);
//    }
//
//    if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, data, &txMaibox) != HAL_OK)
//    {
//        LOG_D("!send message faild!");
//    }
//    LOG_D("send message success ID: 0x%03x", id);
//}

