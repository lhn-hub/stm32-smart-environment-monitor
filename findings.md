# M0 前置核查记录

## 2026-10-13 M8起点
- 正常/栈测试/审计最终分别61036/61344/61332B Flash，20152/20152/20360B RAM；启用MQ正常编译61112B/20152B，增76B而未超预算。最终恢复MQ0/审计0/注入0。
- 桌面/390/320px看板与真实公网隔离WSS复测通过，685/345/337ms；均不包含实板/手机延迟。临界区表122处耗时待测，未发布v0.9。
- 收尾脚本自检与3秒只读MQTT采集生命周期通过；报告正确标未达到24h。当前串口仅COM1/COM2，没有板测数据。
- HAL I2C另有8处PRIMASK，审计源码表补齐至122处；实际M8-audit map显示Master_Receive/Mem_Read都Removing，业务只调用扫描/发送。若将来启用接收，必须增加PRIMASK计时而非复用端口DWT结果。
- 基线e912a93，正常仅余1488B Flash/680B RAM；MDK/sms-hub.uvoptx已有用户修改，不回退。
- 现场文件确认扩展板有5V/GND引出，需按自锁开关；MQ2/135 J1脚1=5V、2=GND、3=DO、4=AO，两路DO有R9=10k到5V，供电5V不等于GPIO信号安全。
- 既有telemetry周期5s且缺mode/alarm/quality，若满足看板3s需变更上报入口并测试协议；仍需保留旧字段顺序和KV布局。
- 官方EMQX页面确认8084为WSS、路径/mqtt；MQTT.js官方文档已读取。初次web搜索为空，直接官方页面成功，不照搬ws://8084。

## 2026-10-13 M7起点
- Git基线0df5c6f已跟踪文件干净；DataHub已包含传感器值/quality/timestamp，菜单和MQTT已读快照。
- M6资源为Flash56172B/RAM19672B，静态RAM仅余808B；新增状态机必须复用现有任务、定时器及事件队列。
- 当前APP_MQ_DO_ENABLED=0，MQ_WARMUP_S=300；MQ安全接线尚未解除，不能为M7盲目启用。M7按150秒预热实现，MQ禁用时显示故障且禁止报警联动。
- MQ驱动已做确认/解除滤波；M7引擎读取快照auxiliary原始DO自行计时，避免3秒+3秒及10秒+10秒的重复延迟。
- 现有风扇持有SPI仲裁器会延后Flash日志；报警event须立即进入既有网络RAM环，持久日志排队待风扇停止，不因日志失败阻止报警。
- PA0原来用于导航；只在报警状态拦截为手动解除，其它状态保留导航。手动解除后仍布防，持续条件重新确认；arm=0通过KV撤防。
- 自检由OLED/MenuTask、JEDEC/LogTask和AT/NetTask上报，SensorTask仍是传感器驱动唯一读者。MCU温度校准禁用不作为外部传感器通信失败。
- 选择网关MQTT时间请求/回复，避免依赖旧ESP固件未验证的SNTP命令；RTC写入仍归SensorTask，失败保留编译/备份时间且不阻塞网络。
- M7首轮正常构建0错误0警告，Flash59888B/RAM19800B，相对M6+3716B/+128B；仅余1552B Flash/680B RAM，继续禁用生产测试钩子，不新增RTOS对象。
- 保留SPI/FAN互斥：配置写入在风扇运行时等待。arm=0接受后优先停止输出以释放总线，KV确认后给最终done；保存失败转故障，旧已确认字段不替换。
- 正常AlarmTask静态调用链为worker(96B)+alarm_poll最大深度496B，低于256字栈；编译报告不能替代板上高水位，实测须继续记录。
- 两路DO及Rd-03均标记SENSOR_NO_OPEN_WIRE_TEST，单数字电平无法保证拔线可观测；DHT/超声通信超时可进入FAULT，MQ/radar拔线DoD需硬件诊断支持，不能伪造通过。
- 严格自检/故障恢复路径经DISARMED再到WARMUP；中间态保留armed位，禁止手动输出抢占。高温确认取min(已配置烟雾时间,1000ms)，避免配置小于1秒时反而延长。
- 复测实际C状态机/NetTask通过；首个测试构建Flash60440B/RAM19864B。最终资源以边界修正后的构建为准，正常宏已恢复为0。
- 最终正常Flash59952B/RAM19800B，M6增量+3780B/+128B；测试Flash60456B/RAM19864B。正常余1488B Flash/680B RAM，仍在红线内。两构建0错误0警告，正常HEX哈希与Objects一致。
- M7验收文档已列T-01~T-12、实际设备topics、queued/done、故障位、网关time入口及50次虚拟时间钩子；未部署常驻网关，MQTTX可回time=UTC秒。软件验证不能关闭单DO开路诊断或M6/M7板测缺口。

## 2026-10-13 TCP-MQTT实机证据核对
- 用户串口日志在 14.296 秒报告网络在线；MQTTX 在 18:20:32.662 收到设备专用 status retained online，18:20:37.350 和 18:20:42.228 收到 telemetry，间隔 4.878 秒。
- 两条 telemetry 按截图转录后均为合法 JSON，字段顺序符合清单，wifi/mqtt 均为1；并非截图上方 11:10 的电脑回环消息。
- 归档日志与截图哈希均与附件一致。5~50秒的10次堆采样均为824B、看门狗故障0，网络栈最低余242/512字。
- 启动 AT/GMR 超时、USART3错误12次（0x06）、间隔保护丢弃、RSSI=0 和超声累计失败109次仍存在；未据此声称全部模块长期稳定。
- 核心链路可提交；全部cmd/resp、5轮断网、LWT及4小时稳定性证据仍缺失，不发布v0.7。

## 2026-10-13 现有 AT 固件承载 MQTT 方案
- 用户明确选择保留 ESP8266 现有 `AT1.7.4.0 / SDK3.0.5`，停止兼容 ESP-AT 升级、Flash 备份和擦写路线。
- 该固件已实机拒绝 `AT+MQTTUSERCFG`、`AT+MQTTCONNCFG`、`AT+MQTTCONN`、`AT+MQTTSUB/PUB`；继续重试不会改变能力。
- 采用 ESP AT 1.x 通用 TCP 命令：`AT+CWMODE=1`、`AT+CWJAP`、`AT+CIPMUX=0`、`AT+CIPMODE=0`、`AT+CIPSTART="TCP",host,1883`、`AT+CIPSEND=n`。STM32 端生成 MQTT 3.1.1 CONNECT/SUBSCRIBE/PUBLISH，并解析 `+IPD` 二进制 CONNACK/SUBACK/PUBLISH。
- 现有串口行分帧不能承载 MQTT 二进制，必须增加 `+IPD,<len>:` 长度模式和 `>` 提示符回调；保留原换行行解析、DMA/IDLE、30ms 丢弃保护。
- 资源策略：复用现有 256 字节 TX DMA 缓冲；MQTT 下行命令采用流式解析和 96 字节 payload，避免新增 256 字节 RX 副本。实际 Flash/RAM 以 Keil map 为准，未提前声称预算通过。

