"""Host tests compile actual device code with bounded HAL/RTOS stubs."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
env=dict(os.environ);env['PATH']='D:/msys64/ucrt64/bin;'+env['PATH']
out=root/'tmp/m2';out.mkdir(exist_ok=True)
def compile_run(name,source):
    (out/(name+'.c')).write_text(source,encoding='utf-8')
    subprocess.run(['D:/msys64/ucrt64/bin/gcc.exe','-std=c99','-Wall','-Wextra','-Werror',
      '-I','Drivers_Device/kvstore','-I','Drivers_Device/eventlog','-I','Drivers_Device','-I','BSP','-I','App','-I','Inc','-I','Drivers_Device/ec11','-I','Drivers_Device/oled_ssd1306','-I','App/menu',
      '-I','Drivers_Device/sg90','-I','Drivers_Device/tb6612_fan','-I','Drivers_Device/oled_ui','-I','Middleware/esp8266','-I','tmp/m2','tmp/m2/'+name+'.c','-o','tmp/m2/'+name+'.exe'],cwd=root,env=env,check=True)
    subprocess.run([str(out/(name+'.exe'))],cwd=root,env=env,check=True)
ec=(root/'Drivers_Device/ec11/ec11.c').read_text(encoding='utf-8')
part=ec[ec.index('void ec11_reset'):ec.index('static uint8_t sample')]
compile_run('encoder',r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define APP_EC11_EDGES_PER_DETENT 4
static uint8_t previous;
static int phase;
static uint32_t invalid;
'''+part+r'''
int main(void){
 unsigned n,i;int sum=0;const uint8_t cw[]={1,0,2,3},ccw[]={2,0,1,3};
 ec11_reset(3);
 for(n=0;n<20;n++)for(i=0;i<4;i++){int d=ec11_feed(cw[i]);assert(d>=0);sum+=d;}
 assert(sum==20);sum=0;
 for(n=0;n<20;n++)for(i=0;i<4;i++){int d=ec11_feed(ccw[i]);assert(d<=0);sum+=d;}
 assert(sum==-20);
 ec11_reset(3);sum=0;
 for(n=0;n<20;n++){
  sum+=ec11_feed(1);sum+=ec11_feed(3);sum+=ec11_feed(1);
  sum+=ec11_feed(0);sum+=ec11_feed(2);sum+=ec11_feed(3);
 }
 assert(sum==20);ec11_reset(3);assert(ec11_feed(0)==0);assert(ec11_feed(3)==0);
 puts("PASS EC11 actual decoder: +20/-20, no reverse, bounced edges, invalid two-bit transitions");return 0;
}
''')
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __BSP_PIN_H
#define __I2C_H__
#define DEF_TEMP_ALARM_C_X10 500
#define DEF_INTRUSION_DIST_CM 100
#define DEF_SMOKE_CONFIRM_MS 3000
#define DEF_AIR_CONFIRM_MS 10000
#define MQ_WARMUP_S 150
#define RGB_ARR 999
#define KV_RECORD_SIZE 512u
#define OLED_ADDR 0x78u
#define I2C_MEMADD_SIZE_8BIT 1
#define HAL_OK 0
static struct {uint32_t ErrorCode;} hi2c1;
static int missing,quiet,deny_quiet,inject_log;
static uint32_t now,us;
static unsigned writes,recoveries;
uint32_t HAL_GetTick(void){return now;}
uint32_t bsp_micros(void){return us;}
int bsp_i2c_recover(void){recoveries++;return 0;}
int bsp_log_quiet_begin(uint32_t timeout_ms){(void)timeout_ms;if(deny_quiet)return -1;assert(!quiet);quiet=1;return 0;}
void bsp_log_quiet_end(void){assert(quiet);quiet=0;}
static int HAL_I2C_Mem_Write(void *h,unsigned addr,unsigned control,unsigned mode,
 uint8_t *data,unsigned n,unsigned timeout){
 (void)h;(void)mode;(void)data;(void)timeout;assert(addr==0x78u);assert(control==0||control==0x40);
 writes++;us+=(n+2)*23;
 /* Model a synchronous30ms higher-priority log burst during the8ms HAL budget. */
 if(inject_log && !quiet){us+=30000;hi2c1.ErrorCode=0x20;return 1;}
 hi2c1.ErrorCode=missing?4u:0u;return missing?1:0;
}
#include "../../Drivers_Device/oled_ssd1306/ssd1306.c"
'''
compile_run('display',prefix+r'''
int main(void){
 unsigned n;
 inject_log=1;assert(oled_init()==0);assert(!quiet);
 assert(oled_flush(1)==0);assert(!quiet);n=writes;
 assert(oled_flush(0)==0 && writes==n);
 oled_pixel(2,2,1);assert(oled_flush(0)==0 && writes==n+5);
 oled_pixel(-1,-1,1);oled_pixel(128,64,1);n=writes;assert(oled_flush(0)==0 && writes==n);
 missing=1;oled_pixel(0,0,1);assert(oled_flush(0)==-1 && !stats.online);
 assert(oled_stats()->last_hal_status==1 && oled_stats()->last_hal_error==4 && oled_stats()->last_failed_us>0);
 n=writes;assert(oled_flush(0)==-1 && writes==n);assert(!quiet);
 missing=0;now+=APP_OLED_RETRY_MS;assert(oled_flush(0)==0);
 assert(stats.online && stats.failures==0 && recoveries==1 && stats.recoveries==1);
 assert(oled_stats()->last_hal_error==4); /* retain failure after successful recovery */
 deny_quiet=1;oled_pixel(4,4,1);n=writes;
 assert(oled_flush(0)==-2 && writes==n && stats.online && stats.failures==0 && !quiet);
 deny_quiet=0;assert(oled_flush(0)==0 && writes>n && !quiet);
 oled_clear();oled_text(0,0,"sms-hub 6x8",0);oled_text(0,12,"8x16",1);
 for(n=0;n<7;n++)oled_hanzi(n*16,36,n);
 {FILE *f=fopen("tmp/m2/fonts.pbm","wb");unsigned x,y;assert(f);fprintf(f,"P1\n128 64\n");
 for(y=0;y<64;y++){for(x=0;x<128;x++)fprintf(f,"%u ",(pixels[y/8*128+x]>>(y&7))&1);fputc('\n',f);}fclose(f);}
 puts("PASS OLED console contention: deferred frame retains online/dirty state; lock released on success and failure");
 puts("PASS OLED actual driver: 1KB buffer, dirty page/no-op refresh, clipping, unplug/retry/recover/reset consecutive failures");return 0;
}
''')
# Minimal headers for the menu's read-only RTOS diagnostics.
(out/'FreeRTOS.h').write_text('')
(out/'task.h').write_text('static unsigned uxTaskGetStackHighWaterMark(void *p){(void)p;return 200;}\n')
compile_run('menu',prefix+r'''
#include "bsp_services.h"
#include "datahub.h"
#include "esp8266.h"
int network_get_status(network_status_t *s){memset(s,0,sizeof(*s));return 0;}
int datahub_snapshot(datahub_snapshot_t *p){memset(p,0,sizeof(*p));p->sensors[SENSOR_DHT11].value=260;p->sensors[SENSOR_DHT11].auxiliary=550;p->sensors[SENSOR_SMOKE].flags=p->sensors[SENSOR_AIR].flags=SENSOR_DISABLED;return 0;}
int datahub_set_gas_limits(uint32_t s,uint32_t a){(void)s;(void)a;return 0;}
#include "storage.h"
static int32_t persisted[CFG_FIELD_COUNT]={50,100,3000,10000,0};
static unsigned history_reads,save_calls;
int storage_get_fields(int32_t v[CFG_FIELD_COUNT],storage_status_t *s){memcpy(v,persisted,sizeof(persisted));memset(s,0,sizeof(*s));s->ready=1;return 0;}
int storage_set(unsigned field,int32_t v){assert(field<CFG_FIELD_COUNT);persisted[field]=v;save_calls++;return 0;}
int storage_event(uint32_t t,int32_t p,int32_t a){(void)t;(void)p;(void)a;return 0;}
int storage_recent(unsigned index,event_record_t *r,unsigned *n){*n=20;if(index>=20)return -1;memset(r,0,sizeof(*r));r->seq=20-index;r->type=1;r->unix_time=1234;history_reads++;return 0;}
static int pending,beeps;
static unsigned colors;
void ec11_start(void){}
int ec11_poll(void){int n=pending;pending=0;return n;}
uint32_t ec11_overflows(void){return 0;}
uint8_t ec11_levels(void){return 3;}
uint32_t ec11_irq_count(void){return 0;}
uint32_t ec11_invalid_count(void){return 0;}
uint32_t rtc_get(void){return 3661;}
uint32_t vdda_mv_get(void){return 3300;}
int beep_ms(uint16_t ms){(void)ms;beeps++;return 0;}
int rgb_set(uint16_t r,uint16_t g,uint16_t b){colors=r+g+b;return 0;}
static unsigned fan_value,servo_value;
int fan_set(unsigned p){assert(p<=100);fan_value=p;return 0;}
unsigned fan_percent(void){return fan_value;}
int servo_write(unsigned p){assert(p<=180);servo_value=p;return 0;}
unsigned servo_angle(void){return servo_value;}
int effect_set(effect_t e){assert(e<EFFECT_COUNT);colors=0;return 0;}
#include "alarm_engine.h"
static alarm_status_t mock_alarm={0,0,0,ALARM_DISARMED,ALARM_NONE};
void alarm_selftest_report(uint32_t mask,int passed){(void)mask;(void)passed;}
void alarm_get_status(alarm_status_t *s){*s=mock_alarm;}
void alarm_acknowledge(void){}
int alarm_set_mode(unsigned a){return storage_set(CFG_MODE,(int32_t)a);}
int alarm_manual_fan(unsigned p){return fan_set(p);}
int alarm_manual_servo(unsigned p){return servo_write(p);}
int alarm_manual_effect(effect_t e){return effect_set(e);}
int alarm_manual_beep(uint16_t ms){return beep_ms(ms);}
int alarm_manual_rgb(uint16_t r,uint16_t g,uint16_t b){return rgb_set(r,g,b);}
unsigned app_time_source(void){return 1;}
void i2c_scan(void){}
void bsp_log(const char *level,const char *format,...){(void)level;(void)format;}
#include "../../Drivers_Device/oled_ui/oled_ui.c"
#include "../../App/menu/menu.c"
static void expect_header(const char *title)
{
 uint8_t rendered[128];
 now+=APP_OLED_FRAME_MS;menu_poll();memcpy(rendered,pixels,sizeof(rendered));
 oled_clear();oled_text(0,0,title,0);
 if(memcmp(rendered,pixels,sizeof(rendered)))fprintf(stderr,"Expected OLED header: %s\n",title);
 assert(!memcmp(rendered,pixels,sizeof(rendered)));
}
int main(void){
 unsigned i;key_event_t next={KEY_WAKEUP,KEY_CLICK,0},prev={KEY_WAKEUP,KEY_LONG,0};key_event_t yes={KEY_CONFIRM,KEY_CLICK,0},back={KEY_BACK,KEY_CLICK,0},hold={KEY_CONFIRM,KEY_LONG,0};
 menu_init();assert(current()==HOME);menu_key(&next);assert(current()==ROOT);
 selected[ROOT]=7;menu_key(&next);assert(selected[ROOT]==0);menu_key(&prev);assert(selected[ROOT]==7);
 for(i=1;i<8;i++){selected[ROOT]=i;menu_key(&yes);assert(depth==2);menu_poll();menu_key(&back);assert(current()==ROOT && depth==1);}
 selected[ROOT]=2;menu_key(&yes);menu_key(&yes);assert(editing);
 menu_key(&next);menu_poll();assert(values[0]==51);menu_key(&prev);menu_poll();assert(values[0]==50);
 menu_key(&hold);menu_key(&next);menu_poll();assert(values[0]==60);
 for(i=0;i<100;i++){menu_key(&next);}menu_poll();assert(values[0]==APP_EDIT_MAX_TEMP);
 assert(save_calls==0);menu_key(&yes);assert(!editing&&save_calls==1&&persisted[0]==APP_EDIT_MAX_TEMP);
 menu_key(&back);selected[ROOT]=5;menu_key(&yes);now+=100;menu_poll();
 assert(history_count==20&&history_reads>0);selected[EVENTLOG]=19;menu_key(&next);assert(selected[EVENTLOG]==0);
 menu_key(&back);selected[ROOT]=3;menu_key(&yes);selected[MANUAL]=9;menu_key(&yes);assert(beeps==1);
 selected[MANUAL]=10;menu_key(&yes);assert(colors==999);selected[MANUAL]=8;menu_key(&yes);assert(colors==0);
 selected[MANUAL]=0;menu_key(&yes);assert(editing);
 menu_key(&prev);assert(fan_percent()==0);
 for(i=0;i<110;i++){menu_key(&next);}assert(fan_value==100);
 menu_key(&prev);assert(fan_percent()==99);menu_key(&next);menu_poll();menu_key(&yes);
 selected[MANUAL]=1;menu_key(&yes);menu_key(&prev);assert(servo_angle()==0);
 for(i=0;i<190;i++){menu_key(&next);}assert(servo_value==180);
 menu_key(&prev);assert(servo_angle()==179);menu_key(&next);menu_poll();menu_key(&yes);
 selected[MANUAL]=2;now+=100;menu_poll();menu_key(&back);assert(!fan_value&&!servo_value);
 menu_key(&yes);
 for(i=0;i<12;i++){menu_key(&next);}
 menu_key(&yes);expect_header("FONTS");
 menu_key(&prev);expect_header("MANUAL TEST");
 menu_key(&next);menu_key(&next);expect_header("Fan %");
 {
  uint8_t frames[15][1024];
  for(i=0;i<15;i++){
   now+=APP_OLED_FRAME_MS;menu_poll();memcpy(frames[i],pixels,sizeof(pixels));menu_key(&next);
  }
  expect_header("Fan %");
  for(i=15;i>0;i--){
   menu_key(&prev);now+=APP_OLED_FRAME_MS;menu_poll();
   assert(!memcmp(frames[i-1],pixels,sizeof(pixels)));
  }
 }
 menu_key(&prev);menu_key(&yes);expect_header("FONTS");
 menu_key(&next);expect_header("Fan %");
 menu_key(&prev);menu_key(&yes);expect_header("FONTS");
 menu_key(&back);expect_header("MANUAL TEST");menu_key(&back);
 assert(!fan_percent()&&!servo_angle());
 puts("PASS Manual Test visible navigation: all 15 items forward/back/wrap, font preview long/click/back, edit decrement/clamp, exit safe");
 stack[depth]=MANUAL;
 for(i=0;i<3;i++){
  char path[64];FILE *f;unsigned x,y;selected[MANUAL]=i;manual_fan=100;manual_servo=180;
  fan_value=100;servo_value=180;oled_clear();render_manual();
  snprintf(path,sizeof(path),"tmp/m2/manual-%u.pbm",i);f=fopen(path,"wb");assert(f);fprintf(f,"P1\n128 64\n");
  for(y=0;y<64;y++){for(x=0;x<128;x++)fprintf(f,"%u ",(pixels[y/8*128+x]>>(y&7))&1);fputc('\n',f);}fclose(f);
 }
 for(i=0;i<PAGE_COUNT;i++){
  char path[64];FILE *f;unsigned x,y;stack[depth]=(page_id)i;oled_clear();pages[i].render();
  snprintf(path,sizeof(path),"tmp/m2/page-%u.pbm",i);f=fopen(path,"wb");assert(f);fprintf(f,"P1\n128 64\n");
  for(y=0;y<64;y++){for(x=0;x<128;x++)fprintf(f,"%u ",(pixels[y/8*128+x]>>(y&7))&1);fputc('\n',f);}fclose(f);
 }
 mock_alarm.state=ALARM_FAULT;mock_alarm.faults=FAULT_SMOKE|FAULT_AIR;mock_alarm.transitions++;
 expect_header("FAULT");menu_key(&back);assert(!notice_visible);
 mock_alarm.state=ALARM_ACTIVE;mock_alarm.kind=ALARM_FIRE;mock_alarm.faults=0;mock_alarm.transitions++;
 expect_header("FIRE");
 mock_alarm.kind=ALARM_INTRUSION;mock_alarm.transitions++;expect_header("INTRUSION");
 mock_alarm.kind=ALARM_AIR;mock_alarm.transitions++;expect_header("AIR NOTICE");
 mock_alarm.state=ALARM_DISARMED;mock_alarm.kind=0;mock_alarm.transitions++;
 puts("PASS M7 actual OLED popup: FAULT/FIRE/INTRUSION/AIR, dismiss and restored navigation");
 puts("PASS actual menu: every page reachable/back, depth3, edit/accelerate/clamp, manual beep/RGB, all pages rendered");return 0;
}
''')
from PIL import Image,ImageOps,ImageDraw
paths=[out/'fonts.pbm']+sorted(out.glob('page-*.pbm'))
sheet=Image.new('RGB',(640, len(paths)*280),'#222222');draw=ImageDraw.Draw(sheet)
for i,p in enumerate(paths):
    im=Image.open(p).convert('L');im=ImageOps.invert(im).resize((512,256),Image.Resampling.NEAREST)
    sheet.paste(im,(0,i*280+18));draw.text((0,i*280),p.stem,fill='white')
sheet.save(out/'preview.png')
manual_sheet=Image.new('RGB',(512,3*256),'black')
for i,p in enumerate(sorted(out.glob('manual-*.pbm'))):
    im=ImageOps.invert(Image.open(p).convert('L')).resize((512,256),Image.Resampling.NEAREST)
    manual_sheet.paste(im,(0,i*256))
manual_sheet.save(root/'Docs/M5-manual-host.png')

console=(root/'BSP/bsp_console.c').read_text(encoding='utf-8')
quiet_code=console[console.index('int bsp_log_quiet_begin'):console.index('static const char *display_task')]
compile_run('log_quiet',r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define taskSCHEDULER_RUNNING 2
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
static void *log_lock;
static unsigned ipsr,primask,basepri,takes,gives,waited;
static int scheduler,available=1;
unsigned __get_IPSR(void){return ipsr;}
unsigned __get_PRIMASK(void){return primask;}
unsigned __get_BASEPRI(void){return basepri;}
int xTaskGetSchedulerState(void){return scheduler;}
int xSemaphoreTake(void *lock,unsigned wait){assert(lock==log_lock);takes++;waited=wait;return available;}
int xSemaphoreGive(void *lock){assert(lock==log_lock);gives++;return 1;}
'''+quiet_code+r'''
int main(void){
 assert(bsp_log_quiet_begin(50)==-1);log_lock=(void*)1;
 assert(bsp_log_quiet_begin(50)==-1);scheduler=taskSCHEDULER_RUNNING;
 ipsr=1;assert(bsp_log_quiet_begin(50)==-1);ipsr=0;
 primask=1;assert(bsp_log_quiet_begin(50)==-1);primask=0;
 basepri=80;assert(bsp_log_quiet_begin(50)==-1);basepri=0;assert(takes==0);
 available=0;assert(bsp_log_quiet_begin(50)==-1);assert(takes==1&&waited==50);
 available=1;assert(bsp_log_quiet_begin(50)==0);bsp_log_quiet_end();assert(gives==1);
 puts("PASS actual console exclusion: existing mutex, bounded wait, IRQ/critical/startup rejection, release");return 0;
}
''')
