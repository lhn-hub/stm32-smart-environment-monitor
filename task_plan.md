# sms-hub M0 工程脚手架

### Phase 12: Manual Test KEY1长按回归
**Status:** complete
- 追踪controls_tick生成KEY_LONG、MenuTask出队和menu_key/navigate/render_manual路径，保留现有GPIO、1.5秒长按门槛及编辑模式减值语义。
- 先用现有主机测试复现导航/字体预览状态，再修复已确认根因；不把未获得现场长按日志的原因猜测当成结论。
- 验证Manual Test向前/向后、首尾回绕、风扇/舵机编辑和返回安全态，再执行普通M5构建并交付新HEX。
- 真实帧缓冲RED复现字体预览遮盖导航，导航清预览一行修复GREEN；15项双向/回绕、字体短按/返回、编辑边界、1500ms事件及M2/M5模型回归通过。
- 普通M5构建0错误0警告，Flash56812B/RAM19648B，仅+28B/0B；交付Docs/M5-key1-fix.hex，实物长按复测待用户执行，不认定已确定用户现场唯一原因。

### Phase 11: M5执行器、联动原语及总线仲裁
**Status:** in_progress
- 复用AlarmTask及controls软件定时器，不增加RTOS对象；先完成SPI/FAN互斥和存储排队保护。
- 实现SG90限速、TB6612 B通道缓启、灯效蜂鸣和Manual Test；保留DHT及Flash修复。
- 两方向各100次板测命令、主机回归、正常/压测编译、资源增量及波形说明。
- v0.5不存在且M4掉电验收待完成；v0.6发布依赖及物理DoD不能提前声称通过。
- M5软件及主机验证完成，两构建0错误0警告，普通56784B/19648B，基线增量2620B/160B；正常宏恢复0。
- 剩余实物三点精度/噪声、双100次板测、共享引脚/A输出截图及断电/急停验收；详细步骤已交付Docs/M5-acceptance.md。
- 2026-10-12用户新日志：Flash就绪/seq3，DHT33/33、超声失败0、看门狗故障0；仅确认本次启动/采样，执行器接线/动作、保存复位及压测仍待实物记录。

## M4当前工作
M4软件、主机故障注入和编译完成；2026-10-11日志已确认真实W25Q ID、KV/事件扫描及启动事件写入成功，保存复位与真实掉电验收仍待执行。当前固件合入DHT修复，Flash54164B/RAM19488B；独立4KB Bank、512B槽不变。M3物理遗留未冒充通过。

### Phase 10: DHT11无响应修复
**Status:** complete
- 已核对模块手册、原理图、官方例程与扩展板第12页：转接板直插IO3时DATA=PB12；裸传感器线序不同。
- 已追踪实际GPIO/EXTI/ISR/采样调用链；当前释放发生在HAL EXTI配置完成之前，随后清PR存在丢响应沿风险，尚不能认定为现场0/26的唯一原因。
- 先补遵循开漏低脉冲/释放/响应时序的GPIO边界回归，再修正交接顺序，保留校验、超时及故障质量。
- 早响应故障注入先失败后通过；断线/持续低/IRQ阻塞/恢复/超长起始/响应边界/PR隔离/回绕及原M3/M2/映射/存储回归通过。
- Keil全量0错误0警告，Flash54164B/RAM19488B；已核对map/实际反汇编/普通HEX及此前Flash保护仍链接，交付Docs/M3-dht11-fix.md和独立构建、测试记录。实际通信成功率仍需新固件板测，未烧录或提前发布v0.4。

### Phase 9: Flash KV 启动失败修复
**Status:** complete
- 已追踪到低优先级 LogTask 中 HAL polling 的墙钟超时；高优先级 OLED/日志可持续抢占超过 10ms。
- 扩展实际 KV/W25Q/仲裁回归，注入扫描发现 seq2 后的 40ms 抢占，先运行失败测试。
- 仅在单次短 HAL SPI 收发期间暂停调度，保留中断与 BUSY 轮询让出；随后运行回归及 Keil 全量构建。
- 抢占回归先失败后通过；真实 HAL 超时注入/掉电存储/OLED/菜单/M3回归通过。Keil 0错误0警告，Flash54092B/RAM19464B；普通 HEX 已生成，实物复测待完成。
- 用户新实板日志：270ms KV扫描成功/seq2，784ms事件扫描成功，852ms存储就绪；至60235ms无存储故障。启动问题实测通过，未记录新配置保存或复位保持。

