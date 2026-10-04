# M1 BSP与FreeRTOS交付及上板验收

## RGB极性与中文日志更新
用户确认低有效修订：TIM2 PWM极性LOW，GPIO熄灭电平高；rgb_set(0,0,0)三灯熄灭，自检红绿蓝各200ms。已编译验证，重新烧录后的现象待确认。
串口输出改为UTF-8中文；调试助手选择UTF-8文本、115200、8N1，不能用GBK显示。RTOS内核任务名仍保留英文以便调试，串口显示中文名。
旧文档中的英文例句用于说明逻辑；新版例如：`[170367][监控任务][信息] 运行正常 栈剩余=108/192字 (56%)`，`[170535][采集任务][信息] 芯片温度=不可用 校准=禁用 VDDA=3558mV(标称估算) RTC=509`。
用户提供六任务片段栈余量56%~82%、堆1208B、看门狗故障0；仅证明该时刻，不代表30分钟完成。VDDA3558mV超出要求3.20~3.40V，保持原始估计结果，需用万用表核对3V3实际电压；温度校准继续禁用。
本次构建Flash31416B、RAM14552B，相比修订前Flash+532B、RAM不变。UTF-8字符串已核对在ELF中，控制/串口主机回归通过。

## 状态
软件构建与主机逻辑验证通过，物理DoD待用户上板执行。M0下载日志由用户提供，含Verify OK/Application running。没有伪造M1下载、30分钟截图或v0.2标签。
用户已裁决：TIM4取代TIM6；三个校准地址只作候选，未确认前禁用。内部温度因此显示FAULT，DoD③的30~60℃尚不能通过，不将其降级为已通过。

## 变更文件与接口
- App/FreeRTOSConfig.h、App/app_config.h：FreeRTOS10.3.1，heap_4=11264B；6任务栈与优先级保持原要求；另有idle和timer系统任务。
- App/tasks/app_tasks.c：六任务、心跳表、启动就绪屏障、IWDG锁定、错误钩子；正式构建不含故障注入分支。
- BSP/bsp_services.h：所有服务接口和调用约束；bsp_board.c：72MHz断言、NOJTAG、安全电平与故障锁定。
- BSP/bsp_console.c：MicroLIB fputc；任务日志互斥；[tick][任务名][等级]；UART轮询有超时。异常或临界区禁止打印，故障原因保存在bsp_fault_reason，先关闭执行器再停机，IWDG不再喂。
- BSP/bsp_controls.c：TIM2 RGB三路、200ms序列；EXTI只入事件队列，timer任务每10ms采样/30ms消抖，长按1500ms且不额外产生单击；蜂鸣器复制请求、状态表非阻塞，所有回调不打印/分配/阻塞。
- BSP/bsp_analog_rtc.c：ADC循环DMA双半缓冲，每半缓冲两通道各16样本；VDDA标称估算；温度FAULT；RTC使用32位硬件秒计数，SensorTask独占set，不调用会24小时回卷的软件日历接口；LSE失败回退LSI。
- BSP/bsp_bus.c：I2C400k扫描和SCL至多9脉冲恢复；USART3环形DMA512B、IDLE IRQ只确认空闲，NetTask按换行组帧；TX复制到拥有的缓冲区、DMA超时中止。循环每10ms排空，超过30ms间隔保守丢弃到下一换行，避免把溢出数据作为完整行。
- Src/main.c / stm32f1xx_it.c：不在SystemClock阻塞等待LSE；TIM4继续HAL tick；SVC/PendSV/SysTick交给原生端口；故障中断先安全态，无ISR打印。
- Middleware/FreeRTOS：官方CubeF1 v1.8.7固定子模块cdc07b553ee9a9676988e5089a1d484da5d37c9c，非CMSIS-RTOS封装；保留许可。

## 1. 上电/RGB/安全电平
只接核心板、扩展板、电源、SWD和CH340；拔除电机驱动和业务传感器。上方USB Device口禁止连接。Keil Rebuild、下载校验、运行；CH340 COM6，115200 8N1。
预期红/绿/蓝各200ms后熄灭，串口出现：
```
[<tick>][boot][I] sms-hub 0.2.0-dev reset_IWDG=0
[<tick>][boot][W] ADC calibration DISABLED: candidate addresses unverified; TEMP=FAULT VDDA=nominal
```
RGB极性未实测；若颜色/熄灭状态与要求不符，记录板卡现象并停止，不擅自翻转极性。
表测PA4/PA7/PB13/PB5低，PB8/PB9高；风扇TIM3 CCR4=0且PWM未启动，PA5输出低、PA6输入、PB0保持输入。SPI外设已初始化后关闭，并将共享输出恢复GPIO安全态；M4接管时需要重配AF。

## 2. 三键
src=0 PA0，src=1 PC13，src=2 PB15。每键短按并释放5次，长按大于1.5s再释放5次；预期各5条CLICK和5条LONG，无重复事件，LONG释放不额外CLICK。
```
[<tick>][MenuTask][I] KEY src=2 action=CLICK tick=<tick>
[<tick>][MenuTask][I] KEY src=1 action=LONG tick=<tick>
```
PC13高有效待实测。PB15单击附带一次短蜂鸣；beep_pattern可在MenuTask临时调用验证多段开/关。正常键事件不在ISR打印。

