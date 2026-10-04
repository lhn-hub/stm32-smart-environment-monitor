# sms-hub

最新状态：2026-10-05用户确认M1验收完毕，发布`v0.2`。保留温度FAULT、VDDA标称估算等已说明限制；下方待验收说明为此前交付记录。

基于 FreeRTOS 的智能环境监测与安防联动系统。当前 M1：BSP与FreeRTOS骨架已实现，编译/主机测试通过，物理DoD待验收。下方M0章节保留作基线说明。

## M1快速开始
关闭旧Keil工程窗口后重新打开 `MDK-ARM/sms-hub.uvprojx`；运行 `Docs/build.ps1` 默认重编译M1。六任务保留原栈/优先级，heap_4=11KB，HAL时基TIM4。
接线、每项DoD步骤和预期串口文字见 `Docs/M1-acceptance.md`。COM6 115200 8N1；业务传感器和电机不接。校准源未确认，温度显示FAULT，VDDA标称估算；不能把30~60℃项标为通过。
构建日志 `Docs/M1-build.log`，map `Docs/M1-map.txt`，主机结果 `Docs/M1-tests.log`。30分钟和看门狗复位时间没有实测，暂不打v0.2。正式构建中故障注入宏为0；测试版步骤见验收文档。

## 工具链
- Keil MDK 5.43.1，Arm Compiler 6.24；CubeMX CLI 6.18.1；STM32CubeF1 v1.8.7；Git2.54.0。
- 官方Cube包来源 https://github.com/STMicroelectronics/STM32CubeF1/tree/v1.8.7 ，HAL/CMSIS固定子模块提交见Docs。
- `MDK-ARM/sms-hub.uvprojx`：C99、-Os、函数/数据分段、MicroLIB、armlink --remove。
- Cortex-M3、64KB Flash、20KB RAM；最终固件Flash红线60KB。

## 编译
打开MDK工程，Rebuild；完整结果见 `Docs/M0-build.log`，map归档 `Docs/M0-map.txt`。
PowerShell可运行 `./Docs/build.ps1`。生成器源码已做配置集中化及最小main后处理；不要直接重新生成覆盖这些修改。重新生成后必须重新应用差异并复跑静态核查，当前不承诺一键无损再生成。

## 接线与下载
1. 核对核心板STM32F103C8T6与扩展板V1.0.2；上方USB Device口禁止插线，PA11/PA12留给EC11。
2. M0仅接DAP的SWDIO/SWCLK/GND与正确电压参考，以及CH340对应USB。保持外设/马达断电。
3. 后续外设跳帽VM=5V、通讯接口VCC3=3V3；DC12V+USB双供电按原板说明核对，禁止把不同电源输出互接。
4. Keil Debug选CMSIS-DAP，Settings中确认探针序列号和SWD；Flash Download确认64KB目标算法、Erase/Program/Verify以及Reset and Run。已保存DAP相关默认配置，但没有探针，尚不能验证驱动参数生效。
5. 下载、校验、复位后观察main/uwTick；连续两次读取uwTick应递增；TIM4 PSC=71、ARR=999、DIER.UIE=1、NVIC优先级4；AFIO.MAPR SWJ_CFG=010。断开调试后复位再次验证独立运行。

## M0可见现象与边界
最小main只HAL_Init、SystemClock_Config、bsp_board_init、空循环。RGB三个GPIO静态低，亮灭取决于板卡真实极性，尚未实测。无串口初始化/打印，无OLED初始化，因此本阶段没有启动串口原文或OLED画面承诺。
TIM4中断仅增加HAL tick；SysTick不调用HAL_IncTick，M1接入FreeRTOS。其他外设初始化函数保留编译，但M0不调用；不运行电机、舵机、ADC或网络。
微秒函数基于tick/CNT/UIF重读；只允许能被TIM4抢占的上下文。delay_us仅线程、非临界区且小于1000us；长等待使用调度/状态机。M0未启用IWDG是附件要求，M1必须加入看门狗，不能当作功能修复删除。

## 兼容性与校准
三个地址仅诊断候选，校准默认禁用，非0/非FFFF不是数据来源证据。M1来源未确认时VDDA用标称1200mV估算，内部温度quality=FAULT；串口/System页明确路径。经验证的30℃单点计算用负斜率；禁止直接解引用候选地址作为温度校准。

## 资源与后续验收
M0基线：Flash5824B，静态RAM1392B（含启动栈/库区），初始空工程增量相同；完整最终map为准。应用六栈共7424B、heap_4 11264B预算尚未接入，不能据M0空闲量宣称最终RAM闭环。
微秒函数主机边界测试通过不替代逻辑分析仪精度测试。PA4~7电气切换/A通道无驱动和TIM1 MOE留待M4/M5实测。
M1最低手工清单：保留DAP、CH340、核心板电源；确认HSE/LSE运行与复位；目前未收到M1具体任务，不擅自要求插接业务外设。

## 阶段记录
M0 DoD：编译通过、NOJTAG位置和引脚配置可查；DAP下载/自动运行待实物；首次提交后保留v0.1标签待上板验收。详见Docs/M0-static-review.md。
