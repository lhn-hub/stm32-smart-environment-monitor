# 附件 A：M0 修订配置

MCU STM32F103C8Tx；sms-hub；MDK-ARM V5；HAL；仅复制必要库；外设.c/.h分文件；不由CubeMX生成FreeRTOS。

## 关键配置页
- SYS：Serial Wire；Timebase TIM4；NOJTAG保留SWD。
- RCC：HSE8MHz晶体，PLL×9；SYSCLK/AHB/APB2=72MHz，APB1=36MHz，ADC=12MHz，USB时钟48MHz但禁用USB外设；LSE32.768kHz供RTC。
- TIM4：HAL时基专用，1MHz计数、PSC=71、ARR=999、更新1kHz；NVIC4。不得由另一个MX_TIM4_Init重复初始化；HAL_Init早期按当前时钟计算PSC。
- NVIC：分组4；TIM4=4；EXTI0/4/15_10=5；USART3和DMA1_CH2/3=6；DMA1_CH1=10；SysTick/PendSV=15；子优先级0；EXTI15_10先分流PA15。PB15轮询10ms。不启用无用户的EXTI9_5。
- TIM1：内部时钟、CH1 PWM PA8，PSC=71、ARR=19999、CCR初始500；M0不启动输出，M5查MOE。
- SPI1：全双工主机8bit，Mode0，MSB，软件NSS，PA5/6/7，预分频8=9MHz；PA4软件管理，默认低。
- TIM2：PA1/2/3 CH2/3/4 PWM；PSC71、ARR999。
- TIM3：PB1 CH4 PWM；PSC0、ARR3599、CCR0；严禁CH1 PWM。
- I2C1：PB6/7，400kHz，7位地址；OLED0x3C。
- USART1：PA9/10，115200 8N1。
- USART3：PB10/11，115200 8N1；RX DMA1_CH3循环512B，TX DMA1_CH2普通；IDLE在后续网络初始化中手工开启。
- ADC1：CH16/17顺序扫描、连续、软件触发，239.5周期；DMA1_CH1循环半字；M0仅生成配置。
- RTC：LSE。F1 RTC为计数器型；日期/编译时间种子由后续应用管理，不冒充硬件日历。
- IWDG：CubeMX不启用；代码后续初始化，64/2500，LSI标称40kHz。

## GPIO
PA0/PC13上升沿EXTI上拉（高有效以实测为准）；PB15输入上拉；PB3输出低；PA15双沿EXTI；PB4上升沿EXTI上拉；PA11/12双沿EXTI上拉；PB0/PB14/PB12输入上拉；PB5/PB13/PA4输出低；PB8/PB9输出高。PA4~7归仲裁器；PB0不切输出。

## Keil
Arm Compiler6：C99、-Os、One ELF Section per Function、MicroLIB；armlink --remove；Flash64KB/RAM20KB，最终固件≤60KB。生成map、保留完整日志。CMSIS-DAP，SWD，Flash Download Reset and Run。

## M1/M4/M5约束
原生FreeRTOS API；TIM4回调仅HAL_IncTick；候选校准来源确认前禁用，VDDA标称回退、温度FAULT；单点温度使用负斜率。KV和日志以bsp_pin.h中已修正边界为准。TB6612 A通道不驱动，SPI/风扇切换要实测，无实测不宣称动态安全已验证。
