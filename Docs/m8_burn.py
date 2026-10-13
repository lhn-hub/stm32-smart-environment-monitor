"""24h read-only serial+MQTT capture, or deterministic log/report analysis."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import math
import os
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BURN = re.compile(r"BURN ms=(\d+) wd=(\d+) boots=(\d+) reconnect=(\d+) heap=(\d+) min=(\d+) mqtt=(\d+) drop=(\d+)")
WEAR = re.compile(r"WEAR sector=(\d+) addr=([0-9A-Fa-f]+) writes=(\d+) erases=(\d+)")
STACK = re.compile(r"\[[^\]]+\]\[([^\]]+)\].*栈剩余=(\d+)/(\d+)字")
BOOT = re.compile(r"BOOT wd=(\d+) count=(\d+) boots=(\d+) cause=(\d+)")
CRIT = re.compile(r"CRIT pc=([0-9A-Fa-f]+) n=(\d+) max_us=(\d+) lost=(\d+)")


def analyze(serial_text, messages):
    burns = [tuple(map(int, m.groups())) for m in BURN.finditer(serial_text)]
    wear = {}
    for m in WEAR.finditer(serial_text):
        sector, addr, writes, erases = m.groups()
        wear[int(sector)] = {"address": int(addr, 16), "writes": int(writes), "erases": int(erases)}
    stacks = {}
    for m in STACK.finditer(serial_text):
        name, free, total = m.groups()
        old = stacks.get(name, {"min_words": int(free), "total_words": int(total)})
        old["min_words"] = min(old["min_words"], int(free))
        stacks[name] = old
    durations = re.findall(r"CAPTURE elapsed_seconds=([\d.]+)", serial_text)
    elapsed = float(durations[-1]) if durations else 0
    resets = sum(int(m[0]) for m in BOOT.findall(serial_text))
    boot_fault = any(int(m[3]) for m in BOOT.findall(serial_text)) or bool(
        re.search(r"HardFault|STACK task=|任务栈溢出|内存分配失败|看门狗故障=[1-9]", serial_text))
    uptime_rollback = any(a[0] >= b[0] or a[1] > b[1] or a[2] > b[2] for a, b in zip(burns, burns[1:]))
    wd_delta = max(0, burns[-1][1] - burns[0][1]) if burns else None
    boots_delta = max(0, burns[-1][2] - burns[0][2]) if burns else None
    reconnect_delta = max(0, burns[-1][3] - burns[0][3]) if burns else None
    telemetry = []
    invalid = 0
    for row in messages:
        if not isinstance(row, dict) or row.get("capture_parse_error"):
            invalid += 1
            continue
        if str(row.get("topic", "")).endswith("/telemetry") and not row.get("retained"):
            t = row.get("payload")
            received = row.get("received_s")
            required = ("ts", "temp", "hum", "distance", "human", "smoke", "air", "wifi", "mqtt", "mode", "state", "alarm", "fault", "q")
            if (isinstance(t, dict) and all(type(t.get(k)) is int for k in required) and
                    type(received) in (int, float) and math.isfinite(received) and received >= 0):
                telemetry.append(float(received))
            else:
                invalid += 1
    gaps = [b-a for a, b in zip(telemetry, telemetry[1:])]
    span = telemetry[-1]-telemetry[0] if len(telemetry) >= 2 else 0
    serial_span = (burns[-1][0]-burns[0][0])/1000 if len(burns) >= 2 and not uptime_rollback else 0
    serial_ok = bool(elapsed >= 86400 and serial_span >= 86300 and len(burns) >= 1439 and
                     wd_delta == 0 and boots_delta == 0 and not resets and not boot_fault and
                     not uptime_rollback and len(wear) == 16 and len(stacks) == 6)
    mqtt_ok = bool(span >= 86390 and len(telemetry) >= 43190 and gaps and
                   max(gaps) <= 3 and min(gaps) > 0 and not invalid)
    return {"serial_seconds": elapsed, "serial_uptime_span_s": serial_span,
            "burn_samples": len(burns), "watchdog_resets": wd_delta, "boot_delta": boots_delta,
            "observed_watchdog_boots": resets, "boot_fault": boot_fault,
            "uptime_rollback": uptime_rollback, "reconnect_delta": reconnect_delta,
            "minimum_free_heap_bytes": min((r[5] for r in burns), default=None),
            "stack_watermarks": stacks, "log_sector_wear": wear,
            "telemetry_count": len(telemetry), "telemetry_span_s": span,
            "maximum_telemetry_gap_s": max(gaps, default=None), "invalid_telemetry": invalid,
            "serial_pass": serial_ok, "mqtt_continuity_pass": mqtt_ok,
            "pass_24h": serial_ok and mqtt_ok}


def self_test():
    lines = ["CAPTURE elapsed_seconds=86400"]
    for i in range(1440):
        lines.append(f"BURN ms={60000*(i+1)} wd=3 boots=7 reconnect=2 heap=700 min=650 mqtt=1 drop=0")
    for i in range(16):
        lines.append(f"WEAR sector={i} addr={0x3000+i*4096:06X} writes={i} erases=1")
    for i in range(6):
        lines.append(f"[5][task{i}][信息] 运行正常 栈剩余=100/256字")
    payload = dict.fromkeys(("ts", "temp", "hum", "distance", "human", "smoke", "air", "wifi", "mqtt", "mode", "state", "alarm", "fault", "q"), 0)
    messages = [{"topic": "smshub/test/telemetry", "received_s": float(i*2), "payload": payload} for i in range(43200)]
    good = "\n".join(lines)
    assert analyze(good, messages)["pass_24h"]
    assert not analyze(good.replace("elapsed_seconds=86400", "elapsed_seconds=60"), messages)["pass_24h"]
    assert not analyze(good + "\nBOOT wd=1 count=4 boots=8 cause=2", messages)["pass_24h"]
    assert not analyze(good + "\nHardFault\r\nPC=08001234", messages)["pass_24h"]
    assert not analyze(good + "\nSTACK task=20001234", messages)["pass_24h"]
    assert not analyze(good + "\n内存分配失败", messages)["pass_24h"]
    assert not analyze(good, messages + [None])["pass_24h"]
    assert not analyze(good, messages + [{**messages[0], "received_s": float("nan")}])["pass_24h"]
    assert not analyze(good, messages[:20])["pass_24h"]
    assert not analyze(good, messages[:100] + messages[110:])["pass_24h"]
    assert not analyze(good + "\nBURN ms=100 wd=4 boots=8 reconnect=0 heap=700 min=650 mqtt=1 drop=0", messages)["pass_24h"]
    assert analyze("", [])["watchdog_resets"] is None
    print("PASS burn report: full synthetic 24h, short run, reset/fault, missing data, gap, reboot and absent evidence")


def capture(args):
    from playwright.sync_api import sync_playwright
    if not args.dev or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", args.dev):
        raise SystemExit("--dev must be the actual device ID")
    args.output.mkdir(parents=True, exist_ok=True)
    serial_path = args.output / "serial.log"
    mqtt_path = args.output / "mqtt.jsonl"
    if serial_path.exists() or mqtt_path.exists():
        raise SystemExit("Capture output already exists; use a fresh directory")
    script = ROOT / "Docs/collect_m8_serial.ps1"
    child = None
    if args.port:
        child = subprocess.Popen(["pwsh", "-NoProfile", "-File", str(script), "-Port", args.port,
                                  "-Seconds", str(args.seconds), "-Output", str(serial_path)],
                                 creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    began = time.monotonic()
    try:
        with mqtt_path.open("w", encoding="utf-8") as log, sync_playwright() as p:
            chrome = Path("C:/Program Files/Google/Chrome/Application/chrome.exe")
            browser = p.chromium.launch(headless=True, executable_path=str(chrome) if chrome.exists() else None)
            try:
                page = browser.new_page()

                def receive(topic, body, retained):
                    try:
                        payload = json.loads(body)
                    except (ValueError, TypeError):
                        payload = body
                    log.write(json.dumps({"received_s": time.monotonic()-began, "topic": topic,
                                          "retained": retained, "payload": payload}, ensure_ascii=False)+"\n")
                    log.flush()

                page.expose_function("m8Record", receive)
                page.goto((ROOT / "WebDashboard/index.html").as_uri() + "?dev=" + args.dev)
                page.wait_for_function("typeof client!=='undefined' && client", timeout=45000)
                page.evaluate("client.on('message',(t,b,p)=>{if(b.length<=1024)window.m8Record(t,b.toString(),!!p.retain);})")
                while time.monotonic()-began < args.seconds:
                    if child is not None and child.poll() is not None:
                        if child.returncode or time.monotonic()-began < args.seconds-2:
                            raise RuntimeError("Serial capture stopped; check port ownership and board connection")
                    page.wait_for_timeout(200)
            finally:
                browser.close()
    finally:
        if child is not None:
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.terminate()
                child.wait(timeout=3)
        report(args.output)


def report(directory):
    serial_path, mqtt_path = directory / "serial.log", directory / "mqtt.jsonl"
    serial_text = serial_path.read_text(encoding="utf-8", errors="replace") if serial_path.exists() else ""
    messages = []
    if mqtt_path.exists():
        for line in mqtt_path.read_text(encoding="utf-8").splitlines():
            try:
                messages.append(json.loads(line))
            except ValueError:
                messages.append({"capture_parse_error": True})
    result = analyze(serial_text, messages)
    result["report_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    firmware = ROOT / "Docs/M8-app.hex"
    result["candidate_hex_sha256"] = hashlib.sha256(firmware.read_bytes()).hexdigest() if firmware.exists() else None
    result["firmware_identity_note"] = "Candidate hash only; verify the programmed board matches before acceptance."
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "report.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--capture", action="store_true", help="Read-only; never publish commands")
    parser.add_argument("--dev")
    parser.add_argument("--port", help="CH340 debug port; close the serial assistant first")
    parser.add_argument("--seconds", type=int, default=86400)
    parser.add_argument("--output", type=Path, default=ROOT / "tmp/M8-burn")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif args.capture:
        if not 1 <= args.seconds <= 172800:
            parser.error("--seconds must be 1..172800")
        capture(args)
    else:
        report(args.output)
