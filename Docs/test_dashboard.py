"""Local-file browser QA. Mock commands never reach physical actuators."""
from pathlib import Path
import argparse
import json
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tmp/m8-dashboard"
OUT.mkdir(parents=True, exist_ok=True)
CHROME = Path("C:/Program Files/Google/Chrome/Application/chrome.exe")
URL = (ROOT / "WebDashboard/index.html").as_uri()
MOCK = """
window.__clients=[];window.__published=[];
window.mqtt={connect:(url,options)=>{
  const c={connected:true,listeners:{},url,options,
    on(k,fn){(this.listeners[k]??=[]).push(fn);return this;},
    emit(k,...v){for(const fn of this.listeners[k]||[])fn(...v);},
    subscribe(topics,opts,cb){this.topics=topics;cb(null,topics.map(topic=>({topic,qos:0})));},
    publish(topic,payload,opts,cb){window.__published.push({topic,payload,opts});if(cb)cb(null);},
    end(){this.connected=false;this.emit('close');}};
  window.__clients.push(c);setTimeout(()=>c.emit('connect'),0);return c;
}};
"""
SAMPLE = dict(ts=1800000000, temp=256, hum=550, distance=85, human=0, smoke=0,
              air=0, wifi=1, mqtt=1, mode=0, state=2, alarm=0, fault=0, q=31)


def emit(page, suffix, body, retained=False):
    payload = body if isinstance(body, str) else json.dumps(body)
    page.evaluate("""([suffix,body,retain])=>{
      const c=window.__clients.at(-1);
      c.emit('message',root+'/'+suffix,{length:body.length,toString:()=>body},{retain});
    }""", [suffix, payload, retained])