## 2026-10-13 TCP-MQTT软件实现与验证
- `BSP/bsp_bus.c` 现在保留原行缓冲，同时识别 `+IPD,<len>:`、逐字节转发二进制 MQTT，并在 `>` 到达时结束 `CIPSEND` 请求；DMA错误、30ms间隔保护会清空未完成的 IPD 帧，避免错位。
- `Middleware/esp8266/esp8266.c` 状态序列为 `CWMODE -> CWJAP -> CIFSR -> CWJAP? -> CIPMUX -> CIPMODE -> CIPSTART -> MQTT CONNECT -> SUBSCRIBE -> ONLINE`。CONNECT 带 60s keepalive、clientId 唯一后缀和 retained LWT；telemetry 每5s产生业务流量，满足 keepalive 的最大无流量间隔。
- MQTT下行仅接受 QoS0 PUBLISH 到 `smshub/{device}/cmd`，payload 上限96字节；CONNACK、SUBACK返回码校验，畸形剩余长度和越界命令 fail-closed。
- `Docs/test_m6.py`、`Docs/test_uart.py`、M2/M3/M5/Flash/存储/DHT/映射回归通过；Keil 0错误0警告，Flash56172B、RAM19672B；HEX SHA256 为 `EECD9DDA8DBDA1EE07D21DE4820AA63B27101FA54D7132D52785075DDC869B73`。
- 以上均为软件/主机证据；未把现有 ESP 的 TCP 实机连接、MQTTX消息、路由器断电、LWT、4小时堆稳定性写成已通过。

## 2026-10-13 TCP-MQTT重连边界
- 发现 TCP 断开时若 MQTT PUBLISH 只收到半帧，解析器状态可能残留到下一次连接；在进入复位和失败轮次时清理 AT 请求及 MQTT 接收状态。
- 新增主机回归覆盖“半帧 PUBLISH -> CLOSED -> 重连 -> CONNACK/SUBACK”，通过；不改变原有 20 条事件缓存或本地功能。

## 2026-10-13 ESP8266升级执行
- 用户报告改线完成并授权检测成功后刷写。当前CH340仍COM5；第一次ROM只读flash-id探测（115200/no-reset/no-stub，2次连接尝试）失败：No serial data received。证据Docs/M6-esp-rom-probe.log；没有获得芯片/容量证据，没有备份、擦除或写Flash。
- 不对同一接线盲目重复刷写；下一步用非持久化AT探测区分“仍在运行模式”与“串口无数据”，必要时请用户在GPIO0低状态对ESP本身复位（开发板RESET不能替代）。
- 随后COM5只读发送AT\r\n的2.5s探测仍收到0字节；74880波特率30s被动启动监听也收到0字节。当前无法区分ESP未供电、CH340 TX/RX侧接错、ESP RST未产生边沿或GPIO0未真正拉低；不能开始容量读取/备份/刷写。
- Pnp确认FireDAP和CH340(COM5)在线；6秒只读采样实际为STM32任务日志（Docs/M6-esp-upgrade-port-probe.log），不是ESP ROM下载口。串口已释放，未发AT/下载同步、未擦写。
- 当前系统Python缺esptool和pyserial；将准备项目内官方工具，不改全局Python或插件。
- 官方GitHub API实际返回已核验：v2.2.1.0_esp8266发布于2021-08-06，正文提供ESP8266-IDF-AT_V2.2.1.0.zip，文档归属release-v2.2.0.0_esp8266。本次按用户指定版本核对，不把2.2.1.0称为最新版。
- 官方AT引脚文档明确ESP8266可选GPIO15/TX、GPIO13/RX或GPIO1/TX、GPIO3/RX，factory_param_data.csv中的CTS/RTS不用时应为-1。当前模块RXD/TXD对应ROM GPIO3/GPIO1，需核对发布包及官方配置工具，不能盲刷默认引脚映像。
- web工具调用无可见页面内容，因此改用GitHub API及raw原文核验；不把空白工具结果当作有效证据。同tag的How_to_set_AT_port_pin.rst已实际读取并确认两组AT GPIO及无流控参数；ROM启动与备份命令还需由官方原文及本地工具核验。
- 已查到官方esptool v5.1.0 Windows独立包及GitHub提供的SHA256：f68a8f7728adfc59cd60f9424928199e76eac66372c7bdc23898aa32753a437a。选择固定版本而非猜测最新版。
- 资料ZIP用默认UTF8显示中文乱码，改用ZipFile.Open的CP936解码后定位规格手册和原理图，不修改原ZIP。
- 两官方ZIP已下载到Git忽略的tmp/esp-upgrade；esptool包SHA256与GitHub发布digest完全一致。ESP-AT包SHA256为CB57E8AFD4CCCCAE8204790ACC41322E6DDBBFD813AFBD1192D828F686F7542B（本地记录，不冒充发布者签名/独立校验）。
- 模块规格手册实际读取：6针接口1=VDD、2=GND、3=TXD(GPIO1)、4=RXD(GPIO3)、5=RST、6=IO(GPIO0)。IO默认上拉，拉低进入下载。板带LDO、VDD输入为3.3V或5V；裸芯片VCC及所有IO仍是3.3V域，不能混淆。
- 发布包含flasher_args.json/download.config/sdkconfig、独立factory_param.bin及2MB合并factory_WROOM-02.bin；尚未选择烧写布局，先核对官方参数工具及实际模块Flash。
- 官方包sdkconfig实际确认CONFIG_AT_MQTT_COMMAND_SUPPORT=y；串口8N1，默认flow_control=1需核对禁用CTS/RTS后的实际初始化。
- 官方flasher_args.json：dio/80m/2MB，factory_param位于0xF1000；这是固件使用布局，不是实际芯片容量。即使实际4MB也不能直接把分区地址随容量改写。
- 独立esptool.exe成功执行version，输出5.1.0；未对任何串口运行chip-id/flash-id/read-flash/write-flash。
- 同tag官方factory_param_generate.py可独立从CSV生成4KB参数分区（依赖xlrd），无需重编译完整AT应用。先对原始CSV生成与发布bin逐字节校验，再仅更改4个UART引脚字段；保持原始发布映像不动。
- 模块原理图文本确认板载RT9013-33GB、VDD为3.3V~5V输入、芯片VCC为3V3；不能将板VDD的5V容许范围扩展到TX/RX/IO/RST。正在视觉核对GPIO0/15拉阻及接口编号。
- 已渲染并视觉核对模块原理图：J1-6为GPIO0，板上R3=1K上拉3V3；GPIO15由R4=1K下拉，RST/EN由R5/R6上拉；GPIO13/15未引到J1。刷写只需控制J1-6和RST，不要求用户焊接未引出的GPIO。
- 同tag官方参数CSV的WROOM-02默认15/13/3/1；已有WROOM-02-N行使用1/3/-1/-1，可佐证官方支持原UART方案。计划保留发布WROOM-02的其它字段，只改变TX/RX/CTS/RTS，不冒充实物型号为WROOM-02。
- 使用官方factory_param_generate.py及CSV生成原参数成功，断言与发布factory_param.bin全部4096字节完全相同，原TX/RX/CTS/RTS为15/13/3/1；仅在tmp内安装xlrd2.0.1供官方工具导入，不改全局Python。
- 已生成factory_param-rx3-tx1.bin并断言长度4096B、差异仅偏移16/17/18/19、TX/RX/CTS/RTS=1/3/-1/-1、波特率115200，其它字节不变。SHA256=87125B9C7BE7303D3299F5662303B6BAD93C3A59D238472A7D57C2BF391AB220。仅为电脑准备，不代表已烧录/启动。
- 用户最新约束：只能通过开发板给扩展板供电。保留原电源链，不需要独立3.3V电源；供电独立与串口直连是两件事。
- 为复用现有硬件，已实际读取并渲染双USB核心板原理图第5/6页：板载CH340X的VCC/V3接3V3；J5-7=PA9、J5-8=CH340 RXD，J5-9=PA10、J5-10=CH340 TXD。拔除7-8、9-10串口跳帽可物理隔离STM32，CH340侧可直接接ESP TXD/RXD，不需新增USB-TTL或修改STM32固件。
- 借用CH340时仍必须断开ESP与PB10/PB11/PB9/其它IO0驱动的信号连接；VDD/GND保留原开发板供电。只拔串口两跳帽，不改BOOT0/BOOT1跳帽。实物J5方向/丝印未确认，不能按照片缺失的左右方位猜引脚。
- 官方资料来源：espressif/esp-at v2.2.1.0_esp8266 release、Espressif Released Firmware、Hardware Connection与esptool Basic Commands/Boot Mode Selection。实际Flash容量尚未读取。

