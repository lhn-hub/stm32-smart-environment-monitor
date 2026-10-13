# M8 软件交付与资源

日期：2026-10-13；M7基线 `e912a93`，当前 `0.9.0-dev`。正常/栈测试/审计版均零错误、零警告。
M8硬件DoD及依赖M7验收仍未闭环，当前是软件交付，不发布验收标签v0.9。

## 交付

| 文件 | 用途 |
| --- | --- |
| WebDashboard/index.html | 双击可运行的单HTML，MQTT.js5.16.0/CDN、WSS、原生canvas |
| Docs/M8-app.hex | 正常固件；MQ仍禁用，诊断注入/计时宏均0 |
| Docs/M8-test.hex | 真实FreeRTOS四字guard故障注入，测试完需烧回正常版 |
| Docs/M8-audit.hex | DWT临界区诊断；最近日志缓存4，磨损统计关闭，禁止用于烤机 |
| Docs/M8*-map.txt / M8*-build.log | 各镜像实际构建资源证据 |
| Docs/M8-acceptance.md / M8-wiring.md | 板测、预留IO1/IO2接线与电平确认 |
| Docs/M8-critical-audit.md / audit_m8.py | 122处业务/RTOS/HAL源码入口及LR解析，实际耗时待测 |
| Docs/m8_burn.py / collect_m8_serial.ps1 | 原生串口及只读MQTT24h采集/报告 |
| Docs/test_m8.py / test_dashboard.py | 实际故障C与真实内核guard、看板控件/质量/布局/曲线回归 |

各HEX/AXF含本地配置，仅本地交付，Git忽略；`App/app_secrets.h`不入库。
未升级ESP，未操作W25Q持久数据；保留用户uvoptx改动和原始模块资料。

## 最终资源

Flash=Code+RO+RW；RAM=RW+ZI，11KB FreeRTOS heap已包含在ZI。

| 构建 | Code | RO | RW | ZI | Flash B | RAM B |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| M7基线 | 54536 | 5368 | 48 | 19752 | 59952 | 19800 |
| M8正常 | 55572 | 5416 | 48 | 20104 | 61036 | 20152 |
| M8栈测试 | 55880 | 5416 | 48 | 20104 | 61344 | 20152 |
| M8审计 | 55868 | 5416 | 48 | 20312 | 61332 | 20360 |

正常增量 **Flash+1084B/RAM+352B**；60KB红线内余404B，20KB物理RAM内余328B。
栈测试Flash余96B；审计Flash余108B/RAM余120B。资源很紧，不加入MAX30102/STOP/OTA。
为满足预算，删去已完成的逐键/弱默认串口回显/ESP寄存器诊断，缩短DHT诊断文字；
没有裁剪边界校验、超时、安全态、任务栈、11KB堆、20条离线网络事件或正常20条最近日志。
审计表初版字符串元数据超60KB，改用LR+map/debug定位；只有专用审计版减少最近日志RAM缓存。

Keil可见调用链：worker网络最大1248B（NetTask栈2048B）；
HardFault打印链48B，独立应急MSP128B。仍存在工具注明的循环/函数指针不可追踪，
这些静态数据不能代替实际任务最小水位。本轮没有板上堆/栈水位或功耗测量，详见24h采集步骤。

| 镜像 | SHA256 |
| --- | --- |
| M8-app.hex | `428949EF4A44149A51296BAAD1553353C9C0E647E31243E381E7CA0B81293BB3` |
| M8-test.hex | `0B00727C998CE12ABF17554AED78A23F191CA06F3D52086F0E671375651E3EA0` |
| M8-audit.hex | `0A741298993FE9B5F981321D23AC31BEF852BD2CF154B4A451F6CB60084D2858` |

当前Objects镜像与M8-app.hex一致；测试/审计AXF留在tmp且不入Git。
确认接线后启用MQ须重新构建记录，不直接沿用这些哈希/资源。
额外编译核查两路MQ启用的正常配置：零错误/零警告，Flash61112B/RAM20152B，
相比宏0仅多76B Flash，仍余328B/328B。日志/map保留tmp/M8-mq-enabled-*；
没有烧录，最终宏已恢复0。这证明预算可用，不证明5V接线或传感器实测通过。

## 主要行为

- 预留接口改为MQ2 PA12/IO1、MQ135 PA11/IO2，NOPULL；AO断开避免SPI/风扇冲突，DO保持转换安全门禁。
- 遥测由5s改2s，保留旧字段顺序，再附mode/state/alarm/fault/q；温湿度两个质量均好才认为位0有效。
- 看板缺省sms01、可填实际dev；订阅telemetry/event/status/config/cmd/resp，5分钟曲线、40条事件上限，
  数据8s过期/离线不发送、QoS0不排队、不retained；执行器仅健康撤防可用，显示实际设备回执。
