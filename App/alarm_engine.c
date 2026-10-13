#include "app_config.h"
#include "alarm_engine.h"
#include "storage.h"
#include "esp8266.h"
#include "sg90.h"
#include "tb6612_fan.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static alarm_engine_t engine;
static uint32_t checked_devices,failed_devices,last_config_seq;
static uint8_t clear_requested,disarm_pending,applied_state=255,applied_kind=255;

void alarm_engine_init(alarm_engine_t *e,uint32_t now)
{ memset(e,0,sizeof(*e));e->entered=now;e->state=ALARM_SELF_TEST; }

static void enter(alarm_engine_t *e,unsigned state,unsigned kind,uint32_t now)
{
    if(e->state!=state || e->kind!=kind){
        e->state=(uint8_t)state;e->kind=(uint8_t)kind;
        e->entered=now;++e->transitions;
    }
}
static void reset_conditions(alarm_engine_t *e)
{ e->tracking=0;e->clearing=0; }

static uint32_t sensor_faults(const datahub_snapshot_t *s,uint32_t now)
{
    static const uint16_t bits[5]={FAULT_DHT,FAULT_DISTANCE,FAULT_SMOKE,FAULT_AIR,FAULT_RADAR};
    uint32_t faults=0;unsigned i;
    if(!s)return FAULT_DATAHUB;
    for(i=0;i<5u;++i){
        const sensor_value_t *v=&s->sensors[i];
        uint32_t period=i==SENSOR_DHT11?DHT11_PERIOD_MS:
                        i==SENSOR_DISTANCE?ULTRA_PERIOD_MS:APP_SENSOR_REGULAR_MS;
        int gas=i==SENSOR_SMOKE || i==SENSOR_AIR;
        if((v->flags&SENSOR_DISABLED) || now-v->timestamp>period*APP_SENSOR_STALE_MULTIPLIER ||
           (gas?(v->auxiliary_quality!=SENSOR_GOOD || (v->auxiliary!=0 && v->auxiliary!=1)):
                 (v->quality!=SENSOR_GOOD || v->value<0)) ||
           (i==SENSOR_DHT11 && v->auxiliary_quality!=SENSOR_GOOD) ||
           (gas && !(v->flags&SENSOR_WARMUP) && v->quality!=SENSOR_GOOD))
            faults|=bits[i];
    }
    return faults;
}
static int held_for(alarm_engine_t *e,unsigned index,int condition,uint32_t now,uint32_t ms)
{
    uint8_t bit=(uint8_t)(1u<<index);
    if(!condition){e->tracking&=(uint8_t)~bit;return 0;}
    if(!(e->tracking&bit)){e->tracking|=bit;e->since[index]=now;}
    return now-e->since[index]>=ms;
}
void alarm_engine_step(alarm_engine_t *e,const datahub_snapshot_t *s,
                       const alarm_config_t *c,uint32_t faults,int done,
                       uint32_t now,int acknowledge)
{
    unsigned kind=ALARM_NONE;int smoke,air,intrusion;uint32_t smoke_ms;
    if(!e || !c)return;
    e->armed=c->armed!=0;
    e->faults=faults|sensor_faults(s,now);
    if(e->state==ALARM_SELF_TEST && (!done || e->faults) && now-e->entered<APP_SELF_TEST_MS)return;
    if(e->faults){reset_conditions(e);enter(e,ALARM_FAULT,ALARM_NONE,now);return;}
    if(e->state==ALARM_SELF_TEST || e->state==ALARM_FAULT){
        reset_conditions(e);enter(e,ALARM_DISARMED,ALARM_NONE,now);return;
    }
    if(!c->armed){reset_conditions(e);enter(e,ALARM_DISARMED,ALARM_NONE,now);return;}
    if(e->state==ALARM_DISARMED){
        reset_conditions(e);enter(e,ALARM_WARMUP,ALARM_NONE,now);return;
    }
    if(e->state==ALARM_WARMUP){
        if(now-e->entered<APP_MQ_WARMUP_MS ||
           ((s->sensors[SENSOR_SMOKE].flags|s->sensors[SENSOR_AIR].flags)&SENSOR_WARMUP))return;
        enter(e,ALARM_ARMED,ALARM_NONE,now);
    }
    if(acknowledge){reset_conditions(e);enter(e,ALARM_ARMED,ALARM_NONE,now);return;}
    smoke=s->sensors[SENSOR_SMOKE].auxiliary==0;
    air=s->sensors[SENSOR_AIR].auxiliary==0;
    intrusion=s->sensors[SENSOR_RADAR].value!=0 &&
              (uint32_t)s->sensors[SENSOR_DISTANCE].value<c->distance_cm;
    smoke_ms=c->smoke_ms;
    if(s->sensors[SENSOR_DHT11].value>=c->temp_x10 && smoke_ms>APP_ALARM_HOT_CONFIRM_MS)
        smoke_ms=APP_ALARM_HOT_CONFIRM_MS;
    if(held_for(e,0,air,now,c->air_ms))kind=ALARM_AIR;
    if(held_for(e,1,intrusion,now,DEF_INTRUSION_CONFIRM_MS))kind=ALARM_INTRUSION;
    if(held_for(e,2,smoke,now,smoke_ms))kind=ALARM_FIRE;
    if(kind>e->kind){e->clearing=0;enter(e,ALARM_ACTIVE,kind,now);}
    if(e->state==ALARM_ACTIVE){
        int active=e->kind==ALARM_FIRE?smoke:e->kind==ALARM_INTRUSION?intrusion:air;
        if(active)e->clearing=0;
        else{
            if(!e->clearing){e->clearing=1;e->cleared=now;}
            if(now-e->cleared>=DEF_ALARM_RELEASE_MS){
                e->clearing=0;
                enter(e,kind?ALARM_ACTIVE:ALARM_ARMED,kind,now);
            }
        }
    }
}
void alarm_init(void)
{
    alarm_engine_init(&engine,HAL_GetTick());
    checked_devices=failed_devices=last_config_seq=0;
    clear_requested=disarm_pending=0;applied_state=applied_kind=255;
}
void alarm_selftest_report(uint32_t mask,int passed)
{
    taskENTER_CRITICAL();
    checked_devices|=mask;
    if(passed)failed_devices&=~mask;else failed_devices|=mask;
    taskEXIT_CRITICAL();
}
void alarm_acknowledge(void)
{ taskENTER_CRITICAL();clear_requested=1;taskEXIT_CRITICAL(); }
void alarm_get_status(alarm_status_t *out)
{
    uint32_t elapsed;
    if(!out)return;
    taskENTER_CRITICAL();
    out->state=engine.state;out->kind=engine.kind;out->faults=engine.faults;
    out->transitions=engine.transitions;elapsed=HAL_GetTick()-engine.entered;
    out->warmup_remaining_ms=engine.state==ALARM_WARMUP && elapsed<APP_MQ_WARMUP_MS?
                             APP_MQ_WARMUP_MS-elapsed:0;
    taskEXIT_CRITICAL();
}
int alarm_set_mode(unsigned armed)
{
    if(armed>1u || storage_set(CFG_MODE,(int32_t)armed))return -1;
    if(!armed){taskENTER_CRITICAL();disarm_pending=1;taskEXIT_CRITICAL();}
    return 0;
}
/* Gate and request are atomic against the higher-priority AlarmTask. */
#define MANUAL_REQUEST(call) do { int result=-1;taskENTER_CRITICAL(); \
    if(engine.state==ALARM_DISARMED && !engine.armed && !engine.faults)result=(call); \
    taskEXIT_CRITICAL();return result; } while(0)