## 2026-10-13 M6逻辑与RAM优化
- 已恢复既有Phase16及脏工作区；基线map为Flash55500B/RAM20176B，RAM剩304B。
- map确认storage.recent与storage.refresh各640B，菜单manual_labels/root_labels分别60B/32B在RW区；优先消除重复缓存并将只读指针表移至RO。
- 存储快照受view_lock保护；不能直接把Flash读取改为在该锁内进行，也不能读取失败后留下半更新列表。需要核对eventlog提交/尾部擦除和调用者后再选最小方案。
- 最近事件仅在storage_start（LogTask启动）重建；recent_count为0期间读者不会触碰尚未发布的数组。运行时可复用log_read_last(1)的CRC有效记录，在短RAM锁内更新，不增加公开API，也不改变Flash记录格式。
- 已移除640B的refresh数组；正常事件路径由两次提交所需读取加至多20次历史读取，改为两次提交读取加一次最新记录读取。首次2100条模型逐字节对比通过，正在补失败与启动并发可见性检查。
- 首次普通Keil构建0错误0警告，Flash55560B/RAM19448B，RAM净省728B（640B缓冲+92B标签表-4B对齐），Flash增60B。运行堆区11264B及任务栈不变，不能将静态节省宣称为堆余量增长。
- 稀疏介质可在轮换时擦掉最近缓存的尾部记录；回归已复现。新增eventlog_count只暴露既有available，不分配RAM、不碰存储布局，用其限制缓存数。
- 最终普通构建Flash55588B/RAM19448B，净+88B/-728B，RAM余1032B；新增计数边界后比首次构建再增28B。map验证一个640B缓存、标签RO、11264B heap不变；正式压测宏0，归档HEX与Keil输出SHA256一致。
- 当前SerialPort仅COM1/COM2且Pnp无FireDAP/CH340匹配，故未下载/采集；9组主机测试及独立存储2304事件/500KV压力模型通过，不代表4小时或真实无线DoD通过。

## 2026-10-13 修正接线后复测
- `Docs/M6-rewire-20261013-115729.log`：COM6实采45秒，收到AT/OK及完整GMR：AT1.7.4.0、SDK3.0.5、Bin1.7.4。运行时RST/RX=1，DMA观测字节从3836增至4389，上一轮全程RST低/零字节故障不再成立。
- AT/GMR仍有超时及USART3错误0x06；尚无网络在线/IP/RSSI。需分别核对接收错误、启动时序和旧AT固件MQTT能力，不将AT应答当作M6通过。
- DHT采集90/90、超声失败0、堆824B、看门狗故障0；只是短时现场结果。
- MQTTX启动后既有f103连接断开，独立订阅保留、无设备消息。首次连接点击被工具报告用户输入干预，须刷新再决定。
- 已以真实MQTT配置命令替换仅测试`=?`的能力探测；实际C回归先RED后GREEN，并验证配置拒绝3次后复位而非ONLINE。UART模型通过，Keil0错误0警告，Flash55500B/RAM20176B。
- 12:16:51 FireDAP下载/校验/运行成功。`M6-mqtt-probe-20261013-121640.log`新启动KV5、存储就绪944ms；5轮各3次MQTT配置均ERROR，共15次，无IP/RSSI/ONLINE，尚未进入CWJAP。
- 当前1.7.4固件不接受本工程内置MQTT配置命令，选择兼容ESP-AT升级路径；静态RAM仅余304B，不直接加STM32端lwMQTT。ESP本轮未刷写。
- 资料ZIP的559B说明文件是UTF8（首次GBK读取乱码，重读UTF8成功），说明小智12F为4MB；附带旧AT1.5.4.1和1471 MQTT透传固件，不能将其当作已核验ESP-AT2.2.1.0镜像。刷写前须实际核对容量及运行AT UART映射。
- 新启动短时DHT21/21、超声失败0、堆824B/看门狗故障0；下载前旧运行段DHT552/612存在无响应54/拉低超时6，保留为长期风险，不忽略。
- MQTTX后续快照显示已连接、仍只有旧消息；末次读取因窗口被最小化未获得新快照。没有设备端遥测证据，也未发送执行器命令。

