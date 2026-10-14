"""Regression for vendor PA15/TRIG, PB3/ECHO wiring: run actual GPIO init/ISRs.

Expected pins are independently fixed from the vendor module and board documents;
they are not generated from the firmware macros being tested.
"""
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tmp/ultra-mapping'
OUT.mkdir(parents=True, exist_ok=True)


def function(path, name):
    source = (ROOT / path).read_text(encoding='utf-8')
    signature = 'void ' + name + '('
    if signature not in source:
        return 'void ' + name + '(void) { assert(!"missing IRQ handler"); }\n'
    start = source.index(signature)
    pos = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos] + '\n'


header = r'''
#ifndef MAPPING_HAL_H
#define MAPPING_HAL_H
#include <stdint.h>
typedef struct {unsigned index;} GPIO_TypeDef;
typedef struct {int unused;} TIM_HandleTypeDef;
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
extern GPIO_TypeDef ports[4];
#define GPIOA (&ports[0])
#define GPIOB (&ports[1])
#define GPIOC (&ports[2])
#define GPIOD (&ports[3])
#define GPIO_PIN_RESET 0u
#define GPIO_PIN_SET 1u
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_OUTPUT_PP 1u
#define GPIO_MODE_IT_RISING 2u
#define GPIO_MODE_IT_RISING_FALLING 3u
#define GPIO_NOPULL 0u
#define GPIO_PULLUP 1u
#define GPIO_PULLDOWN 2u
#define GPIO_SPEED_FREQ_LOW 0u
#define EXTI0_IRQn 0u
#define EXTI3_IRQn 3u
#define EXTI4_IRQn 4u
#define EXTI15_10_IRQn 5u
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE() ((void)0)
void HAL_GPIO_Init(GPIO_TypeDef *,GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(GPIO_TypeDef *,uint32_t,uint32_t);
void HAL_NVIC_SetPriority(unsigned,unsigned,unsigned);
void HAL_NVIC_EnableIRQ(unsigned);
''' + ''.join(f'#define GPIO_PIN_{i} (1u<<{i})\n' for i in range(16)) + '#endif\n'
(OUT / 'stm32f1xx_hal.h').write_text(header)

source = r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "bsp_pin.h"
#define __GPIO_H__
GPIO_TypeDef ports[4]={{0},{1},{2},{3}};
static unsigned mode[4][16],written[4],high[4],owner[16],irq_enabled[6],priority[6];
static unsigned pending,ultra_calls,dht_calls,key_calls;
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint32_t pins,uint32_t value)
{
    assert(!(p==GPIOB && (pins&8u))); /* MCU must never drive physical ECHO. */
    written[p->index]|=pins;
    if(value)high[p->index]|=pins;else high[p->index]&=~pins;
}
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *g)
{
    unsigned i;
    for(i=0;i<16;i++)if(g->Pin&(1u<<i)){
        mode[p->index][i]=g->Mode;
        if(g->Mode==GPIO_MODE_OUTPUT_PP){
            assert(!(p==GPIOB && i==3));
            if(p==GPIOA && i==15)assert((written[0]&0x8000u)&&!(high[0]&0x8000u));
        }
        if(g->Mode==GPIO_MODE_IT_RISING || g->Mode==GPIO_MODE_IT_RISING_FALLING)owner[i]=p->index+1u;
    }
}
void HAL_NVIC_SetPriority(unsigned irq,unsigned prio,unsigned sub)
{assert(irq<6 && sub==0);priority[irq]=prio;}
void HAL_NVIC_EnableIRQ(unsigned irq){assert(irq<6);irq_enabled[irq]=1;}
typedef int BaseType_t;
#define pdFALSE 0
#define portYIELD_FROM_ISR(x) ((void)(x))
static void *edges=(void*)1;
void cs100a_edge_irq(void){ultra_calls++;}
void dht11_edge_irq(void){dht_calls++;}
int xQueueSendFromISR(void *q,const void *p,BaseType_t *wake)
{(void)p;(void)wake;assert(q==edges);key_calls++;return 1;}
void HAL_GPIO_EXTI_Callback(uint16_t pin);
void HAL_GPIO_EXTI_IRQHandler(uint16_t pin)
{if(pending&pin){pending&=~pin;HAL_GPIO_EXTI_Callback(pin);}}
#include "../../Src/gpio.c"
'''
source += function('BSP/bsp_controls.c', 'HAL_GPIO_EXTI_Callback')
source += function('Src/stm32f1xx_it.c', 'EXTI3_IRQHandler')
source += function('Src/stm32f1xx_it.c', 'EXTI15_10_IRQHandler')
source += r'''
int main(void)
{
    assert(ULTRA_TRIG_PORT==GPIOA && ULTRA_TRIG_PIN==0x8000u);
    assert(ULTRA_ECHO_PORT==GPIOB && ULTRA_ECHO_PIN==8u);
    MX_GPIO_Init();
    assert(RADAR_PORT==GPIOB && RADAR_PIN==16u);
    assert(mode[1][4]==GPIO_MODE_INPUT && !(written[1]&16u));
    assert(mode[1][5]==GPIO_MODE_INPUT && !(written[1]&32u));
    assert(mode[0][15]==GPIO_MODE_OUTPUT_PP);
    assert(mode[1][3]==GPIO_MODE_IT_RISING_FALLING && owner[3]==2);
    assert(owner[15]==0); /* PA15 no longer mapped to EXTI. */
    assert(irq_enabled[EXTI3_IRQn] && priority[EXTI3_IRQn]==5);
    assert(irq_enabled[EXTI15_10_IRQn] && priority[EXTI15_10_IRQn]==5);
    pending=8u|4096u|8192u;
    EXTI15_10_IRQHandler();
    assert(dht_calls==1 && key_calls==1 && ultra_calls==0 && pending==8u);
    EXTI3_IRQHandler();assert(ultra_calls==1 && pending==0);
    EXTI3_IRQHandler();assert(ultra_calls==1); /* No duplicate event without PR. */
    puts("PASS vendor mapping: PA15 output-low, PB3 input-only, EXTI3 priority5, DHT/PC13 dispatch preserved");
    return 0;
}
'''
(OUT / 'mapping.c').write_text(source, encoding='utf-8')
env = dict(os.environ)
env['PATH'] = 'D:/msys64/ucrt64/bin;' + env['PATH']
subprocess.run(['D:/msys64/ucrt64/bin/gcc.exe', '-std=c99', '-Wall', '-Wextra', '-Werror',
                '-I', 'tmp/ultra-mapping', '-I', 'BSP', '-I', 'Inc',
                'tmp/ultra-mapping/mapping.c', '-o', 'tmp/ultra-mapping/mapping.exe'],
               cwd=ROOT, env=env, check=True)
subprocess.run([str(OUT / 'mapping.exe')], cwd=ROOT, env=env, check=True)
ioc = dict(line.split('=', 1) for line in (ROOT/'sms-hub.ioc').read_text(encoding='utf-8').splitlines() if '=' in line)
assert ioc['PA15.Signal'] == 'GPIO_Output'
assert ioc['PB3.Signal'] == 'GPXTI3'
assert ioc['PB3.GPIO_ModeDefaultEXTI'] == 'GPIO_MODE_IT_RISING_FALLING'
assert ioc['NVIC.EXTI3_IRQn'].startswith('true\\:5\\:0')
assert 'SH.GPXTI15.0' not in ioc
assert 'void EXTI3_IRQHandler(void);' in (ROOT/'Inc/stm32f1xx_it.h').read_text()
print('PASS CubeMX/header: corrected pin roles, EXTI3 enabled, obsolete EXTI15 mapping removed')
