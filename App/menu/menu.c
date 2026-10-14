#include "menu.h"
#include "ssd1306.h"
#include "oled_ui.h"
#include "app_config.h"
#include "datahub.h"
#include "storage.h"
#include "sg90.h"
#include "tb6612_fan.h"
#include "esp8266.h"
#include "alarm_engine.h"
#include "app_time.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
typedef enum {HOME,ROOT,ARM,THRESHOLDS,MANUAL,NETWORK,EVENTLOG,SYSTEM,ABOUT,PAGE_COUNT} page_id;
typedef struct { void (*enter)(void); void (*exit)(void); void (*render)(void); void (*on_key)(int); } page_t;
static page_id stack[APP_MENU_STACK_DEPTH];
static unsigned depth,selected[PAGE_COUNT];
static int editing,armed;
static int values[4]={DEF_TEMP_ALARM_C_X10/10,DEF_INTRUSION_DIST_CM,DEF_SMOKE_CONFIRM_MS,DEF_AIR_CONFIRM_MS};
static const int maxima[4]={APP_EDIT_MAX_TEMP,APP_EDIT_MAX_DISTANCE,APP_EDIT_MAX_CONFIRM_MS,APP_EDIT_MAX_CONFIRM_MS};
static uint32_t deadline,last_report,frames,accelerate_until;
static char text[48];
static storage_status_t store_state;
static int32_t committed_fields[CFG_FIELD_COUNT];
static unsigned history_count;
static datahub_snapshot_t sensors;
static alarm_status_t alarm_status;
static uint32_t notice_transition,notice_fault;
static uint8_t notice_visible;
static void no_op(void) {}
static page_id current(void) { return stack[depth]; }
static const page_t pages[PAGE_COUNT];
static void push(page_id id)
{
    if(depth+1>=APP_MENU_STACK_DEPTH)return;
    pages[current()].exit();stack[++depth]=id;editing=0;pages[id].enter();
}
static void pop(void)
{ if(depth){pages[current()].exit();--depth;editing=0;pages[current()].enter();} }
static void row(unsigned n,const char *label,int highlight)
{ ui_row(n,label,highlight); }
static void header(const char *name)
{ oled_text(0,0,name,0);oled_line(0,9,127,9,1); }
static void status(void)
{
    network_status_t net;
    network_get_status(&net);
    ui_status(net.wifi,net.mqtt,armed,alarm_status.state==ALARM_ACTIVE,alarm_status.state==ALARM_WARMUP);
}
static void render_home(void)
{
    uint32_t seconds=rtc_get();
    static const char *const labels[]={"SELF TEST","FAULT","DISARM","WARMUP","ARMED","ALARM"};
    header(labels[alarm_status.state]); status();
    if(!datahub_snapshot(&sensors)) {
        const sensor_value_t *t=&sensors.sensors[SENSOR_DHT11];
        const sensor_value_t *u=&sensors.sensors[SENSOR_DISTANCE];
        const sensor_value_t *r=&sensors.sensors[SENSOR_RADAR];
        if(t->quality!=SENSOR_FAULT)snprintf(text,sizeof(text),"T:%s%ld.%ld H:%ld.%ld%%",t->value<0?"-":"",(long)(t->value<0?-(t->value/10):t->value/10),(long)(t->value<0?-(t->value%10):t->value%10),(long)(t->auxiliary/10),(long)(t->auxiliary%10));
        else snprintf(text,sizeof(text),"T:FAULT H:FAULT");
        oled_text(0,16,text,0);
        snprintf(text,sizeof(text),"MQ2:%s MQ135:%s",sensors.sensors[SENSOR_SMOKE].flags&SENSOR_DISABLED?"OFF":sensors.sensors[SENSOR_SMOKE].quality!=SENSOR_GOOD?"?":sensors.sensors[SENSOR_SMOKE].value?"ALM":"OK",
            sensors.sensors[SENSOR_AIR].flags&SENSOR_DISABLED?"OFF":sensors.sensors[SENSOR_AIR].quality!=SENSOR_GOOD?"?":sensors.sensors[SENSOR_AIR].value?"ALM":"OK");
        oled_text(0,24,text,0);
        if(u->quality!=SENSOR_FAULT)snprintf(text,sizeof(text),"D:%ldcm%c Human:%s",(long)u->value,u->quality==SENSOR_STALE?'*':' ',r->quality==SENSOR_GOOD?(r->value?"YES":"NO"):"?");
        else snprintf(text,sizeof(text),"D:FAULT Human:%s",r->quality==SENSOR_GOOD?(r->value?"YES":"NO"):"?");
        oled_text(0,32,text,0);
        if(t->quality==SENSOR_STALE)oled_text(0,48,"T/H: STALE",0);
    } else oled_text(0,16,"Sensors: unavailable",0);
    snprintf(text,sizeof(text),"UTC %02lu:%02lu:%02lu",(unsigned long)(seconds/3600u%24u),(unsigned long)(seconds/60u%60u),(unsigned long)(seconds%60u));
    oled_text(0,40,text,0);
    if(alarm_status.state==ALARM_WARMUP){
        snprintf(text,sizeof(text),"Warmup:%lus",(unsigned long)((alarm_status.warmup_remaining_ms+999u)/1000u));
        oled_text(0,48,text,0);
    }
    oled_text(0,56,"KEY1:NEXT PB15:OK",0);
}
static const char *const root_labels[]={"Home","Arm","Thresholds","Manual Test","Network","Event Log","System","About"};
static void render_list(const char *title,const char *const *labels,unsigned count)
{
    unsigned i,s=selected[current()],first=s>=APP_MENU_ROWS?s-APP_MENU_ROWS+1:0;
    header(title);
    for(i=0;i<APP_MENU_ROWS && first+i<count;i++)row(i,labels[first+i],first+i==s);
}
static void render_root(void) { render_list("MENU",root_labels,8); }
static void render_arm(void)
{
    header("ARM");oled_hanzi(0,16,armed?2:4);oled_hanzi(16,16,3);
    oled_text(0,40,armed?"ARMED":"DISARMED",0);oled_text(0,56,"PB15:TOGGLE",0);
}
static const char *const threshold_labels[]={"Temp C","Distance cm","Smoke ms","Air ms"};
static void render_thresholds(void)
{
    unsigned i;header(!store_state.ready?"STORE OFFLINE":store_state.pending?"SAVING...":editing?"EDIT KEY1:+ HOLD:-":"THRESHOLDS");
    for(i=0;i<4;i++){snprintf(text,sizeof(text),"%s:%d",threshold_labels[i],values[i]);row(i,text,i==selected[THRESHOLDS]);}
    if(editing){unsigned s=selected[THRESHOLDS];ui_progress(0,52,128,9,values[s],maxima[s]);}
}
static const char *const manual_labels[]={"Fan %","Servo deg","Distance","Fire","Intrusion","Air","Fault","Network","Standby","RGB Red","RGB Green","RGB Blue","I2C Scan","Font test"};
static int manual_fan,manual_servo;
static unsigned font_test;
#define MANUAL_COUNT (sizeof(manual_labels)/sizeof(manual_labels[0]))
static void manual_exit(void)
{alarm_manual_fan(0);alarm_manual_servo(0);alarm_manual_effect(EFFECT_STANDBY);manual_fan=manual_servo=0;font_test=0;}
static void render_manual(void)
{
    if(font_test){unsigned i;header("FONTS");oled_text(0,12,"ASCII 6x8",0);oled_text(0,22,"8x16",1);
        for(i=0;i<7;i++) { oled_hanzi((int)i*16,44,i); }
        return;}
    if(selected[MANUAL]<=2u){
        unsigned s=selected[MANUAL];header(manual_labels[s]);
        if(s<2u){
            unsigned actual=s==0?fan_percent():servo_angle();
            int value=s==0?manual_fan:manual_servo;
            snprintf(text,sizeof(text),"Set:%d Out:%u",value,actual);oled_text(0,18,text,0);
            ui_progress(0,32,128,10,value,s==0?100:180);
            oled_text(0,52,editing?"ADJUST":"READY",0);
        }else if(!datahub_snapshot(&sensors)){
            const sensor_value_t *d=&sensors.sensors[SENSOR_DISTANCE];
            if(d->quality==SENSOR_FAULT)snprintf(text,sizeof(text),"FAULT");
            else snprintf(text,sizeof(text),"%ld cm%s",(long)d->value,d->quality==SENSOR_STALE?" STALE":"");
            oled_text(0,24,text,1);
        }
        return;
    }
    render_list("MANUAL TEST",manual_labels,MANUAL_COUNT);
}
static void render_network(void)
{
    network_status_t net;
    network_get_status(&net);
    header("NETWORK");
    snprintf(text,sizeof(text),"WiFi:%s RSSI:%d",net.wifi?"ON":"OFF",(int)net.rssi);
    oled_text(0,18,text,0);
    snprintf(text,sizeof(text),"MQTT:%s",net.mqtt?"ON":"OFF");
    oled_text(0,30,text,0);
    snprintf(text,sizeof(text),"IP:%s",net.ip[0]?net.ip:"-");
    oled_text(0,42,text,0);
}
static void render_log(void)
{
    event_record_t r;unsigned i,first=selected[EVENTLOG]>=APP_MENU_ROWS?selected[EVENTLOG]-APP_MENU_ROWS+1u:0;
    header("EVENT LOG newest");
    for(i=0;i<APP_MENU_ROWS;i++){
        if(storage_recent(first+i,&r,&history_count))break;
        snprintf(text,sizeof(text),"#%lu T%lu P%ld",(unsigned long)r.seq,(unsigned long)r.type,(long)r.parameter);
        row(i,text,first+i==selected[EVENTLOG]);
    }
    if(!history_count)oled_text(0,24,"No stored events",0);
    if(!storage_recent(selected[EVENTLOG],&r,&history_count)){
        snprintf(text,sizeof(text),"UTC:%lu V:%ld",(unsigned long)r.unix_time,(long)r.auxiliary);oled_text(0,56,text,0);
    }
}
static void render_system(void)
{
    const oled_stats_t *s=oled_stats();header("SYSTEM");
    oled_text(0,14,"MCU T:FAULT cal:OFF",0);
    if(!datahub_snapshot(&sensors))
        snprintf(text,sizeof(text),"VDDA:%ldmV nominal",(long)sensors.sensors[SENSOR_MCU].auxiliary);
    else snprintf(text,sizeof(text),"VDDA:FAULT");
    oled_text(0,22,text,0);
    snprintf(text,sizeof(text),"Stack:%lu/%u words",(unsigned long)uxTaskGetStackHighWaterMark(NULL),APP_MENU_STACK);oled_text(0,30,text,0);
    snprintf(text,sizeof(text),"I2C fail:%lu rec:%lu",(unsigned long)s->failures,(unsigned long)s->recoveries);oled_text(0,38,text,0);
    snprintf(text,sizeof(text),"Erase boot:%lu E:%lu",(unsigned long)store_state.erases,(unsigned long)store_state.errors);oled_text(0,46,text,0);
    snprintf(text,sizeof(text),"RTC source:%u",app_time_source());oled_text(0,56,text,0);
}
static void render_about(void){header("ABOUT");oled_text(0,18,"sms-hub",1);oled_text(0,40,APP_FW_VERSION,0);oled_text(0,52,"STM32F103 FreeRTOS",0);}
static void select_key(int key)
{
    page_id id=current();unsigned s=selected[id];
    if(key<0){if(id==MANUAL && font_test){font_test=0;return;}pop();return;}
    if(id==HOME)push(ROOT);
    else if(id==ROOT){static const page_id target[]={HOME,ARM,THRESHOLDS,MANUAL,NETWORK,EVENTLOG,SYSTEM,ABOUT};if(s==0){depth=0;}else push(target[s]);}
    else if(id==ARM){if(alarm_set_mode(!armed))LOGW("布防保存未提交：存储忙或离线");}
    else if(id==THRESHOLDS){
        if(key==2 && editing)accelerate_until=HAL_GetTick()+APP_EDIT_ACCEL_MS;
        else if(editing){if(!storage_set(s,values[s]))editing=0;else LOGW("配置未保存：存储忙或离线");}
        else if(store_state.ready && !store_state.pending)editing=1;
    }
    else if(id==MANUAL){
        if(alarm_status.state!=ALARM_DISARMED || alarm_status.faults){editing=0;LOGW("Manual blocked state=%u fault=%lu",alarm_status.state,(unsigned long)alarm_status.faults);return;}
        if(s<2u){editing=!editing;return;}
        if(s>=3u && s<=11u && storage_event(EVENT_MANUAL,(int32_t)s,0))LOGW("手动测试事件未入队");
        if(s>=3u && s<=8u)alarm_manual_effect(s==8u?EFFECT_STANDBY:(effect_t)(s-2u));
        else switch(s){case 9:case 10:case 11:
            alarm_manual_effect(EFFECT_STANDBY);alarm_manual_rgb(s==9?RGB_ARR:0,s==10?RGB_ARR:0,s==11?RGB_ARR:0);break;
        case 12:i2c_scan();break;case 13:font_test=!font_test;break;default:break;}
    }
}
static const page_t pages[PAGE_COUNT]={
 {no_op,no_op,render_home,select_key},{no_op,no_op,render_root,select_key},
 {no_op,no_op,render_arm,select_key},{no_op,no_op,render_thresholds,select_key},
 {no_op,manual_exit,render_manual,select_key},{no_op,no_op,render_network,select_key},
 {no_op,no_op,render_log,select_key},{no_op,no_op,render_system,select_key},{no_op,no_op,render_about,select_key}};
