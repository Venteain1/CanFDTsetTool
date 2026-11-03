# CanFDTsetTool

> 基于 STM32G474 的双路 CAN FD 调试 / 参数设置工具（个人项目，开发中）

![MCU](https://img.shields.io/badge/MCU-STM32G474VET6-03234B)
![RTOS](https://img.shields.io/badge/RTOS-RT--Thread%20Nano%203.1.4-2E8B57)
![USB](https://img.shields.io/badge/USB-TinyUSB%200.19.0-blue)
![Host](https://img.shields.io/badge/Host-CANgaroo-orange)
![Toolchain](https://img.shields.io/badge/Toolchain-arm--none--eabi--gcc-lightgrey)

## 简介

CanFDTsetTool 是一个自用的 CAN FD 工具固件：使用 STM32G474 的 FDCAN1 / FDCAN2 两路
CAN FD 控制器收发报文，通过 USB 虚拟串口与 PC 通信。

**FDCAN1 被实现为一个 SLCAN（LAWICEL ASCII）+ CAN FD 扩展设备**，可以直接用开源的
[CANgaroo](https://github.com/wikilift/CANgaroo)（Cangaroo 的活跃分支）在 Windows 上
收发报文、DBC 解码、录制 trace，不需要自己写上位机。

### 当前已实现

- **SLCAN-FD 协议**：`t/T`、`r/R`、`d/D`、`b/B` 帧格式，DLC 码 9~F 对应 12~64 字节
- **运行时波特率配置**：上位机 `S`/`Y` 命令（预设档 + 6 位十六进制自定义）实时重算 FDCAN 位时序并重启控制器
- **只听模式**：`M1` 进入 FDCAN Bus Monitoring 模式，只监听不应答
- 双路 CAN FD 初始化：FDCAN1（PD0/PD1）、FDCAN2（PB12/PB13），CAN FD + BRS，最高 64 字节
- 标准帧 / 扩展帧全通滤波，接收走 RX FIFO0
- 两路独立的接收线程；SLCAN 打开时 CAN1 的报文走 USB，否则按十六进制打印到控制台
- MSH 命令 `can_tx64`：在 FDCAN2 上发送一帧 64 字节、ID = `0x123` 的 CAN FD+BRS 测试帧
- USB 全速双 CDC 设备（TinyUSB）：CDC0 = SLCAN 数据通道，CDC1 = 控制台
- 控制台双通道：优先 USB CDC1，未连接时自动回退到 UART1（PA9/PA10，115200 8N1）
- 无堆（RT-Thread Nano）环境下的静态线程管理封装：固定线程表 + 静态栈

> 说明：SLCAN 协议本身是单通道的，因此 CAN1 是 CANgaroo 使用的通道，CAN2 目前仅作为
> 独立的测试/日志通道（`can_tx64`）。双通道同时接入上位机需要扩展私有协议，见文末计划。

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
| FDCAN1_RX | PD0 | AF9，SLCAN 通道 |
| FDCAN1_TX | PD1 | AF9，SLCAN 通道 |
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
| 上位机 | CANgaroo 0.3.1（SLCAN 驱动，支持 CAN FD） |
| 工具链 | arm-none-eabi-gcc（RT-Thread Studio 内置 5.4.1），`-mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16` |
| 构建方式 | RT-Thread Studio 工程（`.project` / `.cproject`），构建目录 `Debug/` |

### 目录结构

```
CanFDTsetTool/
├── applications/            # 应用层
│   ├── main.c / main.h      # 入口、启动函数表 functions[]
│   ├── can/                 # FDCAN1/FDCAN2 初始化、位时序计算、收发
│   ├── slcan/               # SLCAN-FD 协议解析与 USB 收发（CANgaroo 通道）
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
main()                        // RT-Thread 用户 main 线程
  └─ run_on_start()
       ├─ can_init()           // FDCAN1（SLCAN 通道）+ FDCAN2 + 接收线程
       ├─ usb_component_init() // HAL_PCD + TinyUSB 双 CDC + tud_task 线程
       └─ slcan_init()         // SLCAN 命令解析线程
  └─ 主线程进入 1 s 空转循环
```

新增启动项只需在 `applications/main.h` 的 `functions[]` 数组中添加函数指针（以 `NULL` 结尾）。

### 线程模型

| 线程 | 优先级 | 栈大小 | 作用 |
| --- | --- | --- | --- |
| `tshell` | 5 | 4096 | finsh/MSH 命令行 |
| `main` | 10 | 2048 | 启动各组件后空转 |
| `slcan` | 19 | 1536 | 解析 USB CDC0 上的 SLCAN 命令与待发送帧 |
| `tusbt` | 20 | 2048 | USB 栈轮询（`tud_task()`，1 ms 周期） |
| `can_rx1` | 20 | 1536 | FDCAN1 接收轮询（SLCAN 打开时转发到 USB） |
| `can_rx2` | 20 | 1536 | FDCAN2 接收轮询（日志输出） |
| `tidle` | 31 | 256 | 空闲线程 |

受 `THREAD_MANAGER_MAX_COUNT` 限制，静态线程最多 8 个，单线程栈上限 2048 字节
（见 `applications/thread/util_thread.h`）。

## CAN FD 配置

### 位时序

| 总线 | 仲裁段 | 数据段 | 说明 |
| --- | --- | --- | --- |
| CAN1（SLCAN） | 500 kbit/s | 2 Mbit/s | 默认值，可由上位机 `S`/`Y` 实时修改 |
| CAN2（测试） | 500 kbit/s | 3.4 Mbit/s | 编译期固定（`can.c` 中 `hfdcan2.Init`） |

CAN1 的位时序由 `fdcan_find_timing()` 根据 170 MHz 内核时钟自动计算：在预分频范围内
枚举，取采样点约 80%、误差不超过 1% 的最优组合；改动速率时执行
`Stop → Init → 配置滤波器 → Start`，并用互斥量避免与接收线程并发访问。

> 3.4 Mbit/s 这类非标准数据段速率可以通过 `Y` + 6 位十六进制自定义（例如 `Y33E1C0`），
> 但需要收发器和线束支持；CANgaroo 的预设档位为 1/2/3/4/5 Mbit/s。

### 滤波器与模式

`FDCAN_FRAME_FD_BRS`、Normal 模式（或 Bus Monitoring）、自动重传使能、发送暂停使能、
TX FIFO 队列模式、标准滤波器 28 组 / 扩展滤波器 8 组；过滤器 `FilterID1/2 = 0`
配合掩码模式，实现**全接收**（标准帧与扩展帧均进 RX FIFO0）。

## SLCAN-FD 协议与 CANgaroo

### USB 身份

CANgaroo 的 SLCAN 驱动按 VID/PID 枚举设备，本项目采用其可识别的
**WeAct USB2CAN** 组合（支持 CAN FD + 自定义速率的 SLCAN 变体）：

| 项目 | 值 |
| --- | --- |
| VID:PID | `0x0483:0x5740`（STM32 虚拟串口，Windows 自带驱动） |
| 序列号 | `AAA00001`（`AAA` 前缀是 CANgaroo 的识别条件） |
| CDC0 | SLCAN 数据通道（CAN1） |
| CDC1 | finsh 控制台（日志/命令） |
| 端点 | CDC0 notif `0x81`、OUT `0x02`、IN `0x83`；CDC1 notif `0x84`、OUT `0x05`、IN `0x86` |

> 换成这组 VID/PID 只是为了让现成上位机认得这台自定义设备；如果以后要公开发布，
> 建议改为在 CANgaroo 的 `SLCANDriver::update()` 中登记自己的 VID/PID。

### 支持的命令

| 命令 | 说明 | 应答 |
| --- | --- | --- |
| `C` | 关闭通道（CAN1 恢复本地日志） | `\r` |
| `O` | 打开通道 | `\r` |
| `V` | 版本号 | `V1013\r` |
| `S<x>` | 仲裁段速率：单字符预设，或 6 位十六进制 | `\r` / `\a` |
| `Y<x>` | 数据段速率：单字符预设，或 6 位十六进制 | `\r` / `\a` |
| `M0` / `M1` | 正常模式 / 只听模式 | `\r` |
| `t/T` | 标准/扩展经典数据帧 | `\r` / `\a` |
| `r/R` | 标准/扩展远程帧 | `\r` / `\a` |
| `d/D` | 标准/扩展 CAN FD 帧（无 BRS） | `\r` / `\a` |
| `b/B` | 标准/扩展 CAN FD 帧（带 BRS） | `\r` / `\a` |

预设速率码：`S0=10k, S1=20k, S2=50k, S3=100k, S4=125k, S5=250k, S6=500k,
S7=800k, S8=1M, S9=83.333k, SA=75k, SB=62.5k, SC=33.333k, SD=5k`；
`Y1=1M, Y2=2M, Y3=3M, Y4=4M, Y5=5M`。

### 在 Windows 上使用 CANgaroo

1. 从 [CANgaroo Releases](https://github.com/wikilift/CANgaroo/releases/tag/CANgaroo) 下载
   `cangaroo-0.3.1_WIN.zip` 并解压运行；
2. 设备插入后枚举为 USB 串口（COMx），首次会自动安装 `usbser.sys`；
3. 双 CDC 会生成**两个 COM 口**：编号较小的通常是 CDC0（SLCAN 数据口），另一个是 CDC1 控制台
   （用任意串口终端打开它能看到启动日志，可据此确认）；
4. CANgaroo 中选择 `SLCAN` 接口 → 选中 CDC0 对应的 COM 口（显示为 WeAct USB2CAN 属正常）→
   设置仲裁段/数据段速率 → 开始测量；
5. CAN1 接上收发器和总线，即可在 Trace 窗口看到报文、用 DBC 解码，并通过发送窗口发帧。

## 构建与烧录

**方式一：RT-Thread Studio（推荐）**

1. `File → Import → RT-Thread Studio Project`，选择本仓库根目录；
2. 目标配置选择 `Debug`；
3. 由于新增了 `applications/slcan/` 目录，导入后先按 `F5` 刷新工程，再执行
   `Clean` + `Build`，让 Studio 重新生成 `Debug/**/subdir.mk`；
4. 产物：`Debug/rtthread.elf`、`Debug/rtthread.bin`；
5. 通过 ST-Link / J-Link 的 SWD 接口下载。

**方式二：命令行 makefile**

工程内已包含 RT-Thread Studio 生成的 `Debug/makefile`，安装好 `arm-none-eabi-gcc` 后：

```bash
cd Debug
make -j
```

**方式三：scons**

根目录提供 `SConstruct` / `rtconfig.py`，需配置 `RTT_EXEC_PATH` 指向工具链。
注意：`applications/` 目前没有 `SConscript`，使用 scons 前需要自行补充应用层构建脚本，
否则应用代码不会参与编译，因此现阶段以 Studio 工程为准。

## 控制台与命令

控制台输出优先使用 USB CDC1（需要主机打开串口并置位 DTR），否则回退到 UART1
（PA9/PA10，115200 8N1）。可用命令：

| 命令 | 说明 |
| --- | --- |
| `help` | 列出所有 MSH 命令 |
| `can_tx64` | 在 FDCAN2 上发送 64 字节 CAN FD+BRS 测试帧（ID `0x123`，数据为 0x00~0x3F 自增） |
| `usb_test` | 通过 CDC0 发送 `hello world!`（会混入 SLCAN 数据流，仅调试用） |
| `list_thread` / `ps` | 查看线程与栈使用情况 |

未连接上位机时，CAN1/CAN2 收到的报文会以 `LOG_D` 级别打印，例如：

```
CAN FD RX: CAN1 ID=0x00000123 LEN=64 BRS=1 FDF=1
00: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
10: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
```

## 代码导读

| 文件 | 说明 |
| --- | --- |
| `applications/main.c` / `main.h` | 程序入口，`functions[]` 启动函数表 |
| `applications/can/can.c` | FDCAN 初始化、位时序计算、滤波器、接收线程、SLCAN 发送接口 |
| `applications/slcan/slcan.c` | SLCAN-FD 命令/帧解析、CDC0 收发、打开状态管理 |
| `applications/usb/usb.c` | USB 时钟（HSI48）、`HAL_PCD` 初始化与启动 |
| `applications/usb/usb_tiny_cdc.c` | TinyUSB 描述符、双 CDC 读写、DTR 状态、`tud_task` 线程、USB 中断 |
| `applications/thread/util_thread.c` | 静态线程表管理（`create_thread_param`、`find_thread`、`delete_thread` 等） |
| `drivers/drv_usart.c` | UART1 + USB CDC 控制台的输出/输入切换实现 |
| `drivers/drv_clk.c` | 系统时钟配置（170 MHz） |
| `drivers/board.h` | 芯片型号、ROM/RAM、时钟与 UART 引脚宏 |

## 已知限制与后续计划

- [x] 上位机可用的 SLCAN-FD 通道（CAN1）+ 运行时波特率配置
- [ ] **仅单通道 SLCAN**：CAN2 无法同时被 CANgaroo 使用；如需双通道，可在 `S`/`Y` 之外扩展私有命令，或改用 GVRET 双端口协议
- [ ] 未实现 SLCAN 的 `Z`（时间戳）等可选命令，时间戳由上位机接收时刻生成
- [ ] 未上报 CAN 错误帧 / Bus-Off 状态，也没有错误计数器
- [ ] 发送采用「入队即应答」，总线错误不会反馈给上位机
- [ ] CAN2 的数据段速率（3.4 Mbit/s）为编译期固定值，需按实际硬件能力调整
- [ ] `applications/can/can.c` 中 CAN2 仍使用固定时序，未接入自动计算
- [ ] 缺少单元测试与 CI

## 许可证

本项目为个人学习/自用项目，仓库暂未添加顶层 `LICENSE` 文件。所依赖的第三方组件遵循其各自许可：

- RT-Thread：Apache-2.0
- TinyUSB：MIT
- STM32G4 HAL / CMSIS：BSD-3-Clause / Apache-2.0
- CANgaroo：GPL-2.0

## 致谢

- [RT-Thread](https://www.rt-thread.org/)
- [TinyUSB](https://github.com/hathach/tinyusb)
- [CANgaroo](https://github.com/wikilift/CANgaroo)
- [STM32CubeG4](https://github.com/STMicroelectronics/STM32CubeG4)
