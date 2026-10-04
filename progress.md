# sms-hub 工作记录

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
