#include "dht11.h"
#include "app_config.h"
#include "bsp_timer_us.h"
#include "sensor_filter.h"
#include "FreeRTOS.h"
#include "task.h"
static volatile uint16_t widths[DHT11_CAPTURE_WIDTHS];
static volatile uint32_t last_edge;
static volatile unsigned captured;
static volatile uint8_t active,seen_low,level,error;
static sensor_filter_t temperature_filter,humidity_filter;

int dht11_decode(const uint16_t *w,size_t n,int32_t *t,int32_t *h)
{
    uint8_t bytes[5]={0};unsigned i;
    if(!w || !t || !h || n!=DHT11_CAPTURE_WIDTHS)return -1;
    for(i=0;i<2;i++)if(w[i]<DHT11_RESPONSE_MIN_US || w[i]>DHT11_RESPONSE_MAX_US)return -1;
    for(i=0;i<40;i++){
        uint16_t low=w[2u+i*2u],high=w[3u+i*2u];unsigned bit;
        if(low<DHT11_BIT_LOW_MIN_US || low>DHT11_BIT_LOW_MAX_US)return -1;
        if(high>=DHT11_ZERO_MIN_US && high<=DHT11_ZERO_MAX_US)bit=0;
        else if(high>=DHT11_ONE_MIN_US && high<=DHT11_ONE_MAX_US)bit=1;
        else return -1;
        bytes[i/8u]=(uint8_t)((bytes[i/8u]<<1)|bit);
    }
    if((uint8_t)(bytes[0]+bytes[1]+bytes[2]+bytes[3])!=bytes[4])return -1;
    if(bytes[1]>9u || (bytes[3]&0x7fu)>9u)return -1;
    *h=(int32_t)bytes[0]*10+bytes[1];
    *t=(int32_t)bytes[2]*10+(bytes[3]&0x7fu);
    if(bytes[3]&0x80u)*t=-*t;
    if(*h>DHT11_HUM_MAX_X10 || *t<DHT11_TEMP_MIN_X10 || *t>DHT11_TEMP_MAX_X10)return -1;
    return 0;
}
static void input_mode(void)
{
    GPIO_InitTypeDef g={0};g.Pin=DHT11_PIN;g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_PULLUP;
    /* F1 HAL_GPIO_Init(INPUT) does not disable an already configured EXTI. */
    EXTI->IMR&=~DHT11_PIN;__HAL_GPIO_EXTI_CLEAR_IT(DHT11_PIN);
    HAL_GPIO_Init(DHT11_PORT,&g);
}
int dht11_init(sensor_id_t id)
{(void)id;active=0;temperature_filter.count=humidity_filter.count=0;temperature_filter.next=humidity_filter.next=0;input_mode();return 0;}
void dht11_edge_irq(void)
{
    uint8_t next;uint32_t now,dt;
    if(!active)return;
    next=(DHT11_PORT->IDR&DHT11_PIN)?1u:0u;now=bsp_micros();
    if(!seen_low){if(!next){seen_low=1;level=0;last_edge=now;}return;}
    if(next==level)return;
    dt=now-last_edge;level=next;last_edge=now;
    if(dt>DHT11_EDGE_MAX_US || captured>=DHT11_CAPTURE_WIDTHS){error=1;active=0;return;}
    widths[captured++]=(uint16_t)dt;
    if(captured==DHT11_CAPTURE_WIDTHS)active=0;
}
int dht11_read(sensor_id_t id,sensor_value_t *out)
{
    GPIO_InitTypeDef g={0};uint32_t start;int32_t t,h;int ok;
    (void)id;
    if(!out || HAL_GetTick()<DHT11_POWERUP_MS)return -1;
    /* PB12 replaces unused EC11 EXTI12. Never mask full3~5ms response:
     * TIM4 priority4 remains active; short setup critical section only.
     * No delay/print/flash in critical section or ISR. */
    active=0;HAL_GPIO_WritePin(DHT11_PORT,DHT11_PIN,GPIO_PIN_RESET);
    g.Pin=DHT11_PIN;g.Mode=GPIO_MODE_OUTPUT_OD;g.Speed=GPIO_SPEED_FREQ_LOW;g.Pull=GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_PORT,&g);start=HAL_GetTick();
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_LOW_MS)+1u);
    if(HAL_GetTick()-start>DHT11_START_MAX_MS){input_mode();return -1;}
    taskENTER_CRITICAL();
    captured=0;error=seen_low=0;active=1;
    g.Mode=GPIO_MODE_IT_RISING_FALLING;HAL_GPIO_Init(DHT11_PORT,&g);
    __HAL_GPIO_EXTI_CLEAR_IT(DHT11_PIN);
    taskEXIT_CRITICAL();
    start=bsp_micros();
    while(active && (uint32_t)(bsp_micros()-start)<DHT11_CAPTURE_TIMEOUT_US)vTaskDelay(1);
    taskENTER_CRITICAL();active=0;taskEXIT_CRITICAL();
    input_mode();
    ok=!error && dht11_decode((const uint16_t *)widths,captured,&t,&h)==0;
    if(!ok){temperature_filter.count=humidity_filter.count=0;temperature_filter.next=humidity_filter.next=0;return -1;}
    out->value=sensor_mean(&temperature_filter,t);out->auxiliary=sensor_mean(&humidity_filter,h);
    out->quality=out->auxiliary_quality=SENSOR_GOOD;out->timestamp=HAL_GetTick();return 0;
}