def mock_tests(browser):
    page = browser.new_page(viewport={"width": 1280, "height": 1000}, device_scale_factor=1)
    errors = []
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.route("**/mqtt.min.js", lambda route: route.fulfill(body=MOCK, content_type="application/javascript"))
    page.goto(URL)
    page.wait_for_load_state("networkidle")
    page.wait_for_function("window.__clients?.length===1 && document.querySelector('#broker-state').textContent==='看板已连接'")
    assert page.locator("#dev").input_value() == "sms01"
    assert page.evaluate("__clients[0].options.queueQoSZero") is False
    assert page.evaluate("__clients[0].url") == "wss://broker.emqx.io:8084/mqtt"
    assert page.locator("#fan-send").is_disabled()
    emit(page, "status", "online", True)
    emit(page, "telemetry", SAMPLE)
    assert page.locator("#temp").inner_text() == "25.6"
    assert page.locator("#smoke").inner_text() == "正常"
    assert page.locator("#fan-send").is_enabled()
    page.locator("#fan").fill("45")
    page.locator("#fan-send").click()
    assert page.evaluate("__published.at(-1)") == {
        "topic": "smshub/sms01/cmd", "payload": "fan=45", "opts": {"qos": 0, "retain": False}}
    emit(page, "cmd/resp", {"id": 1, "cmd": "fan", "status": "done"})
    page.locator("#servo").fill("90")
    page.locator("#servo-send").click()
    emit(page, "cmd/resp", {"id": 2, "cmd": "servo", "status": "done"})
    page.locator("#arm-on").click()
    emit(page, "cmd/resp", {"id": 3, "cmd": "arm", "status": "queued"})
    assert page.locator("#mode").inner_text() == "撤防"
    emit(page, "cmd/resp", {"id": 4, "cmd": "arm", "status": "done"})
    assert page.locator("#arm-on").is_disabled()
    emit(page, "cmd/resp", {"id": 3, "cmd": "arm", "status": "done"})
    assert page.locator("#arm-off").is_enabled()
    assert [x["payload"] for x in page.evaluate("__published")] == ["fan=45", "servo=90", "arm=1"]
    emit(page, "telemetry", {**SAMPLE, "mode": 1, "state": 4})
    assert page.locator("#fan-send").is_disabled()
    page.locator("#arm-off").click()
    emit(page, "cmd/resp", {"id": 6, "cmd": "arm", "status": "rejected"})
    assert "已拒绝" in page.locator("#response").inner_text()
    page.evaluate("wifiBegin('scan');parseWifiLine('@WIFI +CWLAP:(3,\"测试热点\",-42,\"aa:bb:cc:dd:ee:ff\",1)')")
    assert page.locator("#wifi-list option").count() == 2
    assert "测试热点" in page.locator("#wifi-list option").nth(1).inner_text()
    emit(page, "telemetry", {**SAMPLE, "q": 23, "air": -1, "state": 1, "fault": 16})
    assert page.locator("#air").inner_text() == "不可用"
    assert page.locator("#fan-send").is_disabled()
    emit(page, "event", {"type": 5, "p": 3, "a": 0})
    emit(page, "telemetry", {**SAMPLE, "smoke": 1, "state": 5, "alarm": 3})
    assert page.locator("#alarm").inner_text() == "火灾报警"
    assert page.locator("#smoke").inner_text() == "触发"
    emit(page, "telemetry", "not JSON")
    assert page.locator("#error").inner_text()
    emit(page, "telemetry", {**SAMPLE, "hum": 1001})
    assert page.locator("#error").inner_text()
    emit(page, "event", {"type": "<img src=x onerror=alert(1)>", "p": 0, "a": 0})
    assert page.locator("#events img").count() == 0
    emit(page, "telemetry", SAMPLE, True)
    assert "遥测格式无效" in page.locator("#error").inner_text()
    emit(page, "telemetry", SAMPLE)
    page.evaluate("lastReceived-=9000;renderHealth()")
    assert page.locator("#arm-on").is_disabled()
    assert page.locator("#alarm-title").inner_text() == "数据已过期"
    emit(page, "telemetry", SAMPLE)
    emit(page, "status", "offline")
    assert page.locator("#fan-send").is_disabled()
    old_count = page.evaluate("__published.length")
    page.evaluate("send('fan',100)")
    assert page.evaluate("__published.length") == old_count
    page.locator("#dev").fill("dev02")
    page.locator("#connect").click()
    page.wait_for_function("__clients.length===2")
    assert page.locator("#temp").inner_text() == "--"
    page.evaluate("__clients[0].emit('message','smshub/sms01/status',{length:6,toString:()=> 'online'},{});")
    assert page.locator("#device-state").inner_text() == "设备状态未知"
    emit(page, "status", "online")
    page.evaluate("window.__now=Date.now();Date.now=()=>window.__now")
    for i in range(50):
        page.evaluate("__now+=2000")
        emit(page, "telemetry", {**SAMPLE, "temp": 230 + i % 20, "distance": 50 + i})
    for canvas_id in ("climate", "range"):
        colored = page.locator("#" + canvas_id).evaluate("""c=>{
          const a=c.getContext('2d').getImageData(0,0,c.width,c.height).data;
          let n=0;for(let i=0;i<a.length;i+=4)if(a[i+3]>0 && Math.max(a[i],a[i+1],a[i+2])-Math.min(a[i],a[i+1],a[i+2])>40)n++;
          return n;}""")
        assert colored > 100, (canvas_id, colored)
    page.screenshot(path=str(OUT / "desktop.png"), full_page=True)
    for width, height in ((390, 844), (320, 740)):
        page.set_viewport_size({"width": width, "height": height})
        page.wait_for_timeout(100)
        assert page.evaluate("document.documentElement.scrollWidth<=innerWidth")
        assert page.evaluate("""Array.from(document.querySelectorAll('button')).every(e=>{
          const r=e.getBoundingClientRect();return r.width>0 && e.scrollWidth<=e.clientWidth;})""")
        page.screenshot(path=str(OUT / f"mobile-{width}.png"), full_page=True)
    page.locator("#disconnect").click()
    assert page.locator("#temp").inner_text() == "--"
    assert page.locator("#fan-send").is_disabled()
    assert not errors, errors
    page.close()
    print("PASS file HTML: all controls/acks, offline no queue, stale/retained/invalid data, device isolation, MQ semantics, chart pixels, desktop/390/320 layouts")


def live_test(browser):
    import time
    page = browser.new_page()
    dev = "m8probe_" + str(int(time.time()))
    page.goto(URL + "?dev=" + dev)
    page.wait_for_function("typeof client!=='undefined' && client?.connected", timeout=45000)
    # Isolated diagnostic device/topic; no cmd publishes.
    page.evaluate("client.publish(root+'/status','online',{qos:0,retain:false})")
    delays = []
    for i in range(3):
        sample = {**SAMPLE, "ts": 1800000000 + i}
        page.evaluate("""t=>{window.__sent=performance.now();
          client.publish(root+'/telemetry',JSON.stringify(t),{qos:0,retain:false});}""", sample)
        page.wait_for_function("data?.ts===" + str(sample["ts"]), timeout=15000)
        delays.append(page.evaluate("performance.now()-window.__sent"))
    page.screenshot(path=str(OUT / "wss-live.png"), full_page=True)
    page.locator("#disconnect").click()
    page.close()
    print("PASS real WSS isolated loopback, receive-to-render ms:", [round(v) for v in delays])
    (OUT / "wss.json").write_text(json.dumps({"device": dev, "loopback_ms": delays}), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--live", action="store_true", help="Read-only broker handshake and isolated telemetry loopback")
    options = parser.parse_args()
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True, executable_path=str(CHROME) if CHROME.exists() else None)
        try:
            mock_tests(browser)
            if options.live:
                live_test(browser)
        finally:
            browser.close()
