# M8 云看板与加固验收

日期：2026-10-13。版本 `0.9.0-dev`，M7 基线 `e912a93`。
本轮完成软件、真实 C 主机回归与本地文件浏览器验证；开发板、手机、24h及实际临界区耗时尚未实测。
`v0.9` 待这些项目和 M7 依赖验收闭环后发布。

## 接线与正常镜像

先断电，按 `Docs/M8-wiring.md` 使用拓展板 IO1/IO2：
MQ2 VCC→IO1 VCC0（跳帽5V）、GND→GND、转换后DO→A12/PA12；
MQ135 VCC→IO2 VCC1（跳帽5V）、GND→GND、转换后DO→A11/PA11。
两路AO不接，不直插整个四针模块；原 PB0/PB14 接法已替换。
保持 USB_UART 供电及拓展板自锁开关，USB_DEVICE 不接。

当前 `APP_MQ_DO_ENABLED=1`，正常镜像按两路电平转换接法构建。
未接好转换板时不要把 5V DO 直接接 PA11/PA12；应先使用关闭 MQ 的诊断构建。
保持 `APP_CRITICAL_AUDIT=0`、`APP_M8_TEST_ENABLED=0`，重新构建后再下载。

MQ至少预热5分钟并标定；应用150秒屏蔽不代替模块预热。健康测试前必须所有必需传感器通信正常。
单DO输入不能可靠判断拔线：DHT/超声超时可报FAULT，MQ/雷达开路诊断仍有限制。
下载只操作STM32；保持ESP现有AT固件，不擦除W25Q配置和事件。

## A-01 PC与手机看板

1. 烧录正常固件，串口115200/8N1确认 `sms-hub 0.9.0-dev`、Flash/KV就绪与网络在线。
2. PC双击 `WebDashboard/index.html`，设备栏填 KV 中的 `<dev>` 后点击连接。
   默认 `sms01` 是通用入口，实际设备名取KV/串口/MQTTX；若已改名，以实际值为准。
3. 等待“看板已连接、设备在线”。查看温湿度/距离/人/烟/空气/模式/报警与故障码；
   温湿度单位x10在看板除以10，距离为cm。坏质量显示不可用，曲线断开不补假点。
4. 手机把该单HTML放到浏览器能访问的位置后打开，连接同一dev；核对390px或实际手机视口。
   某些手机文件管理器仅预览HTML，需用真正浏览器打开。页面需联网加载MQTT.js及WSS。
5. PC、手机同时记录至少5分钟，用视频/串口/消息时间记录采样变化到两端显示的最大延迟，
   目标≤3s，曲线持续滚动。串口与手机时钟不同，需先同步或用同屏录像比较，不能只比RTC字段。
6. 断开路由器：看板离线或8s无新数据后禁用控制。恢复后自动重连。
   直接断电后确认LWT offline；正常上电后retained online。旧telemetry不得作为实时数据。

本轮PC模拟布局和命令回归通过；实际公网WSS隔离主题回环为685/345/337ms。
这些回环由浏览器自己发布，不包含STM32/ESP、热点或手机延迟，不代替A-01实物验收。

## A-02 控制与协议

主题前缀 `smshub/<dev>`，订阅以下后缀；下行均到 `/cmd`，QoS0、retain=false。

| 主题 | 内容 |
| --- | --- |
| telemetry | 约2s；固定字段 `ts,temp,hum,distance,human,smoke,air,wifi,mqtt,mode,state,alarm,fault,q` |
| status | retained online，LWT offline |
| event | `{"type":5,"p":3,"a":0}` 等事件 |
| config | retained的已提交阈值/模式/seq |
| cmd/resp | id、cmd、status；queued后等待同id的done/rejected |

示例遥测：

```json
{"ts":1791900000,"temp":256,"hum":550,"distance":85,"human":0,"smoke":0,"air":0,"wifi":1,"mqtt":1,"mode":0,"state":2,"alarm":0,"fault":0,"q":31}
```

