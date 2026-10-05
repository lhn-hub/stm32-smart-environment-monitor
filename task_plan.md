# sms-hub M0 工程脚手架

## Goal
按用户 M0、附件 A/B 完成 CubeMX/Keil 工程，真实编译与 DAP 验证后提交并标记 v0.1。

## Current Phase
M3软件驱动、sensor_ops/DataHub、菜单联动及主机回归已完成；编译0错误0警告。MQ正式禁用，物理精度/时序与开路诊断限制仍未验收。

### Phase 1: 工具链与硬件约束核查
**Status:** complete
- 已读取用户附件并列出手工硬件检查清单。
- 已确认芯片无 TIM6，共享 EXTI 向量不支持按线路独立分配 NVIC 优先级。
- 已检测 Keil、CubeMX、Git；固件包位置与 DAP 实物连接仍需确认。

### Phase 2: 生成工程与配置集中化
**Status:** complete
- 在用户确认冲突处理后生成工程、建立目录与配置头文件。

### Phase 3: 编译、下载、提交与验收
**Status:** complete
- 保留 0 warning 编译日志、map 数据和 DAP 自动运行证据。
- 完成后提交 [M0] 工程:建立脚手架，打 tag v0.1。

## Next Step
用户烧录M3检查DHT/距离/雷达和统一日志；MQ保持断开。安全电平问题及实物验收通过后再创建v0.4。

### Phase 6: M3传感器与DataHub
**Status:** in_progress
- 软件实现完成：六实例、DataHub互斥、5样本滤波、故障质量、阈值菜单同步及实时主页。
- DHT采用EXTI12脉宽捕获；超声PA15/30ms超时；共享ISR先超声再DHT再按键。
- 主机协议/超时/回绕/质量/恢复/MQ门控/菜单回归通过。未模拟为真实50次采样成功率或±1cm实测。
- MQ DO硬件冲突和单DO开路不可观测限制保留；正式固件禁用两路MQ并标FAULT。

### Phase 5: M2 OLED、EC11与菜单
**Status:** complete
- 静态1KB framebuffer、脏页刷新、ASCII/中文小字库。
- 编码器边沿快照通知，任务上下文4态解码；菜单3级栈。
- 编译/主机验证后交付上板20格、帧率、恢复测试；v0.3待验收。

### Phase 4: M1 BSP与FreeRTOS骨架
**Status:** complete
- 用户已提供M1完整任务，明确依赖M0完成。
- 用户补充M0下载成功日志后继续；旧入口核查已过时，以本节为准。
- 保留TIM4及校准默认禁用裁决；M1温度范围DoD仍存在冲突，待明确验收标准。

## Errors Encountered
- 初次技能读取输出被截断，后续分段补读。
- 初次工具检查将不同类型对象混合表格输出，后续改用 JSON/完整路径。
- 初次设备过滤 DAP 误匹配 Adapter，后续使用精确标识过滤。
- CubeMX 配置搜索范围过大命中数据库元数据，输出截断；不依据该搜索断言固件包安装状态。
