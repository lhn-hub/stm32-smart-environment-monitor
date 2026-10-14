#include "bsp_services.h"
#include "app_config.h"
#include "tim.h"
#include "dht11.h"
#include "cs100a.h"
#include "sg90.h"
#include "tb6612_fan.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

static QueueHandle_t keys;
static TimerHandle_t timer;
static QueueHandle_t edges;
typedef struct { uint8_t raw, stable, long_sent; uint32_t changed, pressed; } key_state_t;
static key_state_t state[3];
static uint32_t effect_started;
static effect_t effect;
static uint32_t dropped;

int rgb_set(uint16_t r, uint16_t g, uint16_t b)
{
    if (__get_IPSR() || r >= RGB_PERIOD || g >= RGB_PERIOD || b >= RGB_PERIOD) return -1;
    taskENTER_CRITICAL();
    if(bsp_is_faulted()){taskEXIT_CRITICAL();return -1;}
    __HAL_TIM_SET_COMPARE(&htim2, RGB_R_CH, r);
    __HAL_TIM_SET_COMPARE(&htim2, RGB_G_CH, g);
    __HAL_TIM_SET_COMPARE(&htim2, RGB_B_CH, b);
    taskEXIT_CRITICAL();
    return 0;
}

int effect_set(effect_t value)
{
    if((unsigned)value>=EFFECT_COUNT || __get_IPSR())return -1;
    taskENTER_CRITICAL();
    if(bsp_is_faulted()){taskEXIT_CRITICAL();return -1;}
    effect=value;effect_started=HAL_GetTick();
    rgb_set(0,0,0);
    taskEXIT_CRITICAL();return 0;
}
static void effects_tick(uint32_t now)
{
    uint32_t phase=now-effect_started;
    uint16_t r=0,g=0,b=0;
    switch(effect){
    case EFFECT_FIRE:r=phase%500u<250u?RGB_ARR:0;break;
    case EFFECT_INTRUSION:r=b=phase%1000u<500u?RGB_ARR:0;break;
    case EFFECT_AIR:r=g=RGB_ARR;break;
    case EFFECT_FAULT:r=g=phase%1000u<500u?RGB_ARR:0;break;
    case EFFECT_NETWORK:phase%=2000u;g=(uint16_t)((phase<=1000u?phase:2000u-phase)*RGB_ARR/1000u);break;
    default:return;
    }
    rgb_set(r,g,b);
}
int bsp_key_get(key_event_t *event)
{
    return event && xQueueReceive(keys, event, 0) == pdTRUE;
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    BaseType_t wake=pdFALSE;
    if(pin==ULTRA_ECHO_PIN){cs100a_edge_irq();return;}
    if(pin==DHT11_PIN){dht11_edge_irq();return;}
    /* ISR only records edges. No delays, printing or flash operations.
     * M1 task/timer samples releases because original EXTI is rising-only.
     */
    if (edges && (pin==KEY_WAKEUP_PIN || pin==KEY_PC13_PIN))
        (void)xQueueSendFromISR(edges,&pin,&wake);
    portYIELD_FROM_ISR(wake);
}

static void emit(unsigned i, key_action_t action, uint32_t now)
{
    key_event_t e = {(key_source_t)i, action, now};
    if (xQueueSend(keys, &e, 0) != pdTRUE) ++dropped;
}

static void controls_tick(TimerHandle_t handle)
{
    uint32_t now = HAL_GetTick();
    GPIO_TypeDef *ports[] = {KEY_WAKEUP_GPIO_PORT, KEY_PC13_GPIO_PORT, KEY_USER_GPIO_PORT};
    const uint16_t pins[] = {KEY_WAKEUP_PIN, KEY_PC13_PIN, KEY_USER_PIN};
    unsigned i;
    uint16_t edge;
    (void)handle;
    if(bsp_is_faulted()) {
        bsp_safe_state(); return;
    }
    /* Consume EXTI events here, never perform debounce in the ISR. Periodic
     * level sampling also catches release and safely recovers dropped edges. */
    while(xQueueReceive(edges,&edge,0)==pdTRUE) { (void)edge; }
    /* Timer service is task context; callbacks never print, block or allocate. */
    for (i=0; i<3; ++i) {
        key_state_t *s = &state[i];
        uint8_t raw = HAL_GPIO_ReadPin(ports[i], pins[i]) == KEY_ACTIVE_LEVEL;
        if (raw != s->raw) { s->raw=raw; s->changed=now; }
        if ((uint32_t)(now-s->changed) >= APP_KEY_DEBOUNCE_MS && raw != s->stable) {
            s->stable=raw;
            if (raw) { s->pressed=now; s->long_sent=0; }
            else if (!s->long_sent) emit(i, KEY_CLICK, now);
        }
        if (s->stable && !s->long_sent && (uint32_t)(now-s->pressed)>=KEY_LONGPRESS_MS) {
            s->long_sent=1; emit(i, KEY_LONG, now);
        }
    }
    servo_poll(now);
    taskENTER_CRITICAL();
    if(bsp_is_faulted()){taskEXIT_CRITICAL();bsp_safe_state();return;}
    if(effect!=EFFECT_STANDBY)effects_tick(now);
    taskEXIT_CRITICAL();
}

int bsp_controls_init(void)
{
    keys=xQueueCreate(APP_KEY_QUEUE,sizeof(key_event_t));
    edges=xQueueCreate(APP_KEY_QUEUE,sizeof(uint16_t));
    timer=xTimerCreate("controls",pdMS_TO_TICKS(KEY_POLL_MS),pdTRUE,NULL,controls_tick);
    if (!keys || !edges || !timer) return -1;
    MX_TIM2_Init();
    if (HAL_TIM_PWM_Start(&htim2,RGB_R_CH)!=HAL_OK ||
        HAL_TIM_PWM_Start(&htim2,RGB_G_CH)!=HAL_OK ||
        HAL_TIM_PWM_Start(&htim2,RGB_B_CH)!=HAL_OK) return -1;
    if(servo_init() || fan_init())return -1;
    effect_set(EFFECT_STANDBY);
    return xTimerStart(timer,0)==pdPASS ? 0:-1;
}
