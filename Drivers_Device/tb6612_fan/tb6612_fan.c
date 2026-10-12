/* Shared-pin hardware contract.
 * A channel must never drive. Never initialize TIM3_CH1/PWMA.
 * CS=0 also means STBY=0: TB6612 is in standby during SPI transfers.
 * CS=1: SCK/AIN1 idle low; W25Q releases MISO/PWMA, whose nominal
 * TB6612 internal 200k pull-down holds it low. With PWMA low, neither
 * value of PB0/AIN2 is intended to produce differential motor drive.
 * This steady-state rationale is NOT measured proof of switching safety.
 * bus_arbiter give: PA4/STBY=0, PA7/BIN1=0, PA5/SCK idle=0, FAN CCR=0.
 * M4/M5 must record full-speed SPI stress and AO1/AO2 differential voltage
 * or safely observed A-channel shaft motion. The B fan alone cannot verify A.
 */
#include "tb6612_fan.h"
#include "bus_arbiter.h"
#include "app_config.h"
#include "bsp_services.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"
static volatile unsigned target,current;
static unsigned applied,origin;
static uint32_t ramp_started;
static int held;
int fan_init(void)
{
    target=applied=origin=current=0;
    {
        int result=-1;
        taskENTER_CRITICAL();
        if(!bsp_is_faulted())result=HAL_TIM_PWM_Start(&FAN_TIM,FAN_CH)==HAL_OK?0:-1;
        taskEXIT_CRITICAL();return result;
    }
}
int fan_set(unsigned percent)
{
    if(percent>100u || __get_IPSR())return -1;
    taskENTER_CRITICAL();
    if(bsp_is_faulted()){taskEXIT_CRITICAL();return -1;}
    target=percent;
    if(!percent)TIM3->CCR4=0;
    taskEXIT_CRITICAL();return 0;
}
unsigned fan_percent(void){return (current*100u+(FAN_ARR+1u)/2u)/(FAN_ARR+1u);}
void fan_poll(uint32_t now)
{
    unsigned wanted,ccr;uint32_t elapsed;
    taskENTER_CRITICAL();wanted=bsp_is_faulted()?0:target;taskEXIT_CRITICAL();
    if(!wanted){
        if(held){fan_bus_give();held=0;}
        current=applied=0;return;
    }
    if(!held){
        if(fan_bus_take(APP_ACTUATOR_POLL_MS))return;
        held=1;current=applied=0;
    }
    now=HAL_GetTick();
    taskENTER_CRITICAL();
    wanted=bsp_is_faulted()?0:target;
    if(wanted && wanted!=applied){applied=wanted;origin=current;ramp_started=now;}
    elapsed=now-ramp_started;if(elapsed>FAN_RAMP_MS)elapsed=FAN_RAMP_MS;
    ccr=applied*(FAN_ARR+1u)/100u;
    if(wanted){
        current=(unsigned)((int32_t)origin+((int32_t)ccr-(int32_t)origin)*(int32_t)elapsed/(int32_t)FAN_RAMP_MS);
        TIM3->CCR4=current;
    }
    taskEXIT_CRITICAL();
    if(!wanted){fan_bus_give();held=0;current=applied=0;}
}