## 3. ADC/RTC
每5s预期：`MCU_TEMP=FAULT calibration=disabled VDDA=<mV>mV nominal RTC=<seconds>`。
VDDA用万用表对比3V3，目标3.20~3.40V；标称VREFINT存在器件误差，超出不强行夹值。三候选地址不解引用；只有来源/映射可信后才能新增校准路径。当前不能验收温度30~60℃。
RTC预期`RTC source=LSE`；unix初值可能为0，未自动使用编译时间伪造已同步。通过SensorTask调用rtc_set(可信Unix秒)后，每秒递增；复位应保持备份域计数（电源条件允许）。LSE失败时告警`RTC LSE timeout; LSI fallback (approximate seconds)`，LSI秒不宣称晶振精度。

## 4. I2C
先不插OLED，上电扫描应`I2C scan count=0`；若SCL/SDA无上拉而浮空/拉低，可能报告stuck，须测线路，不能当作正常空总线。
断电插OLED（PB6/PB7、正确电源），上电应出现`I2C 0x3C`和count=1。不要带电改接。恢复函数在SensorTask调用，不允许与其它I2C客户端并发；恢复失败保留错误报告。

## 5. USART3回环
断开ESP8266，先断电，用跳线将PB10(TX)与PB11(RX)短接，恢复供电。固件每2s由NetTask发送 `sms-hub loopback\r\n`，CH340串口应出现：
```
[<tick>][NetTask][I] USART3 RX: sms-hub loopback
```
也可接3.3V逻辑USB-TTL：适配器TX→PB11，RX←PB10，共GND，禁止接5V逻辑或将两路TX短接。适配器串口115200 8N1发送分段文本，最后换行后才出现完整行。适配器单独TX/RX短接只验证适配器，不算MCU回环通过。
超长行(≥128字节)丢弃至换行，下一短行恢复；持续满速数据的吞吐压力测试尚未完成，超过轮询保障的间隔会报告gap并丢弃，不静默接受污染数据。

## 6. 30分钟和栈
串口保存完整日志至少30分钟，截图开始/结束与各任务stack_free行。预期每5s有六任务状态：
`[<tick>][MonitorTask][I] alive stack_free=<words>/192 (<percent>%)`
其余分母256/512/256/384/256。全部≥30%，无fault/reset/startup重复、无心跳故障。日志里的stack_free是真实uxTaskGetStackHighWaterMark，不是预填值。
启动后观察heap_free；Arm ABI核查TCB84B、队列72B、Timer40B、EventGroup24B；估算初始动态分配约10104B，heap仍约1KB余量，必须以实际xPortGetFreeHeapSize为准。不要额外创建大队列或格式化缓冲。
静态调用图worker已知调用深度约264B（间接调用/异常开销不保证完全计入），不替代30%上板验收。idle/timer系统任务也占heap。

## 7. 故意卡死/IWDG
正常版本`APP_WATCHDOG_TEST_ENABLED=0`。专用测试时改为1、Rebuild并烧录；等待所有任务状态和IWDG已启动后，在Keil Watch将`m1_stall_task=5`，继续运行（不要一直暂停CPU）。LogTask自挂起，不再更新时间戳/心跳。
Monitor检测超时后锁故障、关闭执行器且停止喂狗；复位启动应打印`reset_IWDG=1`。记录故障设置时刻、最后喂狗时刻和复位时刻；nominal reload2500/prescaler64在40kHz约4.0016s，LSI容差和最长1s心跳检测延迟使“故障到复位≤4s”不能凭配置保证。
测试后恢复0、重编译、确认ELF不含m1_stall_task，再烧回正式版。当前已验证开关可编译与默认关闭，尚未做实物故障注入，因此不能宣称钩子实测后移除完成。

## 8. 构建和资源
运行`Docs/build.ps1`默认生成M1日志/map，不覆盖M0证据。当前0错误0警告。
M0 Flash5824B/RAM1392B；M1 Flash30884B、RAM14552B，分别增加25060B/13160B；Flash距60KB红线剩30556B，RAM距20KB剩5928B。静态RAM包含整个11KB RTOS heap及MSP栈，勿再把动态任务栈重复相加。
官方生成器的初始化配置仍在IOC，M1应用/RTOS代码是手工集成；重新生成须检查差异，不能覆盖端口中断或候选校准策略。

## 常见坑
- HAL时基是TIM4，F103C8没有TIM6；优先级4禁止任何RTOS API。
- EXTI15_10按PR位由HAL_GPIO_EXTI_IRQHandler逐个分流，PA15先处理；其余只发事件。
- NOJTAG保留SWD；连接失败时先核对SWD/电源，NRST正确连接后用Connect under Reset。
- 临界区仅RAM状态位操作，目标几个指令，未测真实最长屏蔽时间；BASEPRI不屏蔽TIM4，但PRIMASK全局关中断会屏蔽。禁止在临界区/ISR延时、打印或擦Flash。
- malloc/stack钩子若在RTOS临界区不能按字面“先打印”；保留bsp_fault_reason，先安全态，允许上下文才有界输出，不破坏安全规则。

## 未完成项
实际GPIO极性、三键5次、VDDA、RTC切换、OLED扫描、USART3回环、30分钟截图/栈、看门狗时间均未实测。温度正常范围与已确认FAULT策略不兼容；严格4s上界也需测试/后续裁决。完成这些DoD后再标v0.2，不因编译通过提前标里程碑。
