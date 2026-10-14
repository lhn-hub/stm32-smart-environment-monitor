# sms-hub：STM32 智能环境监测与安防联动

本项目基于 STM32F103C8T6、FreeRTOS、W25Q64、OLED、DHT11、超声波、Rd-03 雷达、MQ2/MQ135、SG90、TB6612 风扇和 ESP8266，提供本地菜单、MQTT 控制和单文件云看板。

## 当前交付状态

- 最终生产配置：`APP_MQ_DO_ENABLED=1`，蜂鸣器模块已删除，PB4 保留给 Rd-03 雷达。
- ESP8266 保留现有 `AT1.7.4.0 / SDK3.0.5`，STM32 通过 TCP 承载 MQTT，不擦写 ESP Flash。
- 固件镜像不随源码上传；设备网络配置保存在本地 KV，构建后按验收文档在本机烧录。
- 最后一次 Keil 构建：0 Error、0 Warning；Flash `61416/61440 B`，RAM `20256/20480 B`。
- 主机 C 回归、网页布局/协议回归已通过；本轮真实 H/T 板测、手机看板、24 小时烤机和临界区耗时仍需按 [`Docs/test_report.md`](Docs/test_report.md) 留证。

## 30 分钟复现

1. 准备 STM32F103C8T6 核心板、扩展板 V1.0.2、CMSIS-DAP、USB-UART、3.3 V/5 V 电源和本 README 中的模块。
2. 断电，按下表接线；MQ2/MQ135 的 DO 必须经过 5 V→3.3 V 电平转换，AO 悬空。
3. 打开 `MDK-ARM/sms-hub.uvprojx`，Rebuild，确认本机 Keil 输出 0 Error/0 Warning。
4. 仅通过 SWD 下载 STM32；不要擦除 W25Q64，不要升级 ESP8266。下载后复位并打开 USART1：115200、8N1。
5. 看到 `Flash JEDEC=0xEF4017`、KV/传感器自检和任务水位日志后，等待 MQ 模块至少预热 5 分钟。
6. 双击 `WebDashboard/index.html`，设备名填 KV 中的 `dev`（默认 `sms-hub`；看板输入框默认 `sms01` 仅是示例），连接 `broker.emqx.io:8084`。
7. 订阅 `smshub/<dev>/#`，先发送 `arm=0`，等待 `cmd/resp` 的 `done`，再按 [`Docs/reproduce-checklist.md`](Docs/reproduce-checklist.md) 做最小功能验证。

## 硬件清单与逐线接线

所有模块共地。舵机、风扇和 MQ 加热器的 5 V 电流不能通过 MCU 3.3 V 稳压器承担；上电前测量 5 V。蜂鸣器不再接入。

| 模块/信号 | 扩展板或模块端 | STM32/GPIO | 备注 |
|---|---|---|---|
| USART1 调试/配网 | CH340 TX/RX | PA10 RX、PA9 TX | 115200 8N1；不要插 USB_DEVICE |
| OLED SSD1306 | I2C1 SCL/SDA、3V3、GND | PB6 SCL、PB7 SDA | 7-bit 地址 `0x3C`，400 kHz |
| DHT11 | VCC=3V3、GND、DATA | PB12 | 直插模块 IO3；裸传感器线序不同 |
| 超声波 CS100A | TRIG/ECHO/3V3/GND | PA15 TRIG、PB3 ECHO | NOJTAG 释放 PB3/PA15；断电插拔 |
| Rd-03 雷达 | OT2 | PB4 | 高有效；OT1/RX 只用于独立配置 |
| MQ2 烟雾 | IO1 VCC0、GND、DO | PA12（A12） | VCC0 跳帽拨 5V；DO 经电平转换；AO 不接 |
| MQ135 空气 | IO2 VCC1、GND、DO | PA11（A11） | VCC1 跳帽拨 5V；DO 经电平转换；AO 不接 |
| W25Q64 | CS/SCK/MISO/MOSI、3V3、GND | PA4/PA5/PA6/PA7 | 与 TB6612 共享，SPI 由仲裁器切换 |
| TB6612 B 通道 | STBY、BIN1、BIN2、PWMB、VM、VCC、GND | PA4、PA7、PB13、PB1 | 仅 B 通道；A 通道/PWMA 不初始化；VM 按电机规格供电 |
| SG90 | 红=5V、棕/黑=GND、橙/黄=信号 | PA8 / TIM1_CH1 | 500～2500 us，开机 0°，独立 5 V 电源更稳 |
| RGB 指示灯 | R/G/B | PA1/PA2/PA3 / TIM2_CH2/3/4 | 有源低；软件效果灯 |
| 按键 | KEY1、KEY2、KEY3 | PA0、PC13、PB15 | PA0 单击下一项、长按上一项；PC13 返回；PB15 确认 |
| ESP8266-12F | TX/RX/RST/IO0 | PB10 TX、PB11 RX、PB9 RST、PB8 IO0 | 3.3 V；运行态 IO0=高；USART3 DMA+IDLE |

