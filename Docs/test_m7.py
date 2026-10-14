"""Actual M7 C state-machine/time/runtime tests; no physical sensor claims."""
from pathlib import Path
import os
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tmp/m7-test"
OUT.mkdir(parents=True, exist_ok=True)
ENV = dict(os.environ)
ENV["PATH"] = "D:/msys64/ucrt64/bin;" + ENV.get("PATH", "")


def write(name, body):
    (OUT / name).write_text(textwrap.dedent(body), encoding="utf-8")


write("stm32f1xx_hal.h", """
    #ifndef TEST_HAL_H
    #define TEST_HAL_H
    #include <stdint.h>
    typedef struct { unsigned unused; } TIM_HandleTypeDef;
    uint32_t HAL_GetTick(void);
    #endif
""")
write("FreeRTOS.h", """
    #ifndef TEST_FREERTOS_H
    #define TEST_FREERTOS_H
    #define taskENTER_CRITICAL() ((void)0)
    #define taskEXIT_CRITICAL() ((void)0)
    #endif
""")
write("task.h", "")
write("test.c", r"""
    #include <assert.h>
    #include <stdio.h>
    #include <string.h>
    #include "app_config.h"
    #include "alarm_engine.h"
    #include "app_time.h"
    #include "storage.h"
    #include "esp8266.h"
    static uint32_t tick,clock_value;
    static int clock_failure,hub_failure;
    static unsigned fan,servo,effect,event_count,log_count;
    static datahub_snapshot_t live;
    static int32_t fields[CFG_FIELD_COUNT]={50,100,3000,10000,0};
    static storage_status_t store={1,0,0,0,1,0};
    uint32_t HAL_GetTick(void){return tick;}
    uint32_t rtc_get(void){return clock_value;}
    int rtc_set(uint32_t value){if(clock_failure)return -1;clock_value=value;return 0;}
    void bsp_log(const char *level,const char *format,...){(void)level;(void)format;}
    int effect_set(effect_t v){effect=(unsigned)v;return 0;}
    int fan_set(unsigned v){if(v>100)return -1;fan=v;return 0;}
    int servo_write(unsigned v){if(v>180)return -1;servo=v;return 0;}
    int rgb_set(uint16_t r,uint16_t g,uint16_t b){return r>999||g>999||b>999?-1:0;}
    int storage_set(unsigned field,int32_t value){
        if(field>=CFG_FIELD_COUNT || !store.ready || store.pending || value<0)return -1;
        store.pending=1;return 0;
    }
    int storage_event(uint32_t t,int32_t p,int32_t a){(void)t;(void)p;(void)a;++log_count;return 0;}
    int network_event_enqueue(uint32_t t,int32_t p,int32_t a,uint32_t time){
        (void)t;(void)p;(void)a;(void)time;++event_count;return 0;
    }
    int storage_get_fields(int32_t out[CFG_FIELD_COUNT],storage_status_t *status){
        memcpy(out,fields,sizeof(fields));*status=store;return 0;
    }
    int datahub_snapshot(datahub_snapshot_t *s){*s=live;return hub_failure?-1:0;}
    static void healthy(datahub_snapshot_t *s,uint32_t now){
        unsigned i;memset(s,0,sizeof(*s));
        for(i=0;i<SENSOR_COUNT;i++){s->sensors[i].timestamp=now;s->sensors[i].auxiliary=1;}
        s->sensors[SENSOR_DHT11].value=250;
        s->sensors[SENSOR_DISTANCE].value=150;
    }
    static alarm_config_t cfg={500,100,3000,10000,1};
    static alarm_engine_t e;
    static datahub_snapshot_t s;
    static uint32_t now;
    static void step(uint32_t delta){
        unsigned i;now+=delta;
        for(i=0;i<SENSOR_COUNT;i++)s.sensors[i].timestamp=now;
        alarm_engine_step(&e,&s,&cfg,0,1,now,0);
    }
    static void arm_at(uint32_t start){
        now=start;healthy(&s,now);alarm_engine_init(&e,now);
        step(0);assert(e.state==ALARM_DISARMED);
        step(0);assert(e.state==ALARM_WARMUP);
        step(APP_MQ_WARMUP_MS-1);assert(e.state==ALARM_WARMUP);
        step(1);assert(e.state==ALARM_ARMED);
    }
    int main(void){
        alarm_status_t status;unsigned i;
        now=1;healthy(&s,now);alarm_engine_init(&e,now);
        alarm_engine_step(&e,&s,&cfg,FAULT_ESP,0,now,0);assert(e.state==ALARM_SELF_TEST);
        now+=APP_SELF_TEST_MS;healthy(&s,now);
        alarm_engine_step(&e,&s,&cfg,FAULT_ESP,0,now,0);assert(e.state==ALARM_FAULT);
        step(0);assert(e.state==ALARM_DISARMED);
        step(0);assert(e.state==ALARM_WARMUP);
        puts("PASS T-01 self-test timeout/fault/recovery and 150s warmup");
        arm_at(10);s.sensors[SENSOR_RADAR].value=1;step(10000);
        assert(e.state==ALARM_ARMED);
        s.sensors[SENSOR_RADAR].value=0;s.sensors[SENSOR_DISTANCE].value=30;step(10000);
        assert(e.state==ALARM_ARMED);
        s.sensors[SENSOR_RADAR].value=1;s.sensors[SENSOR_DISTANCE].value=100;step(10000);
        assert(e.state==ALARM_ARMED);
        s.sensors[SENSOR_DISTANCE].value=30;step(0);step(1999);assert(!e.kind);
        s.sensors[SENSOR_RADAR].value=0;step(0);s.sensors[SENSOR_RADAR].value=1;
        step(0);step(1999);assert(!e.kind);
        step(1);assert(e.kind==ALARM_INTRUSION);
        puts("PASS T-02/T-03 radar-only, distance-only, strict <100cm dual 2s");
        arm_at(20);s.sensors[SENSOR_SMOKE].auxiliary=0;step(0);step(2999);assert(!e.kind);
        step(1);assert(e.kind==ALARM_FIRE);
        arm_at(30);s.sensors[SENSOR_DHT11].value=500;s.sensors[SENSOR_SMOKE].auxiliary=0;
        step(0);step(999);assert(!e.kind);step(1);assert(e.kind==ALARM_FIRE);
        cfg.smoke_ms=500;arm_at(31);s.sensors[SENSOR_DHT11].value=500;
        s.sensors[SENSOR_SMOKE].auxiliary=0;step(0);step(499);assert(!e.kind);
        step(1);assert(e.kind==ALARM_FIRE);cfg.smoke_ms=3000;
        puts("PASS T-04/T-05 smoke 3s and temperature >= threshold shortening to 1s");
        arm_at(40);s.sensors[SENSOR_AIR].auxiliary=0;step(0);step(9999);assert(!e.kind);
        step(1);assert(e.kind==ALARM_AIR);
        s.sensors[SENSOR_RADAR].value=1;s.sensors[SENSOR_DISTANCE].value=30;step(0);step(2000);
        assert(e.kind==ALARM_INTRUSION);
        s.sensors[SENSOR_SMOKE].auxiliary=0;step(0);step(3000);assert(e.kind==ALARM_FIRE);
        s.sensors[SENSOR_SMOKE].auxiliary=1;step(0);step(9999);assert(e.kind==ALARM_FIRE);
        step(1);assert(e.kind==ALARM_INTRUSION);
        s.sensors[SENSOR_RADAR].value=0;s.sensors[SENSOR_AIR].auxiliary=1;
        step(0);step(10000);assert(e.state==ALARM_ARMED);
        puts("PASS T-06/T-07 air10s, priority escalation, delayed downgrade/release10s");
        arm_at(50);s.sensors[SENSOR_SMOKE].auxiliary=0;step(0);step(3000);
        s.sensors[SENSOR_DISTANCE].quality=SENSOR_STALE;step(0);
        assert(e.state==ALARM_FAULT && !e.kind && (e.faults&FAULT_DISTANCE));
        s.sensors[SENSOR_DISTANCE].quality=SENSOR_GOOD;s.sensors[SENSOR_SMOKE].flags=SENSOR_DISABLED;
        step(0);assert(e.state==ALARM_FAULT && (e.faults&FAULT_SMOKE));
        alarm_engine_step(&e,NULL,&cfg,0,1,now,0);assert(e.faults&FAULT_DATAHUB);
        arm_at(51);s.sensors[SENSOR_DISTANCE].timestamp=now-ULTRA_PERIOD_MS*APP_SENSOR_STALE_MULTIPLIER-1u;
        alarm_engine_step(&e,&s,&cfg,0,1,now,0);assert(e.faults&FAULT_DISTANCE);
        arm_at(52);s.sensors[SENSOR_SMOKE].auxiliary_quality=SENSOR_FAULT;
        step(0);assert(e.faults&FAULT_SMOKE);
        arm_at(53);s.sensors[SENSOR_SMOKE].auxiliary=2;step(0);assert(e.faults&FAULT_SMOKE);
        arm_at(54);s.sensors[SENSOR_RADAR].flags=SENSOR_DISABLED;step(0);assert(e.faults&FAULT_RADAR);
        puts("PASS T-08 invalid/stale/disabled/missing snapshots fault without alarm");
        arm_at(60);s.sensors[SENSOR_SMOKE].auxiliary=0;step(0);step(3000);
        alarm_engine_step(&e,&s,&cfg,0,1,now,1);assert(e.state==ALARM_ARMED && !e.kind);
        step(0);step(3000);assert(e.kind==ALARM_FIRE);
        cfg.armed=0;step(0);assert(e.state==ALARM_DISARMED && !e.kind);cfg.armed=1;
        puts("PASS T-09 manual acknowledge, persistent condition reconfirm, disarm");
        arm_at(0xffff0000u);s.sensors[SENSOR_RADAR].value=1;s.sensors[SENSOR_DISTANCE].value=30;
        step(0);step(2000);assert(e.kind==ALARM_INTRUSION);
        s.sensors[SENSOR_RADAR].value=0;step(0);step(10000);assert(e.state==ALARM_ARMED);
        healthy(&live,1);tick=1;alarm_init();alarm_selftest_report(ALARM_DEVICE_TESTS,1);
        alarm_poll(tick);alarm_get_status(&status);assert(status.state==ALARM_DISARMED);
        assert(!alarm_manual_fan(20) && fan==20);
        fields[CFG_MODE]=1;++store.config_seq;alarm_poll(tick);assert(!fan && !servo);
        assert(alarm_manual_fan(100)==-1 && alarm_manual_servo(90)==-1);
        tick+=APP_MQ_WARMUP_MS;healthy(&live,tick);alarm_poll(tick);
        live.sensors[SENSOR_SMOKE].auxiliary=0;alarm_poll(tick);
        tick+=3000;healthy(&live,tick);live.sensors[SENSOR_SMOKE].auxiliary=0;alarm_poll(tick);
        assert(fan==100 && servo==90 && effect==EFFECT_FIRE && event_count && log_count);
        assert(!alarm_set_mode(0));alarm_poll(tick);assert(!fan && !servo);
        fields[CFG_MODE]=0;store.pending=0;++store.config_seq;alarm_poll(tick);
        hub_failure=1;alarm_poll(tick);
        assert(effect==EFFECT_FAULT && !fan && !servo && alarm_manual_effect(EFFECT_FIRE)==-1);
        hub_failure=0;alarm_poll(tick);assert(!alarm_manual_fan(10) && fan==10);
        fields[CFG_MODE]=1;++store.config_seq;alarm_poll(tick);
        hub_failure=1;alarm_poll(tick);hub_failure=0;alarm_poll(tick);
        alarm_get_status(&status);assert(status.state==ALARM_DISARMED);
        assert(alarm_manual_fan(100)==-1);
        alarm_poll(tick);
        tick+=APP_MQ_WARMUP_MS;healthy(&live,tick);alarm_poll(tick);
        live.sensors[SENSOR_SMOKE].auxiliary=0;alarm_poll(tick);
        tick+=1000;healthy(&live,tick);live.sensors[SENSOR_SMOKE].auxiliary=0;
        fields[CFG_SMOKE]=1500;++store.config_seq;alarm_poll(tick);
        tick+=1499;healthy(&live,tick);live.sensors[SENSOR_SMOKE].auxiliary=0;alarm_poll(tick);
        assert(!fan);tick+=1;healthy(&live,tick);live.sensors[SENSOR_SMOKE].auxiliary=0;alarm_poll(tick);
        assert(fan==100 && servo==90);
        puts("PASS T-10 runtime linkage, manual arbitration, pending disarm, faults and tick wrap");
        assert(app_time_build_epoch("Jan  1 2020","08:00:00")==1577836800u);
        assert(app_time_build_epoch("Feb 29 2024","08:00:00")==1709164800u);
        assert(!app_time_build_epoch("bad","time"));
        clock_value=0;app_time_start();assert(app_time_source()==TIME_BUILD && clock_value>1577836800u);
        app_time_start();assert(app_time_source()==TIME_BACKUP);
        assert(app_time_request(0)==-1 && !app_time_request(1800000000u));
        assert(app_time_request(1800000001u)==-1);app_time_poll();
        assert(app_time_source()==TIME_GATEWAY && clock_value==1800000000u);
        clock_failure=1;assert(!app_time_request(1800000010u));app_time_poll();
        assert(clock_value==1800000000u);
        puts("PASS T-11 build UTC+8/leap day, backup time, bounded gateway queue, RTC failure");
        m7_test_request=1;
        for(i=0;i<400;i++)alarm_bench_poll(&s);
        assert(m7_test_cycles==50 && !m7_test_failures);
        puts("PASS T-12 actual firmware hook: 50 mixed alarm/recovery cycles, zero failures");
        return 0;
    }
""")
args = ["D:/msys64/ucrt64/bin/gcc.exe", "-std=c99", "-Wall", "-Wextra",
        "-Werror", "-DAPP_NO_LOCAL_SECRETS", "-DAPP_M7_TEST_ENABLED=1"]