`state`：0自检/1故障/2撤防/3预热/4布防/5报警。
`alarm`：0无/1空气/2入侵/3火灾。`smoke/air=1` 表示确认触发，0正常；
原始DO低有效，但不能把遥测1解释成正常。`q`位0温湿度、1距离、2烟、3空气、4雷达。
温度与湿度质量均好才置位0；MCU内部温度不属于这五位。

先撤防 `arm=0`，等待同id done、config arm0、telemetry state2/fault0/q31。
此后逐条操作并记录物理动作和回执，不只看publish成功：

| 控件/命令 | 预期 |
| --- | --- |
| 风扇45% / 停止 | `fan=45` / `fan=0`，缓启/停转，同cmd done |
| 舵机90° / 回零 | `servo=90` / `servo=0`，限速到目标，同cmd done |
| 布防 / 撤防 | `arm=1` / `arm=0`，queued→done，config与菜单同步；布防先预热 |

故障、布防、过期、离线时控制应受限；离线按键不缓存，恢复网络不能补发旧执行器命令。
看板发送一条后等待最终回执；12s超时显示“执行结果未确认”，勿假定已经完成。
其余阈值、ack、reboot、time沿用M7命令清单，本轮未新增可选健康页/低功耗/OTA。

## A-03 真实栈保护与故障记录

专用镜像 `Docs/M8-test.hex`，对应 `tmp/M8-test.axf`；仅供故障注入。
先确保风扇停、舵机安全，接调试器并开始串口记录。

1. 烧录测试镜像，用匹配AXF加载调试符号；正常运行至少10s，确认看门狗已启动。
2. Keil Watch中设置 `m8_overflow_request=1` 并继续运行。
   钩子取得MonitorTask真实栈基址，破坏四个内核guard字后yield，
   由实际FreeRTOS mode2检查调用溢出钩子；不是直接调用错误处理函数。
3. 预期立即进入安全态，串口 `STACK task=<TCB地址>`、`任务栈溢出`。
   不再喂狗，随后复位的BOOT记录 `wd=1,cause=2`、累计count增加。
   若日志不全，通过BKP DR4=2/调试器验证；禁止反复在物理危险动作中注入。
4. 测试结束烧回正常 `M8-app.hex`，代码宏0，确认map中没有 `m8_overflow_request`、
   没有DWT审计符号，启动恢复正常。真实栈保护/故障处理继续保留，测试注入分支被移除。

HardFault入口选择MSP/PSP原故障帧，切换128B独立应急栈，先安全停机再打印
R0–R3/R12/LR/PC/xPSR/SP/EXC_RETURN/CFSR/HFSR/MMFAR/BFAR。
栈帧不在20KB SRAM、未对齐或存在stacking/unstacking错误时打印FFFFFFFF，不解引用坏指针。
若需调试HardFault，可在专用调试会话暂停后向PC安排UDF指令；该项不要求增加生产测试钩子。
真实UART输出依赖时钟/串口仍可用，不能承诺所有总线损坏下都能输出。

故障cause：1 HardFault、2栈溢出、3malloc失败；PC拆存BKP DR5/DR6。
有备用电源时可跨复位保留，完全断电且无备份电源不保证保存；本轮不写Flash故障帧以免再阻塞故障路径。
正常监控每5s输出六任务栈水位，字=4B；不得将静态链接RAM余量当作任务栈水位。

## A-04 临界区≤2ms

烧录 `Docs/M8-audit.hex`，使用它的同版本map/AXF。该镜像不作为正常固件或烤机固件：
最近日志RAM缓存4条、磨损统计关闭；网络离线事件环仍20条。

运行菜单编辑/云端控制/两方向Flash风扇压力/DHT与超声恢复/UART错误与DMA/断网重连/KV提交，
至少记录完整一轮96条调用点轮巡，并持续压测故障恢复；保存为 `tmp/M8-critical-001.log`。
`CRIT pc=... n=... max_us=... lost=...`：最大值上取整，包含嵌套及诊断开销。
`pc=FFFFFFF0` 是SysTick，`FFFFFFF1` 是PendSV；lost必须0，n至少1。