## 2026-10-13 MQTTX现场配置
- 当前存在USB-SERIAL CH340(COM5)和FireDAP CMSIS-DAP(USB VID0416/PID5021)。此前只按FriendlyName筛选漏掉FireDAP；SWD实际下载仍待验证。
- 注册表确认已安装MQTTX 1.13.1，路径D:/Program Files/MQTTX/MQTTX.exe。
- 当前电脑Wi-Fi是OPPO A5，5GHz/channel44；待核对模块频段及可用2.4GHz网络。
- 固件当前默认SSID为空，NetTask在config_ready检查失败时不会进入AT流程；storage_get_network_text提供读取但没有网络字符串写入入口。不能只改编译默认值并假设seq3的旧KV会自动迁移。
- MQTTX已连接broker.emqx.io:1883，电脑Client ID=mqttx-pc-m6-20261013；smshub/sms-hub/#订阅生效。独立非retained回显在11:10:17.546发送、11:10:17.749收到，电脑端实测通过。
- 12秒COM5现场采样：堆824B、看门狗故障0、任务栈余量52~70%。DHT当前GOOD，但累计163/173成功（起始拉低超时1、无响应9）；超声GOOD/37cm/失败0。无设备上线证据。
- 用户已提供本地热点参数并切换2.4GHz；netsh实查OPPO A5/2.4GHz/频道11/WPA2。密码仅保存于已被Git忽略的App/app_secrets.h。
- 新增受本地编译开关控制的网络配置提交：仅更新网络字段，保留阈值/模式/日志，同值重启不重复写KV；设备主题使用smshub/sms-hub-8e7c4b91。
- 网络配置保留/重启幂等/提交失败回退的实际C主机回归通过，M6模型通过；构建0错误0警告，Flash55172B/RAM20176B，较配置前+396B/0B。
- FireDAP实际Erase/Programming/Verify OK/Application running成功；串口确认旧KV序号4->5，事件下一序号31，存储就绪，擦除0。ESP未上线，约每9秒出现一次USART3帧错误0x04，需核对启动输出和AT链路。
- 实板AT状态2连续超时，未进入CWJAP。临时74880未获得启动文本，已恢复115200。最终GPIO证据CRH=0x88264B22、ODR=0xC311、IDR=0x45D0：PB9为推挽输出且ODR高，但IDR低；5秒汇总仍RST=0/RX=0/观测字节0。需断电排查外部拉低/线序/模块，不能仅凭代码指定具体损坏位置。
- MQTTX已增加本机独立订阅smshub/sms-hub-8e7c4b91/#，未收到设备消息、未下发执行器命令。最终构建Flash55504B/RAM20176B、0错误0警告，KV仍序号5，短时堆824B/看门狗故障0。

## 2026-10-12 M6无线通信取证
- 当前工程已预留USART3 DMA1 CH3循环接收/CH2发送、PB10/PB11串口、PB9复位；`bsp_bus.c`仅完成行缓冲与回环发送，没有AT状态机或MQTT层。
- 野火ESP8266-12F资料默认115200，12F的IO0应保持3V3运行态；PB8在ESP-01例程中是CH_PD，不能当作12F的IO0控制脚。M6只对PB9执行低100ms再拉高复位。
- 例程出现`AT+MQTTUSERCFG/MQTTCONN/MQTTSUB/MQTTPUB`，但没有当前模块`AT+GMR`回显；版本、MQTT能力、IP/RSSI和LWT仍需上电实测，不能从资料宣称支持ESP-AT v2.2.1.0。
- M5普通构建Flash约56812B、RAM约19648B，剩余约4628B/832B；不新增任务/队列，采用固定RAM环形缓存和现有NetTask。
- 配置由LogTask私有持有；网络层需要受锁快照API。`storage_set()`异步入队，MQTT命令响应必须表达queued/rejected，不能在入队时宣称Flash已提交。

## 2026-10-13 M6实板验收边界

- 用户最新日志确认本地基线正常：JEDEC=0xEF4017、SPI=9MHz、KV序号3、事件日志下一序号27、存储就绪942ms，DHT连续成功、超声失败0、看门狗故障0，堆保持824B，任务栈余量均高于30%。
- 日志唯一网络异常为“串口3汇总：超时=1 错误=0 码=0x00 重启失败=0”；这不是HardFault，但验收时必须观察是否再次累计、是否在接入ESP8266后恢复。
- 当前日志没有AT+GMR、MQTT能力探测、WIFI CONNECTED、IP、RSSI、+MQTTCONNECTED、ONLINE、telemetry、cmd/resp、断网重连或LWT证据，因此只能判定STM32本地功能通过，不能判定M6无线DoD通过。

## 2026-10-12 Manual Test长按调查
- 当前controls_tick按稳定高电平计时，在KEY_LONGPRESS_MS=1500ms后产生一次KEY_LONG，释放不再补CLICK；MenuTask出队后传给menu_key。
- menu_key对KEY_WAKEUP/KEY_LONG调用navigate(-1)，MANUAL非编辑状态已有前项和首尾回绕；ADJUST模式长按应减值，须先PB15退出编辑才能切项。
- render_manual在font_test非零时无条件显示字体预览；navigate改变selected[MANUAL]但未清font_test，可导致已导航却仍显示预览。先补用户可见渲染回归验证，不据此声称已确定用户现场唯一原因。
- 既有Docs/test_m2.py只覆盖ROOT前项以及Manual Test增加值/返回，没有字体预览后导航的渲染断言；新增通过menu_key/menu_poll及真实OLED framebuffer比较标题的单条回归，准备先运行RED。
- RED确认：字体预览标题FONTS通过，随后KEY_LONG应显示MANUAL TEST的真实framebuffer断言失败；失败不是编译/依赖错误。最小修复在navigate中遇MANUAL导航清font_test，不改KEY_LONG生成、1.5秒阈值或编辑语义。
- 单行修复后原菜单/OLED回归GREEN。进一步通过15项真实帧缓冲逐帧比较覆盖前进/后退/首尾回绕，覆盖字体预览短按/返回和Fan/Servo长按减值/零边界；controls测试细化1490ms不发、1500ms发一次且释放不补CLICK。
- 扩展M2/OLED/菜单与M5驱动/controls/bench/急停模型均通过，输出归档Docs/M5-key1-tests.log。普通Keil Rebuild0错误0警告，Flash56812B/RAM19648B，较上一普通仅+28B/0B，压测宏仍0。
- 当前Objects HEX、Docs/M5-normal.hex、Docs/M5-key1-fix.hex SHA256均为6BD817F2F8492A14D1669D49FC6673A098AC4AC09C1C99139103ADBA56FC053F；旧普通构建日志/map/HEX保存在tmp/M5-before-key1。未进行本轮烧录或实板长按验证，未改变引脚/执行器/Flash策略，旧压测HEX与增量patch仍为历史交付。

