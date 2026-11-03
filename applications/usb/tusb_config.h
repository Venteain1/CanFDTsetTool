#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#include "usb.h"
/* RT-Thread Nano 环境最小 CDC 设备配置 */

#ifdef __cplusplus
extern "C" {
#endif

#define RT_DEBUG

#define CFG_TUSB_MCU                OPT_MCU_STM32G4
#define CFG_TUSB_OS                 OPT_OS_NONE    /* 使用轮询线程 tud_task */
#define CFG_TUSB_DEBUG              0

#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE)
#define CFG_TUD_ENABLED             1
#define CFG_TUD_ENDPOINT0_SIZE      64

/* 启用三个 CDC 实例：CDC0=CAN1 数据，CDC1=CAN2 数据，CDC2 作为控制台
 * （每个 CDC 的 bulk IN/OUT 共用端点编号，才能放进 8 个端点内） */
#define CFG_TUD_CDC                 3
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_VENDOR              0
#define CFG_TUD_AUDIO               0
#define CFG_TUD_VIDEO               0
#define CFG_TUD_ECM_RNDIS           0
#define CFG_TUD_MIDI                0
#define CFG_TUD_DFU                 0
#define CFG_TUD_DFU_RUNTIME         0

/* CDC 缓冲尺寸 */
#define CFG_TUD_CDC_RX_BUFSIZE      512
#define CFG_TUD_CDC_TX_BUFSIZE      512
#define CFG_TUD_CDC_EP_BUFSIZE      64

/* 内存分配：使用编译期静态数组，不依赖 malloc */
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
