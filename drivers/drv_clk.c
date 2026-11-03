/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-10-26     ChenYong     first version
 * 2020-01-08     xiangxistu   add HSI configuration
 */

#include <rtthread.h>
#include <stm32g4xx.h>
#include <board.h>
#include "drv_common.h"

#define DBG_TAG "board"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

void system_clock_config(int target_freq_mhz)
{
    // RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    // RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // /** Configure the main internal regulator output voltage
    // */
    // HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    // /** Initializes the RCC Oscillators according to the specified parameters
    // * in the RCC_OscInitTypeDef structure.
    // */
    // RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    // RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    // RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    // RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    // RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
    // RCC_OscInitStruct.PLL.PLLN = 85;
    // RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    // RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    // RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    // if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    // {
    //   Error_Handler();
    // }

    // /** Initializes the CPU, AHB and APB buses clocks
    // */
    // RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
    //                             |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    // RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    // RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    // RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    // RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    // if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    // {
    //   Error_Handler();
    // }

    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
    */
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
    RCC_OscInitStruct.PLL.PLLN = 85;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      LOG_E("clock init faild!");
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
      LOG_E("clock init faild!");
    }
}

int clock_information(void)
{
    LOG_D("System Clock information");
    LOG_D("SYSCLK_Frequency = %d", HAL_RCC_GetSysClockFreq());
    LOG_D("HCLK_Frequency   = %d", HAL_RCC_GetHCLKFreq());
    LOG_D("PCLK1_Frequency  = %d", HAL_RCC_GetPCLK1Freq());
    LOG_D("PCLK2_Frequency  = %d", HAL_RCC_GetPCLK2Freq());

    return RT_EOK;
}
INIT_BOARD_EXPORT(clock_information);

void clk_init(char *clk_source, int source_freq, int target_freq)
{
    system_clock_config(target_freq);
}
