# M0 修订静态核查

2026-10-05。静态核查通过不等于上板实测通过。

| 项目 | 结论 |
|---|---|
| TIM4 | F103C8 支持；PSC=71、ARR=999，72MHz 定时器时钟得到 1MHz/1kHz；优先级4；回调仅 HAL_IncTick；无 TIM6 实现 |
| EXTI共享 | EXTI15_10 全部优先级5，PA15先处理；EXTI0/4=5；PB15每10ms轮询 |
| Flash | A=0x000000，B=0x001000，均4KB；0x002000预留；16扇区日志0x003000~0x012FFF。末地址修正已获用户确认 |
| SPI | APB2=72MHz；/8=9MHz；/4仅后续实测对比后考虑 |
| 校准地址 | 保留三个地址为诊断候选，默认禁用校准。用户已确认不能凭非0/非FFFF判断来源；不得把候选地址写成F103官方校准区 |
| TIM1 | PSC=71、ARR=19999；M5启动PWM后核查BDTR.MOE，M0不启动舵机 |
| PA4~7 | 已将约束替换为A通道不驱动；SPI期间STBY低；安全归还状态PA4/5/7低、风扇CCR零；不能在SPI读取时强制PA6低 |

## 微秒读取边界
仅 tick/CNT/tick 一致并不排除 CNT 已归零而更新 IRQ 尚未执行的硬件窗口。实现需额外读 UIF 并重采样；禁止在全局关中断、TIM4 IRQ 本身或优先级0~4 ISR内调用等待服务。FreeRTOS BASEPRI 临界区不屏蔽优先级4；PRIMASK仍会屏蔽。
HAL 初始化期间时钟先是复位时钟再变72MHz，HAL_InitTick 必须按当前定时器时钟重算 PSC；稳定运行时才是71。不能在 HAL_Init 阶段硬写71并宣称tick正确。

## 温度来源与公式
已读取 ST 官网 RM0008 Rev21 第235~236页、第1076~1078页与 STM32F103x8/xB 数据手册。RM0008温度公式为 `(V25 - VSENSE)/Avg_Slope + 25`；若未来获得可信30℃单点记录，对应 `30 + (TS_CAL1_mv - TS_DATA_mv)/4.3`。
双点换算还需校验ADC原始值非零、范围有效、两校准点不相等、斜率合理，并使用有符号中间值。候选地址非0/非FFFF不足以证明它们不是系统ROM指令。
当前默认：VDDA用1200mV标称估计；温度质量FAULT；M1串口/System页显示采用路径。诊断读取也需要确认目标存储映射。

## 工具链语义
本机 Arm Compiler 6.24，MDK5.43；用 -Os、-ffunction-sections/-fdata-sections 和 armlink --remove 对应 section GC；不能把 GNU ld 的 --gc-sections 生搬到 armlink。MicroLIB、C99、DAP Reset and Run 按项目设置。

## 尚须硬件验收
- PA6高阻释放和TB6612内部下拉的动态行为不能由互斥锁证明；M4/M5测AO1/AO2差分与SPI切换瞬态，B通道风扇不动不能替代A通道无驱动证据。
- MQ模块5V供电时DO电平与板上电路需要测量；不擅自改线。
- DAP下载、自动运行、晶振、GPIO和计时精度需实物。