int alarm_manual_fan(unsigned percent){MANUAL_REQUEST(fan_set(percent));}
int alarm_manual_servo(unsigned angle){MANUAL_REQUEST(servo_write(angle));}
int alarm_manual_effect(effect_t effect){MANUAL_REQUEST(effect_set(effect));}
int alarm_manual_beep(uint16_t ms){MANUAL_REQUEST(beep_ms(ms));}
int alarm_manual_rgb(uint16_t r,uint16_t g,uint16_t b){MANUAL_REQUEST(rgb_set(r,g,b));}

static void emit_change(uint32_t type,int32_t parameter,int32_t auxiliary)
{
    /* Fan owns shared pins while running; network event must not wait for Flash. */
    network_event_enqueue(type,parameter,auxiliary,rtc_get());
    if(storage_event(type,parameter,auxiliary))LOGW("报警日志未入队");
}
void alarm_poll(uint32_t now)
{
    datahub_snapshot_t snapshot;int32_t fields[CFG_FIELD_COUNT];storage_status_t store;
    alarm_config_t config;uint32_t faults,old_faults;unsigned old_state,old_kind;
    int done,result,ack;
    if(storage_get_fields(fields,&store)){memset(&store,0,sizeof(store));memset(fields,0,sizeof(fields));}
    config.temp_x10=fields[CFG_TEMP]*10;config.distance_cm=(uint32_t)fields[CFG_DISTANCE];
    config.smoke_ms=(uint32_t)fields[CFG_SMOKE];config.air_ms=(uint32_t)fields[CFG_AIR];
    result=datahub_snapshot(&snapshot);
    now=HAL_GetTick();
    taskENTER_CRITICAL();
    faults=failed_devices|(ALARM_DEVICE_TESTS&~checked_devices);
    done=(checked_devices&ALARM_DEVICE_TESTS)==ALARM_DEVICE_TESTS;
    if(!store.ready)faults|=FAULT_STORAGE;
    if(disarm_pending && !fields[CFG_MODE])disarm_pending=0;
    config.armed=!disarm_pending && fields[CFG_MODE]!=0;
    if(store.config_seq!=last_config_seq){reset_conditions(&engine);last_config_seq=store.config_seq;}
    ack=clear_requested;clear_requested=0;
    old_state=engine.state;old_kind=engine.kind;old_faults=engine.faults;
    alarm_engine_step(&engine,result?NULL:&snapshot,&config,faults,done,now,ack);
    taskEXIT_CRITICAL();
    if(applied_state!=engine.state || applied_kind!=engine.kind){
        effect_t effect=engine.state==ALARM_FAULT?EFFECT_FAULT:engine.kind==ALARM_FIRE?EFFECT_FIRE:
                        engine.kind==ALARM_INTRUSION?EFFECT_INTRUSION:engine.kind==ALARM_AIR?EFFECT_AIR:EFFECT_STANDBY;
        int failed=effect_set(effect);
        failed|=fan_set(engine.kind==ALARM_FIRE?100u:engine.kind==ALARM_AIR?45u:0u);
        failed|=servo_write(engine.kind==ALARM_FIRE?90u:0u);
        applied_state=engine.state;applied_kind=engine.kind;
        if(failed)alarm_selftest_report(FAULT_OUTPUT,0);
    }
    if(old_state!=engine.state || old_kind!=engine.kind || old_faults!=engine.faults){
        LOGI("报警状态=%u 级别=%u 故障=0x%lX 次数=%lu",engine.state,engine.kind,
             (unsigned long)engine.faults,(unsigned long)engine.transitions);
        if(engine.kind!=ALARM_NONE && old_kind!=engine.kind)emit_change(EVENT_ALARM,(int32_t)engine.kind,0);
        else if(old_kind!=ALARM_NONE)emit_change(EVENT_ALARM_CLEAR,(int32_t)old_kind,(int32_t)engine.state);
        if(engine.state==ALARM_FAULT && (old_state!=ALARM_FAULT || old_faults!=engine.faults))
            emit_change(EVENT_FAULT,(int32_t)engine.faults,0);
    }
#if APP_M7_TEST_ENABLED
    alarm_bench_poll(&snapshot);
#endif
}

