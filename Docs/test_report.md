# M9 测试报告与证据矩阵

日期：2026-10-14
固件：本机按 `.\Docs\build.ps1 -Stage M8` 构建的正常镜像（固件附件不上传仓库）
构建：应为 Keil 0 Error / 0 Warning；Flash 不超过 61440 B；RAM 不超过 20480 B

## 判定规则

| 标记 | 含义 |
|---|---|
| 软件 PASS | 已由实际 C 主机模型或浏览器回归证明，不代表板测 |
| 待板测 | 必须补串口、照片、波形、MQTTX 或实物动作证据 |
| 阻断 | 会造成误报警、失控、数据丢失或超过资源红线，未关闭前不能发 `v1.0` |

统一记录：串口 `115200 8N1`；照片位置 `tmp/evidence/H-xx.jpg`；串口位置 `tmp/evidence/H-xx.log`；MQTTX/看板截图位置 `tmp/evidence/T-xx.png`。

## H-01～H-10 硬件检查

| 编号 | 步骤 | 预期 | 实测/证据 | 结论 |
|---|---|---|---|---|
| H-01 供电与共地 | 断电检查 3V3/5V/GND；上电测扩展板 5V 与 3V3；风扇/舵机/MQ 加热器接入后再测跌落 | 3V3/5V 在模块允许范围，共地，复位不抖 | 待填写：万用表照片、上电日志 | 待板测；电源不足为阻断 |
| H-02 SWD/NOJTAG | 连接 SWDIO/SWCLK/GND/Vref；下载后断开探针，确认 PB3/PB4/PA15 可用 | 可下载、复位运行；SWD 保留，JTAG 关闭 | 软件配置见 `Src/stm32f1xx_hal_msp.c`；现场待填 | 软件证据 PASS，实物待测 |
| H-03 OLED/I2C1 | 接 PB6/PB7，扫描 I2C；观察启动页和报警页 | 地址 `0x3C`，无 I2C 锁死；断线进入 FAULT | 主机/日志已有路径；照片待填 | 待板测 |
| H-04 DHT11 | PB12 接线；冷启动后等待 2.1 s 周期至少 5 次 | 返回温湿度、校验通过；超时不联动，恢复后质量回 GOOD | `Docs/test_dht_handshake.py` 软件 PASS；成功率待填 | 软件 PASS，实物待测 |
| H-05 超声波 | PA15 TRIG、PB3 ECHO；固定 30/100/150 cm 目标 | 距离合理；ECHO 卡高/无回波在 30 ms 内退出 | `Docs/test_ultra_mapping.py` 映射 PASS；精度待填 | 软件 PASS，实物待测 |
| H-06 Rd-03 雷达 | OT2 接 PB4，无遮挡/挥手各 5 s | PB4 高有效；去抖约 1 s；无蜂鸣器占用 | PB4 代码映射已核对；现场视频待填 | 待板测 |
| H-07 MQ2/MQ135 | VCC0/VCC1 跳 5V；HV/LV 转换；AO 悬空；预热至少 5 min | LV 高约 3.3 V、触发低约 0 V；PA12/PA11 不超过 3.3 V | 接线表已完成；电平照片/预热日志待填 | 未完成前为阻断 |
| H-08 W25Q64/SPI | 接 PA4/5/6/7；记录 JEDEC、KV 扫描、保存后复位 | `0xEF4017`；读写/校验成功；TB6612 在 SPI 时处于安全态 | `Docs/test_flash_bus.py` 软件 PASS；板上波形待填 | 软件 PASS，实物待测 |
| H-09 SG90/TB6612 | SG90 PA8；TB6612 仅 B 通道 PB1/PA7/PB13/PA4；风扇 0→45→100→0 | 上电风扇停、舵机 0°；PWM 平滑；A 通道不驱动 | `Docs/test_m5.py` 100 次模型 PASS；角度/噪声待填 | 待板测 |
| H-10 ESP/调试串口 | ESP 3.3 V、PB9 RST、PB10/11 USART3；USART1 记录日志 | AT 应答、DMA+IDLE 不丢帧；掉电上电执行器安全 | 现有 AT 版本已确认；本轮串口/网络照片待填 | 待板测 |

## T-01～T-15 功能用例

