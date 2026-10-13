"""Compile actual M3 C against deterministic GPIO/time/RTOS boundaries; no board claims."""
from pathlib import Path
import os, subprocess

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'tmp/m3-test';OUT.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ);ENV['PATH']='D:/msys64/ucrt64/bin;'+ENV['PATH']
(OUT/'stm32f1xx_hal.h').write_text(r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t IDR;} GPIO_TypeDef;
typedef struct {int unused;} TIM_HandleTypeDef;
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
extern GPIO_TypeDef gpio_a,gpio_b,gpio_c;
typedef struct {uint32_t IMR;} EXTI_TypeDef;
extern EXTI_TypeDef exti;
#define EXTI (&exti)
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOC (&gpio_c)
#define GPIO_PIN_RESET 0u
#define GPIO_PIN_SET 1u
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_OUTPUT_OD 1u
#define GPIO_MODE_IT_RISING_FALLING 2u
#define GPIO_PULLUP 1u
#define GPIO_SPEED_FREQ_LOW 0u
#define __HAL_GPIO_EXTI_CLEAR_IT(p) ((void)(p))
uint32_t HAL_GetTick(void);
unsigned HAL_GPIO_ReadPin(GPIO_TypeDef *,unsigned);
void HAL_GPIO_WritePin(GPIO_TypeDef *,unsigned,unsigned);
void HAL_GPIO_Init(GPIO_TypeDef *,GPIO_InitTypeDef *);
''' + ''.join(f'#define GPIO_PIN_{i} (1u<<{i})\n' for i in range(16))+ '#endif\n')
(OUT/'FreeRTOS.h').write_text(r'''
#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#endif
''')
(OUT/'task.h').write_text('void vTaskDelay(unsigned ticks);\n')
(OUT/'semphr.h').write_text('typedef void *SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutex(void);\nint xSemaphoreTake(SemaphoreHandle_t,unsigned);\nint xSemaphoreGive(SemaphoreHandle_t);\n')
common=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "sensor.h"
GPIO_TypeDef gpio_a,gpio_b,gpio_c;
EXTI_TypeDef exti;
static uint32_t now_us,now_ms;
uint32_t HAL_GetTick(void){return now_ms;}
uint32_t bsp_micros(void){return now_us;}
unsigned HAL_GPIO_ReadPin(GPIO_TypeDef *p,unsigned pin){return (p->IDR&pin)?1u:0u;}
void HAL_GPIO_WritePin(GPIO_TypeDef *p,unsigned pin,unsigned v){if(v)p->IDR|=pin;else p->IDR&=~pin;}
void bsp_log(const char *level,const char *format,...){(void)level;(void)format;}
'''
def run(name,code,enable_mq=False):
    (OUT/(name+'.c')).write_text(common+code,encoding='utf-8')
    args=['D:/msys64/ucrt64/bin/gcc.exe','-std=c99','-Wall','-Wextra','-Werror','-I','tmp/m3-test']
    for p in ['App','BSP','Drivers_Device','Drivers_Device/dht11','Drivers_Device/cs100a','Drivers_Device/mq_gas','Drivers_Device/radar_rd03','Drivers_Device/mcu_internal','Middleware/filter']:
        args+=['-I',p]
    args+=['tmp/m3-test/'+name+'.c','-o','tmp/m3-test/'+name+'.exe']
    subprocess.run(args,cwd=ROOT,env=ENV,check=True)
    subprocess.run([str(OUT/(name+'.exe'))],cwd=ROOT,env=ENV,check=True)

