# M6 无线通信验收草稿

日期：2026-10-13。目标版本 `0.7.0-dev`，本文件是硬件验收清单，不把主机模型或源码审查当成 ESP8266 实测证据。用户选择保留现有 ESP8266 `AT1.7.4.0 / SDK3.0.5`，不升级模块固件；STM32 通过 AT TCP 承载手写 MQTT 3.1.1。已收到实机 GMR、14.296 秒上线日志、MQTTX retained online 和约 5 秒间隔 telemetry；证据见 `M6-live-test.md`。RSSI=0 的读数、全部 cmd/resp、5轮断网恢复、LWT 和 4 小时堆水位仍待验收。

## 1. 选型与版本记录

USART3 由 NetTask 独占：115200 8N1，STM32 `PB10(TX) -> ESP8266 RX`、`PB11(RX) <- ESP8266 TX`，共地；`PB9` 是模块复位脚，低电平保持 100 ms 后释放。模块必须使用 3.3 V 逻辑和足够的 3.3 V 电源，不能把 ESP8266 IO 接到 5 V。`PB8` 的用途按实际拓展板/模块丝印确认；若作为 CH_PD/EN 使用，应在运行时保持高电平，不能把 12F 模块的 GPIO0 与 CH_PD 混接。

首次上电先在串口日志中保留完整 `AT+GMR` 回显和日期：

```text
AT+GMR
<AT version>
<SDK version>
<compile time>
OK
```

随后确认 TCP 能力：

```text
AT
AT+CIPMUX=0
AT+CIPMODE=0
AT+CIPSTART="TCP","broker.emqx.io",1883
```

结论填写规则：

| 探测结果 | 交付选型 |
|---|---|
| 旧固件支持 `CIPSTART/CIPSEND` | 采用 TCP AT + STM32 手写 MQTT 3.1.1；不依赖 `AT+MQTT*` |
| TCP 命令也返回 `ERROR`/无响应 | 先排查供电、UART、复位和 Wi-Fi；不得把主机模型当作实机通过 |

现有固件已实机拒绝本工程的 `AT+MQTTUSERCFG`、`AT+MQTTCONNCFG`、`AT+MQTTCONN`、`AT+MQTTSUB/PUB`；不再重复这些命令。TCP 探测只使用 host/port，不打印 Wi-Fi 或 MQTT 密码。

`AT+GMR` 截图、模块丝印/接线照片和最终选择应归档到本文件对应的验收附件；仓库没有实机回显时不得填写具体固件版本。

## 2. 入网状态机

NetTask 每 10 ms 轮询 DMA/IDLE 行缓冲和状态机，不在 `CWJAP` 等待期间阻塞任务。目标顺序：

```text
INIT -> RESET -> AT_TEST -> GMR -> CWMODE=1
     -> CWJAP (最多20 s) -> CIFSR -> RSSI
     -> CIPMUX=0 -> CIPMODE=0 -> CIPSTART
     -> MQTT CONNECT -> SUBSCRIBE -> ONLINE
```

每个步骤最多重试 3 次；本轮失败硬复位模块。连续 5 轮失败进入 60 s 退避，本地传感器、Flash、舵机、风扇和灯效仍应继续运行。DMA/IDLE 接收同时支持文本行和 `+IPD,<len>:` 二进制帧：文本处理 `ready`、`WIFI CONNECTED`、`WIFI DISCONNECTED`、`SEND OK`/`SEND FAIL`；二进制处理 MQTT `CONNACK`、`SUBACK` 和下行 `PUBLISH`。`>` 是 `CIPSEND` 数据提示符。

## 3. MQTTX/手机助手清单

默认配置来自 KV；当前编译默认值为：

```text
host       broker.emqx.io
port       1883
device     sms-hub
keepalive  60
```

现场必须把 SSID、Wi-Fi 密码、Broker、用户名和密码写入 KV 后再测试；不要把真实密码提交到仓库或串口日志。令 `D` 为 KV 中的 `device`，则 topic 为：

| 方向 | Topic | QoS/保留 | 示例 payload |
|---|---|---:|---|
| 设备 -> Broker | `smshub/D/status` | 0 / retained | `online`；异常掉电由 LWT 发布 `offline` |
| 设备 -> Broker | `smshub/D/telemetry` | 0 | `{"ts":1234,"temp":260,"hum":550,"distance":30,"human":0,"smoke":0,"air":0,"wifi":1,"mqtt":1}` |
| 设备 -> Broker | `smshub/D/event` | 0 | `{"type":3,"p":4,"a":0}` |
| 设备 -> Broker | `smshub/D/cmd/resp` | 0 | `{"id":7,"cmd":"fan","status":"done"}` |
| Broker -> 设备 | `smshub/D/cmd` | 0 | 见下表 |

