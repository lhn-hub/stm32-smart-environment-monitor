/* Host-only boundary test: gcc -std=c99 -I BSP Docs/test_micros.c -o tmp/test_micros.exe */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define __BSP_PIN_H
#define BSP_US_PER_MS 1000u
#define BSP_US_BUSYWAIT_LIMIT 1000u
#define TIM_SR_UIF 1u
static struct { uint32_t CNT, SR; } timer;
#define BSP_US_TIM (&timer)
static uint32_t tick, calls, mode, ipsr, primask, basepri;
static uint32_t HAL_GetTick(void)
{
    ++calls;
    if (mode == 1u && calls == 2u) { ++tick; timer.CNT=0; }
    if (mode == 2u && calls == 2u) { ++tick; timer.SR=0; }
    return tick;
}
static uint32_t __get_IPSR(void) { return ipsr; }
static uint32_t __get_PRIMASK(void) { return primask; }
static uint32_t __get_BASEPRI(void) { return basepri; }
#define __NOP() ((void)0)
#include "../BSP/bsp_timer_us.c"
int main(void)
{
    tick=7; timer.CNT=999; assert(bsp_micros()==7999);
    calls=0; mode=1; assert(bsp_micros()==8000);
    tick=9; timer.CNT=0; timer.SR=1; calls=0; mode=2;
    assert(bsp_micros()==10000 && calls>=4);
    mode=0; tick=4294967u; timer.CNT=999u;
    assert(bsp_micros()==703u);
    assert((uint32_t)(20u-UINT32_C(4294967290))==26u);
    assert(bsp_delay_us(0)==0);
    assert(bsp_delay_us(1000)==-1);
    ipsr=1; assert(bsp_delay_us(12)==-1); ipsr=0;
    primask=1; assert(bsp_delay_us(12)==-1); primask=0;
    basepri=80; assert(bsp_delay_us(12)==-1);
    puts("PASS: stable tick, rollover, pending UIF, uint32 wrap, invalid delay/context");
    return 0;
}