## 2026-10-12 接线与烧录后的实板日志
- 来源：用户附件632611e9，单次启动至70605ms；用户报告已完成接线和烧录，日志不能识别所用HEX或舵机的实际接头。
- 209ms JEDEC=EF4017，272ms KV扫描=0/seq3，844ms事件扫描=0/下一序号11，859ms存储就绪；存储故障/停止写入0条，配置已保存0条。
- 70419ms DHT=33/33、连续失败0、超声失败0、发布失败0；DHT主/附值均GOOD，超声显示23..62cm/GOOD。确认通信恢复，尚未达到连续50次验收或温湿度/距离精度验收。
- 看门狗故障0、堆剩余816B，观察到的最低栈剩余54%；MQ仍禁用、MCU温度仍未校准，FAULT符合当前安全策略。Rd-03始终为1，只证明输入采样，不能验证无人场景解除。
- USART3汇总超时1/错误0/重启失败0，不能宣称联网成功；无M5压测统计、执行器动作记录、波形或复位保持证据。
- 当前固件SG90=PA8、风扇PWMB=PB1；扩展板PWM1=PB0、PWM2=PB1。先确认实际舵机信号没有误接PWM2，再开展Manual Test；PA6/MISO/PWMA及A通道物理风险仍待验证。

## M5约束（2026-10-11）
- 基线Flash54164B/RAM19488B，预算61440B/20480B；剩余7276B/992B。不增加任务、队列或定时器。
- PA4同时CS/STBY、PA7同时MOSI/BIN1；风扇独占直到停止，SPI事务在所有权内完成；PA6始终输入、PWMA不初始化、PB0保持输入。
- W25Q100ms拿锁失败现被storage视为介质故障；在出队之前拿外层SPI所有权，内部同任务SPI调用支持嵌套，跨模式嵌套拒绝。
- 现有标签v0.2/v0.3，没有v0.5；软件继续实现，发布与实物验收状态单独记录。
- 急停跨SPI收发存在后续取消片选拉高PA4/STBY风险；safe_state直接禁SPI1，select_flash在短临界区内遇故障只允许PA4低；回归注入传输中急停。
- M5最终普通56784B/19648B（+2620B/+160B），压测57876B/19696B；两种Keil构建0错误0警告，宏已恢复0。
- 主机真实M5状态机两方向100轮通过，JEDEC错误注入及中止停止通过；实际arbiter/W25Q独立模型双向100次、新读ID200次通过。不是实板无误转/无HardFault证据。
- 本地HAL_TIM_PWM_Start含MOE开启；源码/模型/调用图通过，50Hz/20kHz/机械误差/噪声/PA6及A输出瞬态仍须上板确认。
- M5相关当前HEAD93c3556；增量补丁标明包含此前未提交DHT/Flash修复及Keil调整，不能声称对不存在的v0.5发布。

## 2026-10-11 DHT11资料与无响应调查

- 最新日志DHT0/26、无响应26次、捕获段0、起始低21ms；不能用解码或校验解释从未看到响应下降沿，也不能直接断言模块损坏。
- 模块ZIP原理图J4线序3V3/GND/NC/DATA，10k上拉至3V3；扩展板规格书第12页IO3线序3V3/GND/NC/PB12。带转接板直插IO3时当前PB12正确，官方例程GPIOB0只是示例接线。裸传感器VDD/DATA/NC/GND与转接板不同。
- 奥松V1.3手册表10：起始低18~30ms，释放后10~35us开始响应；上电等待1秒，采样间隔>2秒。模块手册允许上拉输入释放，例程高30us后切输入不能照搬为RTOS无限轮询。
- 本地F1 HAL在输入上拉/EXTI初始化中先置ODR高再改CRH、AFIO/触发/IMR。现有驱动确实会释放总线；缺陷是接收配置晚于释放，而且随后清PR，可能丢掉早响应沿。仅此风险不足以证明现场所有捕获段0的根因。
- 现有Docs/test_m3.py只在vTaskDelay中按active强行注入帧，不模拟ODR、方向、释放或PR，不能验收握手顺序。下一步补实际GPIO边界模型。
- 本轮保留BSP/bsp_timer_us.c及此前Flash修复、既有PB12/优先级/捕获82段/脉宽范围/累加校验/看门狗，不凭推测换引脚或放宽验证。
- 新增Docs/test_dht_handshake.py：传感器只在物理开漏低18~30ms后释放时启动定时响应；PR不依赖IMR，临界区延后ISR，模型遵循本地HAL先写ODR后配EXTI的顺序。13us响应/零配置延迟可正常解码；10us响应叠加注入16us配置延迟时旧驱动报INCOMPLETE/81段，断言失败。16us是故障注入，不是板测时间，亦未复现现场NO_RESPONSE/0段。
- 最小修复：初始化时预配置双沿EXTI并屏蔽；每次起始低后先清旧PR、开启接收再显式释放开漏，接收期间不重新配置GPIO/AFIO也不清PR，结束恢复上拉输入。外部10k上拉来自当前模块原理图。
- 修复后早响应与原M3回归通过。增加最近失败的DATA起始低末端/刚释放/捕获结束电平与接收IRQ次数；0/1为采样电平、2为该阶段未执行。断线模型=0/1/1且IRQ1，持续低模型=0/0/0且IRQ0，禁止IRQ模型=0/1/1且IRQ0，均有界失败并释放开漏/关闭DHT IMR；这些是模型观测，不替代电压或真实波形。
- ST RM0008 Rev21本地PDF第163页9.1.8确认开漏ODR1为高阻、输入缓冲/IDR仍有效、内部上下拉关闭；接收依赖模块原有外部10k上拉。刚释放电平为瞬时采样，不能单独用低电平断言短路。
- 最终握手边界、原M3/M2/实际GPIO IRQ映射与Flash/掉电存储全部回归通过。Keil全量0错误0警告，Flash54164B/RAM19488B，较前版+72B/+24B，余7276B/992B；map/反汇编确认新握手和W25Q保护链接、普通无测试入口。HEX SHA256=2BE9A21D00F66D0DD3690BF67B98E28D555404D03F9C98E6BA0573A1F79B9956。
- 独立交付Docs/M3-dht11-fix.md、M3-dht-fix-build.log、M3-dht-fix-map.txt及M3-dht-fix-tests.log，未覆盖旧M4构建资料。实板0段根因及50次成功率仍待新日志，未烧录、未假报全部硬件修复。

## 2026-10-11 Flash修复后的实板日志