Telemetry 字段顺序固定为 `ts,temp,hum,distance,human,smoke,air,wifi,mqtt`，数值使用整数（温度/湿度按工程现有 x10 约定），禁止 cJSON。每 5 s 发布一次；断网期间事件进入至少 20 条 RAM 环形缓存，恢复后先补传再恢复周期 telemetry。测试工具应订阅 `smshub/+/+` 和 `smshub/+/cmd/resp`，并单独订阅 `smshub/D/status` 观察 retained/LWT。

### 下行命令

| 命令 payload | 合法范围 | 预期动作 |
|---|---|---|
| `arm=0` / `arm=1` | 0 或 1 | 切换布防并通过 KV 保存 |
| `fan=0` .. `fan=100` | 0..100 | 风扇 B 通道目标占空比，返回 `done` 或明确拒绝 |
| `servo=0` .. `servo=180` | 0..180 | 舵机目标角度，返回 `done` 或明确拒绝 |
| `buzzer=1000` | 1..10000 ms | 非阻塞蜂鸣，返回 `done` 或明确拒绝 |
| `th=temp:52` | 0..100 摄氏度整数 | 修改温度阈值并写 KV |
| `th=distance:100` | 依工程字段范围 | 修改距离阈值并写 KV |
| `th=smoke:3000` / `th=air:10000` | 依工程字段范围 | 修改气体确认时间并写 KV |
| `reboot=1` | 仅 1 | 软件复位 |

下发空 payload、未知 key、越界值、额外尾随字符都应被拒绝并返回 `rejected`，不能执行截断后的值。响应 `id` 必须对应命令，不能因为断网而静默丢失已解析的命令。

## 4. 硬件 DoD

| 编号 | 操作 | 通过证据 |
|---|---|---|
| ① | 冷启动并连接已配置路由器和 Broker | 30 s 内日志有 `AT+GMR`、IP、RSSI、`ONLINE`；MQTTX 收到 retained `online` |
| ② | 路由器断电再上电，重复 5 轮 | 每轮自动重连，无 HardFault；本地功能持续工作，退避/重试日志与状态位一致 |
| ③ | MQTTX 订阅 telemetry | 至少每 5 s 一条，JSON 可解析，字段顺序与表格一致 |
| ④ | 逐条发送命令 | 每条命令执行一次且收到对应 `cmd/resp`；越界/未知命令无误动作 |
| ⑤ | 直接切断设备电源 | Broker 在 keepalive 超时后收到 `offline` LWT；重新上线后收到 retained `online` |
| ⑥ | 连续运行 4 h | 每 5 min 记录 Monitor 的 free heap；线性拟合斜率接近 0，不能持续下降；同时记录各任务栈余量 |
| ⑦ | 资源预算 | Keil 日志显示 `Flash <= 61440 B`、`RAM <= 20480 B`；记录 M5 基线、M6 增量及最终余量 |

4 小时测试记录至少包含：起止时间、设备固件哈希、Broker、Wi-Fi 断线次数、重连次数、telemetry 条数、事件补传条数、free heap 最小/最大/起始值、各任务最小栈余量和 HardFault 计数。未完成这些记录时，不得把“能连上 Broker”写成完整 DoD 通过。

## 5. 资源红线与裁剪顺序

先运行 `Docs/test_m6.py` 和 `Docs/build.ps1`，把 `Code + RO-data + RW-data` 与 `RW-data + ZI-data` 写入下表：

| 构建 | Flash | RAM | Flash 增量 | RAM 增量 | 证据 |
|---|---:|---:|---:|---:|---|
| M5 普通基线 | 56812 B | 19648 B | - | - | `Docs/M5-build.log`/`M5-map.txt` |
| M6 TCP-MQTT | 56172 B | 19672 B | -680 B | +24 B | `Docs/M6-build.log`/`M6-map.txt` |

若逼近或超过 60 KiB，按以下顺序裁剪并每步重建回归：删除旧的 USART3 loopback/冗余日志；缩短 AT/URC 文本和 topic 临时缓冲但保留边界检查；减少非关键诊断字符串；降低事件/回复缓存到仍满足“事件至少 20 条”；最后才考虑减少 telemetry 字段。不得删除超时、重试上限、LWT、命令范围校验、KV readback、故障安全和 DMA 溢出保护。若仍超过红线，应报告而不是静默放宽预算。

## 6. 主机模型边界

`Docs/test_m6.py` 编译实际 ESP8266 网络模块，并用确定性桩验证 TCP AT 状态序列、MQTT 二进制 CONNECT/SUBSCRIBE/PUBLISH、命令范围、固定 telemetry 字段顺序、ISR 事件入口和 20 条缓存契约。它不能证明射频灵敏度、3.3 V 电源瞬态、真实 AT 固件能力、Broker ACL、LWT 到达时间、路由器断电恢复或 4 小时堆稳定性；这些必须用上述硬件记录完成。
