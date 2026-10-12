"""Exercise actual DHT driver with an open-drain bus and timed EXTI events."""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tmp/dht-handshake'
OUT.mkdir(parents=True, exist_ok=True)

(OUT / 'stm32f1xx_hal.h').write_text(r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t IDR,ODR;} GPIO_TypeDef;
typedef struct {int unused;} TIM_HandleTypeDef;
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
extern GPIO_TypeDef gpio_a,gpio_b,gpio_c;
typedef struct {uint32_t IMR,PR;} EXTI_TypeDef;
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
#define __HAL_GPIO_EXTI_CLEAR_IT(p) (EXTI->PR&=~(p))
uint32_t HAL_GetTick(void);
unsigned HAL_GPIO_ReadPin(GPIO_TypeDef *,unsigned);
void HAL_GPIO_WritePin(GPIO_TypeDef *,unsigned,unsigned);
void HAL_GPIO_Init(GPIO_TypeDef *,GPIO_InitTypeDef *);
''' + ''.join(f'#define GPIO_PIN_{i} (1u<<{i})\n' for i in range(16)) + '#endif\n')
(OUT / 'FreeRTOS.h').write_text(r'''
#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#define pdMS_TO_TICKS(x) (x)
void enter_critical(void);
void exit_critical(void);
#define taskENTER_CRITICAL() enter_critical()
#define taskEXIT_CRITICAL() exit_critical()
#endif
''')
(OUT / 'task.h').write_text('void vTaskDelay(unsigned ticks);\n')
(OUT / 'handshake.c').write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "dht11.h"
GPIO_TypeDef gpio_a,gpio_b,gpio_c;
EXTI_TypeDef exti;
static uint64_t clock_us=2000000,low_since,event_at;
static unsigned mode,host_low,sensor_low,connected=1,critical,exti_configured;
static unsigned response_delay=13,setup_stall,event_index,event_count,releases;
static unsigned stuck_low,irq_blocked,stretch_start;
static uint16_t frame[DHT11_CAPTURE_WIDTHS];
uint32_t HAL_GetTick(void){return (uint32_t)(clock_us/1000u);}
uint32_t bsp_micros(void){return (uint32_t)clock_us;}
unsigned HAL_GPIO_ReadPin(GPIO_TypeDef *p,unsigned pin){return !!(p->IDR&pin);}
static void dispatch(void)
{
    if(!critical && !irq_blocked && (EXTI->IMR&EXTI->PR&DHT11_PIN)){
        __HAL_GPIO_EXTI_CLEAR_IT(DHT11_PIN);
        dht11_edge_irq();
    }
}
static void update_bus(void)
{
    unsigned was=!!(gpio_b.IDR&DHT11_PIN);
    if(host_low || sensor_low || stuck_low)gpio_b.IDR&=~DHT11_PIN;
    else gpio_b.IDR|=DHT11_PIN; /* Module's external 10k pull-up. */
    if(was!=!!(gpio_b.IDR&DHT11_PIN) && exti_configured)EXTI->PR|=DHT11_PIN;
    dispatch();
}
static void update_host(void)
{
    unsigned next=mode==GPIO_MODE_OUTPUT_OD && !(gpio_b.ODR&DHT11_PIN);
    if(next && !host_low)low_since=clock_us;
    if(!next && host_low){
        uint64_t duration=clock_us-low_since;
        if(connected && duration>=18000u && duration<=30000u){
            ++releases;event_index=0;event_count=84;event_at=clock_us+response_delay;
        }
    }
    host_low=next;update_bus();
}
static void advance(unsigned us)
{
    uint64_t target=clock_us+us;
    while(event_count && event_at<=target){
        clock_us=event_at;sensor_low^=1u;update_bus();
        if(event_index<82u)event_at+=frame[event_index];
        else event_at+=54u;
        ++event_index;--event_count;
    }
    clock_us=target;dispatch();
}
void enter_critical(void){++critical;}
void exit_critical(void){assert(critical);--critical;dispatch();}
void HAL_GPIO_WritePin(GPIO_TypeDef *p,unsigned pin,unsigned v)
{
    assert(p==GPIOB && pin==4096u);
    if(v)p->ODR|=pin;else p->ODR&=~pin;
    update_host();
}
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g)
{
    assert(p==GPIOB && g->Pin==4096u);
    /* F1 HAL sets ODR high BEFORE mode/AFIO/EXTI when configuring input pull-up. */
    if(g->Mode!=GPIO_MODE_OUTPUT_OD && g->Pull==GPIO_PULLUP){
        p->ODR|=g->Pin;update_host();
        if(g->Mode==GPIO_MODE_IT_RISING_FALLING)advance(setup_stall);
    }
    mode=g->Mode;update_host();
    if(g->Mode==GPIO_MODE_IT_RISING_FALLING){
        exti_configured=1;EXTI->IMR|=g->Pin;
    }
    dispatch();
}
void vTaskDelay(unsigned ticks)
{
    assert(!critical);
    if(stretch_start && ticks>1u)ticks+=20u;
    advance(ticks*1000u);
}
static void make_frame(void)
{
    const uint8_t bytes[5]={55,0,26,0,81};unsigned i;
    frame[0]=83;frame[1]=87;
    for(i=0;i<40;i++){
        frame[2+2*i]=54;
        frame[3+2*i]=(bytes[i/8]&(0x80u>>(i%8)))?71:24;
    }
}
static void read_good(void)
{
    sensor_value_t v={0};int result=dht11_read(SENSOR_DHT11,&v);
    if(result)fprintf(stderr,"DHT handshake failed: response=%uus setup stall=%uus error=%u widths=%lu\n",
        response_delay,setup_stall,(unsigned)dht11_debug()->last_error,
        (unsigned long)dht11_debug()->captured_widths);
    assert(result==0);
    assert(v.value==260 && v.auxiliary==550 && v.quality==SENSOR_GOOD);
    assert(v.auxiliary_quality==SENSOR_GOOD && v.timestamp==HAL_GetTick());
    assert(!critical && !host_low && !(EXTI->IMR&DHT11_PIN));
    assert(gpio_b.IDR&DHT11_PIN);
}
static void read_missing(unsigned release_level,unsigned irqs)
{
    uint64_t before=clock_us;sensor_value_t v={0};
    v.value=-999;v.timestamp=123;
    assert(dht11_read(SENSOR_DHT11,&v)==-1);
    assert(clock_us-before>=27000u && clock_us-before<40000u);
    assert(v.value==-999 && v.timestamp==123); /* Failure cannot publish fabricated data. */
    assert(!critical && !host_low && !(EXTI->IMR&DHT11_PIN));
    assert(dht11_debug()->last_error==DHT_ERR_NO_RESPONSE);
    assert(dht11_debug()->captured_widths==0);
    assert(dht11_debug()->low_level==0);
    assert(dht11_debug()->released_level==release_level);
    assert(dht11_debug()->end_level==release_level);
    assert(dht11_debug()->irqs==irqs);
}
int main(void)
{
    unsigned i;sensor_value_t v={0};dht11_debug_t failed;
    assert(DHT11_PORT==GPIOB && DHT11_PIN==4096u);
    make_frame();dht11_init(SENSOR_DHT11);read_good();assert(releases==1);
    puts("PASS DHT handshake: physical low/release triggers a complete validated frame");
    advance(DHT11_PERIOD_MS*1000u);
    /* Inject setup latency, not a measured board duration. Earliest legal response=10us. */
    response_delay=10;setup_stall=16;dht11_init(SENSOR_DHT11);
    read_good();assert(releases==2);
    puts("PASS DHT early response: interrupt setup latency cannot discard the first falling edge");
    advance(DHT11_PERIOD_MS*1000u);connected=0;read_missing(1,1);
    puts("PASS DHT unplug: released line high, release IRQ only, bounded NO_RESPONSE");
    advance(DHT11_PERIOD_MS*1000u);stuck_low=1;update_bus();read_missing(0,0);
    puts("PASS DHT stuck-low: release stays low, no IRQ, safe input restored");
    stuck_low=0;update_bus();advance(DHT11_PERIOD_MS*1000u);
    irq_blocked=1;read_missing(1,0);irq_blocked=0;
    puts("PASS DHT missing IRQ: released line high, pending edges, zero callbacks");
    failed=*dht11_debug();connected=1;advance(DHT11_PERIOD_MS*1000u);read_good();
    assert(!memcmp(&failed,dht11_debug(),sizeof(failed)));
    puts("PASS DHT reconnect: fresh valid reading, last failure evidence retained");
    advance(DHT11_PERIOD_MS*1000u);stretch_start=1;
    assert(dht11_read(SENSOR_DHT11,&v)==-1);stretch_start=0;
    assert(dht11_debug()->last_error==DHT_ERR_START_LONG);
    assert(dht11_debug()->start_ms>30u && dht11_debug()->released_level==2u);
    assert(!critical && !host_low && mode==GPIO_MODE_INPUT && !(EXTI->IMR&DHT11_PIN));
    advance(DHT11_PERIOD_MS*1000u);read_good();
    puts("PASS DHT excessive start: bounded failure, bus released, next read recovers");
    for(i=0;i<3;i++){
        const unsigned delays[]={10,13,35};
        advance(DHT11_PERIOD_MS*1000u);response_delay=delays[i];
        EXTI->PR|=DHT11_PIN|GPIO_PIN_13;EXTI->IMR|=GPIO_PIN_13;
        read_good();assert(EXTI->PR&GPIO_PIN_13);assert(EXTI->IMR&GPIO_PIN_13);
    }
    puts("PASS DHT datasheet response limits: 10/13/35us, stale PR cleared, other EXTI lines retained");
    clock_us=0xffffffffu-21500u;read_good();
    puts("PASS DHT capture across uint32 microsecond wrap");
    return 0;
}
''', encoding='utf-8')

env = dict(os.environ)
env['PATH'] = 'D:/msys64/ucrt64/bin;' + env['PATH']
args = ['D:/msys64/ucrt64/bin/gcc.exe', '-std=c99', '-Wall', '-Wextra', '-Werror']
for directory in ['tmp/dht-handshake', 'App', 'BSP', 'Drivers_Device',
                  'Drivers_Device/dht11', 'Middleware/filter']:
    args += ['-I', directory]
args += ['tmp/dht-handshake/handshake.c', 'Drivers_Device/dht11/dht11.c',
         'Middleware/filter/sensor_filter.c', '-o', 'tmp/dht-handshake/handshake.exe']
subprocess.run(args, cwd=ROOT, env=env, check=True)
subprocess.run([str(OUT / 'handshake.exe')], cwd=ROOT, env=env, check=True)