void menu_init(void){depth=0;stack[0]=HOME;alarm_selftest_report(FAULT_OLED,oled_init()==0);deadline=HAL_GetTick();last_report=deadline;frames=0;}
static void navigate(int steps)
{
    uint32_t now=HAL_GetTick();page_id id=current();
    if(id==HOME){push(ROOT);return;}
    if(steps){
        if(id==MANUAL)font_test=0;
        if(id==THRESHOLDS && editing){unsigned s=selected[id];int gain=(int32_t)(accelerate_until-now)>0?APP_EDIT_ACCEL:1;
            values[s]+=steps*gain;if(values[s]<0)values[s]=0;if(values[s]>maxima[s])values[s]=maxima[s];}
        else if(id==MANUAL && editing && selected[id]<2u){
            int *value=selected[id]==0?&manual_fan:&manual_servo;
            int max=selected[id]==0?100:180;
            *value+=steps;if(*value<0)*value=0;if(*value>max)*value=max;
            if(selected[id]==0)alarm_manual_fan((unsigned)*value);else alarm_manual_servo((unsigned)*value);
        }
        else {int count=id==ROOT?8:id==THRESHOLDS?4:id==MANUAL?(int)MANUAL_COUNT:id==EVENTLOG?(history_count?(int)history_count:1):1;int n=(int)selected[id]+steps;
            if(n<0)n=count-1;
            if(n>=count)n=0;
            selected[id]=(unsigned)n;}
    }
}
void menu_key(const key_event_t *e)
{
    alarm_get_status(&alarm_status);
    if(e->source==KEY_WAKEUP && alarm_status.state==ALARM_ACTIVE){alarm_acknowledge();notice_visible=0;return;}
    if(e->source==KEY_BACK && notice_visible){notice_visible=0;return;}
    if(e->source==KEY_WAKEUP){navigate(e->action==KEY_LONG?-1:1);return;}
    if(e->source==KEY_BACK){pages[current()].on_key(-1);return;}
    if(e->source==KEY_CONFIRM)pages[current()].on_key(e->action==KEY_LONG?2:1);
}
void menu_poll(void)
{
    uint32_t now=HAL_GetTick();page_id id=current();unsigned i;
    if(!storage_get_fields(committed_fields,&store_state)){
        armed=committed_fields[CFG_MODE]!=0;
        if(!editing && !store_state.pending)for(i=0;i<4;i++)values[i]=(int)committed_fields[i];
    }
    alarm_get_status(&alarm_status);
    if(notice_transition!=alarm_status.transitions || notice_fault!=alarm_status.faults){
        notice_transition=alarm_status.transitions;notice_fault=alarm_status.faults;
        notice_visible=alarm_status.state==ALARM_ACTIVE || alarm_status.state==ALARM_FAULT;
        editing=0;font_test=0;
    }
    if((int32_t)(now-deadline)>=0){
        deadline=now+APP_OLED_FRAME_MS;oled_clear();
        if(notice_visible && alarm_status.state==ALARM_FAULT){
            header("FAULT");
            snprintf(text,sizeof(text),"Code:0x%03lX",(unsigned long)alarm_status.faults);oled_text(0,16,text,1);
            oled_text(0,40,"Outputs stopped",0);oled_text(0,56,"PC13:BACK",0);
        }else if(notice_visible && alarm_status.state==ALARM_ACTIVE){
            header(alarm_status.kind==ALARM_FIRE?"FIRE":alarm_status.kind==ALARM_INTRUSION?"INTRUSION":"AIR NOTICE");
            oled_hanzi(0,16,0);oled_hanzi(16,16,1);oled_text(0,48,"PA0:ACK PC13:BACK",0);
        }else pages[id].render();
        if(!oled_flush(0))++frames;
        alarm_selftest_report(FAULT_OLED,oled_stats()->online);
    }
    if(now-last_report>=APP_STATUS_MS){
        const oled_stats_t *s=oled_stats();
        LOGI("显示：帧率x10=%lu 耗时=%luus 连败=%lu 恢复=%lu",
            (unsigned long)(frames*10000u/(now-last_report)),(unsigned long)s->last_us,
            (unsigned long)s->failures,(unsigned long)s->recoveries);
        frames=0;last_report=now;
        if(s->recoveries || s->failures)LOGW("OLED诊断 最近状态=%lu 错误码=0x%lX 失败传输=%luus",
            (unsigned long)s->last_hal_status,(unsigned long)s->last_hal_error,(unsigned long)s->last_failed_us);
    }
}
