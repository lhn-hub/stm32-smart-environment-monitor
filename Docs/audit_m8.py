"""Inventory masked sections and resolve observed LR sites using Keil evidence."""
from pathlib import Path
import argparse
import bisect
import re
from m8_burn import CRIT

ROOT = Path(__file__).resolve().parents[1]
ENTRY = re.compile(r"\b(?:taskENTER_CRITICAL|portENTER_CRITICAL|taskENTER_CRITICAL_FROM_ISR|portSET_INTERRUPT_MASK_FROM_ISR|portDISABLE_INTERRUPTS|taskDISABLE_INTERRUPTS|__disable_irq)\s*\(|cpsid i|msr basepri")


def inventory():
    paths = []
    for folder in ("App", "BSP", "Drivers_Device", "Src", "Middleware/esp8266",
                   "Drivers/STM32F1xx_HAL_Driver/Src"):
        paths.extend((ROOT / folder).rglob("*.c"))
    paths.extend((ROOT / "Middleware/FreeRTOS").glob("*.c"))
    paths.extend((ROOT / "Middleware/FreeRTOS/portable").rglob("*.c"))
    return [(p.relative_to(ROOT).as_posix(), n) for p in sorted(paths)
            for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1)
            if ENTRY.search(line.split("/*")[0]) and not line.lstrip().startswith(("*", "//"))]


def symbols(text):
    return sorted((int(addr, 16) & ~1, int(size), name) for name, addr, size in
                  re.findall(r"^\s+(\S+)\s+0x([0-9a-fA-F]+)\s+Thumb Code\s+(\d+)", text, re.M))


def resolve(pc, table, lines):
    if pc == 0xfffffff0:
        return "SysTick", "port.c / RTOS tick"
    if pc == 0xfffffff1:
        return "PendSV", "port.c / context switch"
    call = (pc & ~1) - 4
    i = bisect.bisect_right([s[0] for s in table], call)-1
    name = table[i][2] if i >= 0 and call < table[i][0]+table[i][1] else "unknown"
    i = bisect.bisect_right([row[0] for row in lines], call)-1
    source = lines[i][1] if i >= 0 and call-lines[i][0] <= 16 else "debug line unavailable"
    return name, source


