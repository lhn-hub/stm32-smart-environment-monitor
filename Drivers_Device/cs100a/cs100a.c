#include "cs100a.h"
#include "app_config.h"
#include "sensor_filter.h"
#include "bsp_timer_us.h"
#include "FreeRTOS.h"
#include "task.h"
static volatile uint32_t rising,width,finished;
static volatile uint8_t phase;
static sensor_filter_t filter;
static cs100a_debug_t ultra_debug;
static volatile uint32_t rise_count,fall_count;
const cs100a_debug_t *cs100a_debug(void)
{
    taskENTER_CRITICAL();ultra_debug.rise_irqs=rise_count;ultra_debug.fall_irqs=fall_count;taskEXIT_CRITICAL();
    return &ultra_debug;
}
int cs100a_init(sensor_id_t id)
{(void)id;phase=0;filter.count=filter.next=0;HAL_GPIO_WritePin(ULTRA_TRIG_PORT,ULTRA_TRIG_PIN,GPIO_PIN_RESET);return 0;}
void cs100a_edge_irq(void)
{
    uint32_t now=bsp_micros();uint8_t high=(ULTRA_ECHO_PORT->IDR&ULTRA_ECHO_PIN)!=0;
    if(high)++rise_count;else ++fall_count;
    if(phase==1 && high){rising=now;phase=2;}
    else if(phase==2 && !high){width=now-rising;finished=now;phase=3;}
}
int cs100a_read(sensor_id_t id,sensor_value_t *out)
{
    uint32_t start=bsp_micros(),us=0;unsigned end_phase=0;
    cs100a_error_t why=ULTRA_ERR_NONE;(void)id;
    if(!out)return -1;
    if(HAL_GPIO_ReadPin(ULTRA_ECHO_PORT,ULTRA_ECHO_PIN)!=GPIO_PIN_RESET){why=ULTRA_ERR_IDLE_HIGH;goto fail;}
    taskENTER_CRITICAL();width=0;__HAL_GPIO_EXTI_CLEAR_IT(ULTRA_ECHO_PIN);phase=1;taskEXIT_CRITICAL();
    start=bsp_micros();HAL_GPIO_WritePin(ULTRA_TRIG_PORT,ULTRA_TRIG_PIN,GPIO_PIN_SET);
    if(bsp_delay_us(ULTRA_TRIG_PULSE_US)){why=ULTRA_ERR_TRIGGER;goto fail;}
    HAL_GPIO_WritePin(ULTRA_TRIG_PORT,ULTRA_TRIG_PIN,GPIO_PIN_RESET);
    while(phase!=3 && (uint32_t)(bsp_micros()-start)<ULTRA_ECHO_TIMEOUT_US)vTaskDelay(1);
    taskENTER_CRITICAL();end_phase=phase;us=width;phase=0;taskEXIT_CRITICAL();
    if(end_phase!=3){why=end_phase==2?ULTRA_ERR_NO_FALL:ULTRA_ERR_NO_RISE;goto fail;}
    if(finished-start>ULTRA_ECHO_TIMEOUT_US){why=ULTRA_ERR_LATE;goto fail;}
    if(us<ULTRA_MIN_ECHO_US || us>ULTRA_ECHO_TIMEOUT_US){why=ULTRA_ERR_WIDTH;goto fail;}
    /* Five-sample median in us before integer-cm rounding avoids early quantization. */
    us=(uint32_t)sensor_median(&filter,(int32_t)us);
    out->value=(int32_t)((us+ULTRA_US_PER_CM/2u)/ULTRA_US_PER_CM);
    out->quality=filter.count>=ULTRA_FILTER_N?SENSOR_GOOD:SENSOR_STALE;
    out->timestamp=HAL_GetTick();return 0;
fail:
    ultra_debug.last_error=why;++ultra_debug.errors[why];
    ultra_debug.width_us=us;ultra_debug.wait_us=bsp_micros()-start;
    phase=0;HAL_GPIO_WritePin(ULTRA_TRIG_PORT,ULTRA_TRIG_PIN,GPIO_PIN_RESET);
    filter.count=filter.next=0;return -1;
}