```powershell
& D:/Keil_v5/ARM/ARMCLANG/bin/fromelf.exe --text -g --output=tmp/M8-audit-debug.txt tmp/M8-audit.axf
python Docs/audit_m8.py --log tmp/M8-critical-001.log --map Docs/M8-audit-map.txt --debug tmp/M8-audit-debug.txt --output tmp/M8-critical-001.md
```

`Docs/M8-critical-audit.md` 当前枚举122处源码入口，运行时长仍待测。
被链接器移除的HAL I2C接收/tickless分支不纳入运行耗时，但保留审计说明。
上电对象创建尚未启动DWT钩子，需硬件trace/逻辑分析仪补证；编译器共享/outline调用点不能证明每分支都覆盖。
一旦lost非0、任何max_us>2000或有未覆盖运行路径，该项不通过；分析源位置后修复/补测再生成表。
结束后烧回正常固件。不得从“没有测得超标”推断DoD④通过。

## A-05 至少24h烤机

健康接线与正常镜像确认后开始，关闭串口助手释放日志口，PC禁用休眠并保持供电/网络。
先完成一次清洁冷启动（BOOT wd0/cause0），避免栈注入复位混进烤机数据；
记录实际下载镜像的SHA256，BKP历史累计计数无需清零。
确认COM口后运行，以下COM5仅是示例：

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
python Docs/m8_burn.py --capture --dev <dev> --port COM5 --seconds 86460 --output tmp/M8-burn-001
```

运行期间只订阅，不发送cmd，自动同时保存serial.log、mqtt.jsonl；结束生成report.json。
86460s留出连接和采样边界余量。若手动结束或出错，也会生成未完成报告；不能把短采集判24h成功。
用新输出目录避免覆盖原证据。也可独立采串口：

```powershell
& ./Docs/collect_m8_serial.ps1 -Port COM5 -Seconds 86460 -Output tmp/M8-serial-001.log
python Docs/m8_burn.py --output tmp/M8-burn-001
```

报告记录：看门狗计数增量、BOOT次数变化、重连计数增量、历史最小堆、六任务最小栈余量、
16个日志扇区的成功program/erase计数、非retained遥测数量/跨度/最大间隔与非法数据。
WEAR每1s报告一扇区，16s覆盖全表；成功program按操作次数计，不是字节数/事件条数，擦写失败不计。
磨损计数仅本次启动，不能当作芯片寿命累计值；BURN每60s，栈水位每5s。

通过条件：连续采集≥24h，串口uptime跨度≥86300s/样本≥1439，0看门狗复位增量、
无重启/HardFault/栈溢出/malloc失败，六任务/16扇区齐全；
MQTT跨度≥86390s、遥测≥43190条、最大间隔≤3s且无非法数据。
正常期堆最小值应稳定；报告不自动判定传感器准确度、机械动作或手机延迟，
仍须人工检查q/fault及传感器数据是否合理。注入断网的重连测试独立进行，不能同时要求其遥测连续。
正式归档时记录新增日志前后的擦写差异；本轮没有真实24h数据，不能填0复位已通过。

## 实物记录

| DoD | 需留证 | 当前 |
| --- | --- | --- |
| ① 两端≤3s/连续曲线/按钮生效 | PC+手机录像、设备cmd/resp与动作 | 待测；仅浏览器/WSS模型通过 |
| ② 内核栈保护且移除注入 | STACK/BOOT cause2，烧回宏0镜像 | 主机真实guard通过；板测待测 |
| ③ 24h0复位/连续 | 完整serial/MQTT/report及镜像身份 | 未开始 |
| ④ 每处≤2ms | 覆盖表、CRIT/启动期硬件测量 | 清单/采集钩子完成；耗时待测 |
| ⑤ v0.9/map/水位 | 验收记录、最终map、板上水位 | map交付；tag和板上水位待验收 |