def main(args):
    table = symbols(args.map.read_text(encoding="utf-8", errors="replace")) if args.map.exists() else []
    i2c_receive_linked = any(s[2] in ("HAL_I2C_Master_Receive", "HAL_I2C_Mem_Read") for s in table)
    debug = args.debug.read_text(encoding="utf-8", errors="replace") if args.debug and args.debug.exists() else ""
    lines = sorted((int(addr, 16), file.replace("\\", "/").removeprefix("../")+":"+line)
                   for addr, file, line in re.findall(r"\b([0-9A-Fa-f]{8}):\s+(.+?):(\d+)\.\d+", debug)
                   if int(line))
    metrics = {}
    if args.log:
        for m in CRIT.finditer(args.log.read_text(encoding="utf-8", errors="replace")):
            pc, count, maximum, lost = m.groups()
            key = int(pc, 16)
            old = metrics.get(key, (0, 0, 0))
            metrics[key] = (max(old[0], int(count)), max(old[1], int(maximum)), max(old[2], int(lost)))
    out = [
        "# M8 关中断临界区审计表", "",
        "本表由实际源码清单与 Keil map/debug 生成。没有串口 CRIT 记录时，耗时均为待测；静态审查不能代替板上时长。",
        "BASEPRI=0x50 屏蔽优先级 5–15，TIM4=4 继续运行。生产构建无测量钩子。",
        "审计构建使用 DWT 72MHz，记录每个调用点的最大微秒数（含嵌套和钩子开销，向上取整并加 200 cycle 退出余量）。",
        "FromISR 地址为外层 API 调用点；编译器合并/outline 可能共享调用点，不把一条记录推断为多个源位置均已覆盖。",
        "最多 96 个调用点、嵌套 8 层；n 饱和 65535，max_us 饱和 65535。lost 必须始终为 0；未覆盖分支仍需专项触发。",
        "上电建对象在调度器启动前未计时，需逻辑分析仪补测；异常终止时的有界 UART 输出不属于正常运行临界区。",
        "Flash tx/rx 使用 vTaskSuspendAll，IRQ 仍开；600ms 擦除等待、DHT20ms起始低、ESP100ms复位均不属于全程关中断。",
        "未编译 configUSE_TICKLESS_IDLE，port.c 中 cpsid i 的 STOP/tickless 分支不在当前运行路径。", "",
        "HAL I2C 的8处 PRIMASK 入口位于 Master_Receive/Mem_Read；当前 map 中这两函数" +
        ("仍在镜像，须额外测量，DWT端口钩子不覆盖 PRIMASK。" if i2c_receive_linked else
         "均被链接器移除，正常路径仅扫描和 OLED 发送；将来启用接收必须重新审计。"), "",
        "## 每处源码入口", "", "| 位置 | 操作检查 | 板上最大耗时 |", "| --- | --- | --- |"
    ]
    for source, number in inventory():
        duration = "待板测"
        if "STM32F1xx_HAL_Driver" in source:
            check = "I2C 接收 PRIMASK；端口钩子不覆盖，核对实际链接路径"
            if not i2c_receive_linked:
                duration = "未链接（无运行耗时）"
        elif source.endswith("port.c"):
            check = "内核屏蔽/启动/切换；区分未编译 tickless"
        elif "FreeRTOS/" in source:
            check = "内核队列/任务/事件/heap链表；含 FromISR，需压力覆盖"
        elif source.endswith("bus_arbiter.c"):
            check = "短 GPIO 重配与计数；无 SPI 传输/等待"
        elif source.endswith("bsp_bus.c"):
            check = "RX状态/错误交换；512B清零已移到临界区外"
        else:
            check = "短状态/快照/输出更新；检查嵌套，无 printf/延时/Flash 等待"
        out.append(f"| `{source}:{number}` | {check} | {duration} |")
    out += ["", "## 已采集调用点", "", "| LR/特殊键 | 函数 | debug 位置 | n | 最大 µs | lost |",
            "| --- | --- | --- | ---: | ---: | ---: |"]
    for pc, (count, maximum, lost) in sorted(metrics.items()):
        name, source = resolve(pc, table, lines)
        out.append(f"| `0x{pc:08X}` | `{name}` | `{source}` | {count} | {maximum} | {lost} |")
    if not metrics:
        out.append("| 无硬件记录 | 待采集 | — | — | — | — |")
    observed_ok = bool(metrics and all(maximum <= 2000 and lost == 0 for _, maximum, lost in metrics.values()))
    out += ["", f"已采集调用点是否全部 ≤2ms 且 lost=0：**{'是（仍须核对分支覆盖）' if observed_ok else '未通过/未测'}**。",
            "DoD④需每条正常运行路径都有实测证据；不得因本表没有超标数值就写成通过。", "",
            "## 采集", "",
            "烧录 M8-audit.hex 后，覆盖菜单/云端动作、软件定时器、DHT/超声波/USART DMA ISR、重连、KV保存、Flash失败与恢复。",
            "保持串口日志至少覆盖一次完整96条轮巡。不要用审计固件烤机或发布，它的最近日志RAM缓存为4条且不统计磨损，网络离线事件环仍20条。",
            "用同一审计构建 AXF 导出 debug；LR、map、debug 必须来自同一镜像，不能用正常固件 map 解释。",
            "命令见 M8-acceptance.md。", ""]
    args.output.write_text("\n".join(out), encoding="utf-8")
    print(f"Audit inventory: {len(inventory())} source entries, {len(metrics)} measured sites; output={args.output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--map", type=Path, default=ROOT / "Docs/M8-audit-map.txt")
    parser.add_argument("--debug", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "Docs/M8-critical-audit.md")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        assert resolve(0xfffffff0, [], [])[0] == "SysTick"
        assert resolve(0xfffffff1, [], [])[0] == "PendSV"
        assert resolve(0x08001205, [(0x08001200, 20, "fn")], [(0x08001200, "x.c:7")]) == ("fn", "x.c:7")
        assert resolve(0x08009905, [], []) == ("unknown", "debug line unavailable")
        assert any("stm32f1xx_hal_i2c.c" in path for path, _ in inventory())
        print("PASS audit inventory and LR/Thumb/sentinel/source boundary resolution")
    else:
        main(args)