for path in ["tmp/m7-test", "App", "BSP", "Drivers_Device", "Drivers_Device/kvstore",
             "Drivers_Device/eventlog", "Drivers_Device/sg90",
             "Drivers_Device/tb6612_fan", "Middleware/esp8266"]:
    args += ["-I", path]
args += ["tmp/m7-test/test.c", "App/alarm_engine.c", "App/app_time.c",
         "-o", "tmp/m7-test/test.exe"]
subprocess.run(args, cwd=ROOT, env=ENV, check=True)
subprocess.run(["tmp/m7-test/test.exe"], cwd=ROOT, env=ENV, check=True)

# Compile the actual dispatch function and exercise asynchronous KV completion.
task_source = (ROOT / "App/tasks/app_tasks.c").read_text(encoding="utf-8")
dispatch = task_source[task_source.index("static void network_commands_poll"):
                       task_source.index("static void worker")]
write("commands.c", r"""
    #include <assert.h>
    #include <string.h>
    #include <stdio.h>
    #include "app_config.h"
    #include "storage.h"
    #include "alarm_engine.h"
    #include "esp8266.h"
    #include "app_time.h"
    static network_command_t input,saving_command;
    static uint32_t saving_sequence,reboot_at;
    static uint8_t saving_reply,reboot_pending;
    static int available,allowed,clears;
    static unsigned replies,last_reply;
    static int32_t fields[CFG_FIELD_COUNT]={50,100,3000,10000,0};
    static storage_status_t state={1,0,0,0,1,0};
    int network_command_pop(network_command_t *out){
        if(!available)return -1;
        *out=input;available=0;return 0;
    }
    void network_command_reply(const network_command_t *c,unsigned result){
        assert(c->id==7);++replies;last_reply=result;
    }
    int storage_get_fields(int32_t out[CFG_FIELD_COUNT],storage_status_t *s){
        memcpy(out,fields,sizeof(fields));*s=state;return 0;
    }
    int storage_set(unsigned field,int32_t value){
        (void)value;if(field>=CFG_FIELD_COUNT || !state.ready || state.pending)return -1;
        state.pending=1;return 0;
    }
    int alarm_set_mode(unsigned a){return storage_set(CFG_MODE,(int32_t)a);}
    void alarm_acknowledge(void){++clears;}
    int alarm_manual_fan(unsigned value){(void)value;return allowed?0:-1;}
    int alarm_manual_servo(unsigned value){(void)value;return allowed?0:-1;}
    int app_time_request(uint32_t value){return value>=1577836800u?0:-1;}
""" + dispatch + r"""
    static void command(unsigned kind,unsigned field,int32_t value){
        input.id=7;input.kind=(uint8_t)kind;input.field=(uint8_t)field;input.value=value;
        available=1;network_commands_poll(100);
    }
    int main(void){
        command(NETWORK_CMD_THRESHOLD,CFG_TEMP,52);
        assert(last_reply==NETWORK_REPLY_QUEUED && saving_reply && fields[CFG_TEMP]==50);
        network_commands_poll(101);assert(replies==1);
        fields[CFG_TEMP]=52;++state.config_seq;state.pending=0;
        network_commands_poll(102);assert(replies==2 && last_reply==NETWORK_REPLY_DONE);
        command(NETWORK_CMD_THRESHOLD,CFG_TEMP,53);assert(saving_reply);
        state.ready=0;state.pending=0;network_commands_poll(103);
        assert(last_reply==NETWORK_REPLY_REJECTED && fields[CFG_TEMP]==52);
        state.ready=1;command(NETWORK_CMD_ARM,CFG_MODE,1);
        assert(last_reply==NETWORK_REPLY_QUEUED);
        fields[CFG_MODE]=1;++state.config_seq;state.pending=0;network_commands_poll(104);
        assert(last_reply==NETWORK_REPLY_DONE);
        command(NETWORK_CMD_FAN,0,100);assert(last_reply==NETWORK_REPLY_REJECTED);
        allowed=1;command(NETWORK_CMD_SERVO,0,90);assert(last_reply==NETWORK_REPLY_DONE);
        command(NETWORK_CMD_ACK,0,1);assert(clears==1 && last_reply==NETWORK_REPLY_DONE);
        command(NETWORK_CMD_TIME,0,1800000000);assert(last_reply==NETWORK_REPLY_QUEUED);
        command(NETWORK_CMD_REBOOT,0,1);
        assert(last_reply==NETWORK_REPLY_ACCEPTED && reboot_pending && reboot_at==1100);
        puts("PASS actual NetTask dispatch: queued -> committed done, failure retains old config, mode, gated outputs, ack/time/reboot");
        return 0;
    }
""")
command_args = args[:args.index("tmp/m7-test/test.c")]
command_args += ["tmp/m7-test/commands.c", "-o", "tmp/m7-test/commands.exe"]
subprocess.run(command_args, cwd=ROOT, env=ENV, check=True)
subprocess.run(["tmp/m7-test/commands.exe"], cwd=ROOT, env=ENV, check=True)
