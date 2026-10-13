"""Actual KV/eventlog C on a NOR model with byte-granular power loss.
The model enforces1->0, page bounds, reserved-range guards; not hardware evidence.
"""
from pathlib import Path
import os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'tmp/storage-test';OUT.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env['PATH']='D:/msys64/ucrt64/bin;'+env['PATH']
(OUT/'stm32f1xx_hal.h').write_text('#include <stdint.h>\ntypedef struct {int unused;} TIM_HandleTypeDef;\nunsigned __get_IPSR(void);\n')
test=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stddef.h>
#include "storage.h"
#include "kvstore.h"
#include "eventlog.h"
#include "w25q64.h"
static uint32_t applied_smoke,applied_air;
typedef struct {unsigned n,size,head,tail,count;uint8_t data[128];} TestQueue;
typedef struct {int held;} TestMutex;
static unsigned view_held;
void *xSemaphoreCreateMutex(void){return calloc(1,sizeof(TestMutex));}
int xSemaphoreTake(void *m,unsigned ms){TestMutex *lock=m;(void)ms;if(!m)return 0;assert(!lock->held);lock->held=1;++view_held;return 1;}
int xSemaphoreGive(void *m){TestMutex *lock=m;if(!m)return 0;assert(lock->held);lock->held=0;--view_held;return 1;}
void *xQueueCreate(unsigned n,unsigned size){TestQueue*q=calloc(1,sizeof(*q));assert(n*size<=128);q->n=n;q->size=size;return q;}
int xQueueSend(void *v,const void*p,unsigned ms){TestQueue*q=v;(void)ms;if(q->count==q->n)return 0;memcpy(q->data+q->tail*q->size,p,q->size);q->tail=(q->tail+1)%q->n;q->count++;return 1;}
int xQueueReceive(void *v,void*p,unsigned ms){TestQueue*q=v;(void)ms;if(!q->count)return 0;memcpy(p,q->data+q->head*q->size,q->size);q->head=(q->head+1)%q->n;q->count--;return 1;}
unsigned uxQueueMessagesWaiting(void *v){return ((TestQueue*)v)->count;}
static int bus_busy,bus_held;
int spi_bus_init(void){return 0;}
int spi_bus_take(uint32_t ms){(void)ms;if(bus_busy)return -1;assert(!bus_held);bus_held=1;return 0;}
void spi_bus_give(void){assert(bus_held);bus_held=0;}
unsigned __get_IPSR(void){return 0;}
uint32_t rtc_get(void){return 12345;}
int datahub_set_gas_limits(uint32_t a,uint32_t b){applied_smoke=a;applied_air=b;return 0;}
void bsp_log(const char*l,const char*f,...){(void)l;(void)f;}
int network_event_enqueue(uint32_t t,int32_t p,int32_t a,uint32_t time)
{(void)t;(void)p;(void)a;(void)time;return 0;}
static uint8_t flash[FLASH_LOG_END_EXCLUSIVE],backup[sizeof(flash)];
static config_t c;
static event_record_t records[APP_STORAGE_RECENT];
static jmp_buf reboot;
static int cut=-1,writes,program_error;
static unsigned event_reads,fail_event_read,check_unpublished;
static unsigned erase_count;
static void mutate(void){if(cut==0)longjmp(reboot,1);if(cut>0)--cut;++writes;}
int w25q_read(uint32_t a,void *p,size_t n){
 assert(!view_held&&n&&n<=256&&a+n<=sizeof(flash));
 if(check_unpublished){event_record_t r;unsigned nr;assert(storage_recent(0,&r,&nr)==-1&&nr==0);}
 if(a>=FLASH_LOG_ADDR && ++event_reads==fail_event_read)return -1;
 memcpy(p,flash+a,n);return 0;
}
int w25q_program(uint32_t a,const void *p,size_t n){
 const uint8_t *s=p;size_t i;if(program_error)return -1;assert(n&&n<=256-(a%256)&&a+n<=sizeof(flash));
 assert(a+n<=FLASH_RSVD_ADDR || a>=FLASH_LOG_ADDR);
 for(i=0;i<n;i++){assert(flash[a+i]==0xff);mutate();flash[a+i]&=s[i];}return 0;
}
int w25q_erase(uint32_t a){unsigned i;assert(a%4096==0&&a+4096<=sizeof(flash)&&a!=FLASH_RSVD_ADDR);
 for(i=0;i<4096;i++){mutate();flash[a+i]=0xff;}erase_count++;return 0;}
