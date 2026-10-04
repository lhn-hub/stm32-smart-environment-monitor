# M0 前置核查记录

RGB低有效修订已获用户确认并实施：TIM2 LOW极性、GPIO高电平熄灭。之前“等待批准”记录为历史。中文日志采用UTF-8，编译及ELF字节检查通过，烧录后现象待用户验证。

## M1 RGB现场反馈与根因
用户反馈：烧录后RGB常亮，复位后依次闪烁再常亮。已视觉核对用户核心板原理图第3页：LED1/2/3阳极接3V3，阴极经1K到PA1/PA2/PA3，为低有效。当前TIM2 PWM1+HIGH使CCR=0输出低，rgb_set(0,0,0)实际点亮三路，和反馈一致。板级初始GPIO低也会点亮。
修订建议：TIM2三路OCPolarity改LOW，GPIO初始熄灭电平改SET；保持rgb_set(0)=灭、999=最亮及故障CCR=0语义，不改引脚。按用户硬件不符须等待确认的要求，本轮未修改固件，等待批准低有效约束。

## M1最新状态
M1代码已实现，0错误0警告；主机实际按键/蜂鸣器、USART3解析和微秒边界测试通过。FreeRTOS10.3.1固定内核，8个任务含6业务+idle+timer。M1软件可交付，上板DoD未通过，不打v0.2；详见Docs/M1-acceptance.md。
候选校准继续禁用；内部温度FAULT。IWDG4秒是标称约数，故障到复位还包括检测延迟和LSI容差。不得声称满足严格4s上界。
故障注入测试构建通过；恢复宏0后的正式ELF无m1_stall_task。恢复构建时Keil工程曾被覆盖回M0分组，修复后正式全量构建通过。

## 最新状态（下方首次核查保留作历史）
用户已确认：TIM4替代TIM6；共享EXTI优先级5；A通道不驱动；KV独立4KB扇区；SPI除8；日志末地址0x012FFF；三个校准地址仅诊断候选且默认禁用。
源码已由CubeMX6.18.1生成并后处理；Arm Compiler6.24完整重编译0错误0警告。Flash5824B，RAM1392B。见Docs/M0-build.log、Docs/M0-map.txt及Docs/M0-tests.log。
NOJTAG位于BSP/bsp_board.c及生成的Src/stm32f1xx_hal_msp.c；共享EXTI先PA15；主机边界测试与配置编译断言通过。
生成器遇到I2C参数元数据就绪警告；实际生成I2C源码为400kHz/Fast模式参数，编译通过。生成器提示不等于固件编译警告，日志完整保留。
DAP未检测到，尚无下载/自动运行实测；阶段不标完成，v0.1标签待上板验收。

核查日期：2026-10-05。以下为静态约束与本机软件检查，不是板上实测。

## 输入
- 用户附件：C:/Users/l/.codex/attachments/90f17c1a-2f5c-4deb-b6ab-3091bbaf6fec/已粘贴的文本.txt
- 工作区原有两份板卡 PDF 和一张 JPG，本轮未修改、未解析这些文件。

## 已核实的硬件配置冲突
1. ST 官方 CMSIS stm32f103xb.h 仅定义 TIM1~TIM4，没有 TIM6。附件 A 的 HAL TIM6 时基与 TIM6 IRQ 不能用于 STM32F103C8。
2. 同一头文件定义 EXTI15_10_IRQn，PA11/PA12/PC13/PA15 共用一个 NVIC 向量，不能各设优先级 12/5。
来源：https://github.com/STMicroelectronics/cmsis-device-f1/blob/master/Include/stm32f103xb.h
建议供用户审阅：M0 暂用 SysTick 作 HAL 时基；M1 用 TIM4 更新中断及软件扩展计数兼顾 HAL tick/us 计时，SysTick 交给 FreeRTOS。此建议会改变“不使能 TIM4 中断”，尚未实施。共享 EXTI15_10 统一优先级 5，ISR 保持短小，尚未实施。

## 依据用户连接与参数可推导的冲突
- PA6 同连 W25Q MISO 与 TB6612 PWMA。Flash 读数据时 PA6 必须允许变化，不能始终为 0；强制输出 0 会与 Flash 输出冲突。PA4=0 时 Flash 被选中，同时 TB6612 STBY=0，可令桥处于待机。因此可以讨论“A 通道始终不驱动”，但不能声称满足“PWMA 永远为 0”。用户未批准改变表述。
- PA5 同连 SPI SCK/AIN1，SPI 工作时不能满足附件 B 注释的 AIN1 固定 0。
- PB0 保持输入，不能为控制 AIN2 而驱动 MQ2 DO。
- 两份 KV 分别占 0x000000~0x0007FF、0x000800~0x000FFF，而附件 B 定义扇区为 4096 字节；两份落在同一擦除扇区。需要回收擦除时，两份会一起消失，双副本+CRC 本身不能保证断电原子更新。建议保持主地址，使用空闲 0x001000 扇区作恢复暂存；这属于新增布局用途，待确认。
- SPI1 在 APB2=72MHz，/4 是 18MHz，不是 9MHz；如目标为 9MHz，应 /8。未修改配置。
- 附件 B 的 VREFINT_CAL_ADDR=0x1FFFF7BAu 不能直接当作 F103C8 已验证校准数据；需要依据该芯片资料确认，不能直接解引用用于测量。
- 附件 B 缺少部分显式引脚宏（如 RGB、I2C、SPI 数据线、串口），而用户又要求完整集中定义；后续需要保留已确认映射并补充宏。
- M0 指定的最小 main 不启动串口/OLED，不能以串口启动文字或 OLED 显示作为本阶段已有证据。

## 工具链现场检查
- 注册表：Keil μVision 5.43.1.0，路径 D:/Keil_v5/UV4/UV4.exe。
- 注册表：STM32CubeMX 6.17.0，路径 D:/stm32/STM32CubeMX.exe。
- 进一步读取该 EXE 的 FileVersion 为 6.18.1-RC2，与注册表不一致；运行版本尚需通过 About/命令行确认，不能以注册表版本作为最终工具链基线。
- git --version：git version 2.54.0.windows.1。
- 默认 C:/Users/l/STM32Cube/Repository 未发现 STM32Cube_FW_F1_* 目录；不代表其它位置未安装。
- 已存在 KEIL USB 驱动包记录，不代表已连接 CMSIS-DAP 或可以下载。
- 精确筛选当前设备名称 CMSIS-DAP、DAP、CH340、ST-Link、J-Link、ULINK，未返回匹配项；尚未实测 DAP 下载。

## 验收状态
- 固件代码未生成，未编译，未烧录，无 map；Flash/RAM 增量未测得。
- NOJTAG 需求已确认，尚无实现位置可验收。
- bsp_pin.h 未生成，不宣称与附件 B 一致。
- M0 未完成，未创建阶段提交及 v0.1 标签。
- M1 具体任务未提供，暂建议仅核心板、DAP 与 CH340 调试连接，外设清单待 M1 明确。
