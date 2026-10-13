# M8 关中断临界区审计表

本表由实际源码清单与 Keil map/debug 生成。没有串口 CRIT 记录时，耗时均为待测；静态审查不能代替板上时长。
BASEPRI=0x50 屏蔽优先级 5–15，TIM4=4 继续运行。生产构建无测量钩子。
审计构建使用 DWT 72MHz，记录每个调用点的最大微秒数（含嵌套和钩子开销，向上取整并加 200 cycle 退出余量）。
FromISR 地址为外层 API 调用点；编译器合并/outline 可能共享调用点，不把一条记录推断为多个源位置均已覆盖。
最多 96 个调用点、嵌套 8 层；n 饱和 65535，max_us 饱和 65535。lost 必须始终为 0；未覆盖分支仍需专项触发。
上电建对象在调度器启动前未计时，需逻辑分析仪补测；异常终止时的有界 UART 输出不属于正常运行临界区。
Flash tx/rx 使用 vTaskSuspendAll，IRQ 仍开；600ms 擦除等待、DHT20ms起始低、ESP100ms复位均不属于全程关中断。
未编译 configUSE_TICKLESS_IDLE，port.c 中 cpsid i 的 STOP/tickless 分支不在当前运行路径。

HAL I2C 的8处 PRIMASK 入口位于 Master_Receive/Mem_Read；当前 map 中这两函数均被链接器移除，正常路径仅扫描和 OLED 发送；将来启用接收必须重新审计。

## 每处源码入口

| 位置 | 操作检查 | 板上最大耗时 |
| --- | --- | --- |
| `App/alarm_engine.c:108` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/alarm_engine.c:114` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/alarm_engine.c:119` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/alarm_engine.c:129` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/alarm_engine.c:133` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/alarm_engine.c:158` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/app_time.c:44` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/app_time.c:50` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `App/tasks/app_tasks.c:140` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `BSP/bsp_analog_rtc.c:36` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `BSP/bsp_bus.c:84` | RX状态/错误交换；512B清零已移到临界区外 | 待板测 |
| `BSP/bsp_bus.c:135` | RX状态/错误交换；512B清零已移到临界区外 | 待板测 |
| `BSP/bsp_bus.c:157` | RX状态/错误交换；512B清零已移到临界区外 | 待板测 |
| `BSP/bsp_controls.c:27` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `BSP/bsp_controls.c:51` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `BSP/bsp_controls.c:125` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `BSP/bus_arbiter.c:34` | 短 GPIO 重配与计数；无 SPI 传输/等待 | 待板测 |
| `BSP/bus_arbiter.c:39` | 短 GPIO 重配与计数；无 SPI 传输/等待 | 待板测 |
| `BSP/bus_arbiter.c:65` | 短 GPIO 重配与计数；无 SPI 传输/等待 | 待板测 |
| `BSP/bus_arbiter.c:80` | 短 GPIO 重配与计数；无 SPI 传输/等待 | 待板测 |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:1254` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:1272` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:1326` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:1368` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:2745` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:2763` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:2816` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c:2858` | I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径 | 未链接（无运行耗时） |
| `Drivers_Device/cs100a/cs100a.c:14` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/cs100a/cs100a.c:32` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/cs100a/cs100a.c:37` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/dht11/dht11.c:101` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/dht11/dht11.c:111` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/ec11/ec11.c:54` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/sg90/sg90.c:14` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/sg90/sg90.c:22` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/sg90/sg90.c:29` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/tb6612_fan/tb6612_fan.c:28` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/tb6612_fan/tb6612_fan.c:36` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/tb6612_fan/tb6612_fan.c:46` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/tb6612_fan/tb6612_fan.c:56` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Drivers_Device/w25q64/w25q64.c:62` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:139` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:148` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:175` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:189` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:203` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:217` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/esp8266/esp8266.c:1054` | 短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待 | 待板测 |
| `Middleware/FreeRTOS/event_groups.c:271` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/event_groups.c:418` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/event_groups.c:471` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/event_groups.c:509` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:203` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:363` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:412` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:421` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:444` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:494` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/ARM_CM3/port.c:555` | 内核屏蔽/启动/切换；区分未编译 tickless | 待板测 |
| `Middleware/FreeRTOS/portable/heap_4.c:483` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:241` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:261` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:542` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:761` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:981` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1151` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1303` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1448` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1556` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1607` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1663` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1810` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1903` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1936` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:1953` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2179` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2260` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2295` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2335` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2381` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2404` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2458` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2485` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2791` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/queue.c:2839` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1081` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1166` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1401` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1483` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1523` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1558` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1708` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1756` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1862` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:1931` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2047` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2104` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2208` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2653` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2898` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2920` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:2945` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:3257` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:3281` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:3652` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4291` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4639` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4670` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4707` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4743` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4792` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:4926` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:5054` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:5121` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/tasks.c:5152` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/timers.c:447` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/timers.c:468` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/timers.c:946` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/timers.c:1000` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/timers.c:1024` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |
| `Middleware/FreeRTOS/timers.c:1040` | 内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖 | 待板测 |

## 已采集调用点

| LR/特殊键 | 函数 | debug 位置 | n | 最大 µs | lost |
| --- | --- | --- | ---: | ---: | ---: |
| 无硬件记录 | 待采集 | — | — | — | — |

已采集调用点是否全部 ≤2ms 且 lost=0：**未通过/未测**。
DoD④需每条正常运行路径都有实测证据；不得因本表没有超标数值就写成通过。

## 采集

烧录 M8-audit.hex 后，覆盖菜单/云端动作、软件定时器、DHT/超声波/USART DMA ISR、重连、KV保存、Flash失败与恢复。
保持串口日志至少覆盖一次完整96条轮巡。不要用审计固件烤机或发布，它的最近日志RAM缓存为4条且不统计磨损，网络离线事件环仍20条。
用同一审计构建 AXF 导出 debug；LR、map、debug 必须来自同一镜像，不能用正常固件 map 解释。
命令见 M8-acceptance.md。