uint32_t w25q_erases(void){return erase_count;}
int w25q_init(void){return 0;}uint32_t w25q_id(void){return W25Q_JEDEC_ID;}
static void reset_media(void){memset(flash,0xff,sizeof(flash));cut=-1;erase_count=0;}
int main(void){
 unsigned i,k;size_t n;uint32_t seq,old;int r;
 assert(sizeof(config_t)==512&&sizeof(event_record_t)==32);
 assert(storage_crc32("123456789",9)==0xcbf43926u);
 reset_media();assert(kv_load(&c)==1&&c.seq==2);assert(kv_load(&c)==0&&c.seq==2);
 for(i=0;i<500;i++){c.temp_x10=(int32_t)(i%101)*10;assert(!kv_save(&c));seq=c.seq;
  assert(!kv_load(&c)&&c.seq==seq&&c.temp_x10==(int32_t)(i%101)*10);}
 assert(erase_count>0);puts("PASS KV500 saves with reset each time; alternating independent sectors/CRC");
 /* Full both banks: next save erases only the sector without newest record. */
 reset_media();assert(kv_load(&c)==1);for(i=0;i<14;i++){c.distance_cm=i;assert(!kv_save(&c));}
 old=c.seq;memcpy(backup,flash,sizeof(flash));
 for(k=0;k<=4096+512;k++){
  memcpy(flash,backup,sizeof(flash));cut=-1;assert(!kv_load(&c));c.distance_cm=321;
  if(!setjmp(reboot)){cut=(int)k;r=kv_save(&c);assert(!r);}
  cut=-1;assert(!kv_load(&c));assert(c.seq==old || c.seq==old+1);
  assert(c.distance_cm==(c.seq==old?13u:321u));
  c.distance_cm=222;assert(!kv_save(&c));assert(!kv_load(&c)&&c.distance_cm==222);
 }
 puts("PASS KV power cuts at every byte of sector erase/data/commit; old-or-new only, resave after reboot");
 reset_media();memcpy(backup,flash,sizeof(flash));
 for(k=0;k<1024;k+=7){memcpy(flash,backup,sizeof(flash));
  if(!setjmp(reboot)){cut=(int)k;(void)kv_load(&c);}cut=-1;assert(kv_load(&c)>=0&&config_valid(&c));}
 puts("PASS blank initialization power cuts recover without erasing sole valid default");
 reset_media();assert(!eventlog_init());
 for(i=1;i<=2200;i++){assert(!log_append(i,1,(int32_t)i,0));if(i%127==0)assert(!eventlog_init());}
 assert(!eventlog_init());assert(eventlog_next_seq()==2201);assert(!log_read_last(20,records,&n)&&n==20);
 for(i=0;i<20;i++)assert(records[i].seq==2200-i && records[i].parameter==(int32_t)(2200-i));
 puts("PASS events2200 entries:64KB ring wrap, newest20 CRC/sequence after reboot");
 /* Prepare exact sector rollover, inject through erase+record commit. */
 reset_media();assert(!eventlog_init());for(i=1;i<=128;i++)assert(!log_append(i,1,(int32_t)i,0));
 memcpy(backup,flash,sizeof(flash));
 for(k=0;k<=4096+32;k++){
  memcpy(flash,backup,sizeof(flash));cut=-1;assert(!eventlog_init());
  if(!setjmp(reboot)){cut=(int)k;assert(!log_append(129,1,129,0));}
  cut=-1;assert(!eventlog_init());assert(!log_read_last(20,records,&n));
  assert(records[0].seq==128||records[0].seq==129);seq=records[0].seq;
  assert(!log_append(130,1,130,0));assert(!eventlog_init());assert(!log_read_last(1,records,&n));assert(records[0].seq==seq+1);
 }
 puts("PASS event power cuts at every erase/data/commit byte: last committed survives, next seq continuous");
 /* Erase at full-ring wrap must preserve newest sector under partial erasure. */
 reset_media();assert(!eventlog_init());for(i=1;i<=2048;i++)assert(!log_append(i,1,(int32_t)i,0));
 memcpy(backup,flash,sizeof(flash));
 for(k=0;k<=4096+32;k+=7){
  memcpy(flash,backup,sizeof(flash));cut=-1;assert(!eventlog_init());
  if(!setjmp(reboot)){cut=(int)k;(void)log_append(2049,1,2049,0);}cut=-1;
  assert(!eventlog_init());assert(!log_read_last(20,records,&n)&&n==20);
  assert(records[0].seq==2048 || records[0].seq==2049);
  seq=records[0].seq;assert(!log_append(2050,1,2050,0));assert(!eventlog_init());assert(eventlog_next_seq()==seq+2);
 }
 puts("PASS partial erase of oldest sector at full-ring wrap preserves latest records");
 /* Torn very first record; no old committed record exists. */
 reset_media();assert(!eventlog_init());memcpy(backup,flash,sizeof(flash));
 for(k=4096;k<4096+32;k++){
  memcpy(flash,backup,sizeof(flash));cut=-1;assert(!eventlog_init());
  if(!setjmp(reboot)){cut=(int)k;(void)log_append(1,1,1,0);}cut=-1;
  assert(!eventlog_init());assert(!log_append(2,1,2,0));assert(!log_read_last(1,records,&n)&&n==1&&records[0].seq==1);
 }
 puts("PASS torn first log entry recovery; reserved sector never mutated");
 /* Corrupt newest config payload: earlier committed bank must win. */
 reset_media();assert(kv_load(&c)==1);c.temp_x10=510;assert(!kv_save(&c));
 flash[FLASH_KV_ADDR_A+KV_RECORD_SIZE+16]^=1u;
 assert(!kv_load(&c)&&c.seq==2&&c.temp_x10==500);
 /* Force near-wrap sequence in both banks with valid CRC. */
 reset_media();assert(kv_load(&c)==1);c.seq=0xfffffffeu;c.crc32=storage_crc32(&c,offsetof(config_t,crc32));
 memcpy(flash+FLASH_KV_ADDR_B,&c,sizeof(c));c.seq=0xfffffffdu;c.crc32=storage_crc32(&c,offsetof(config_t,crc32));memcpy(flash,&c,sizeof(c));
 assert(!kv_load(&c)&&c.seq==0xfffffffeu);assert(!kv_save(&c)&&c.seq==0xffffffffu);assert(!kv_save(&c)&&c.seq==0u);assert(!kv_load(&c)&&c.seq==0u);
 puts("PASS corrupted newest KV falls back; sequence wrap selects newest committed record");
 /* Actual async application: reject until mounted, apply ONLY after pump/commit. */
 reset_media();assert(!storage_setup());assert(storage_set(CFG_TEMP,66)==-1);storage_start();
 {int32_t fields[CFG_FIELD_COUNT];storage_status_t status;event_record_t rec;unsigned nr;
  assert(!storage_get_fields(fields,&status)&&status.ready&&fields[CFG_TEMP]==50);
  assert(!storage_set(CFG_TEMP,66));assert(storage_set(CFG_TEMP,67)==-1);
  assert(!storage_get_fields(fields,&status)&&fields[CFG_TEMP]==50&&status.pending);
  bus_busy=1;for(i=0;i<100;i++)storage_poll();
  assert(!storage_get_fields(fields,&status)&&status.ready&&!status.errors&&status.pending&&fields[CFG_TEMP]==50);
  bus_busy=0;
  storage_poll();assert(!storage_get_fields(fields,&status)&&fields[CFG_TEMP]==66&&!status.pending);
  assert(!bus_held);
  assert(applied_smoke==DEF_SMOKE_CONFIRM_MS);assert(!kv_load(&c)&&c.temp_x10==660);
  assert(!storage_recent(0,&rec,&nr)&&rec.type==EVENT_CONFIG);
  for(i=0;i<APP_STORAGE_QUEUE;i++)assert(!storage_event(EVENT_MANUAL,(int32_t)i,0));
  assert(storage_event(EVENT_MANUAL,99,0)==-1);for(i=0;i<APP_STORAGE_QUEUE;i++)storage_poll();
  for(i=0;i<2100;i++){
   assert(!storage_event(EVENT_MANUAL,(int32_t)i,-(int32_t)i));
   event_reads=0;storage_poll();assert(event_reads==3);
   assert(!log_read_last(APP_STORAGE_RECENT,records,&n));
   for(k=0;k<n;k++){
    assert(!storage_recent(k,&rec,&nr)&&nr==n);
    assert(!memcmp(&rec,&records[k],sizeof(rec)));
   }
   assert(storage_recent((unsigned)n,&rec,&nr)==-1&&nr==n);
  }
  puts("PASS recent cache matches committed Flash after 2100 appends/sector/ring wrap; only 3 event reads per append");
  assert(!storage_set(CFG_TEMP,77));program_error=1;storage_poll();program_error=0;
  assert(!storage_get_fields(fields,&status)&&!status.ready&&status.errors&&fields[CFG_TEMP]==66);
  assert(storage_set(CFG_TEMP,88)==-1);assert(!kv_load(&c)&&c.temp_x10==660);
 }
 puts("PASS actual storage queue: 100 fan waits preserve queued config/ready state, offline/busy/full refusal, commit-only apply, failure retains prior config");
 reset_media();assert(!storage_setup());storage_start();
 for(i=0;i<APP_STORAGE_RECENT+3;i++){assert(!storage_event(EVENT_MANUAL,(int32_t)i,7));storage_poll();}
 {int32_t values[CFG_FIELD_COUNT];storage_status_t s;event_record_t rec;unsigned nr;uint32_t errors;
  assert(!storage_get_fields(values,&s));errors=s.errors;
  for(k=0;k<APP_STORAGE_RECENT;k++)assert(!storage_recent(k,&records[k],&nr));
  assert(!storage_event(EVENT_MANUAL,999,9));event_reads=0;fail_event_read=3;
  storage_poll();fail_event_read=0;
  assert(!storage_get_fields(values,&s)&&!s.ready&&s.errors==errors+1);
  for(k=0;k<APP_STORAGE_RECENT;k++){
   assert(!storage_recent(k,&rec,&nr)&&nr==APP_STORAGE_RECENT);
   assert(!memcmp(&rec,&records[k],sizeof(rec)));
  }
  assert(storage_event(EVENT_MANUAL,1000,0)==-1&&!bus_held);
 }
 puts("PASS latest-event read failure stops writes and preserves the entire previously published cache");
 reset_media();assert(!eventlog_init());
 for(i=0;i<25;i++)assert(!log_append(i,EVENT_MANUAL,(int32_t)i,0));
 assert(!storage_setup());event_reads=0;fail_event_read=FLASH_LOG_SIZE/LOG_ITEM_SIZE+5;
 check_unpublished=1;storage_start();check_unpublished=0;fail_event_read=0;
 {int32_t values[CFG_FIELD_COUNT];storage_status_t s;event_record_t rec;unsigned nr;
  assert(!storage_get_fields(values,&s)&&!s.ready&&s.errors);
  assert(storage_recent(0,&rec,&nr)==-1&&nr==0);
  check_unpublished=1;storage_start();check_unpublished=0;
  assert(!storage_get_fields(values,&s)&&s.ready);
  assert(!log_read_last(APP_STORAGE_RECENT,records,&n)&&n==APP_STORAGE_RECENT);
  for(k=0;k<n;k++){assert(!storage_recent(k,&rec,&nr)&&nr==n);assert(!memcmp(&rec,&records[k],sizeof(rec)));}
 }
 puts("PASS startup publishes no partial cache on read failure; retry loads complete newest20 without Flash I/O under view mutex");
 /* Sparse committed records after torn/corrupt slots: rotation can erase a cached tail. */
 reset_media();
 memcpy(flash+FLASH_LOG_ADDR+125*LOG_ITEM_SIZE,&records[0],sizeof(records[0]));
 memcpy(flash+FLASH_LOG_ADDR+128*LOG_ITEM_SIZE,&records[1],sizeof(records[1]));
 assert(!storage_setup());storage_start();
 for(i=0;i<2;i++){assert(!storage_event(EVENT_MANUAL,400+(int32_t)i,0));storage_poll();}
 assert(!log_read_last(APP_STORAGE_RECENT,records,&n));
 {event_record_t rec;unsigned nr;
  for(k=0;k<n;k++){assert(!storage_recent(k,&rec,&nr)&&nr==n);assert(!memcmp(&rec,&records[k],sizeof(rec)));}
  assert(storage_recent((unsigned)n,&rec,&nr)==-1&&nr==n);
 }
 puts("PASS sparse-media sector rotation trims erased cached records to the committed Flash count");
#if APP_NETWORK_PROVISION
 reset_media();assert(kv_load(&c)==1);
 c.temp_x10=630;c.distance_cm=44;c.smoke_ms=600;c.air_ms=800;c.mode=1;
 strcpy(c.ssid,"old-network");strcpy(c.wifi_password,"old-pass");strcpy(c.device,"old-device");
 memset(c.reserved,0xa5,sizeof(c.reserved));assert(!kv_save(&c));old=c.seq;
 assert(!eventlog_init());assert(!log_append(99,EVENT_MANUAL,49,0));
 assert(!storage_setup());storage_start();assert(!kv_load(&c)&&c.seq==old+1);
 assert(!strcmp(c.ssid,APP_WIFI_SSID)&&!strcmp(c.wifi_password,APP_WIFI_PASSWORD));
 assert(!strcmp(c.device,APP_DEVICE_DEFAULT)&&c.temp_x10==630&&c.distance_cm==44);
 assert(c.smoke_ms==600&&c.air_ms==800&&c.mode==1&&c.reserved[0]==0xa5);
 assert(!log_read_last(2,records,&n)&&n==2&&records[1].parameter==49);
 old=c.seq;storage_start();assert(!kv_load(&c)&&c.seq==old);
 strcpy(c.ssid,"old-network");assert(!kv_save(&c));old=c.seq;
 assert(!storage_setup());program_error=1;storage_start();program_error=0;
 {int32_t values[CFG_FIELD_COUNT];storage_status_t s;
  assert(!storage_get_fields(values,&s)&&!s.ready&&s.errors);
  assert(!kv_load(&c)&&c.seq==old&&!strcmp(c.ssid,"old-network"));}
 puts("PASS local network provisioning: legacy thresholds/mode/reserved/events survive, idempotent reboot, failed commit leaves old KV valid and storage offline");
#endif
#if APP_STORAGE_STRESS_ENABLED
 reset_media();assert(!storage_setup());storage_start();
 extern volatile uint32_t storage_bench_request;
 storage_bench_request=2;
 for(i=0;i<APP_STORAGE_EVENT_STRESS;i++)storage_poll();
 assert(!log_read_last(APP_STORAGE_RECENT,records,&n)&&n==APP_STORAGE_RECENT);
 {event_record_t rec;unsigned nr;
  for(k=0;k<n;k++){assert(!storage_recent(k,&rec,&nr)&&nr==n);assert(!memcmp(&rec,&records[k],sizeof(rec)));}
 }
 storage_bench_request=1;
 for(i=0;i<APP_STORAGE_STRESS_WRITES;i++)storage_poll();
 {int32_t values[CFG_FIELD_COUNT];storage_status_t s;
  assert(!storage_get_fields(values,&s)&&s.ready&&values[CFG_TEMP]==50);
  assert(!kv_load(&c)&&c.temp_x10==500);
 }
 puts("PASS storage bench cache publication: 2304 events match Flash, 500 KV writes restore original threshold");
#endif
 return 0;
}
'''
(OUT/'FreeRTOS.h').write_text('#include <assert.h>\n#define configASSERT(x) assert(x)\n#define pdTRUE 1\n#define pdMS_TO_TICKS(x) (x)\n#define portMAX_DELAY 0xffffffffu\n')
(OUT/'semphr.h').write_text('typedef void* SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutex(void);\nint xSemaphoreTake(void*,unsigned);\nint xSemaphoreGive(void*);\n')
(OUT/'queue.h').write_text('typedef void* QueueHandle_t;\nQueueHandle_t xQueueCreate(unsigned,unsigned);\nint xQueueSend(void*,const void*,unsigned);\nint xQueueReceive(void*,void*,unsigned);\nunsigned uxQueueMessagesWaiting(void*);\n')
(OUT/'test.c').write_text(test)
args=['D:/msys64/ucrt64/bin/gcc.exe','-std=c99','-O1','-Wall','-Wextra','-Werror','-DAPP_NO_LOCAL_SECRETS']
for path in ['tmp/storage-test','Drivers_Device','App','BSP','Drivers_Device/kvstore','Drivers_Device/eventlog','Drivers_Device/w25q64','Middleware/esp8266']:args+=['-I',path]
args+=['tmp/storage-test/test.c','Drivers_Device/kvstore/kvstore.c','Drivers_Device/eventlog/eventlog.c','App/storage.c','-o','tmp/storage-test/test.exe']
subprocess.run(args,cwd=ROOT,env=env,check=True)
subprocess.run([str(OUT/'test.exe')],cwd=ROOT,env=env,check=True)
provision=args+['-DAPP_NETWORK_PROVISION=1','-DAPP_WIFI_SSID="test-network"','-DAPP_WIFI_PASSWORD="test-pass"','-DAPP_DEVICE_DEFAULT="test-device"']
subprocess.run(provision,cwd=ROOT,env=env,check=True)
subprocess.run([str(OUT/'test.exe')],cwd=ROOT,env=env,check=True)
stress_config=OUT/'app_config.h'
stress_config.write_text((ROOT/'App/app_config.h').read_text(encoding='utf-8').replace(
    '#define APP_STORAGE_STRESS_ENABLED 0', '#define APP_STORAGE_STRESS_ENABLED 1'),encoding='utf-8')
try:
    subprocess.run(args,cwd=ROOT,env=env,check=True)
    subprocess.run([str(OUT/'test.exe')],cwd=ROOT,env=env,check=True)
finally:
    stress_config.unlink()
