#include "sg90.h"
#include "app_config.h"
#include "bsp_services.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"
static volatile unsigned target,current;
static uint32_t last;
int servo_init(void)
{
    MX_TIM1_Init();target=current=0;last=HAL_GetTick();
    {
        int result=-1;
        taskENTER_CRITICAL();
        if(!bsp_is_faulted())result=HAL_TIM_PWM_Start(&SERVO_TIM,SERVO_CH)==HAL_OK?0:-1;
        taskEXIT_CRITICAL();return result;
    }
}
int servo_write(unsigned angle)
{
    if(angle>180u || __get_IPSR())return -1;
    taskENTER_CRITICAL();
    if(bsp_is_faulted()){taskEXIT_CRITICAL();return -1;}
    target=angle;taskEXIT_CRITICAL();return 0;
}
unsigned servo_angle(void){return current;}
void servo_poll(uint32_t now)
{
    taskENTER_CRITICAL();
    if(!bsp_is_faulted() && now-last>=SERVO_SLEW_MS_PER_DEG){
        last=now;
        if(current<target)++current;else if(current>target)--current;
        __HAL_TIM_SET_COMPARE(&SERVO_TIM,SERVO_CH,
            SERVO_CCR_MIN+((SERVO_CCR_MAX-SERVO_CCR_MIN)*current+90u)/180u);
    }
    taskEXIT_CRITICAL();
}
