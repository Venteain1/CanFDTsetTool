#include "usb_tiny_cdc.h"
#include "tusb.h"
#include "util_thread.h"
#include "usb.h"
#include <finsh.h>
#define DBG_TAG "TUSB"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#define USB_LOG LOG_D

/* 描述符实现: 设备 + 配置 + 字符串 (双 CDC: CDC0 数据接口 + CDC1 控制台)
 *
 * VID/PID 使用 STM32 虚拟串口 (0483:5740)，序列号以 "AAA" 开头，
 * 这样 CANgaroo 的 SLCAN 驱动会把它识别成支持 CAN FD + 自定义速率的
 * WeAct USB2CAN 设备（见 CANgaroo SLCANDriver 的厂商匹配逻辑）。 */
#define USB_VID 0x0483
#define USB_PID 0x5740

/* 设备描述符 */
uint8_t const desc_device[] = {
    0x12,       // bLength
    0x01,       // bDescriptorType (Device)
    0x00, 0x02, // bcdUSB 2.00
    0x00,       // bDeviceClass = 0 (使用接口级 + IAD 进行分类)
    0x00,       // bDeviceSubClass
    0x00,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0
    USB_VID & 0xFF, (USB_VID >> 8) & 0xFF, // idVendor
    USB_PID & 0xFF, (USB_PID >> 8) & 0xFF, // idProduct
    0x00, 0x01, // bcdDevice
    0x01,       // iManufacturer
    0x02,       // iProduct
    0x03,       // iSerialNumber
    0x01        // bNumConfigurations
};

/* 配置描述符：使用 TinyUSB CDC helper */
enum {
    ITF_NUM_CDC0 = 0,
    ITF_NUM_CDC0_DATA,
    ITF_NUM_CDC1,
    ITF_NUM_CDC1_DATA,
    ITF_NUM_TOTAL
};

/* 端点分配：保持奇数 IN, 偶数 OUT，避免冲突
 * CDC0: Notif EP1 IN (0x81), Data OUT EP2 (0x02), Data IN EP3 (0x83)
 * CDC1: Notif EP4 IN (0x84), Data OUT EP5 (0x05), Data IN EP6 (0x86)
 */
#define EPNUM_CDC0_NOTIF  0x81
#define EPNUM_CDC0_OUT    0x02
#define EPNUM_CDC0_IN     0x83

#define EPNUM_CDC1_NOTIF  0x84
#define EPNUM_CDC1_OUT    0x05
#define EPNUM_CDC1_IN     0x86

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN*2)

uint8_t const desc_configuration[] = {
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 50),
        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC0, 0, EPNUM_CDC0_NOTIF, 8, EPNUM_CDC0_OUT, EPNUM_CDC0_IN, 64),
        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC1, 0, EPNUM_CDC1_NOTIF, 8, EPNUM_CDC1_OUT, EPNUM_CDC1_IN, 64)
};

/* 字符串表 */
static char serial_str[16] = "AAA00001"; /* "AAA" 前缀用于 CANgaroo 识别 */

char const *string_desc_arr[] = {
    NULL,              /* 0: Language ID handled separately */
    "CANFD Bridge",    /* 1: Manufacturer */
    "CANFD CDC",       /* 2: Product */
    serial_str          /* 3: Serial */
};

static uint16_t _desc_str[32];

void tud_cdc_rx_cb(uint8_t itf)
{
    
}

/* 回调: 设备描述符 */
uint8_t const *tud_descriptor_device_cb(void)
{
    return desc_device;
}

/* 回调: 配置描述符 */
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

/* 回调: 字符串描述符 */
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    if (index == 0) {
        /* 语言 ID = 0x0409 (English US) */
        _desc_str[0] = (uint16_t)((2 + 2) | (0x03 << 8)); /* bLength=4, bDescriptorType=STRING */
        _desc_str[1] = 0x0409; /* LANGID 列表 */
        return _desc_str;
    }
    if (index >= (sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) || !string_desc_arr[index]) {
        return NULL;
    }
    const char *str = string_desc_arr[index];
    uint8_t chr_count = (uint8_t)strlen(str);
    if (chr_count > 31) chr_count = 31; /* 限定缓冲 */
    for (uint8_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = (uint16_t)str[i];
    }
    _desc_str[0] = (uint16_t)((2 + chr_count * 2) | (0x03 << 8));
    return _desc_str;
}

/* CDC line state (DTR) 跟踪 */
static volatile bool g_dtr[CFG_TUD_CDC] = {false,false}; /* 每个 CDC 端口的 DTR */

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    int8_t port = itf;
    if (port >= CFG_TUD_CDC)
    {
        USB_LOG("[WARN] line_state itf=%u out of range dtr=%d\r\n", itf, dtr?1:0);
        return;
    }
    g_dtr[port] = dtr;
    USB_LOG("CDC%u (itf=%u) DTR=%d RTS=%d\r\n", port, itf, dtr?1:0, rts?1:0);
}

bool tinyusb_cdc_ready(void)
{
    return tud_cdc_connected() && g_dtr[0]; /* CDC0 数据口 */
}

