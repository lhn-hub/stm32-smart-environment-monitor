# M6 现有 AT 固件的 TCP-MQTT 实现

本方案保留 ESP8266 `AT1.7.4.0 / SDK3.0.5`，不写 ESP Flash。ESP 只负责 Wi-Fi 和 TCP，STM32 负责 MQTT 3.1.1 报文。

## ESP AT 流程

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
AT+CIPSEND=<报文长度>
>
<MQTT二进制报文>
SEND OK
```

`CIPMUX=0` 使接收头保持 `+IPD,<len>:`。`BSP/bsp_bus.c` 在 DMA 环形缓冲中识别该头，随后按长度把原始字节交给 `net_on_rx_bytes()`；普通文本仍走 `net_on_rx_line()`，`>` 走 `net_on_rx_prompt()`。

## MQTT 报文

- CONNECT：协议级别 4，keepalive 60 s，clientId 为 `sms01` 加 STM32 唯一 ID；LWT 为 retained `smshub/{device}/status=offline`。
- CONNACK：返回码 0 后进入订阅。
- SUBSCRIBE：订阅 `smshub/{device}/cmd`，QoS 0。
- PUBLISH：发送 retained `online`、事件、命令响应和每 5 s telemetry。
- PINGREQ/PINGRESP：当前公共 Broker 的 TCP 保活依赖后续现场验证；若实机日志显示 60 s 内无业务流量即断链，再加入最小软件定时 PING。

## 资源与边界

发送复用现有 256 字节 DMA TX 缓冲，`CIPSEND` 先计算报文长度，收到 `>` 后重新生成并发送，避免第二个 MQTT TX 副本。下行 MQTT 命令为流式解析，payload 上限 96 字节。Keil M6 构建为 Flash 56172 B、RAM 19672 B，均低于 61440/20480 B。

`Docs/test_m6.py` 覆盖原始 CONNACK/SUBACK/PUBLISH 和固定字段顺序；`Docs/test_uart.py` 覆盖 `+IPD` 二进制字节、DMA 环回和 `>` 提示符。两者均为主机模型，不替代实际 MQTTX、断网、LWT 和 4 小时测试。