| 编号 | 步骤 | 预期 | 实测/证据 | 结论 |
|---|---|---|---|---|
| T-01 上电自检 | 冷启动，观察 OLED、DHT、超声、W25Q、ESP 日志 | 逐项自检；故障进入 FAULT，风扇停、舵机 0° | `Docs/test_m7.py` 自检模型 PASS；板上日志待填 | 软件 PASS，实物待测 |
| T-02 雷达单鉴 | ARMED，距离 >120 cm，仅挥手 5 s | 保持 ARMED，不报警 | 模型 PASS；现场视频待填 | 待板测 |
| T-03 超声单鉴 | human=0，目标 <100 cm 5 s | 不报警 | 模型 PASS；现场视频待填 | 待板测 |
| T-04 入侵双鉴 | human=1 且距离 30～50 cm 持续 2 s | 紫闪、入侵事件；不启动风扇/舵机 | 模型 PASS；现场日志待填 | 待板测 |
| T-05 常温火灾 | MQ2 DO 低持续 3 s | 红闪、风扇 100%、舵机 90°、火灾事件 | 模型 PASS；隔离夹具/视频待填 | 待板测 |
| T-06 高温火灾 | 温度达到阈值，MQ2 DO 低 1 s | 1 s 路径触发；温度本身不触发 | 模型 PASS；板测待填 | 待板测 |
| T-07 空气提示 | MQ135 DO 低持续 10 s | 黄常亮、风扇 45%、不鸣笛 | 模型 PASS；板测待填 | 待板测 |
| T-08 优先级/恢复 | 空气→入侵→火灾；按顺序解除并等待 10 s | 火灾>入侵>空气；逐级降级到 ARMED | 模型 PASS；板测待填 | 待板测 |
| T-09 故障安全 | 断开 DHT 或让超声超时，再恢复 | FAULT、联动停止；恢复后重新自检/预热 | 模型 PASS；串口/照片待填 | 待板测 |
| T-10 菜单与云端配置 | 菜单改阈值；MQTT 发 `th=temp:53` 等 | KV seq 增长；菜单、config、任务热加载一致 | `Docs/test_m7.py` PASS；MQTTX 截图待填 | 软件 PASS，实物待测 |
| T-11 手动控制 | 撤防后发 `fan=45`、`servo=90`、归零 | 收到 done，动作可见；布防/故障时 rejected | 看板模型 PASS；动作视频待填 | 待板测 |
| T-12 PA0/云端解除 | 报警中单击 PA0，再用 `ack=1`，最后 `arm=0` | 停止联动；arm=0 持久化；条件未解除时按规则重报警 | 50 轮模型钩子 PASS；板测待填 | 待板测 |
| T-13 掉电恢复/RTC | 保存阈值和 arm=1，复位；发送 `time=<Unix秒>` | 阈值/模式保持；执行器先安全；RTC 不阻塞 | 存储模型 PASS；真实复位和 MQTTX 待填 | 待板测 |
| T-14 MQTT/看板/LWT | 连接看板，收 telemetry/event/status；断网、断电、恢复 | ≤3 s 显示；offline LWT；恢复 retained online；命令有 resp | `Docs/test_dashboard.py` WSS 回环 PASS；设备证据待填 | 待板测 |
| T-15 长稳与安全 | 正常镜像连续运行 ≥86400 s；审计镜像采集 CRIT | 0 看门狗复位；堆不持续下降；无非法遥测；max_us≤2000、lost=0 | `m8_burn.py --self-test` 仅合成 PASS；真实报告待填 | 阻断，未完成前不能 v1.0 |

## 截图与日志插槽

- 串口启动：`tmp/evidence/boot.log`
- OLED 自检/报警：`tmp/evidence/oled-selftest.jpg`、`tmp/evidence/oled-alarm.jpg`
- MQTTX 订阅和命令：`tmp/evidence/mqttx-status.png`、`tmp/evidence/mqttx-cmd.png`
- WebDashboard PC/手机：`tmp/evidence/dashboard-pc.png`、`tmp/evidence/dashboard-phone.png`
- PA4/PA7 切换波形：`tmp/evidence/spi-fan-waveform.png`
- 24 小时报告：`tmp/evidence/M8-burn-001/report.json`

## 未关闭问题与规避措施

1. **严重：缺少本轮实物证据。** 规避：按 H/T 表逐项补串口、照片、MQTTX、动作视频和报告；在补齐前不创建 `v1.0`。
2. **严重：MQ 输入是 5 V。** 规避：只使用 IO1/IO2 的 5 V 供电和 HV/LV 转换，先测 LV 电平再下载生产镜像。
3. **中等：单 DO 无法可靠开路诊断。** 规避：拔线 DoD 标为限制；需要开路诊断时增加硬件分压/上拉检测并重新评估资源。
4. **中等：Flash/RAM 余量极小。** 规避：M9 不改固件；二期功能必须先做 map 预算。
