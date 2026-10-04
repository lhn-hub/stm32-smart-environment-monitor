# M1 入口核查

更新：用户随后已提供M0 Rebuild 0错误0警告、Erase/Programming Done、Verify OK、Application running日志，并再次要求继续M1。已继续实现，当前以Docs/M1-acceptance.md为准；以下保留首次入口核查历史。

M1明确依赖M0完成。本轮只检查入口条件，尚未修改M0固件。

## 当前证据
- M0提交：58edba6；编译0错误0警告，Flash5824B/RAM1392B。
- 当前已连接USB-SERIAL CH340 (COM6)。当前设备名称筛选未发现CMSIS-DAP/ST-Link/J-Link/ULINK；这不是对所有HID探针的绝对排除。
- 尚无M0下载成功、Reset and Run、独立复位运行的实测证据；v0.1尚未创建。

## 已有裁决继续有效
- HAL时基TIM4，不能使用本芯片不存在的TIM6；M1末尾TIM6视为旧模板遗留。
- 校准地址仅诊断候选且默认禁用。内部温度quality=FAULT，VDDA可标称估计。不得把FAULT伪装成30~60℃；DoD③温度范围验收在可信校准来源确认前无法满足。
- EXTI15_10先处理PA15，再处理其余PR位；ISR只记录事件，任务做消抖。
- 不主动更换固定硬件/引脚。业务模块和电机断开，OLED只作临时I2C扫描验证。

## M0需用户实测的最小动作
1. DAP连接SWDIO/SWCLK/GND和正确目标电压参考；CH340连接COM6对应USB口，上方USB Device口保持不接。
2. 打开MDK-ARM/sms-hub.uvprojx，Debug选择实际探针/SWD；Flash Download勾选Reset and Run，执行下载/校验。
3. 记录下载日志；运行后暂停，确认停在main空循环，uwTick变化，TIM4 PSC=71/ARR=999；AFIO SWJ_CFG=010。
4. 独立复位再次确认运行。NOJTAG不会禁用SWD；若连接失败，可在确认NRST接线后尝试Connect under Reset。
5. 提供下载及复位现象后，完成M0验收并标记v0.1，再开始M1实现。

## M1测试不能由主机构建代替
三键各5次、实际VDDA、OLED在线扫描、USART3回环、30分钟运行和栈余量、故障注入看门狗复位均需实物。IWDG“4s”是LSI标称近似值；任务故障检测延迟和LSI容差必须单独记录，不把标称周期当作实测上界。
