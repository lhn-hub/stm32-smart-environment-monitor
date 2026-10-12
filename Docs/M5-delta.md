# M5增量说明

M5普通固件相对本轮开始时的DHT/Flash修复基线：Flash +2620B，RAM +160B。
完整资源与验收见M5-acceptance.md。

M5-delta.patch以仓库当前HEAD `93c3556` 为基点，包含当前工作区相关代码增量和新文件，
不是针对尚不存在的v0.5生成的发布补丁。它含有本轮之前未提交的DHT/Flash修复及Keil工程调整，
用于保留可审阅的当前源码状态；不包含用户模块ZIP、PDF、照片、另一个sms-hub目录或调试器选项。
当前工作区已经应用这些改动，不要重复套用补丁。

M5修改范围：

- BSP/bus_arbiter：同一mutex的SPI/FAN模式、SPI嵌套、竞争计数及安全切换。
- App/storage：出队前等待完整事务所有权；忙不离线，异常仍停止写入。
- Drivers_Device/sg90、tb6612_fan：舵机目标限速、风扇缓启/改速/取消/停止。
- BSP/bsp_controls、bsp_board、bsp_services：复用软件定时器的灯效、蜂鸣、安全上电/急停。
- Drivers_Device/w25q64：新读JEDEC接口、故障态片选保护，保留已有短SPI暂停调度修复。
- App/tasks、menu、m5_bench、app_config：复用AlarmTask、滑杆/实时距离、两种100次压测。
- MDK-ARM/sms-hub.uvprojx、Docs/build.ps1、相关主机测试：编译接入、预算检查与回归。

发布仍等待实际v0.5依赖与M5实物DoD；普通代码版本标识保留0.6.0-dev，未创建发布tag。
