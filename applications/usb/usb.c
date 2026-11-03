/* Basic USB device initialization (FS) for STM32G4 (conflicts with FDCAN1 on PA11/PA12) */
#include "usb.h"
#include "stm32g4xx_hal.h"
#include "util_thread.h"
#define DBG_TAG "USB"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

static PCD_HandleTypeDef hpcd_usbfs;

void usb_component_init(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
       LOG_E("USB clock config failed");
       return;
    }

    __HAL_RCC_USB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    hpcd_usbfs.Instance = USB;
    hpcd_usbfs.Init.dev_endpoints = 8;
    hpcd_usbfs.Init.speed = PCD_SPEED_FULL;
    hpcd_usbfs.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_usbfs.Init.Sof_enable = DISABLE;
    hpcd_usbfs.Init.low_power_enable = DISABLE;
    hpcd_usbfs.Init.lpm_enable = DISABLE;
    hpcd_usbfs.Init.battery_charging_enable = DISABLE;
    if (HAL_PCD_Init(&hpcd_usbfs) != HAL_OK)
    {
        LOG_E("HAL_PCD init failed");
        return;
    }

    if (HAL_PCD_Start(&hpcd_usbfs) != HAL_OK)
    {
        LOG_E("HAL_PCD start failed");
        return;
    }
    LOG_I("HAL PCD started, launching TinyUSB CDC...");

    LOG_I("tusb_init begin...");
    tinyusb_cdc_init();
    LOG_I("tusb_init done");
}
