#include "menu.h"
#include "ssd1306.h"
#include "oled_ui.h"
#include "app_config.h"
#include "datahub.h"
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
static datahub_snapshot_t sensors;
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
    /* Outlined W/M = offline placeholders. A/L/P icons show arm/alarm/warmup. */
    ui_status(0,0,armed,0,HAL_GetTick()<APP_MQ_WARMUP_MS);
}
static void render_home(void)
{
    uint32_t seconds=rtc_get();
    header(armed?"ARMED":"DISARM"); status();
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
    oled_text(0,40,text,0);oled_text(0,56,"KEY1:NEXT PB15:OK",0);
}
static const char *root_labels[]={"Home","Arm","Thresholds","Manual Test","Network","Event Log","System","About"};
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
    oled_text(0,40,armed?"ARMED (RAM)":"DISARMED (RAM)",0);oled_text(0,56,"PB15:TOGGLE",0);
}
static const char *threshold_labels[]={"Temp C","Distance cm","Smoke ms","Air ms"};
static void render_thresholds(void)
{
    unsigned i;header(editing?"EDIT KEY1:+ HOLD:-":"THRESHOLDS");
    for(i=0;i<4;i++){snprintf(text,sizeof(text),"%s:%d",threshold_labels[i],values[i]);row(i,text,i==selected[THRESHOLDS]);}
    if(editing){unsigned s=selected[THRESHOLDS];ui_progress(0,52,128,9,values[s],maxima[s]);}
}
static const char *manual_labels[]={"Beep","RGB Red","RGB Green","RGB Blue","RGB Off","I2C Scan","Servo (M5)","Fan (M5)","Font test"};
static unsigned font_test;
static void render_manual(void)
{
    if(font_test){unsigned i;header("FONTS");oled_text(0,12,"ASCII 6x8",0);oled_text(0,22,"8x16",1);
        for(i=0;i<7;i++) { oled_hanzi((int)i*16,44,i); }
        return;}
    render_list("MANUAL TEST",manual_labels,9);
}
static void render_network(void) {header("NETWORK");oled_text(0,18,"WiFi: offline",0);oled_text(0,30,"MQTT: offline",0);oled_text(0,44,"M6 placeholder",0);}
static void render_log(void) {header("EVENT LOG");oled_text(0,24,"M4 placeholder",0);}
static void render_system(void)
{
    const oled_stats_t *s=oled_stats();header("SYSTEM");
    oled_text(0,14,"MCU T:FAULT cal:OFF",0);
    snprintf(text,sizeof(text),"VDDA:%lumV nominal",(unsigned long)vdda_mv_get());oled_text(0,22,text,0);
    snprintf(text,sizeof(text),"Stack:%lu/%u words",(unsigned long)uxTaskGetStackHighWaterMark(NULL),APP_MENU_STACK);oled_text(0,30,text,0);
    snprintf(text,sizeof(text),"I2C fail:%lu rec:%lu",(unsigned long)s->failures,(unsigned long)s->recoveries);oled_text(0,38,text,0);
    snprintf(text,sizeof(text),"refresh:%luus",(unsigned long)s->last_us);oled_text(0,46,text,0);
    oled_text(0,56,APP_FW_VERSION,0);
}
static void render_about(void){header("ABOUT");oled_text(0,18,"sms-hub",1);oled_text(0,40,APP_FW_VERSION,0);oled_text(0,52,"STM32F103 FreeRTOS",0);}
static void select_key(int key)
{
    page_id id=current();unsigned s=selected[id];
    if(key<0){if(id==MANUAL && font_test){font_test=0;return;}pop();return;}
    if(id==HOME)push(ROOT);
    else if(id==ROOT){static const page_id target[]={HOME,ARM,THRESHOLDS,MANUAL,NETWORK,EVENTLOG,SYSTEM,ABOUT};if(s==0){depth=0;}else push(target[s]);}
    else if(id==ARM)armed=!armed;
    else if(id==THRESHOLDS){if(key==2 && editing)accelerate_until=HAL_GetTick()+APP_EDIT_ACCEL_MS;else editing=!editing;}
    else if(id==MANUAL){
        switch(s){case 0:beep_ms(APP_MENU_BEEP_MS);break;case 1:rgb_set(RGB_ARR,0,0);break;
        case 2:rgb_set(0,RGB_ARR,0);break;case 3:rgb_set(0,0,RGB_ARR);break;case 4:rgb_set(0,0,0);break;
        case 5:i2c_scan();break;case 8:font_test=!font_test;break;default:break;}
    }
}
static const page_t pages[PAGE_COUNT]={
 {no_op,no_op,render_home,select_key},{no_op,no_op,render_root,select_key},
 {no_op,no_op,render_arm,select_key},{no_op,no_op,render_thresholds,select_key},
 {no_op,no_op,render_manual,select_key},{no_op,no_op,render_network,select_key},
 {no_op,no_op,render_log,select_key},{no_op,no_op,render_system,select_key},{no_op,no_op,render_about,select_key}};
void menu_init(void){depth=0;stack[0]=HOME;(void)oled_init();deadline=HAL_GetTick();last_report=deadline;frames=0;}
static void navigate(int steps)
{
    uint32_t now=HAL_GetTick();page_id id=current();
    if(id==HOME){push(ROOT);return;}
    if(steps){
        if(id==THRESHOLDS && editing){unsigned s=selected[id];int gain=(int32_t)(accelerate_until-now)>0?APP_EDIT_ACCEL:1;
            values[s]+=steps*gain;if(values[s]<0)values[s]=0;if(values[s]>maxima[s])values[s]=maxima[s];
            if(s>=2 && datahub_set_gas_limits((uint32_t)values[2],(uint32_t)values[3]))LOGW("阈值暂未同步");}
        else {int count=id==ROOT?8:id==THRESHOLDS?4:id==MANUAL?9:1;int n=(int)selected[id]+steps;
            if(n<0)n=count-1;
            if(n>=count)n=0;
            selected[id]=(unsigned)n;}
    }
}
void menu_key(const key_event_t *e)
{
    if(e->source==KEY_WAKEUP){navigate(e->action==KEY_LONG?-1:1);return;}
    if(e->source==KEY_BACK){pages[current()].on_key(-1);return;}
    if(e->source==KEY_CONFIRM)pages[current()].on_key(e->action==KEY_LONG?2:1);
}
void menu_poll(void)
{
    uint32_t now=HAL_GetTick();page_id id=current();
    if((int32_t)(now-deadline)>=0){deadline=now+APP_OLED_FRAME_MS;oled_clear();pages[id].render();if(!oled_flush(0))++frames;}
    if(now-last_report>=APP_STATUS_MS){
        const oled_stats_t *s=oled_stats();
        LOGI("显示：帧率x10=%lu 耗时=%luus 连败=%lu 恢复=%lu",
            (unsigned long)(frames*10000u/(now-last_report)),(unsigned long)s->last_us,
            (unsigned long)s->failures,(unsigned long)s->recoveries);
        frames=0;last_report=now;
    }
}