- 来源：`C:/Users/l/.codex/attachments/8d5ac49f-7c15-403a-bc74-679852cb7bea/已粘贴的文本.txt`。日志截至60235ms，单次启动；不是20次复位或掉电验收记录。
- 209ms JEDEC=EF4017；270ms KV扫描=0/seq2；784ms事件扫描=0/下一序号1；852ms存储就绪/最近事件1/擦除1。按当前storage_start路径，达到就绪意味着启动事件编程、读回及最近事件读取已成功。
- 全文存储就绪1条、存储故障0条、配置已保存0条。可确认启动故障修复实测有效，不能据此宣称新配置保存及复位保持已通过。
- 系统日志堆824B，看门狗故障0，最低栈剩余55%；OLED帧率约11.0~12.1fps，连败0/恢复0。
- 其它现象分别记录，不扩大本轮固件修改：DHT 0/26且无响应26次/捕获段0；超声累计失败109，最近失败4在本地enum中为NO_FALL，随后有GOOD恢复；USART3错误码0x04在当前HAL头文件中为FE帧错误，恢复失败0。MQ与MCU温度仍为设计上禁用/FAULT，不能当作本次Flash故障证据。

## 2026-10-11 Flash KV 写入失败日志分析

### 日志证据

- 用户日志中的关键顺序是：`Flash JEDEC=0xEF4017 SPI=9MHz`，随后 `KV扫描结果=-1 序号=2`，再报 `KV扫描/默认配置写入失败` 和 `停止写入`。
- JEDEC 正确只证明一次 SPI 命令和基本收发成功，不等于后续 256B 读、页编程、擦除、读回校验均成功。
- 当前 `storage_start()` 将 `kv_load()` 的所有负返回合并为同一条错误日志，因此现有串口信息无法区分扫描读失败、默认写失败、页编程失败、读回/CRC失败。

### 当前源码对 `-1` 的精确定义

`Drivers_Device/kvstore/kvstore.c` 中 `kv_load()` 返回 `-1` 的路径只有：

1. 扫描某个 512B 槽时 `read_record()` 调用 `w25q_read()` 失败；
2. 没有任何合法记录且任一 Bank 没有尾部空槽，代码拒绝自动抹掉未知数据；
3. 没有合法记录时，写入两份默认配置的任一次 `kv_save()` 失败。

单个记录的 magic/version/字段/CRC/commit 不合法本身不会直接返回 `-1`，只会被跳过。

### `序号=2` 的约束

- `storage_setup()` 先将 `current_config` 清零并填默认字段；初始 `seq` 为 0。
- `kv_save()` 只有在完整写入、读回并校验成功后才执行 `*c=work`；失败不会把调用者的序号更新到失败中的值。
- 因此在当前源码与当前日志格式一致的前提下，`KV扫描结果=-1 序号=2` 更符合：扫描先发现了一个 `seq=2` 的合法记录，之后读取另一个槽时发生底层 `w25q_read()` 失败。若实际烧录的固件不是当前源码，则需先核对 ELF/烧录时间。

### 根因优先级（按当前证据修订）

1. **首要调查：扫描中途 SPI 读取失败。** 当前 HAL master2-line Receive 实际走 TransmitReceive，8位收发循环逐次检查 `HAL_GetTick()-tickstart >= Timeout`。LogTask 优先级 1，OLED 菜单 2、同步 UART 日志可能来自更高优先级；一次抢占可超过 10ms SPI 超时。SPI mutex 只排除其它 SPI 使用者，不能阻止这些任务抢占。
2. **仍待板测：硬件/共享总线条件。** 供电、接触及 PA4~7 与 TB6612 共享的电气风险仍需实测，不能用一次 JEDEC 正确或 9MHz 低于例程 18MHz 排除。
3. **当前日志不支持作为同等主因：旧数据/保护位。** 当前实现首次引入 KV 即 512B，未证实旧 128B 格式曾烧录；BP 保护导致写失败也不能单独解释读到合法 seq2 后扫描失败。无合法记录且任一 Bank 没有尾部空槽会保护性拒绝初始化，这是不同路径。

### 最小验证顺序

1. 完全断电，拔掉 TB6612 和电机，只保留 W25Q64；确认 `3V3 -> VCC`、`GND -> GND`、`CS -> PA4`、`CLK -> PA5`、`DO -> PA6`、`DI -> PA7`。
2. 先做独立总线测试：读状态寄存器 -> WREN/WEL -> 擦除 `0x000000` -> 全 4KB 读回 `0xFF` -> 写一小页 -> 读回逐字节比较。
3. 若独立测试通过，再在允许丢弃旧 KV 的前提下只擦除 `0x000000` 和 `0x001000` 两个 KV 扇区；不要先修改代码为自动擦除未知数据。
4. 若仍失败，增加 bank/slot/address、操作阶段、WEL/WIP/status、BUSY 超时和读回首个不一致地址的日志；否则无法从当前汇总日志定位到具体事务。

### 2026-10-11 新日志追加证据

- 新附件中至少出现两次完整启动，均在 `JEDEC=0xEF4017` 后约 57~58ms 得到 `KV扫描结果=-1 序号=2`；两次均没有出现“存储就绪”。
- 存储故障后仍持续打印传感器、菜单和任务心跳；堆保持 824 字节，看门狗故障为 0，任务栈最低约 55%。因此本次 KV 故障不能归因于看门狗复位、堆耗尽或任务栈溢出。
- 故障在复位后稳定复现，说明更像固定介质状态/固定槽地址/固定 SPI 事务失败，而不是一次性的忙状态或随机接触不良。
- 两次失败都保留 `序号=2`，进一步支持“扫描读到 seq=2 后在后续槽读取失败”或“实际烧录程序与当前源码不一致”的判断；下一步必须打印失败的 bank、slot 和地址。

### 2026-10-11 修复回归

- 在 `Docs/test_flash_bus.py` 的实际 KV/W25Q/仲裁组合中，先初始化两份合法默认值，再在 Bank B 第二槽 `0x001200` 的接收阶段注入 40ms 任务抢占。未修改驱动时输出 `result=-1 seq=2` 并断言失败；保护单次 HAL 收发后输出 `result=0 seq=2` 并通过。
- `vTaskSuspendAll()`/`xTaskResumeAll()` 仅包围单次 HAL polling，最长 256B；中断保持开启，TIM4 HAL tick 可继续计时。BUSY 轮询、mutex、GPIO切换、日志均不在暂停区内。正常 256B 在 9MHz 的线速约 0.228ms，不代替板上总耗时实测；真正外设故障仍最多占用原 10ms HAL 超时窗口。
- `Docs/test_storage.py` 已通过：500 次配置保存/复位、逐字节擦除/数据/commit断电、事件环覆盖/恢复、CRC回退和应用失败保留配置。
- 本测试证明软件存在可修复的调度超时缺陷，不能证明现场硬件只有这一个原因。若新固件仍故障，需补充具体失败地址/HAL状态并复查共享总线电气条件；本次不自动擦除未知数据、不重试结果未知的写入。
- 注入真正的 HAL TX/RX 超时后仍报错并释放总线、恢复调度；扫描 I/O 失败后 `kv_save` 拒绝写入，重新成功扫描才恢复。OLED/菜单/M3回归均通过。
- Keil Arm Compiler V6.24 全量构建 0错误0警告，map 验证 tx/rx 均调用暂停/恢复调度，普通 ELF 不含压力/看门狗故障注入变量。Flash=54092B（Code49360+RO4620+RW112），RAM=19464B（RW112+ZI19352），余量分别7348B/1016B。最初进度消息只相加Code+RO报53980B，这里已按完整ROM映像更正。