run('pulse_drivers',r'''
#include "dht11.h"
#include "cs100a.h"
static uint16_t fixture[DHT11_CAPTURE_WIDTHS];
static unsigned simulate_dht,simulate_echo,stretch_start,echo_stuck,trigger_fail;
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g){(void)p;if(g->Mode==GPIO_MODE_IT_RISING_FALLING)EXTI->IMR|=g->Pin;}
void vTaskDelay(unsigned ticks);
int bsp_delay_us(uint32_t us){now_us+=us;return trigger_fail?-1:0;}
#include "../../Middleware/filter/sensor_filter.c"
#include "../../Drivers_Device/dht11/dht11.c"
#include "../../Drivers_Device/cs100a/cs100a.c"
void vTaskDelay(unsigned ticks){
 unsigned i;
 if(stretch_start && ticks>1u)ticks+=20u;
 now_us+=ticks*1000u;now_ms+=ticks;
 if(active && simulate_dht){
  gpio_b.IDR&=~DHT11_PIN;dht11_edge_irq();
  for(i=0;i<DHT11_CAPTURE_WIDTHS;i++){now_us+=fixture[i];gpio_b.IDR^=DHT11_PIN;dht11_edge_irq();}
 }
 if(phase==1 && simulate_echo){
  gpio_b.IDR|=GPIO_PIN_3;cs100a_edge_irq();now_us+=simulate_echo;
  if(!echo_stuck){gpio_b.IDR&=~GPIO_PIN_3;cs100a_edge_irq();}
 }
}
static void make_frame(uint8_t *bytes){unsigned i;fixture[0]=83;fixture[1]=87;
 for(i=0;i<40;i++){fixture[2+2*i]=54;fixture[3+2*i]=(bytes[i/8]&(0x80u>>(i%8)))?71:24;}}
int main(void){
 assert(ULTRA_TRIG_PORT==&gpio_a && ULTRA_TRIG_PIN==GPIO_PIN_15);
 assert(ULTRA_ECHO_PORT==&gpio_b && ULTRA_ECHO_PIN==GPIO_PIN_3);
 uint8_t bytes[5]={55,0,26,0,81};int32_t t,h;sensor_value_t v;unsigned i;uint32_t before;
 make_frame(bytes);
 for(i=0;i<50;i++){assert(!dht11_decode(fixture,82,&t,&h));assert(t==260&&h==550);}
 assert(dht11_decode(fixture,81,&t,&h)==-1);fixture[3]=49;assert(dht11_decode(fixture,82,&t,&h)==-1);
 bytes[4]++;make_frame(bytes);assert(dht11_decode(fixture,82,&t,&h)==-1);bytes[4]--;make_frame(bytes);
 now_ms=2000;now_us=2000000;simulate_dht=1;dht11_init(SENSOR_DHT11);
 memset(&v,0,sizeof(v));assert(!dht11_read(SENSOR_DHT11,&v));assert(v.value==260&&v.auxiliary==550&&v.quality==SENSOR_GOOD);assert(!(EXTI->IMR&DHT11_PIN));
 fixture[3]=49;assert(dht11_read(SENSOR_DHT11,&v)==-1);
 assert(dht11_debug()->last_error==DHT_ERR_BIT_WIDTH && dht11_debug()->bad_index==3 && dht11_debug()->bad_width_us==49);
 make_frame(bytes);bytes[4]++;make_frame(bytes);assert(dht11_read(SENSOR_DHT11,&v)==-1);
 assert(dht11_debug()->last_error==DHT_ERR_CHECKSUM);bytes[4]--;make_frame(bytes);
 fixture[0]=200;assert(dht11_read(SENSOR_DHT11,&v)==-1);assert(dht11_debug()->last_error==DHT_ERR_RESPONSE_WIDTH);
 fixture[0]=201;assert(dht11_read(SENSOR_DHT11,&v)==-1);assert(dht11_debug()->last_error==DHT_ERR_EDGE_GAP);make_frame(bytes);
 stretch_start=1;assert(dht11_read(SENSOR_DHT11,&v)==-1);
 assert(dht11_debug()->last_error==DHT_ERR_START_LONG && dht11_debug()->start_ms>30u);
 stretch_start=0;assert(!dht11_read(SENSOR_DHT11,&v));
 assert(dht11_debug()->last_error==DHT_ERR_START_LONG); /* success preserves last failure */
 simulate_dht=0;before=now_ms;assert(dht11_read(SENSOR_DHT11,&v)==-1);assert(now_ms-before<40);assert(dht11_debug()->last_error==DHT_ERR_NO_RESPONSE);
 for(i=0;i<3;i++){unsigned j;const unsigned cm[]={10,30,50};cs100a_init(SENSOR_DISTANCE);simulate_echo=cm[i]*58;for(j=0;j<5;j++){assert(!cs100a_read(SENSOR_DISTANCE,&v));}assert(v.value==(int32_t)cm[i]);}
 cs100a_init(SENSOR_DISTANCE);simulate_echo=1740;
 for(i=0;i<5;i++){assert(!cs100a_read(SENSOR_DISTANCE,&v));}assert(v.value==30&&v.quality==SENSOR_GOOD);
 simulate_echo=0;before=now_ms;assert(cs100a_read(SENSOR_DISTANCE,&v)==-1);assert(now_ms-before<=31);assert(cs100a_debug()->last_error==ULTRA_ERR_NO_RISE);
 gpio_b.IDR|=GPIO_PIN_3;assert(cs100a_read(SENSOR_DISTANCE,&v)==-1);
 assert(cs100a_debug()->last_error==ULTRA_ERR_IDLE_HIGH);
 gpio_b.IDR&=~GPIO_PIN_3;
 trigger_fail=1;assert(cs100a_read(SENSOR_DISTANCE,&v)==-1);assert(cs100a_debug()->last_error==ULTRA_ERR_TRIGGER);trigger_fail=0;
 simulate_echo=100;assert(cs100a_read(SENSOR_DISTANCE,&v)==-1);assert(cs100a_debug()->last_error==ULTRA_ERR_WIDTH);
 simulate_echo=30000;assert(cs100a_read(SENSOR_DISTANCE,&v)==-1);assert(cs100a_debug()->last_error==ULTRA_ERR_LATE);
 simulate_echo=1740;echo_stuck=1;assert(cs100a_read(SENSOR_DISTANCE,&v)==-1);assert(cs100a_debug()->last_error==ULTRA_ERR_NO_FALL);
 echo_stuck=0;gpio_b.IDR&=~GPIO_PIN_3;
 puts("PASS diagnostics: DHT start/no-response/bit/response/checksum/gap and ultrasonic idle/trigger/no-rise/no-fall/late/width");
 phase=1;now_us=0xfffffff0u;gpio_b.IDR|=GPIO_PIN_3;cs100a_edge_irq();now_us+=1740;
 gpio_b.IDR&=~GPIO_PIN_3;cs100a_edge_irq();assert(phase==3&&width==1740);
 puts("PASS actual DHT/ultrasonic: valid frame, checksum, malformed pulse, interrupt capture, unplug timeout, median30cm, uint32 wrap");return 0;
}
''')
run('gpio_filter',r'''
#include "../../Middleware/filter/sensor_filter.c"
#include "../../Drivers_Device/mq_gas/mq_gas.c"
#include "../../Drivers_Device/radar_rd03/radar_rd03.c"
int main(void){
 sensor_value_t v={0};sensor_switch_t sw;sensor_filter_t f={0};sensor_limits_t limits={3000,10000};
 now_us=0;assert(!mq_init(SENSOR_SMOKE));assert(!mq_read(SENSOR_SMOKE,&v));
 assert(v.quality==SENSOR_FAULT && (v.flags&SENSOR_DISABLED) && v.warmup_remaining_ms==APP_MQ_WARMUP_MS);
 assert(!mq_ioctl(SENSOR_SMOKE,SENSOR_IOCTL_LIMITS,&limits));limits.confirm_ms=60001;assert(mq_ioctl(SENSOR_SMOKE,SENSOR_IOCTL_LIMITS,&limits)==-1);
 sensor_switch_init(&sw,0,0);assert(!sensor_switch_update(&sw,1,100,3000,10000));
 assert(!sensor_switch_update(&sw,1,3099,3000,10000));assert(sensor_switch_update(&sw,1,3100,3000,10000));
 assert(sensor_switch_update(&sw,0,3200,3000,10000));assert(sensor_switch_update(&sw,0,13199,3000,10000));
 assert(!sensor_switch_update(&sw,0,13200,3000,10000));
 sensor_switch_init(&sw,0,0xfffffff0u);sensor_switch_update(&sw,1,0xfffffff0u,30,30);assert(sensor_switch_update(&sw,1,20,30,30));
 sensor_median(&f,10);sensor_median(&f,30);sensor_median(&f,500);sensor_median(&f,29);assert(sensor_median(&f,31)==30);
 now_ms=100;radar_init(SENSOR_RADAR);gpio_b.IDR|=RADAR_PIN;radar_read(SENSOR_RADAR,&v);assert(v.quality==SENSOR_STALE);
 now_ms=1100;radar_read(SENSOR_RADAR,&v);assert(v.value==1&&v.quality==SENSOR_GOOD);
 gpio_b.IDR&=~RADAR_PIN;now_ms=1200;radar_read(SENSOR_RADAR,&v);assert(v.value==1);
 now_ms=2200;radar_read(SENSOR_RADAR,&v);assert(v.value==0);
 puts("PASS actual MQ guard/filter/Rd03: disabled unsafe DO, bounds, warmup metadata, assert/release times, wrap, median,1s debounce");return 0;
}
''')
run('mq_enabled_logic_only',r'''
/* Override ONLY in a host simulation. Hardware production gate remains zero. */
#undef APP_MQ_DO_ENABLED
#define APP_MQ_DO_ENABLED 1
#include "../../Middleware/filter/sensor_filter.c"
#include "../../Drivers_Device/mq_gas/mq_gas.c"
int main(void){
 sensor_value_t v={0};sensor_limits_t l={3000,10000};now_us=0;
 mq_init(SENSOR_SMOKE);MQ_SMOKE_PORT->IDR&=~MQ_SMOKE_PIN;now_ms=100;
 assert(!mq_read(SENSOR_SMOKE,&v));assert(v.value==0&&v.quality==SENSOR_STALE&&v.auxiliary==0);
 now_ms=APP_MQ_WARMUP_MS;mq_read(SENSOR_SMOKE,&v);assert(!v.value&&!(v.flags&SENSOR_WARMUP));
 now_ms+=2999;mq_read(SENSOR_SMOKE,&v);assert(!v.value);now_ms++;mq_read(SENSOR_SMOKE,&v);assert(v.value==1);
 MQ_SMOKE_PORT->IDR|=MQ_SMOKE_PIN;mq_read(SENSOR_SMOKE,&v);assert(v.auxiliary==1&&v.value==1);
 now_ms+=10000;mq_read(SENSOR_SMOKE,&v);assert(v.value==0);
 l.confirm_ms=1000;mq_ioctl(SENSOR_SMOKE,SENSOR_IOCTL_LIMITS,&l);MQ_SMOKE_PORT->IDR&=~MQ_SMOKE_PIN;
 mq_read(SENSOR_SMOKE,&v);now_ms+=1000;mq_read(SENSOR_SMOKE,&v);assert(v.value==1);
 now_ms=20;mq_read(SENSOR_SMOKE,&v);assert(v.warmup_remaining_ms==0);
 puts("PASS host-only enabled MQ path: raw DO, warmup suppression, confirm/release, runtime config, no repeated warmup after tick wrap");return 0;
}
''')
run('mcu',r'''
static uint32_t mv=3300,stamp;
int mcu_temp_get(int32_t *v){*v=APP_TEMP_FAULT_VALUE;return -1;}
void bsp_adc_snapshot(uint32_t *v,uint32_t *s){*v=mv;*s=stamp;}
#include "../../Drivers_Device/mcu_internal/mcu_internal.c"
int main(void){sensor_value_t v={0};now_us=0;stamp=100;now_ms=100;
 assert(!mcu_sensor_read(SENSOR_MCU,&v));assert(v.quality==SENSOR_FAULT&&v.auxiliary==3300&&v.auxiliary_quality==SENSOR_GOOD);
 now_ms+=APP_MCU_STALE_MS+1;mcu_sensor_read(SENSOR_MCU,&v);assert(v.auxiliary_quality==SENSOR_STALE);
 mv=0;mcu_sensor_read(SENSOR_MCU,&v);assert(v.auxiliary_quality==SENSOR_FAULT);
 puts("PASS MCU wrapper: temperature FAULT preserved, VDDA auxiliary GOOD/STALE/FAULT");return 0;}
''')
run('registry_hub',r'''
#include "datahub.h"
static unsigned fail,blocked;
void *xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(void *p,unsigned wait){(void)p;(void)wait;return !blocked;}
int xSemaphoreGive(void *p){(void)p;return 1;}
static int mock_init(sensor_id_t id){(void)id;return 0;}
static int mock_read(sensor_id_t id,sensor_value_t *v){
 if(fail&&(id==SENSOR_DHT11||id==SENSOR_DISTANCE))return -1;
 v->value=42;v->quality=SENSOR_GOOD;v->timestamp=HAL_GetTick();return 0;
}
#define dht11_init mock_init
#define cs100a_init mock_init
#define mq_init mock_init
#define radar_init mock_init
#define mcu_sensor_init mock_init
#define dht11_read mock_read
#define cs100a_read mock_read
#define mq_read mock_read
#define radar_read mock_read
#define mcu_sensor_read mock_read
#include "dht11.h"
#include "cs100a.h"
const dht11_debug_t *dht11_debug(void){static dht11_debug_t d;return &d;}
const cs100a_debug_t *cs100a_debug(void){static cs100a_debug_t u;return &u;}
int mq_ioctl(sensor_id_t id,unsigned cmd,void *p){(void)id;(void)cmd;(void)p;return 0;}
#include "../../App/datahub.c"
#include "../../Drivers_Device/sensor.c"
int main(void){
 sensor_value_t v;datahub_snapshot_t snap;sensor_diag_t d;unsigned i;size_t count;uint32_t original;
 now_us=0;now_ms=0;assert(!datahub_init());sensors_init();sensors_poll();
 assert(!sensor_ioctl(SENSOR_DHT11,SENSOR_IOCTL_DIAG,&d));assert(d.attempts==0);
 now_ms=DHT11_POWERUP_MS-1;sensors_poll();sensor_ioctl(SENSOR_DHT11,SENSOR_IOCTL_DIAG,&d);assert(d.attempts==0);
 now_ms=DHT11_POWERUP_MS;sensors_poll();sensor_ioctl(SENSOR_DHT11,SENSOR_IOCTL_DIAG,&d);assert(d.attempts==1&&d.successes==1);
 now_ms=5000;sensors_poll();
 assert(sensor_registry(&count)&&count==SENSOR_COUNT);
 for(i=0;i<SENSOR_COUNT;i++){assert(!sensor_read((sensor_id_t)i,&v));assert(v.value==42&&v.quality==SENSOR_GOOD);}
 original=v.timestamp;fail=1;
 for(i=1;i<=10;i++){now_ms+=DHT11_PERIOD_MS;sensors_poll();sensor_read(SENSOR_DHT11,&v);
   assert(v.timestamp==original);assert(v.quality==(i<10?SENSOR_STALE:SENSOR_FAULT));}
 sensor_read(SENSOR_DISTANCE,&v);assert(v.quality==SENSOR_FAULT);assert(!sensor_ioctl(SENSOR_DHT11,SENSOR_IOCTL_DIAG,&d));assert(d.consecutive_failures==10);
 fail=0;now_ms+=DHT11_PERIOD_MS;sensors_poll();sensor_read(SENSOR_DHT11,&v);assert(v.quality==SENSOR_GOOD&&v.timestamp==now_ms);
 now_ms+=DHT11_PERIOD_MS*APP_SENSOR_STALE_MULTIPLIER+1;assert(!datahub_snapshot(&snap));assert(snap.sensors[SENSOR_DHT11].quality==SENSOR_STALE);
 blocked=1;assert(datahub_snapshot(&snap)==-1);blocked=0;
 assert(sensor_read(SENSOR_COUNT,&v)==-1);assert(sensor_read(SENSOR_DHT11,NULL)==-1);
 puts("PASS actual registry/DataHub: six sensor_read entries, failure limits, timestamp retention, recovery, stale snapshots, mutex failure, invalid ID");return 0;
}
''')
