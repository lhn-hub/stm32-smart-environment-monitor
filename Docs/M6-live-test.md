# M6 MQTTX现场联调

日期：2026-10-13。状态：现有 ESP8266 固件的 TCP-MQTT 核心链路已实机通过；命令、断网/LWT 和 4 小时稳定性仍未闭环。此前接线故障和 `AT+MQTT*` 拒绝记录保留为历史，不再作为当前方案失败依据。

## 本次实机通过证据

- 串口日志：启动后收到 `ready`、完整 `AT+GMR`，并在 `[14296]` 报告 `网络在线 IP=10.97.164.35`。
- MQTTX：订阅 `smshub/sms-hub-8e7c4b91/#` 后收到设备 retained 状态 `smshub/sms-hub-8e7c4b91/status=online`。
- MQTTX：收到设备 telemetry `smshub/sms-hub-8e7c4b91/telemetry`，时间为 `18:20:37.350` 和 `18:20:42.228`，间隔约 5 秒；payload 中 `wifi=1,mqtt=1`，字段顺序正确。
- 原始串口记录归档为 `M6-mqtt-live-20261013.log`，MQTTX 截图归档为 `M6-mqttx-live-20261013.png`。
- 本次证据证明“现有 AT 固件 + STM32 TCP-MQTT”已打通 Wi-Fi、TCP、MQTT CONNECT/SUBSCRIBE/PUBLISH 和周期 telemetry；不等同于全部 M6 DoD 已完成。
- 5~50 秒内的 10 次堆采样均为 824 B，看门狗故障均为 0；网络任务最低剩余栈为 242/512 字（47%）。这是短时结果，不能替代 4 小时统计。
- 已知遗留：启动阶段有 AT/GMR 超时、12 次 USART3 错误（汇总码 0x06）及调度间隔保护丢弃；上线日志 RSSI=0，实际信号强度尚未确认。超声失败累计 109 次，期间出现 FAULT/STALE 后恢复 GOOD，不能据此宣布传感器长期稳定。

## 已完成

- 热点实查：OPPO A5，2.4GHz，频道11，WPA2。
- MQTTX 1.13.1连接broker.emqx.io:1883，Client ID=mqttx-pc-m6-20261013。
- 电脑独立非retained回显：11:10:17.546发送，11:10:17.749收到；这不是开发板消息。
- 本机开发板的设备名设为sms-hub-8e7c4b91，MQTTX已实际添加订阅smshub/sms-hub-8e7c4b91/#。
- FireDAP实际下载、校验、启动成功；证据见M6-download.log。
- 网络字段从旧KV序号4提交为5，旧事件日志继续可读；后续重复下载/重启仍为序号5，不重复配置写入。
- 实际C存储回归覆盖非网络字段/保留区/事件保留、重启幂等、失败时旧KV有效；M6模型、UART模型通过。
- TCP-MQTT 构建0错误0警告，Flash56172/61440B、RAM19672/20480B；较 M5 普通基线减少680B、增加24B RAM。
- 短时现场堆824B、看门狗故障0。不能代替4小时稳定性验收。

热点密码仅在Git忽略的App/app_secrets.h与设备配置中，未放入本文档；固件仍包含本地凭据，不应公开分享该HEX/AXF。

## 修线后复测

- CH340日志口由COM5变为COM6，FireDAP正常。
- 首次45秒采样：`M6-rewire-20261013-115729.log`。收到AT/OK和完整GMR；运行态RST/RX=1且DMA观测到字节，已排除上一轮持续复位低/零接收的现象。刚释放RST的即时读数仍为0，不能据此推翻随后运行态高和有效应答。
- 实际模块版本：`AT version:1.7.4.0(May 11 2020 19:13:04)`、`SDK version:3.0.5(52383f9)`、`compile time:May 27 2020 10:12:17`、`Bin version:1.7.4`。
- 修正探测：不再只发`AT+MQTTUSERCFG=?`，复用既有`build_mqtt_user()`实际配置命令；不打印含密码的命令回显，不修改KV。
- 实际C回归先RED后GREEN，并覆盖实际配置被拒绝后3次重试/复位、不误报ONLINE；原UART回归通过。
- 构建0错误0警告，Flash55500/61440B、RAM20176/20480B，较上一诊断版本-4B/0B；静态RAM仅余304B。
- FireDAP在12:16:51实际下载/Verify OK/Application running；见`M6-mqtt-probe-download-20261013-121640.log`。
- 烧录HEX SHA256：`44B2A656ED2603BE7434498959AA89012B20D2E80BF371B77AE3A0ED41BF3338`。该固件仍含本地凭据，不可公开。
- 新启动证据：`M6-mqtt-probe-20261013-121640.log`。KV扫描序号5、事件下一序号39、存储就绪944ms；原网络配置未重复提交。
- 新启动后共5轮、每轮3次实际MQTT配置均返回`ERROR`，共15次；没有探测OK、IP、RSSI或网络在线。程序在MQTT能力探测处停止前进，尚未进入CWJAP，不应归因为热点密码或Broker。
- 可确定当前1.7.4固件不接受本工程这组内置MQTT配置命令；尚未核验其它厂商命令集，不把此结论泛化为模块绝对不能使用任何MQTT固件。
- 仍有AT/GMR首轮超时、USART3错误0x06及接收调度保护丢弃；未测波形，不能宣称UART全链路已无错误。暂不据此更改波特率或放宽保护。
- 新启动至45秒：DHT21/21、超声失败0、堆824B、看门狗故障0。下载前旧运行段出现DHT552/612、无响应54/拉低超时6，不能用新启动短窗口抵消此前失败或声称长期传感器通过。
- 此前复测尚无设备上线证据；本次新增的核心链路通过结果见本文首节。没有 cmd/resp、路由器断电恢复、真实掉电 LWT 和 4 小时稳定性证据。