#if APP_M7_TEST_ENABLED
volatile uint32_t m7_test_request,m7_test_cycles,m7_test_failures;
void alarm_bench_poll(datahub_snapshot_t *s)
{
    static alarm_engine_t test;
    static uint32_t time;
    static unsigned phase,running;
    alarm_config_t c={500,100,3000,10000,1};unsigned i,kind;
    if(m7_test_request==2u){running=0;m7_test_request=0;}
    if(m7_test_request==1u){
        m7_test_request=0;m7_test_cycles=m7_test_failures=0;
        time=1;phase=0;running=1;alarm_engine_init(&test,time);
    }
    if(!running)return;
    memset(s,0,sizeof(*s));
    for(i=0;i<SENSOR_COUNT;++i){s->sensors[i].timestamp=time;s->sensors[i].auxiliary=1;}
    s->sensors[SENSOR_DHT11].value=250;s->sensors[SENSOR_DISTANCE].value=150;
    kind=m7_test_cycles%3u+1u;
    if(phase==0u){
        alarm_engine_step(&test,s,&c,0,1,time,0);
        alarm_engine_step(&test,s,&c,0,1,time,0);
        time+=APP_MQ_WARMUP_MS;phase=1;return;
    }
    if(phase>=2u && phase<=4u){
        if(kind==ALARM_AIR)s->sensors[SENSOR_AIR].auxiliary=0;
        if(kind==ALARM_FIRE)s->sensors[SENSOR_SMOKE].auxiliary=0;
        if(kind==ALARM_INTRUSION){s->sensors[SENSOR_RADAR].value=1;s->sensors[SENSOR_DISTANCE].value=30;}
    }
    alarm_engine_step(&test,s,&c,0,1,time,0);
    if(phase==1u){if(test.state!=ALARM_ARMED)++m7_test_failures;phase=2;}
    else if(phase==2u){time+=kind==ALARM_AIR?10000u:kind==ALARM_FIRE?3000u:2000u;phase=3;}
    else if(phase==3u){if(test.kind!=kind)++m7_test_failures;phase=5;}
    else if(phase==5u){time+=DEF_ALARM_RELEASE_MS;phase=6;}
    else{
        if(test.state!=ALARM_ARMED || test.kind)++m7_test_failures;
        if(++m7_test_cycles>=50u){running=0;LOGI("M7循环=%lu 失败=%lu",(unsigned long)m7_test_cycles,(unsigned long)m7_test_failures);}
        phase=2;
    }
}
#endif