### Phase 8: Flash KV 启动失败分析
**Status:** complete
- 已读取用户启动日志、当前 KV/W25Q/SPI 仲裁源码及 W25Q64 模块资料。
- 已按源码返回路径解释 `kv_load=-1`，并使用日志中 `序号=2` 约束根因排序。
- 结论：优先验证扫描中途 SPI 读失败及共享总线/供电条件；旧格式/非空介质和 BP 保护为次级假设。

### Phase 7: M4掉电存储
**Status:** in_progress
- 驱动及提交协议、主机断电注入、LogTask异步队列、最近20条浏览。
- 超60KB Flash/20KB RAM须先报告；v0.5待实物断电验收。

## Goal
按用户 M0、附件 A/B 完成 CubeMX/Keil 工程，真实编译与 DAP 验证后提交并标记 v0.1。

## Current Phase
Manual Test字体预览导航修复已交付，等待实板长按复测；M5其它物理验收、M3精度/50次、M4掉电仍待执行，MQ禁用。

### Phase 1: 工具链与硬件约束核查
**Status:** complete
- 已读取用户附件并列出手工硬件检查清单。
- 已确认芯片无 TIM6，共享 EXTI 向量不支持按线路独立分配 NVIC 优先级。
- 已检测 Keil、CubeMX、Git；固件包位置与 DAP 实物连接仍需确认。

### Phase 2: 生成工程与配置集中化
**Status:** complete
- 在用户确认冲突处理后生成工程、建立目录与配置头文件。

### Phase 3: 编译、下载、提交与验收
**Status:** complete
- 保留 0 warning 编译日志、map 数据和 DAP 自动运行证据。
- 完成后提交 [M0] 工程:建立脚手架，打 tag v0.1。

## Next Step
烧录Docs/M5-key1-fix.hex，在Manual Test READY状态按住KEY1约2秒验证前项；若仍不切换，提供对应按键事件日志及当时项目/编辑状态。

### Phase 6: M3传感器与DataHub
**Status:** in_progress
- 软件实现完成：六实例、DataHub互斥、5样本滤波、故障质量、阈值菜单同步及实时主页。
- DHT采用EXTI12脉宽捕获；超声PA15/30ms超时；共享ISR先超声再DHT再按键。
- 主机协议/超时/回绕/质量/恢复/MQ门控/菜单回归通过。未模拟为真实50次采样成功率或±1cm实测。
- MQ DO硬件冲突和单DO开路不可观测限制保留；正式固件禁用两路MQ并标FAULT。

### Phase 5: M2 OLED、EC11与菜单
**Status:** complete
- 静态1KB framebuffer、脏页刷新、ASCII/中文小字库。
- 编码器边沿快照通知，任务上下文4态解码；菜单3级栈。
- 编译/主机验证后交付上板20格、帧率、恢复测试；v0.3待验收。

### Phase 4: M1 BSP与FreeRTOS骨架
**Status:** complete
- 用户已提供M1完整任务，明确依赖M0完成。
- 用户补充M0下载成功日志后继续；旧入口核查已过时，以本节为准。
- 保留TIM4及校准默认禁用裁决；M1温度范围DoD仍存在冲突，待明确验收标准。

## Errors Encountered
- Manual Test字体预览后长按RED为预期失败：真实帧缓冲仍为FONTS，期望MANUAL TEST；增加测试错误标题后复现同一行为，随后修复导航时清预览状态。
- 首次导出补丁被PowerShell写成CRLF，临时index校验所有上下文失败；改为UTF8无BOM/LF后git apply --check --cached通过，实际工作区与真实index未被套用。
- 压测宏开启期间存储普通模型未链接bench hook，恢复正式宏0后验证；bench边界头缺vTaskDelay/pdMS_TO_TICKS声明，补齐测试头。驱动本身Keil正常/压测均0错误0警告。
- M5首轮菜单主机测试因单行for触发-Wmisleading-indentation，补大括号；新驱动测试的绝对中文GCC输出路径不能解析，改工作区相对路径。两者不禁用警告。
- 本轮Windows rg不展开Docs/test_*.py，改为目录加-g筛选；Bundled Poppler无pdftotext，改用已有pypdf；PDF输出GBK无法编码注册标识，使用Python -X utf8读取成功，未安装依赖。
- DHT握手RED为预期81段/INCOMPLETE；诊断新增字段RED为预期编译缺字段。后续测试均通过，未屏蔽断言或警告。
- 初次技能读取输出被截断，后续分段补读。
- 初次工具检查将不同类型对象混合表格输出，后续改用 JSON/完整路径。
- 初次设备过滤 DAP 误匹配 Adapter，后续使用精确标识过滤。
- CubeMX 配置搜索范围过大命中数据库元数据，输出截断；不依据该搜索断言固件包安装状态。
