# M8 WiFi 本地配置

网页的 WiFi 页通过 USB 调试串口访问开发板 USART1，波特率 115200、8N1。密码只在本地串口发送到 STM32，再由 STM32 通过 USART3 发给 ESP8266，不经过公共 MQTT。

## 协议

| 方向 | 行 |
| --- | --- |
| 网页 -> STM32 | `SCAN` |
| STM32 -> 网页 | `@WIFI +CWLAP:(<ecn>,"<ssid>",<rssi>,"<mac>")` |
| 网页 -> STM32 | `JOIN<TAB><ssid><TAB><password>` |
| STM32 -> 网页 | `@WIFI JOIN_QUEUED` → `@WIFI SAVE_PENDING` → `@WIFI JOIN_SAVED` |

`SCAN` 会让网络状态机复位 ESP8266 并执行 `AT+CWLAP`，扫描结果逐行显示在网页下拉框，完成后回 `SCAN_END`。`JOIN` 先执行 `AT+CWJAP` 验证新热点；收到 ESP 的 `OK` 后才排入 KV 写入队列，完成后回 `JOIN_SAVED`，失败回 `JOIN_ERROR`/`SAVE_ERROR`。密码或 SSID 超出 KV 容量、含引号、逗号、反斜线、控制字符或重复请求会被拒绝。

网页需在支持 Web Serial 的 Chrome/Edge 中打开。当前 Codex 内置浏览器只允许 HTTP/HTTPS 页面，不能打开本地 `file:` 页面并取得串口权限；双击本地 HTML 仍可查看看板，配网操作请使用支持 Web Serial 的浏览器。

## 接线

- USART1 调试串口：PA9 TX、PA10 RX、GND。
- 蜂鸣器模块已删除，PB5 保持低偏置输入。
- Rd-03 雷达 OT2：PB4，高有效。