bool tinyusb_cdc_console_ready(void)
{
    return tud_cdc_n_connected(1) && g_dtr[1]; /* CDC1 控制台口 */
}

int tinyusb_cdc_write(const uint8_t *data, rt_size_t len)
{
    if (!tinyusb_cdc_ready()) return 0;
    uint32_t w = tud_cdc_write(data, (uint32_t)len);
    tud_cdc_write_flush();
    return (int)w;
}

/* SLCAN 数据通道：不依赖 DTR，只要 USB 已连接就写 */
int tinyusb_cdc_write_force(const uint8_t *data, rt_size_t len)
{
    uint32_t w;

    if (!data || len == 0) return 0;
    if (!tud_cdc_n_connected(0)) return 0;

    w = tud_cdc_n_write(0, data, (uint32_t)len);
    if (w > 0) tud_cdc_n_write_flush(0);

    return (int)w;
}

int tinyusb_cdc_read(uint8_t *data, rt_size_t len)
{
    uint32_t avail;

    if (!data || len == 0) return 0;
    if (!tud_cdc_n_connected(0)) return 0;

    avail = tud_cdc_n_available(0);
    if (avail == 0) return 0;
    if (len > avail) len = avail;

    return (int)tud_cdc_n_read(0, data, (uint32_t)len);
}

void tinyusb_cdc_flush_input(void)
{
    tud_cdc_n_read_flush(0);
}

bool tinyusb_cdc_connected(void)
{
    return tud_cdc_n_connected(0);
}

int tinyusb_cdc_console_write(const uint8_t *data, rt_size_t len)
{
    if (!data || len == 0) return 0;
    if (!tinyusb_cdc_console_ready()) return 0;

    rt_size_t offset = 0;
    int wait_ms = 0;            /* 等待计数 */
    const int max_wait_ms = 200;/* 最大等待毫秒 */

    while (offset < len && tinyusb_cdc_console_ready())
    {
        rt_size_t remain = len - offset; /* 剩余字节数 */
        uint32_t avail = tud_cdc_n_write_available(1); /* 当前缓冲可写容量 */
        if (avail == 0)
        {
            if (wait_ms >= max_wait_ms) {
                USB_LOG("CDC1 no space timeout remain=%u\r\n", (unsigned)remain);
                break;
            }
            rt_thread_mdelay(1);
            wait_ms++;
            continue;
        }

        uint32_t chunk = (uint32_t)(remain > avail ? avail : remain);
        uint32_t w = tud_cdc_n_write(1, data + offset, chunk);
        if (w == 0)
        {
            if (wait_ms >= max_wait_ms) {
                USB_LOG("CDC1 write stalled timeout remain=%u\r\n", (unsigned)remain);
                break;
            }
            rt_thread_mdelay(1);
            wait_ms++;
            continue;
        }

        offset += w;
        wait_ms = 0;
        tud_cdc_n_write_flush(1);
    }

    return (int)offset;
}

int tinyusb_cdc_console_getchar(void)
{
    if (!tinyusb_cdc_console_ready()) return -1;
    if (tud_cdc_n_available(1)) {
        int ch = (int)tud_cdc_n_read_char(1);
        return ch;
    }
    return -1;
}

/* 主轮询线程 */
static void tinyusb_task_entry(void *param)
{
    (void)param;
    while (1) {
        tud_task();
        rt_thread_mdelay(1); /* 1ms 轮询 */
    }
}

void tinyusb_cdc_init(void)
{
    USB_LOG("TinyUSB stack init...\r\n");
    tusb_init();

    if (find_thread("tusbt") == RT_NULL) {
        rt_err_t ret = create_thread_param(tinyusb_task_entry, RT_NULL, "tusbt", 2048, 20, 10);
        if (ret != RT_EOK) {
            USB_LOG("create tinyusb thread failed err=%d\r\n", ret);
        }
    }
}

/* Finsh/MSH 命令：usb_test 通过 CDC 发送 hello world! */
int usb_test(int argc, char **argv)
{
    (void)argc; (void)argv;
    if (!tinyusb_cdc_ready()) {
        rt_kprintf("CDC0 not ready (host or DTR)\n");
        return -1;
    }
    const char *msg = "hello world!\r\n";
    int sent = tinyusb_cdc_write((const uint8_t*)msg, strlen(msg));
    rt_kprintf("usb_test sent %d bytes\n, one time available %d\n", sent, tud_cdc_n_write_available(1));
    return 0;
}
MSH_CMD_EXPORT(usb_test, send hello world over CDC0);

void USB_LP_IRQHandler(void)
{
	rt_interrupt_enter();
	tud_int_handler(0);  /* RHPort 0 */
	rt_interrupt_leave();
}

void USB_HP_IRQHandler(void)
{
	rt_interrupt_enter();
	tud_int_handler(0); /* 与 LP 相同分发 */
	rt_interrupt_leave();
}

void USBWakeUp_IRQHandler(void)
{
	rt_interrupt_enter();
	rt_interrupt_leave();
}