MQ 电平转换连接：转换板 `HV=扩展板5V`、`LV=扩展板3V3`、GND 共地；MQ2 DO→HV1/LV1→PA12，MQ135 DO→HV2/LV2→PA11。未接转换板时禁止下载生产镜像。

## 编译、下载与版本

```powershell
.\Docs\build.ps1 -Stage M8
```

正常构建必须保持 `APP_M8_TEST_ENABLED=0`、`APP_CRITICAL_AUDIT=0`。测试镜像仅用于专门注入：

- `Docs/M8-test.hex`：栈保护测试，结束后必须烧回正常镜像。
- `Docs/M8-audit.hex`：DWT 临界区采集，不用于烤机。

Keil 选 CMSIS-DAP、64 KB Flash 算法、Erase/Program/Verify、Reset and Run；SWD 保留，代码只关闭 JTAG（NOJTAG），不关闭 SWD。

## WiFi / MQTT

设备默认连接配置在 W25Q64 KV 中；网页 WiFi 页通过本地 USB Web Serial 扫描 `CWLAP`、选择热点并保存密码，密码不会发布到 MQTT。ESP 仍使用当前 AT 固件。

| 参数 | 值 |
|---|---|
| 设备 MQTT Broker | `broker.emqx.io:1883` |
| 看板 WebSocket | `wss://broker.emqx.io:8084/mqtt` |
| 根主题 | `smshub/<dev>` |
| 发布 | `/status`、`/telemetry`、`/event`、`/config` |
| 订阅 | `/cmd`；回执 `/cmd/resp` |
| LWT | `/status` payload `offline`；上线 retained `online` |
| keepalive | 60 s |

常用命令：`arm=0`、`arm=1`、`fan=45`、`fan=0`、`servo=90`、`servo=0`、`ack=1`、`th=temp:50`、`th=distance:100`、`th=smoke:3000`、`th=air:10000`、`time=<Unix秒>`、`reboot=1`。每条命令必须等待最终 `done` 或 `rejected`，`queued` 不代表已经写入 KV。

## 目录结构

```text
App/                 任务、DataHub、报警状态机、菜单、存储
BSP/                 引脚、时钟、控制、总线仲裁、串口、看门狗
Drivers_Device/      DHT11、超声波、Rd-03、W25Q64、SG90、TB6612
Middleware/FreeRTOS  FreeRTOS 内核
Src/ Inc/             CubeMX/HAL 初始化与中断
WebDashboard/         单文件 MQTT.js + Canvas 看板
Docs/                 构建日志、验收、测试脚本、接线和交付文档
MDK-ARM/              Keil 工程
```

## 已知限制与二期扩展

- 单 DO 的 MQ/雷达输入无法仅靠软件可靠判断“拔线”；DHT11/超声波可通过超时和质量位进入 FAULT。
- 公共 EMQX 仅用于演示，没有项目级访问隔离；不要在其中使用真实生产凭据。
- 传感器精度、舵机角度、风扇噪声、电平波形、手机延迟、24 小时稳定性必须由实物证据确认。
- Flash 余量仅 24 B，RAM 余量 224 B；不再加入运行时功能。
- 二期候选：智慧农业规则、低功耗 STOP+RTC、IAP 双区 OTA、MAX30102 健康页。每项先做 64 KB/20 KB 可行性评估。

## M9 交付索引

- [`Docs/test_report.md`](Docs/test_report.md)：H-01～H-10、T-01～T-15 实测矩阵和证据插槽
- [`Docs/demo_storyboard.md`](Docs/demo_storyboard.md)：3 分钟六段演示脚本
- [`Docs/resume-and-interview.md`](Docs/resume-and-interview.md)：中英文简历条目与 20 个面试问答
- [`Docs/reproduce-checklist.md`](Docs/reproduce-checklist.md)：陌生人 30 分钟复现清单
- [`Docs/project-architecture-summary.md`](Docs/project-architecture-summary.md)：任务图、数据流、Flash/RAM 资源页
- [`Docs/M8-acceptance.md`](Docs/M8-acceptance.md)：M8 详细板测步骤

`v1.0` 只有在测试报告中的严重问题全部关闭、硬件证据归档后创建；当前工作区不提前伪造该标签。
