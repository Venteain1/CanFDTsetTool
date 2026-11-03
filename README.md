# CanFDTsetTool

> 基于 STM32G474 的双路 CAN FD 调试 / 参数设置工具（个人项目，开发中）

![MCU](https://img.shields.io/badge/MCU-STM32G474VET6-03234B)
![RTOS](https://img.shields.io/badge/RTOS-RT--Thread%20Nano%203.1.4-2E8B57)
![USB](https://img.shields.io/badge/USB-TinyUSB%200.19.0-blue)
![Toolchain](https://img.shields.io/badge/Toolchain-arm--none--eabi--gcc-orange)

## 简介

CanFDTsetTool 是一个自用的 CAN FD 工具固件原型：使用 STM32G474 的 FDCAN1 / FDCAN2 两路
CAN FD 控制器收发报文，通过 USB 全速设备（双 CDC 虚拟串口）与控制台 / 上位机交互，
目标是做成一个可以随身携带、能看报文、能发报文、能做参数设置的 CAN FD 小工具。

目前工程已经完成底层通路验证，**USB ↔ CAN 的桥接协议和上位机尚未实现**，
详见 [已知限制与后续计划](#已知限制与后续计划)。

### 当前已实现

- 双路 CAN FD 初始化：FDCAN1（PD0/PD1）、FDCAN2（PB12/PB13），CAN FD + BRS，最高 64 字节数据
- 标准帧 / 扩展帧全通滤波，接收走 RX FIFO0
- 两路独立的接收线程，按帧打印 ID、长度、BRS、FDF 以及十六进制数据（每行 16 字节）
- MSH 命令 `can_tx64`：在 FDCAN2 上发送一帧 64 字节、ID = `0x123` 的 CAN FD+BRS 测试帧
- USB 全速双 CDC 设备（TinyUSB）：CDC0 数据口、CDC1 控制台口
- 控制台双通道：优先 USB CDC1，未连接时自动回退到 UART1（PA9/PA10，115200 8N1）
- 无堆（RT-Thread Nano）环境下的静态线程管理封装：固定线程表 + 静态栈

## 硬件平台

| 项目 | 参数 |
| --- | --- |
| MCU | STM32G474VET6（LQFP100，Cortex-M4F，带 FPU） |
| 主频 | 170 MHz（HSE 8 MHz → PLL：M=2, N=85, R=2） |
| Flash / RAM | 512 KB / 128 KB（链接脚本：`linkscripts/STM32G474VETx/link.lds`） |
| CAN 控制器 | FDCAN1、FDCAN2（内核时钟 PCLK1 = 170 MHz） |
| USB | USB 2.0 全速设备，内置 PHY，时钟源 HSI48 |
| 调试口 | SWD（ST-Link / J-Link） |

> 板上需外接 CAN 收发器（如 TJA1050、ATA6561、MCP2551 等）以及 120 Ω 终端电阻，
> MCU 引脚只提供 TTL 电平的 FDCAN TX/RX 信号。

### 引脚分配

| 功能 | 引脚 | 复用 / 说明 |
| --- | --- | --- |
| FDCAN1_RX | PD0 | AF9 |
| FDCAN1_TX | PD1 | AF9 |
| FDCAN2_RX | PB12 | AF9 |
| FDCAN2_TX | PB13 | AF9 |
| USB_DM | PA11 | USB FS，内置 PHY |
| USB_DP | PA12 | USB FS，内置 PHY |
| USART1_TX | PA9 | 备用控制台，AF7，115200 8N1 |
| USART1_RX | PA10 | 备用控制台，AF7，115200 8N1 |

## 软件架构

| 组件 | 版本 / 说明 |
| --- | --- |
| RT-Thread Nano | 3.1.4（`RT_VER_NUM 0x30104`），无堆（`RT_USING_NOHEAP`），启用 finsh/MSH |
| STM32G4 HAL | 来自 STM32Cube FW_G4 V1.6.1（`libraries/STM32G4xx_HAL_Driver`） |
| TinyUSB | 0.19.0（`packages/tinyusb-0.19.0`），STM32 FSDEV 端口，`OPT_OS_NONE` |
| 工具链 | arm-none-eabi-gcc（RT-Thread Studio 内置 5.4.1），`-mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16` |
| 构建方式 | RT-Thread Studio 工程（`.project` / `.cproject`），构建目录 `Debug/` |

### 目录结构

```
CanFDTsetTool/
├── applications/            # 应用层
│   ├── main.c / main.h      # 入口、启动函数表 functions[]
│   ├── can/                 # FDCAN1/FDCAN2 初始化与收发
│   ├── usb/                 # USB PCD 初始化 + TinyUSB 双 CDC 设备
│   └── thread/              # 无堆环境下的静态线程管理（创建/删除/改优先级）
├── drivers/                 # 板级驱动：时钟、串口/控制台、HAL 配置、中断
├── libraries/               # CMSIS + STM32G4xx HAL Driver
├── cubemx/                  # STM32CubeMX 工程（cubemx.ioc，仅作引脚/时钟参考）
├── linkscripts/             # 链接脚本（STM32G474VETx/link.lds）
├── packages/                # TinyUSB 0.19.0
├── rt-thread/               # RT-Thread Nano 内核 + finsh 组件
├── rtconfig.h / rtconfig.py # 内核裁剪与工具链配置
├── SConstruct / SConscript  # scons 构建入口（备选）
└── Debug/                   # RT-Thread Studio 构建输出（已加入 .gitignore）
```

### 启动流程

```
rt_hw_board_init()            // 时钟 170 MHz、串口初始化
  └─ rt_components_board_init()
main()                        // RT-Thread 用户 main 线程
  └─ run_on_start()
       ├─ can_init()          // FDCAN1/FDCAN2 + 接收线程
       └─ usb_component_init()// HAL_PCD + TinyUSB 双 CDC + tud_task 线程
  └─ 主线程进入 1 s 空转循环
```

新增启动项只需在 `applications/main.h` 的 `functions[]` 数组中添加函数指针（以 `NULL` 结尾）。

### 线程模型

| 线程 | 优先级 | 栈大小 | 作用 |
| --- | --- | --- | --- |
| `tshell` | 5 | 4096 | finsh/MSH 命令行 |
| `main` | 10 | 2048 | 启动各组件后空转 |
| `tusbt` | 20 | 2048 | USB 栈轮询（`tud_task()`，1 ms 周期） |
| `can_rx1` | 20 | 1024 | FDCAN1 接收轮询 |
| `can_rx2` | 20 | 1024 | FDCAN2 接收轮询 |
| `tidle` | 31 | 256 | 空闲线程 |

受 `THREAD_MANAGER_MAX_COUNT` 限制，静态线程最多 8 个，单线程栈上限 2048 字节
（见 `applications/thread/util_thread.h`）。

## CAN FD 配置

| 参数 | 仲裁段 | 数据段 |
| --- | --- | --- |
| 时钟 | PCLK1 = 170 MHz | PCLK1 = 170 MHz |
| 预分频（Prescaler） | 17 | 5 |
| tq 频率 | 10 MHz | 34 MHz |
| Sync + TSeg1 + TSeg2 | 1 + 15 + 4 = 20 | 1 + 7 + 2 = 10 |
| 波特率 | **500 kbit/s** | **3.4 Mbit/s** |
| 采样点 | 80 % | 80 % |
| SJW | 4 | 2 |

其他设置：`FDCAN_FRAME_FD_BRS`、Normal 模式、自动重传使能、发送暂停使能、
TX FIFO 队列模式、标准滤波器 28 组 / 扩展滤波器 8 组；过滤器 `FilterID1/2 = 0`
配合掩码模式，实现**全接收**（标准帧与扩展帧均进 RX FIFO0）。

> 数据段 3.4 Mbit/s 属于偏高的自定义速率，实际使用请根据收发器、线束长度和对端设备
> 能力调整 `applications/can/can.c` 中的 `DataPrescaler` / `DataTimeSeg1` / `DataTimeSeg2`。

## USB 设备

- 双 CDC 复合设备（两个接口 + IAD），**VID:PID = `0x16C0:0x05DC`**
- CDC0：数据通道（预留作 USB ↔ CAN 桥接）
- CDC1：控制台通道，`rt_hw_console_output()` / `rt_hw_console_getchar()` 优先走这里
- 端点分配：CDC0 notif `0x81`、OUT `0x02`、IN `0x83`；CDC1 notif `0x84`、OUT `0x05`、IN `0x86`
- 驱动接口：`applications/usb/usb_tiny_cdc.h`

## 控制台与命令

控制台输出优先使用 USB CDC1（需要主机打开串口并置位 DTR），否则回退到 UART1
（PA9/PA10，115200 8N1）。可用命令：

| 命令 | 说明 |
| --- | --- |
| `help` | 列出所有 MSH 命令 |
| `can_tx64` | 在 FDCAN2 上发送 64 字节 CAN FD+BRS 测试帧（ID `0x123`，数据为 0x00~0x3F 自增） |
| `usb_test` | 通过 CDC0 向主机发送 `hello world!` |
| `list_thread` / `ps` | 查看线程与栈使用情况 |

接收报文会以 `LOG_D` 级别打印，例如：

```
CAN FD RX: CAN1 ID=0x00000123 LEN=64 BRS=1 FDF=1
00: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
10: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
```

## 构建与烧录

**方式一：RT-Thread Studio（推荐）**

1. `File → Import → RT-Thread Studio Project`，选择本仓库根目录；
2. 目标配置选择 `Debug`，直接编译；
3. 产物：`Debug/rtthread.elf`、`Debug/rtthread.bin`（当前约 70 KB）；
4. 通过 ST-Link / J-Link 的 SWD 接口下载。

**方式二：命令行 makefile**

工程内已包含 RT-Thread Studio 生成的 `Debug/makefile`，安装好 `arm-none-eabi-gcc` 后：

```bash
cd Debug
make -j
```

**方式三：scons**

根目录提供 `SConstruct` / `rtconfig.py`（`CROSS_TOOL = 'gcc'`，链接脚本
`linkscripts//STM32G474VETx//link.lds`），需配置 `RTT_EXEC_PATH` 指向工具链。
注意：`applications/` 目前没有 `SConscript`，使用 scons 前需要自行补充应用层构建脚本，
否则应用代码不会参与编译，因此现阶段以 Studio 工程为准。

## 代码导读

| 文件 | 说明 |
| --- | --- |
| `applications/main.c` / `main.h` | 程序入口，`functions[]` 启动函数表 |
| `applications/can/can.c` | FDCAN1/FDCAN2 初始化、滤波器、接收轮询线程、`can_tx64` 命令 |
| `applications/usb/usb.c` | USB 时钟（HSI48）、`HAL_PCD` 初始化与启动 |
| `applications/usb/usb_tiny_cdc.c` | TinyUSB 描述符、双 CDC 读写、DTR 状态、`tud_task` 线程、USB 中断 |
| `applications/thread/util_thread.c` | 静态线程表管理（`create_thread_param`、`find_thread`、`delete_thread` 等） |
| `drivers/drv_usart.c` | UART1 + USB CDC 控制台的输出/输入切换实现 |
| `drivers/drv_clk.c` | 系统时钟配置（170 MHz） |
| `drivers/board.h` | 芯片型号、ROM/RAM、时钟与 UART 引脚宏 |

## 已知限制与后续计划

- [ ] **USB ↔ CAN 桥接协议未实现**：`tud_cdc_rx_cb()` 为空，CDC0 下行数据尚未解析，接收到的 CAN 帧也只打印日志、未上传主机
- [ ] 无上位机软件（PC 端工具）与命令协议
- [ ] 波特率、滤波器、工作模式均为编译期固定，缺少运行时配置命令
- [ ] 未处理 CAN 错误帧 / 总线关闭（Bus-Off）恢复，也未统计错误计数器
- [ ] 接收为轮询 + `LOG_D` 打印，缺少时间戳、环形缓冲和批量上报
- [ ] 数据段速率（3.4 Mbit/s）需按实际硬件能力重新核算
- [ ] `applications/can/can.c` 末尾保留了旧 HAL CAN 代码注释，可清理
- [ ] 缺少单元测试与 CI

## 许可证

本项目为个人学习/自用项目，仓库暂未添加顶层 `LICENSE` 文件。所依赖的第三方组件遵循其各自许可：

- RT-Thread：Apache-2.0
- TinyUSB：MIT
- STM32G4 HAL / CMSIS：BSD-3-Clause / Apache-2.0

## 致谢

- [RT-Thread](https://www.rt-thread.org/)
- [TinyUSB](https://github.com/hathach/tinyusb)
- [STM32CubeG4](https://github.com/STMicroelectronics/STM32CubeG4)
