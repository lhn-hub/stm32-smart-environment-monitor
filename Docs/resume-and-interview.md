# 简历描述与面试问答

## 中文简历条目（5 条）

1. 基于 STM32F103C8T6 + FreeRTOS 设计智能环境监测与安防联动系统，拆分采集、DataHub、报警、网络、菜单、日志六类任务并配置优先级、栈水位和看门狗监控。
2. 实现 W25Q64 双 Bank KV 存储、CRC/提交标记/读回校验和事件环；通过 SPI 总线仲裁解决 Flash 与 TB6612 风扇共享 PA4～PA7 的切换安全问题。
3. 使用 DMA+IDLE 接收 ESP8266 AT 数据，完成 TCP 承载 MQTT、URC 分发、状态机重连、LWT、遥测、事件、下行命令和 20 条断网缓存。
4. 实现火灾、入侵、空气三级报警状态机，采用 MQ 持续确认、雷达+超声双鉴、优先级抢占和 10 秒恢复，故障或数据过期时进入安全态。
5. 在 64 KB Flash/20 KB RAM 约束下完成单文件 MQTT 看板、HardFault/栈溢出诊断和临界区审计；最终生产镜像 61416 B Flash、20256 B RAM，Keil 0 错误 0 警告。

## English Resume Bullets (5)

1. Designed an STM32F103C8T6 and FreeRTOS smart-environment security hub with dedicated sensing, DataHub, alarm, networking, UI, and logging tasks, including watchdog and stack-watermark supervision.
2. Implemented a dual-bank W25Q64 KV store with CRC, commit markers, read-back verification, and an event ring; protected the shared PA4–PA7 SPI/TB6612 bus with a priority-aware arbiter.
3. Built an ESP8266 DMA+IDLE AT transport and a TCP-carried MQTT 3.1.1 client with URC dispatch, reconnect backoff, LWT, telemetry, events, downlink commands, and a 20-entry offline cache.
4. Implemented fire, intrusion, and air-quality alarm state machines using persistence timers, radar-ultrasonic dual confirmation, priority escalation, delayed recovery, and fail-safe sensor-quality handling.
5. Delivered a single-file MQTT dashboard, HardFault/stack-overflow diagnostics, and critical-section audit within a 64 KB Flash / 20 KB RAM budget; production build is 61,416 B Flash and 20,256 B RAM with zero Keil warnings.

## 20 个面试问答要点

1. **FreeRTOS 优先级怎么分？** 监控 5、报警 4、网络 3、采集/菜单 2、日志 1；高优先级只做实时状态和安全动作，Flash 日志放低优先级队列。
2. **如何避免优先级反转？** 共享 SPI/控制资源使用 FreeRTOS mutex 的优先级继承；不在 ISR 中拿互斥锁，网络事件先入 RAM 环，日志异步落盘。
3. **为什么 Flash 只剩 24 B 还不继续加功能？** 61416/61440 B 已越过“可维护余量”边界；先删蜂鸣器和诊断字符串，再考虑功能裁剪，二期必须先做 map 预算。
4. **KV 双副本怎么保证掉电安全？** 两个 4 KB Bank、512 B 槽位；先写除 commit 外字段和 CRC，再写 commit marker，最后整记录读回验证；不确定时停止写入并保留最后确认值。
5. **磨损均衡怎么做？** 有效配置在两个 Bank 之间轮换，Bank 满后只擦除非当前 Bank；事件日志使用 16 个扇区轮转并统计 program/erase。
6. **SPI 总线仲裁解决什么？** PA4/5/6/7 同时承载 W25Q64 和 TB6612 相关信号；切换时先 CCR=0、STBY=0、关 SPI，再重配 GPIO，释放时回到安全态。
7. **超声波如何计时？** PA15 输出约 12 us TRIG，PB3 EXTI 捕获上升/下降沿，用 TIM4 1 MHz 计时，30 ms 超时并做 5 样本滤波。
8. **NOJTAG 为什么保留 SWD？** F1 的 PB3/PB4/PA15 被超声和雷达使用，只关闭 JTAG-DP，保留 SW-DP，仍可调试下载。
9. **DHT11 为什么需要临界区/时序保护？** 起始低电平约 20 ms，随后释放为输入并在 EXTI 采集微秒脉宽；ISR 只记录边沿，任务上下文解析，避免在中断里延时。
10. **MQ 预热怎么处理？** 模块硬件先预热至少 5 分钟；软件布防还有 150 s warmup，预热标记未解除前不允许报警联动。
11. **双鉴如何去误报？** 雷达有人和距离严格小于阈值必须同时持续 2 s；单独挥手或单独近物都不能触发。
12. **TIM 如何分配？** TIM1_CH1/PA8 舵机 50 Hz；TIM2_CH2~4 RGB；TIM3_CH4/PB1 风扇 PWM；TIM4 1 MHz HAL tick/微秒基准。
13. **TB6612 真值表怎么用？** B 通道启动前 STBY=0、CCR=0，配置 BIN1=1/BIN2=0 后再 STBY=1；停止先归零再 STBY=0；A 通道/PWMA 永不初始化。
14. **DMA+IDLE 如何处理粘包？** USART3 DMA 写环形缓冲，IDLE 中断只冻结长度/位置并通知任务；任务按行、`+IPD` 长度和 MQTT 剩余长度解析，不依赖固定 delay。
15. **MQTT 状态机怎么防止卡死？** INIT→RESET→AT_TEST→入网→TCP→MQTT→ONLINE，每步最多三次，五轮失败退避 60 s；本地采集/报警不依赖网络。
16. **LWT 如何验证？** CONNECT 时设置 `/status` 的 offline，成功上线发布 retained online；直接断电只看 broker 收到 LWT，不能用正常 reboot 伪造。
17. **喂狗策略是什么？** MonitorTask 汇总六任务心跳、堆和栈水位，全部健康才喂 IWDG；发现异常进入安全态并停止喂狗，让复位原因可追踪。
18. **I2C 锁死怎么恢复？** 先停 I2C 外设，PB6/PB7 改开漏，最多输出 9 个 SCL 脉冲，再发 STOP，重新初始化并扫描；失败保留 OLED 故障位。
19. **如何处理传感器拔线？** DHT/超声可由通信超时、质量和过期检测；单 DO 的 MQ/雷达无法可靠区分拔线与正常高电平，必须把这一限制写进验收报告。
20. **如何证明不是只跑了模型？** 分开保存主机测试日志、Keil map、串口原文、OLED/MQTTX/动作照片和 24 h 报告；没有实物证据的项只能标为待板测。
