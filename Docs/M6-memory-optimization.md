# M6 逻辑与 RAM 优化

日期：2026-10-13。软件回归与构建完成；本轮没有实板下载或 ESP8266 刷写。

## 资源结果

| 项目 | 优化前 | 优化后 | 增量 |
|---|---:|---:|---:|
| Code | 50092 B | 50176 B | +84 B |
| RO-data | 5272 B | 5368 B | +96 B |
| RW-data | 136 B | 44 B | -92 B |
| ZI-data | 20040 B | 19404 B | -636 B |
| Flash（Code + RO + RW） | 55500 B | 55588 B | +88 B |
| RAM（RW + ZI） | 20176 B | 19448 B | -728 B |
| 20KB RAM 预算余量 | 304 B | 1032 B | +728 B |
| 60KB Flash 预算余量 | 5940 B | 5852 B | -88 B |

依据为 `M6-memory-before-build.log`、`M6-memory-before-map.txt`、
`M6-memory-after-build.log`、`M6-memory-after-map.txt`。保留完整前后证据。

## 修改范围

- `App/storage.c`：删除 640B 的 `refresh[20]`，保留完整 32B 记录的 `recent[20]`。
- 启动扫描时 `recent_count=0`，读者不能访问正在加载的数组；完整读取成功后才发布数量。启动读取失败不会发布半份列表。
- 运行时提交成功后，只读最新一条 CRC 有效事件，在既有短 RAM mutex 内移动并发布。读取或提交失败不修改旧快照，仍进入停止写入状态。
- `Drivers_Device/eventlog`：暴露已有的有效记录计数，不新增缓存。稀疏日志扇区轮换后按实际数量裁去已擦除的尾部记录。
- `App/menu/menu.c`：标签表指针也设为 `const`；map 证实 `root_labels` 32B、`manual_labels` 60B 从 RW 移到 Flash RO。
- 净节省 728B，而不是名义 732B，差异为链接对齐。未改变 KV/事件介质格式或地址、公开存储快照接口和提交顺序。

正常空槽追加路径的事件区读取从缓存满时 22 次降至 3 次：
空槽检查、提交回读、最新记录读取。遇到残缺槽仍按原逻辑扫描，
不能把该模型读取次数当作板上耗时或全部 Flash 事务数量。

## 验证

`M6-memory-tests.log` 保存 9 组实际 C 主机测试结果：

- 存储普通、网络配置与存储压测三个构建；保留原 KV500 次及逐字节擦除/数据/commit 掉电注入。
- 新增 2100 次追加、跨扇区和整环覆盖，每次与真实 C 的 `log_read_last(20)` 逐字节比对，并检查正常路径 3 次事件区读取。
- 新增最新记录读取失败：旧 20 条快照全部不变，状态离线、拒绝后续写入、总线释放。
- 新增启动扫描中读者检查、部分读取失败及重新加载；Flash 读取时断言没有持有 view mutex。
- 新增稀疏介质轮换：缓存数量及每条内容与仍然存在的已提交 Flash 记录一致。
- 存储压测开关单独验证 2304 条事件以及 500 次 KV 写入后恢复阈值；正式工程两种压测开关仍为 0。
- 原 Flash 仲裁/抢占保护、M2 菜单、M5 执行器、M6 AT/MQTT 模型、UART、M3、DHT 和管脚映射回归通过，GCC 使用 `-Wall -Wextra -Werror`。
- Keil Arm Compiler V6.24 全量构建：0 Error(s), 0 Warning(s)。最终 map 没有 `storage.refresh`，标签表均在 RO。

M2 菜单测试补齐已有 M6 网络状态依赖的 include 路径与离线状态桩，
未修改其原导航、长按或安全态断言。

## 固件

文件：`M6-memory-optimized.hex`，与最终 Keil 输出逐字节哈希一致。

SHA256：

```text
A9F130132965BC60E91B881138BFE5B62CBC2F9F778B71FCB07433FEEF4E098A
```

## 未做与风险

- 未缩减任务栈、11KB FreeRTOS heap、DMA 环或 20 条离线事件容量，也未新增动态分配。静态 RAM 节省不等于 `xPortGetFreeHeapSize()` 增长；824B 是旧固件此前短时实测，新固件堆/栈尚未实测。
- 当前仅枚举到 COM1/COM2，没有 FireDAP/CH340 匹配设备，故未下载或采集新板上日志，未进行 4 小时稳定性验收。
- ESP8266 保持断开且未刷写；此次优化不解决此前实测的旧 AT 固件 MQTT 配置拒绝。仍按 `M6-live-test.md` 的兼容升级路径继续，不因余出 728B 就引入未测的 STM32 MQTT 客户端。
- 没有创建 Git 提交或 v0.7 标签；无线 DoD 仍待 ESP 升级及真实 MQTT 验收。