## M4当前实现
M4存储软件实现/主机故障注入通过。完整配置512B已无法用旧128B槽，改为每独立4KB Bank8槽；地址不变，预留扇区不碰。事件头尾通过全环CRC扫描重建，无独立热指针扇区。
最重要边界：配置收到成功提示才热加载；IO结果未知后离线等重启扫描，禁止盲目重试。Flash与TB6612实际共享引脚的电气切换仍待后续实测，M4先拔电机驱动测试存储。System Erase boot是启动内计数不是寿命计数。
普通Flash53664B/RAM19464B，余7776B/1016B，后续需严格预算；真实掉电性能/ID/栈余量尚未验证。

2026-10-07用户已确认超声更正：当前固件PA15触发/PB3回波，新增EXTI3优先级5，PB3彻底移出GPIO输出组，旧EXTI15回波移除。独立官方映射/实际GPIO init/实际IRQ回调测试从失败变通过；脉宽模拟改为物理GPIOB pin3，不能沿用旧GPIOA假设。Flash45484B/RAM16960B，0错误0警告，未宣称板上测距已验证。

## 2026-10-07超声波管脚根因
用户新增的官方模块资料与原理图、HAL头文件、扩展板接口手册共同确认：TRIG应为PA15、ECHO应为PB3，当前恰好反了。原模块接口GND/ECHO/TRIG/VCC与扩展板GND/PB3/PA15/3V3对齐。旧固件PB3输出与模块ECHO输出可能争用；历史脉宽值不能当作可靠测距。详见Docs/M3-ultrasonic-root-cause.md与独立宏对照JSON。本轮只记录证据，因固定接线约束须等用户确认后再改固件；不要照搬占用RGB TIM2的官方输入捕获例程。

最新M3日志：DHT37/38且分类0，已确认早于1秒的首次注册表采样计入一次未就绪失败，调度已修正。超声曾测107cm，42.804秒后时间戳停滞、50/50边沿不再增加且ECHO1；触发前高保护生效，硬件来源未确定。OLED0x20为HAL timeout，短块polling可被较长同步日志抢占；新增console mutex逐块保护并保留deferred语义，待复测物理错误是否消失。详见Docs/M3-runtime-diagnosis.md最新节。

## M3实际运行异常待定位
2026-10-06用户日志：DHT末次13/24=54.2%，超声时间戳0且488次失败，OLED平均11.3~11.7fps但恢复计数增加。用户确认超声已插原接口。六栈余量55~79%，暂无看门狗锁故障；这些数据不能证明传感器DoD通过。
旧代码缺少失败分类；本次只增加DHT/超声/OLED诊断，不猜测物理根因、交换线或放宽校验。细节见Docs/M3-runtime-diagnosis.md。

## M3当前实现结论
M3软件已实现（2026-10-06）：DHT11 PB12占用已停用EC11的EXTI12，保留TIM4和共享EXTI15_10优先级；未改硬件管脚。F1 HAL_GPIO_Init(INPUT)不自动清除EXTI IMR，完成DHT捕获后需显式关闭该线，已实现。
DO不能检测真实开路；MQ因5V上拉默认禁用/FAULT，Rd-03标记无开路自检。内部温度仍FAULT，VDDA独立质量和DMA时间戳。未把这些通道假定为在线。
主机测试和编译通过，Flash44064B/RAM16816B；不能据主机模拟声称板上50次成功率、三点±1cm或真实抗抖已通过。

## M2最新
OLED用静态1KB显存，整个画面在RAM完成后发送，不将清屏中间态发送到OLED。按80ms截止时间刷新；报告成功帧率x10和TIM4实测传输us，不能把理论I2C时间或主机仿真当作实测。
EC11编码器ISR保存AB快照+任务通知，MenuTask解码每4边沿一格；通知本身不能保存多次AB边沿，所以有64槽静态快照环。物理每卡点边沿数/方向仍需20格测试。
恢复后连续失败归零、累计恢复保留，已在文档说明。独占I2C所有权由启动SensorTask顺序交给MenuTask，无新增大堆分配。

USART3刷屏：用户提供18~28ms重复警告。确认源码将间隔超时/硬件错误混为一条并在恢复前逐次打印；无法仅凭旧日志确认PB11浮空或溢出的具体根因。已改为恢复后5s汇总、保留错误码/次数和30ms保护；PB11浮空输入未改。模拟200连续错误及全部原UART测试通过；板上仍需错误码与接线信息。

RGB低有效修订已获用户确认并实施：TIM2 LOW极性、GPIO高电平熄灭。之前“等待批准”记录为历史。中文日志采用UTF-8，编译及ELF字节检查通过，烧录后现象待用户验证。

## M1 RGB现场反馈与根因
用户反馈：烧录后RGB常亮，复位后依次闪烁再常亮。已视觉核对用户核心板原理图第3页：LED1/2/3阳极接3V3，阴极经1K到PA1/PA2/PA3，为低有效。当前TIM2 PWM1+HIGH使CCR=0输出低，rgb_set(0,0,0)实际点亮三路，和反馈一致。板级初始GPIO低也会点亮。
修订建议：TIM2三路OCPolarity改LOW，GPIO初始熄灭电平改SET；保持rgb_set(0)=灭、999=最亮及故障CCR=0语义，不改引脚。按用户硬件不符须等待确认的要求，本轮未修改固件，等待批准低有效约束。

## M1最新状态
M1代码已实现，0错误0警告；主机实际按键/蜂鸣器、USART3解析和微秒边界测试通过。FreeRTOS10.3.1固定内核，8个任务含6业务+idle+timer。M1软件可交付，上板DoD未通过，不打v0.2；详见Docs/M1-acceptance.md。
候选校准继续禁用；内部温度FAULT。IWDG4秒是标称约数，故障到复位还包括检测延迟和LSI容差。不得声称满足严格4s上界。
故障注入测试构建通过；恢复宏0后的正式ELF无m1_stall_task。恢复构建时Keil工程曾被覆盖回M0分组，修复后正式全量构建通过。

