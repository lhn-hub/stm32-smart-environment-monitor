# ESP8266升级准备与板载CH340接线

日期：2026-10-13。状态：升级路线已取消，本文件仅保留历史准备记录，以下下载接线与刷写步骤不属于当前方案。现有 AT 固件的 TCP-MQTT 已实机上线，见 `M6-live-test.md`；未备份或刷写 ESP Flash。

## 当前证据

- COM5只读采样是STM32任务日志，不是ESP ROM下载通道；见M6-esp-upgrade-port-probe.log。串口已释放，未发送下载同步或擦写命令。
- 模块曾实测AT1.7.4.0/SDK3.0.5，实际MQTT配置15次均ERROR；不能用电脑MQTTX回显代替设备上线。
- 用户要求继续由开发板给扩展板供电。本方案保留原供电，不需要独立电源或额外USB-TTL。
- 用户改线后第一次ROM `flash-id` 探测（COM5，115200，no-reset/no-stub）返回 `No serial data received`；随后AT只读和74880启动监听均为0字节。当前接线尚未通过检测，尚未读取容量、备份或擦写。

## 复用板载CH340

已按用户提供的双USB核心板原理图第5、6页核对：CH340X工作在3V3域，J5的两组串口跳帽连接它与STM32。

| J5引脚 | 网络 |
|---|---|
| 7 | STM32 PA9 |
| 8 | CH340 RXD |
| 9 | STM32 PA10 |
| 10 | CH340 TXD |

只拔掉7-8、9-10两组串口跳帽；BOOT0/BOOT1跳帽保持原状。不要根据照片左右位置猜针脚，以板上PA9/RXD、PA10/TXD丝印识别；实物不一致时先提供近照。

断电后改线：

1. 保留开发板到扩展板，以及扩展板到ESP的VDD/GND供电链。不要再并接第二路电源。
2. 断开ESP的RXD/TXD/RST与STM32的PB10/PB11/PB9连接。若模块IO还连着PB8或其它输出，也断开。六针插座若不能逐根隔离，须改用杜邦线保留电源两针，不能整组插着同时叠加串口。
3. 拔掉核心板PA9/RXD、PA10/TXD两组串口跳帽，保管好。将CH340侧RXD（J5-8）接ESP的TXD，将CH340侧TXD（J5-10）接ESP的RXD。不能接跳帽另一侧PA9/PA10。
4. 模块六针接口的IO（GPIO0，J1-6）接GND。模块RST（J1-5）与PB9断开后可悬空，由模块板上电阻上拉。
5. 重新给开发板供电，电脑连接板载CH340的USB口，模块即在GPIO0低状态启动。保持IO接地，等待容量读取及备份。核心板RESET按键只复位STM32，不能代替ESP复位。

模块原理图已确认GPIO0上拉、GPIO15下拉、RST/EN上拉，不需要用户焊接GPIO13/15或GPIO2。所有信号按3.3V逻辑连接；本方案不改变现有VDD电源。暂时断开风扇/舵机电源，防止改线或STM32复位时产生动作。

## 已准备材料

均保存在Git忽略目录tmp/esp-upgrade；原始发布包未修改。

- esptool v5.1.0 Windows独立包：执行version成功，SHA256与GitHub发布digest一致：F68A8F7728ADFC59CD60F9424928199E76EAC66372C7BDC23898AA32753A437A。
- 官方ESP8266-IDF-AT_V2.2.1.0.zip：本地SHA256为CB57E8AFD4CCCCAE8204790ACC41322E6DDBBFD813AFBD1192D828F686F7542B。该值用于后续本地核验，不冒充发布者签名。
- 包内sdkconfig明确CONFIG_AT_MQTT_COMMAND_SUPPORT=y；flasher_args.json规定dio/80m/2MB布局，factory_param地址0xF1000。2MB是镜像布局，不是已测实际芯片容量。
- 使用同tag官方factory_param_generate.py生成原参数，与发布factory_param.bin全部4096字节一致。
- 生成factory_param-rx3-tx1.bin：只改变偏移16..19四字节，TX/RX/CTS/RTS=1/3/-1/-1，波特率115200。SHA256为87125B9C7BE7303D3299F5662303B6BAD93C3A59D238472A7D57C2BF391AB220。其余参数未修改，不重新编译AT或STM32。
- 工具的xlrd2.0.1依赖仅在tmp/esp-upgrade/pydeps，不修改全局Python。

默认官方固件使用GPIO15/13，与模块六针接口不匹配。升级时须使用已检查的GPIO1/3参数分区，不能直接刷默认映像后继续使用旧接线。

## 接线后的执行顺序

1. 重新枚举串口并核验ROM连接，只进行chip-id/read-mac/flash-id，记录实际容量。资料标称4MB不能替代检测。
2. 按检测容量从地址0读取全部原Flash，保存到Git忽略目录；检查长度和SHA256，再独立重读并比较。读取失败或两次不一致时停止，不擦写。
3. 根据实际容量确认官方分区边界和模块兼容性。按flasher_args.json生成烧写清单，替换且仅替换factory_param分区；不猜地址或根据标称容量重排分区。
4. 备份校验成功后才擦写，并验证实际Flash与目标文件。是否需要完整擦除、Flash速度兼容性以实际检测为准。
5. 断电恢复模块IO悬空/上拉；先通过直连CH340验证AT、GMR、UART和实际MQTT配置，再断电恢复两串口跳帽及PB10/PB11/PB9正常接线。
6. 回到M6联调验证IP/RSSI、online、telemetry与cmd/resp；优化版STM32 HEX的下载是另一步，不把本次ESP升级当成已下载优化版。

备份可能含Wi-Fi凭据，不公开上传、不提交Git。尚未有升级后AT应答、实际Flash容量或MQTT上线证据；不标记升级成功、不创建v0.7。

## 官方来源

- https://github.com/espressif/esp-at/releases/tag/v2.2.1.0_esp8266
- https://github.com/espressif/esp-at/files/6946235/ESP8266-IDF-AT_V2.2.1.0.zip
- https://github.com/espressif/esp-at/blob/v2.2.1.0_esp8266/docs/en/Compile_and_Develop/How_to_set_AT_port_pin.rst
- https://github.com/espressif/esp-at/blob/v2.2.1.0_esp8266/tools/factory_param_generate.py
- https://github.com/espressif/esp-at/blob/v2.2.1.0_esp8266/components/customized_partitions/raw_data/factory_param/factory_param_data.csv
- https://github.com/espressif/esptool/releases/tag/v5.1.0

web工具本轮返回空白，以上发布与参数原文通过GitHub API/raw实际读取核验。本地工具仅执行version/help，未向设备发送下载协议。
