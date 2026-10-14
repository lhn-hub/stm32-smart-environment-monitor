/* Host simulation of the actual controls implementation, not board evidence. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define __BSP_PIN_H
#define __TIM_H__
#define INC_FREERTOS_H
#define INC_TASK_H
#define QUEUE_H
#define TIMERS_H
#include "app_config.h"
typedef int BaseType_t;
typedef unsigned GPIO_TypeDef;
typedef void *TimerHandle_t;
typedef struct { unsigned size,count,head,tail,capacity; uint8_t data[256]; } *QueueHandle_t;
static unsigned pins[3],now,pwm[3],buzz;
static int htim2;
static int fault;
int servo_init(void){return 0;}
int fan_init(void){return 0;}
void servo_poll(uint32_t tick){(void)tick;}
unsigned __get_IPSR(void){return 0;}
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
void ec11_irq(void) {}
void dht11_edge_irq(void) {}
void cs100a_edge_irq(void) {}
#define DHT11_PIN 4096u
#define ULTRA_ECHO_PIN 8u
#define EC11_A_PIN 16u
#define EC11_B_PIN 32u
int bsp_is_faulted(void) { return fault; }
void bsp_safe_state(void) { buzz=0; memset(pwm,0,sizeof(pwm)); }
#define KEY_WAKEUP_GPIO_PORT ((GPIO_TypeDef*)0)
#define KEY_PC13_GPIO_PORT ((GPIO_TypeDef*)1)
#define KEY_USER_GPIO_PORT ((GPIO_TypeDef*)2)
#define KEY_WAKEUP_PIN 1u
#define KEY_PC13_PIN 2u
#define KEY_USER_PIN 4u
#define KEY_ACTIVE_LEVEL 1u
#define KEY_POLL_MS 10u
#define KEY_LONGPRESS_MS 1500u
#define RGB_PERIOD 1000u
#define RGB_ARR 999u
#define RGB_R_CH 0u
#define RGB_G_CH 1u
#define RGB_B_CH 2u
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define HAL_OK 0
#define pdMS_TO_TICKS(x) (x)
#define portYIELD_FROM_ISR(x) ((void)(x))
#define __HAL_TIM_SET_COMPARE(t,c,v) ((void)(t),pwm[c]=(v))
static uint32_t HAL_GetTick(void) { return now; }
static unsigned HAL_GPIO_ReadPin(GPIO_TypeDef *p,unsigned pin)
{ (void)pin; return pins[(uintptr_t)p]; }
static void MX_TIM2_Init(void) { }
static int HAL_TIM_PWM_Start(void *t,unsigned c) { (void)t;(void)c;return 0; }
static QueueHandle_t xQueueCreate(unsigned n,unsigned size)
{ QueueHandle_t q=calloc(1,sizeof(*q));q->size=size;q->capacity=n;return q; }
static int xQueueSend(QueueHandle_t q,const void *p,unsigned timeout)
{ (void)timeout;if(q->count==q->capacity)return 0;memcpy(q->data+q->tail*q->size,p,q->size);q->tail=(q->tail+1)%q->capacity;q->count++;return 1; }
static int xQueueReceive(QueueHandle_t q,void *p,unsigned timeout)
{ (void)timeout;if(!q->count)return 0;memcpy(p,q->data+q->head*q->size,q->size);q->head=(q->head+1)%q->capacity;q->count--;return 1; }
static int xQueueSendFromISR(QueueHandle_t q,const void *p,BaseType_t *wake)
{ *wake=0;return xQueueSend(q,p,0); }
static TimerHandle_t xTimerCreate(const char *n,unsigned t,int reload,void *id,void (*cb)(TimerHandle_t))
{ (void)n;(void)t;(void)reload;(void)id;(void)cb;return (void*)1; }
static int xTimerStart(TimerHandle_t t,unsigned wait) { (void)t;(void)wait;return 1; }
#include "../BSP/bsp_controls.c"
static void step(unsigned ms) { now+=ms;controls_tick(NULL); }
int main(void)
{
    key_event_t e; unsigned i;
    assert(!bsp_controls_init()); assert(pwm[0]+pwm[1]+pwm[2]==0&&!buzz);
    assert(!effect_set(EFFECT_FIRE));step(10);assert(pwm[0]==999);
    step(100);step(150);assert(!pwm[0]);step(250);assert(pwm[0]==999);
    assert(!effect_set(EFFECT_INTRUSION));step(10);assert(pwm[0]==999&&pwm[2]==999);
    step(500);assert(!pwm[0]&&!pwm[2]);step(1000);
    assert(!effect_set(EFFECT_AIR));step(10);assert(pwm[0]==999&&pwm[1]==999&&!buzz);
    assert(!effect_set(EFFECT_FAULT));step(10);assert(pwm[0]==999&&pwm[1]==999);step(500);assert(!pwm[0]&&!pwm[1]);
    assert(!effect_set(EFFECT_NETWORK));step(500);assert(pwm[1]==499);step(500);assert(pwm[1]==999);step(1000);assert(!pwm[1]);
    now=UINT32_MAX-100;assert(!effect_set(EFFECT_FIRE));step(400);assert(!pwm[0]);
    assert(effect_set(EFFECT_COUNT)==-1);assert(!effect_set(EFFECT_STANDBY));step(10);assert(!buzz&&!pwm[0]&&!pwm[1]&&!pwm[2]);
    for(i=0;i<3;i++) {
        pins[i]=1;step(10);pins[i]=0;step(10);assert(!bsp_key_get(&e));
        pins[i]=1;step(10);step(30);step(100);pins[i]=0;step(10);step(30);
        assert(bsp_key_get(&e) && e.source==(key_source_t)i && e.action==KEY_CLICK);
        pins[i]=1;step(10);step(30);step(1490);assert(!bsp_key_get(&e));step(10);
        assert(bsp_key_get(&e) && e.source==(key_source_t)i && e.action==KEY_LONG);
        step(1500);assert(!bsp_key_get(&e));pins[i]=0;step(10);step(30);assert(!bsp_key_get(&e));
    }
    assert(rgb_set(1000,0,0)==-1);
    fault=1;assert(rgb_set(1,1,1)==-1);step(10);assert(!buzz);
    assert(effect_set(EFFECT_FIRE)==-1);
    puts("PASS actual controls: safe startup, six light effects/timing/wrap/fault, debounce/click/1500ms long/no release click");
    free(keys);free(edges);return 0;
}