## 首次接线故障历史

M6-uart-final.log实测：

```text
[277] KV扫描结果=0 序号=5
[940] 存储就绪 KV序号=5 最近事件=20 擦除=0
[1053] ESP复位释放 RST=0 RX=1 波特率=115200
[1060] ESP引脚 CRH=0x88264B22 ODR=0xC311 IDR=0x45D0
[4078] ESP AT等待超时 状态=2
[5024] 串口3汇总：超时=1 错误=1 码=0x04 重启失败=0 字节=0 RST=0 RX=0
```

- CRH中PB9配置半字节=2：通用推挽输出、2MHz。
- ODR bit9=1：软件已命令PB9拉高。
- IDR bit9=0，运行5秒时仍RST=0：实际引脚未被释放为高。
- USART3未观测到有效DMA字节；0x04为HAL_UART_ERROR_FE。
- 状态2是NET_AT，尚未进入CWJAP；不能归因为热点密码或MQTT Broker。
- 临时74880启动日志采样未获得可解析启动文本，已恢复115200；该结果不证明模块固件或波特率一定正确。

可以确定软件配置、下载、KV提交已完成，并确认复位脚读回异常。仅凭寄存器不能确定是插接方向、线序、外部拉低/短接、模块/拓展板损坏或其它电气原因；需断电检查实物。

## 当前方案与下一步

不升级 ESP8266、不读取或擦写其 Flash。现有固件仅使用：

```text
AT
AT+GMR
AT+CWMODE=1
AT+CWJAP="ssid","password"
AT+CIFSR
AT+CWJAP?
AT+CIPMUX=0
AT+CIPMODE=0
AT+CIPSTART="TCP","broker.emqx.io",1883
AT+CIPSEND=<MQTT报文长度>
```

STM32 在 `>` 后发送 MQTT CONNECT、SUBSCRIBE、PUBLISH 原始报文；从 `+IPD,<len>:` 接收 CONNACK、SUBACK 和命令 PUBLISH。主机验证和实机在线/telemetry 已通过，后续补做 cmd/resp、断网、LWT 和 4 小时稳定性验收。本地交付 HEX 为 `Docs/M6-raw-mqtt.hex`，SHA256=`EECD9DDA8DBDA1EE07D21DE4820AA63B27101FA54D7132D52785075DDC869B73`；日志没有固件哈希，未单独核验板上镜像哈希。

ESP8266 升级材料和历史接线记录仍保留在 `Docs/M6-esp-upgrade.md`，但当前交付不读取、不擦写、不升级 ESP Flash，也不要求切换下载模式。现场保持已验证的运行接线和开发板供电方案。

| STM32 | ESP8266 |
|---|---|
| PB10/TX | RXD |
| PB11/RX | TXD |
| PB9 | RST，释放后必须为高 |
| 3V3 | VCC/3V3 |
| GND | GND |

按实际模块确认PB8接的是EN还是GPIO0，不能混接。ESP-12F裸模组运行要求GPIO0高、GPIO2高、GPIO15低、EN/RST高。用户模块配套规格书建议3.3V供电峰值能力500mA以上；拓展板带稳压接口的输入电压必须按其原理图确认，不能直接给ESP芯片IO/VCC接5V。

未发送任何风扇、舵机或蜂鸣器远程指令。核心在线和 telemetry 已通过；全部 cmd/resp、断网重连、真实掉电 LWT、4 小时稳定性仍待验收。公共 Broker/1883 不提供私密性或本设备访问控制，只用于临时联调。
