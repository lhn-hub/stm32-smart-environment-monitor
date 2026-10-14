# 陌生人 30 分钟复现清单

目标：从干净电脑和已接线开发板开始，在约 30 分钟内完成构建、下载、启动、看板连接和最小控制验证。

## 0～5 分钟：环境与安全

- [ ] 安装 Keil MDK 5.43.1 / Arm Compiler 6.24、Git、Python 3。
- [ ] 准备 CMSIS-DAP、USB-UART；核心板 USB Device 口不接。
- [ ] 断电检查共地；MQ2/MQ135 的 VCC0/VCC1 跳帽为 5V，AO 悬空。
- [ ] 确认两路 DO 经过 HV/LV 电平转换，LV 侧未超过 3.3 V。
- [ ] 确认蜂鸣器不接 PB4，PB4 只接 Rd-03 OT2。

## 5～12 分钟：接线与构建

- [ ] 对照 `README.md` 逐线检查 PA4/5/6/7、PA8、PB1、PB3、PB4、PB6/7、PB9～12、PB13、PA11/12、PA15。
- [ ] 打开 `MDK-ARM/sms-hub.uvprojx`，确认 `APP_MQ_DO_ENABLED=1`、`APP_M8_TEST_ENABLED=0`、`APP_CRITICAL_AUDIT=0`。
- [ ] 执行 `.\Docs\build.ps1 -Stage M8`，确认 0 Error/0 Warning。
- [ ] 核对本机 map：Flash 不超过 61440 B、RAM 不超过 20480 B；构建日志和 map 保存在本地，不上传仓库。

## 12～17 分钟：下载与启动

- [ ] Keil 选择 CMSIS-DAP，64 KB Flash 算法，勾选 Verify、Reset and Run。
- [ ] 只下载 STM32；不要 Mass Erase W25Q64，不要升级 ESP8266。
- [ ] 打开 USART1，115200 8N1，从复位开始保存 `tmp/evidence/boot.log`。
- [ ] 检查 `Flash JEDEC=0xEF4017`、KV ready、OLED 地址和任务水位。
- [ ] 检查上电风扇停、舵机 0°、RGB 待机；MQ 至少预热 5 分钟。

## 17～23 分钟：网络与看板

- [ ] 确认 ESP 使用当前 AT1.7.4.0，路由器为 2.4 GHz。
- [ ] 双击 `WebDashboard/index.html`，输入实际 dev；WSS 使用 `broker.emqx.io:8084/mqtt`。
- [ ] MQTTX 订阅 `smshub/<dev>/#`，确认 retained online、telemetry 和 status。
- [ ] 看板确认温湿度、距离、人、烟、空气、模式、报警、故障卡片和滚动曲线。

## 23～30 分钟：最小验收

- [ ] 看板或 MQTTX 发送 `arm=0`，等待 `cmd/resp` 最终 `done`。
- [ ] 发送 `fan=45`、`fan=0`，确认回执与风扇动作；发送 `servo=90`、`servo=0`，确认回零。
- [ ] 发送 `th=temp:53`，等待 `queued→done`，检查 `/config` seq 增长。
- [ ] PA0 单击/长按与 PC13 返回至少各做一次。
- [ ] 将本次串口、MQTTX、看板截图填入 `Docs/test_report.md`。

## 失败分流

- 无 JEDEC/KV：断电检查 W25Q64 3V3、CS/SCK/MISO/MOSI 和 TB6612 STBY，不要擦除未知介质。
- MQ 故障：先断电，测 LV 电平和 5V 供电；不要把 5V DO 直接接 PA11/PA12。
- 看板不在线：先看 USART3 AT 应答，再检查 broker、设备 dev 和电脑网络；不要把网页自回环当成设备上线。
- 资源超限：停止新增功能，恢复 `Docs/M8-app.hex` 对应源码配置并记录 map。
