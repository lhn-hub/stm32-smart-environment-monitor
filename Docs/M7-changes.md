# M7 软件变更与资源

2026-10-13，目标 `0.8.0-dev`，基线 `0df5c6f`。
正常固件与测试固件均 Keil 全量构建 0 Error(s), 0 Warning(s)。

## 交付与边界

| 文件 | 用途 |
| --- | --- |
| `Docs/M7-app.hex` | 正常固件，测试钩子关闭；与当前 Keil Objects/sms-hub.hex 哈希相同 |
| `Docs/M7-test.hex` | 50 次独立状态机循环钩子；不作为发布固件 |
| `Docs/M7-acceptance.md` | T-01～T-12 操作、预期、故障码、MQTT 清单与实物记录表 |
| `Docs/M7-build.log` / `Docs/M7-map.txt` | 正常构建与链接资源 |
| `Docs/M7-test-build.log` / `Docs/M7-test-map.txt` | 钩子构建与链接资源 |
| `Docs/M7-tests.log` | 本轮主要实际 C 回归输出 |
| `Docs/test_m7.py` | 实际 C 状态机、RTC、联动请求、命令派发和循环测试 |

HEX/AXF 含本地网络配置，继续受 Git ignore 保护；源码中的 `App/app_secrets.h` 不入库。
ESP 保持现有固件，无升级或模块 Flash 操作。未下载 STM32、未做本轮实机验收。

## 资源

Flash 按 Code+RO-data+RW-data，RAM 按 RW-data+ZI-data；RTOS heap 已包含在 ZI，不重复相加。

| 构建 | Code | RO | RW | ZI | Flash | RAM |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| M6 基线 | 51016 | 5112 | 44 | 19628 | 56172 | 19672 |
| M7 正常 | 54536 | 5368 | 48 | 19752 | 59952 | 19800 |
| M7 测试 | 55040 | 5368 | 48 | 19816 | 60456 | 19864 |

正常增量：**Flash +3780 B、RAM +128 B**。正常余量：60 KB 软件红线内 1488 B、20 KB RAM 内 680 B。
钩子额外 +504 B Flash/+64 B RAM，测试构建余量 984 B/616 B。
继续复用 6 个任务、已有软件定时器、存储队列和网络环形缓存，未新增 RTOS 对象/运行期堆分配/cJSON/客户端依赖。
目前尚未超过 60 KB；空间已紧，后续功能需先看 map，不能继续无预算扩展。

Keil 可见静态调用链：正常 alarm_poll 最大深度 496 B，加 worker 自身 96 B 约 592 B；
测试约 624 B，均小于 AlarmTask 的 1024 B 分配。
NetTask 可见最大链 1072 B，分配 2048 B。工具仍注明不可追踪函数指针/循环，静态报告不能代替实际栈高水位。
本轮未测量板上功耗、任务实时栈水位和长期堆趋势。

| 镜像 | SHA256 |
| --- | --- |
| M7-app.hex | `0A3449DA32AA9103A791512FBD4929595EFE4234057886D312B23A93607B2C1C` |
| M7-test.hex | `934ED0412832AEAE6A2554B00F4CA3E9FC7C4DAFB1D6F5D5978E2210E7E1873A` |

## 实现范围

- `App/alarm_engine.*`：纯状态评估及现有 AlarmTask 联动，SELF_TEST→DISARMED→WARMUP150s→ARMED→ALARM；FAULT 停输出，恢复后重新走布防路径。
- 只从 DataHub 读传感器快照；SensorTask 作为唯一生产者读驱动，值/quality/时间戳失效时禁止联动。MQ 使用快照 auxiliary 原始 DO，避免双重滤波延迟。
- 火灾 3s/高温 min(配置,1s)，双鉴严格距离小于阈值持续 2s，空气 10s，优先级火灾>入侵>空气，解除/降级 10s，PA0/ack 清除本次，arm0 写 KV 撤防。
- 火灾/空气/入侵声光、风扇/舵机请求、OLED 弹窗、event/日志接入；手动输出只允许健康且已确认撤防。上一次级别的输出退出后回安全目标。
- 菜单/云端共用 KV 提交，成功后配置热加载；新增 retained config，云端配置 queued→同 id done/rejected，保存失败不替换已确认字段。
- `App/app_time.*`：有效 RTC 备份优先，无有效备份用编译时间转 UTC，网络 type8 请求网关，`time=` 排队给 SensorTask，失败保留旧时间，不阻塞本地功能。
- OLED/Flash JEDEC/ESP AT 各所有者上报自检；FAULT 页显示位码。MQ/radar 没有硬件开路诊断，不虚构拔线检测。

## 验证

本轮完成下列命令；主机编译采用 `-Wall -Wextra -Werror`。均为实际 C 实现配 HAL/RTOS 边界桩，不把模型结果写成硬件成功。

| 命令 | 结果 |
| --- | --- |
| `python Docs/test_m7.py` | 时长边界、双鉴、优先级、无效/过期快照、PA0 对应 ack 逻辑、手动仲裁、故障恢复、回绕、热加载、RTC、50 次钩子、真实 NetTask 回执通过 |
| `python Docs/test_m6.py` | 旧 ESP TCP-MQTT、下行严格解析、20 条缓存、固定遥测字段、config 发布/重发、ack/time 通过 |
| `python Docs/test_m2.py` | 菜单导航、编辑、Manual Test、真实 OLED FAULT/FIRE/INTRUSION/AIR 弹窗通过 |
| `python Docs/test_storage.py` | 普通/配置/压力三变体、500 次 KV、掉电原子性、日志环、100 次风扇等待、只在提交成功后应用通过 |
| `python Docs/test_m3.py` | 传感器协议/质量/时间戳、MQ 门禁及启用路径、DataHub 互斥/恢复通过 |
| `python Docs/test_m5.py` | 舵机/风扇/效果/急停、两个 100 次交叉仲裁模型通过 |
| `python Docs/test_uart.py` | USART3 DMA/IDLE 文本/二进制分帧、错误与溢出恢复通过 |
| `python Docs/test_dht_handshake.py` | 启动握手、早响应、拔线/低电平/缺中断、恢复与回绕通过 |
| `python Docs/test_flash_bus.py` | SPI/FAN 双向各 100 次、JEDEC 200 次、Flash 校验/错误安全通过 |
| `python Docs/test_ultra_mapping.py` | PA15 TRIG、PB3 ECHO/EXTI 和 CubeMX/头文件一致通过 |
| `& Docs/build.ps1 -Stage M7-test` / `-Stage M7` | 两构建 0 错误/0 警告，预算通过，最终恢复正常宏0 |

当前未解除的交付限制：MQ 安全接口和单 DO 开路诊断、M6 完整硬件验收、M7 T-01～T-12 实物验证、机械 50 次循环和栈/堆水位。
版本保留 dev，未发布 `v0.8`；验收脚本中的故障路径可先执行，健康路径须满足硬件前提。
