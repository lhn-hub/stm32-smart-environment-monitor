# sms-hub 工作记录

## RGB修正与中文日志
- 用户确认低有效方案，已改TIM2极性LOW、GPIO熄灭为高，保留0灭/999最亮。
- 中文UTF-8任务显示名/等级/正常与错误日志，英文内核任务名不变。
- 0错误0警告，Flash31416B/RAM14552B；控制与UART主机测试通过，ELF中文UTF-8字节验证通过。
- UART测试脚本Windows默认GBK读源码失败，已显式UTF-8读写后通过。
- 用户现场日志栈余量全部>30%，堆1208B；VDDA3558mV超范围，仅记录，不修改估计值。

## RGB常亮排查
- 已接收实物反馈并核对原理图第3页LED电路，确认低电平点亮；当前PWM高极性/CCR0导致常亮。
- 已追踪rgb_set、初始化与安全态，提出仅改变TIM2极性和GPIO熄灭电平的修订；等待用户按原硬件裁决规则确认，未改代码。

## M1实施
- 最终普通固件：Code30244/RO620/RW20/ZI14532，Flash30884B/RAM14552B；M0增量25060B/13160B。
- GPIO/按键/RGB/蜂鸣器实际源码模拟、USART3实际函数与微秒回绕主机测试通过；无物理DoD通过声明。
- 已归档M1构建、map、故障注入构建、测试日志和验收操作表；v0.2待真实硬件验收。
- 用户提供M0实际0警告编译、Erase/Programming/Verify OK及Application running日志，并再次要求M1，继续开发。
- 加入固定FreeRTOS10.3.1、CM3 GCC/ArmClang兼容端口、heap_4=11264B；六任务及timer/idle系统任务。
- 日志、控制、ADC/RTC、I2C和USART3服务已实现；候选温度校准保持禁用，温度FAULT不伪装正常。
- 时基继续TIM4，不使用旧模板TIM6；共享EXTI优先级5，PA15优先。
- 主机实际控制实现测试通过；实际UART解析函数测试通过；M1编译0错误0警告。
- 故障态新增永久锁定防止Timer再次开启执行器；UART重启后丢弃旧DMA索引。
- 上板30分钟、每键5次、OLED、回环和看门狗实测未完成；不标v0.2。

## M1请求入口核查
- 已接收M1，按其“依赖M0完成”先核查当前状态。
- 当前CH340 COM6存在；没有M0下载/独立运行证据，未创建v0.1。
- 未修改固件；新增Docs/M1-entry-check.md，列出真实上板验收步骤和仍需解决的温度DoD冲突。

## 2026-10-05 修订继续
- 最终Arm6.24重编译0错误0警告；Flash5824B、RAM1392B；配置断言与微秒边界测试通过。
- CubeMX生成后集中引脚/业务参数，最小main不启动外设；独立保留初始化函数。
- 修复生成项目中的重复源文件、路径及C99选项；更正舵机初始CCR=500。
- I2C生成器元数据警告保留，源码已核对400kHz；无实际I2C总线验证。
- 无DAP连接证据，下载/Reset and Run未验收；不打v0.1。
- 用户确认TIM4/NVIC/独立KV扇区/SPI除8/TIM1 MOE方案。
- 日志范围笔误已获确认改为0x003000~0x012FFF；校准来源确认前禁用，单点负斜率。
- 已新增BSP/bsp_pin.h、Docs/attachment-A.md、Docs/M0-static-review.md及RM0008证据文本。
- CubeMX CLI实际运行6.18.1，基础IOC已生成；Arm Compiler6.24可运行。
- 获取官方STM32CubeF1 v1.8.7及固定CMSIS/HAL子模块，仅在tmp下作生成来源。
- 保留用户新宏名，CAL_ADDR_INVALID地址解引用改为检查已读取值的函数，防止误称校准有效。
- 遇到并修复Java -D参数PowerShell解析问题；PDF改用已安装pypdf/Poppler；输出Unicode改UTF8；二进制安装元数据不可按文本读取，未据此确定版本。

## 2026-10-05 M0
- 应用 Ponytail full，读取 planning-with-files 和 stm32-freertos-developer 技能及 CubeMX 参考。
- 读取完整 M0 与附件 A/B；先向用户列出手工检查清单。
- 检查空白工程工作区、工具注册记录和 Git 版本。
- 在线读取 ST 官方 stm32f103xb.h，确认无 TIM6 及共享 EXTI15_10 向量。
- 记录引脚电平、KV 同扇区擦除、SPI 时钟与 ADC 校准地址问题。
- 遵循用户“不得自行改方案”要求，停在前置核查，仅新增三份计划记录，未改硬件/固件方案。
- 待用户确认后继续工具链、CubeMX 工程生成与 M0 验收；无编译、下载、串口、OLED 或 map 成功声明。
- 核对真实 EXE：Keil 5.43.1.0；CubeMX EXE FileVersion=6.18.1-RC2，与注册表 6.17.0 不符，已记录，待确认运行版本。
- 最后检查三份记录存在且非空；未发现匹配的已连接 DAP/CH340 设备。