- 修复CIPSEND期间重编码快照可能改变packet长度的问题：保存AT覆盖的20B前缀，提示后恢复同一DMA报文。
  回归在提示前后温度9→1000，包长和原内容仍与CIPSEND声明一致。
- HardFault选择原MSP/PSP帧后切独立应急栈；先停输出，有界寄存器UART输出，无printf/RTOS锁。
  帧地址/对齐/stacking错误均检查，备份域记录cause与PC。
- 启动在清reset flags前记录IWDG原因与计数；RTC备份域切换恢复计数。故障计数不是无电池断电永久存储。
- 栈钩子记录cause2、TCB地址后安全停机；malloc记录cause3。生产保留钩子，去除guard注入代码。
- BURN每60s输出复位/重连/free/min heap/网络/drops，六任务每5s栈水位；
  W25Q成功校验后计16个日志扇区program/erase，本次启动计数，LogTask每秒报告一扇区。
- DWT专用构建覆盖普通临界区、FromISR、SysTick、PendSV，96点/8层嵌套/饱和及lost；
  512B RX清零移出临界区，DMA已停止。耗时和运行覆盖未获得板上证据。

## 创建结果检查

本轮核查实际调用位置，未为已有检查重复造封装，运行期不新增分配。

| 入口 | 对象/分配 | 失败处理 |
| --- | --- | --- |
| BSP/bsp_console.c | 日志mutex | NULL→初始化失败→安全panic |
| BSP/bsp_controls.c | keys/edges/beeps队列、软件timer | 全部判NULL；timer start也检查pdPASS |
| BSP/bus_arbiter.c | SPI/FAN mutex | NULL→init -1→storage_setup失败→安全panic |
| App/datahub.c | 快照mutex | NULL→init -1→安全panic |
| App/storage.c | 请求queue、view mutex | 任一NULL→setup -1→安全panic |
| App/tasks/app_tasks.c | heartbeats event group、六task | NULL/pdPASS逐项检查；调度器返回也panic |
| FreeRTOS/tasks.c / timers.c | idle/timer task与内部timer queue | 内核检查创建结果/configASSERT，heap_4失败调用malloc钩子 |
| 业务代码 | malloc/pvPortMalloc | 没有直接调用；固定缓冲及既有堆对象复用 |

panic锁存安全态、停止喂狗；不把失败对象继续用于任务运行。内核原实现不做无关修改。

## 本轮验证记录

| 命令 | 本轮实际结果 |
| --- | --- |
| python Docs/test_m8.py | 普通/审计两变体；实际fault帧、SRAM/对齐/stacking检查、真实FreeRTOS mode2 guard、malloc、BKP/DWT回绕/容量通过 |
| python Docs/test_m6.py | TCP-MQTT、20事件、命令边界、质量/字段顺序与CIPSEND快照跨提示稳定通过 |
| python Docs/test_m7.py | T-01～T-12时长/优先级/故障/热加载、50轮模型、真实派发/回执通过 |
| python Docs/test_storage.py | 正常/配置/压力三个变体、逐字节掉电、KV500、20条缓存与风扇等待通过 |
| python Docs/test_m2.py | Manual Test双向/回绕、OLED故障与报警popup、console锁通过 |
| python Docs/test_m3.py | 新PA12/11映射、MQ门禁/启用模型、DataHub质量通过 |
| python Docs/test_m5.py | 执行器安全、限速、双向各100仲裁模型通过 |
| python Docs/test_uart.py | DMA/IDLE二进制分帧、200错误汇总与恢复通过 |
| python Docs/test_flash_bus.py | HAL抢占/失败闭锁、JEDEC/仲裁、成功磨损计数与失败不计通过 |
| python Docs/test_dht_handshake.py / test_ultra_mapping.py | 早响应/断线/回绕与PA15/PB3映射通过 |
| python Docs/test_dashboard.py --live | 桌面/390/320布局、曲线像素、所有按钮/回执/非法/过期/离线/换设备；独立WSS回环685/345/337ms通过 |
| python Docs/audit_m8.py --self-test | Thumb/LR/特殊键/源地址范围及HAL完整清单通过 |
| python Docs/m8_burn.py --self-test | 合成24h与短/缺/断/复位/终止故障/非法证据拒绝通过，非实际烤机 |
| 3秒只读采集 | 生命周期正常，report正确为pass_24h=false，未向物理设备发送命令 |
| Keil M8/M8-test/M8-audit | 全量零错误/零警告；最终正常宏0，map无注入/DWT符号 |

浏览器图片在tmp/m8-dashboard，已人工检查桌面与320px无遮挡/横向溢出；
agent-browser辅助无障碍检查零violations，一项空事件表检查需人工，未误报全覆盖。
所有主机回归为实际C配边界桩，不等于SPI电气波形/执行器动作/传感器精度实测。
当前只有COM1/COM2，没有本轮STM32下载、手机操作或24h/临界区板测证据。
EMQX公共broker不带本项目设备访问隔离，只用于实验演示，不视为生产远程控制安全边界。