## 最新状态（下方首次核查保留作历史）
用户已确认：TIM4替代TIM6；共享EXTI优先级5；A通道不驱动；KV独立4KB扇区；SPI除8；日志末地址0x012FFF；三个校准地址仅诊断候选且默认禁用。
源码已由CubeMX6.18.1生成并后处理；Arm Compiler6.24完整重编译0错误0警告。Flash5824B，RAM1392B。见Docs/M0-build.log、Docs/M0-map.txt及Docs/M0-tests.log。
NOJTAG位于BSP/bsp_board.c及生成的Src/stm32f1xx_hal_msp.c；共享EXTI先PA15；主机边界测试与配置编译断言通过。
生成器遇到I2C参数元数据就绪警告；实际生成I2C源码为400kHz/Fast模式参数，编译通过。生成器提示不等于固件编译警告，日志完整保留。
DAP未检测到，尚无下载/自动运行实测；阶段不标完成，v0.1标签待上板验收。

核查日期：2026-10-05。以下为静态约束与本机软件检查，不是板上实测。

## 输入
- 用户附件：C:/Users/l/.codex/attachments/90f17c1a-2f5c-4deb-b6ab-3091bbaf6fec/已粘贴的文本.txt
- 工作区原有两份板卡 PDF 和一张 JPG，本轮未修改、未解析这些文件。

## 已核实的硬件配置冲突
1. ST 官方 CMSIS stm32f103xb.h 仅定义 TIM1~TIM4，没有 TIM6。附件 A 的 HAL TIM6 时基与 TIM6 IRQ 不能用于 STM32F103C8。
2. 同一头文件定义 EXTI15_10_IRQn，PA11/PA12/PC13/PA15 共用一个 NVIC 向量，不能各设优先级 12/5。
来源：https://github.com/STMicroelectronics/cmsis-device-f1/blob/master/Include/stm32f103xb.h
建议供用户审阅：M0 暂用 SysTick 作 HAL 时基；M1 用 TIM4 更新中断及软件扩展计数兼顾 HAL tick/us 计时，SysTick 交给 FreeRTOS。此建议会改变“不使能 TIM4 中断”，尚未实施。共享 EXTI15_10 统一优先级 5，ISR 保持短小，尚未实施。

## 依据用户连接与参数可推导的冲突
- PA6 同连 W25Q MISO 与 TB6612 PWMA。Flash 读数据时 PA6 必须允许变化，不能始终为 0；强制输出 0 会与 Flash 输出冲突。PA4=0 时 Flash 被选中，同时 TB6612 STBY=0，可令桥处于待机。因此可以讨论“A 通道始终不驱动”，但不能声称满足“PWMA 永远为 0”。用户未批准改变表述。
- PA5 同连 SPI SCK/AIN1，SPI 工作时不能满足附件 B 注释的 AIN1 固定 0。
- PB0 保持输入，不能为控制 AIN2 而驱动 MQ2 DO。
- 两份 KV 分别占 0x000000~0x0007FF、0x000800~0x000FFF，而附件 B 定义扇区为 4096 字节；两份落在同一擦除扇区。需要回收擦除时，两份会一起消失，双副本+CRC 本身不能保证断电原子更新。建议保持主地址，使用空闲 0x001000 扇区作恢复暂存；这属于新增布局用途，待确认。
- SPI1 在 APB2=72MHz，/4 是 18MHz，不是 9MHz；如目标为 9MHz，应 /8。未修改配置。
- 附件 B 的 VREFINT_CAL_ADDR=0x1FFFF7BAu 不能直接当作 F103C8 已验证校准数据；需要依据该芯片资料确认，不能直接解引用用于测量。
- 附件 B 缺少部分显式引脚宏（如 RGB、I2C、SPI 数据线、串口），而用户又要求完整集中定义；后续需要保留已确认映射并补充宏。
- M0 指定的最小 main 不启动串口/OLED，不能以串口启动文字或 OLED 显示作为本阶段已有证据。

## 工具链现场检查
- 注册表：Keil μVision 5.43.1.0，路径 D:/Keil_v5/UV4/UV4.exe。
- 注册表：STM32CubeMX 6.17.0，路径 D:/stm32/STM32CubeMX.exe。
- 进一步读取该 EXE 的 FileVersion 为 6.18.1-RC2，与注册表不一致；运行版本尚需通过 About/命令行确认，不能以注册表版本作为最终工具链基线。
- git --version：git version 2.54.0.windows.1。
- 默认 C:/Users/l/STM32Cube/Repository 未发现 STM32Cube_FW_F1_* 目录；不代表其它位置未安装。
- 已存在 KEIL USB 驱动包记录，不代表已连接 CMSIS-DAP 或可以下载。
- 精确筛选当前设备名称 CMSIS-DAP、DAP、CH340、ST-Link、J-Link、ULINK，未返回匹配项；尚未实测 DAP 下载。

## 验收状态
- 固件代码未生成，未编译，未烧录，无 map；Flash/RAM 增量未测得。
- NOJTAG 需求已确认，尚无实现位置可验收。
- bsp_pin.h 未生成，不宣称与附件 B 一致。
- M0 未完成，未创建阶段提交及 v0.1 标签。
- M1 具体任务未提供，暂建议仅核心板、DAP 与 CH340 调试连接，外设清单待 M1 明确。

## 2026-10-13 M6归档边界复核
- 拟提交51个相关源码、测试、工程、文档和现场证据文件，全部通过本地密码匹配检查；凭据文件及含凭据的 HEX/AXF 不进入 Git。
- 用户日志和截图可确认约14.3秒上线、retained online及约5秒 telemetry；完整 cmd/resp、断网/LWT 和4小时稳定性仍待实机验收，本次不发布 `v0.7`。
# M8 接口收敛
- 用户要求优先预留接口：通用IO1= A5/A12/GND/VCC0，IO2=VCC1/GND/A11/A4，电压选择5V；MQ2 DO改PA12，MQ135 DO改PA11，AO均空。保留转换与MQ门禁，避免AO接共享SPI。
- ST本地DS5319 Rev20第31页将PA11/PA12标FT；核心板第3页USB DP含1.5k上拉至3V3，所以仍采用转换，不直接四针插入。web核验工具仍返回空内容，不据此宣称在线来源成功。
- M8实现保持KV记录布局、20事件网络环和六任务；遥测采用2s固定周期，附质量/状态；故障终止输出不用printf或RTOS锁。
- 原MQTT在CIPSEND提示后重新读DataHub编码，数值位数变化会导致包长不一致并复位。M8复用DMA缓冲，仅保存AT命令覆盖的20B前缀，提示后恢复同一份报文；增加跨提示9→1000的真实C回归。
- 第一版临界区源码/行元数据构建63092B超过60KB；改为LR地址键、map/debug解析，不裁剪校验或超时，审计构建61268B/RAM20344B。GCC存储回归曾在审计宏1/最近缓存4下运行并触发假设20条的断言，正式宏恢复后重测。
