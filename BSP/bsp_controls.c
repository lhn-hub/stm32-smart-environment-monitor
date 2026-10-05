#include "bsp_services.h"
#include "app_config.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

static QueueHandle_t keys, beeps;
static TimerHandle_t timer;
static QueueHandle_t edges;
typedef struct { uint8_t raw, stable, long_sent; uint32_t changed, pressed; } key_state_t;
static key_state_t state[3];
static beep_pattern_t tone;
static uint32_t tone_deadline, selftest_started;
static uint8_t tone_on, rgb_phase;
static uint32_t dropped;

int rgb_set(uint16_t r, uint16_t g, uint16_t b)
{
    if (bsp_is_faulted() || r >= RGB_PERIOD || g >= RGB_PERIOD || b >= RGB_PERIOD) return -1;
    __HAL_TIM_SET_COMPARE(&htim2, RGB_R_CH, r);
    __HAL_TIM_SET_COMPARE(&htim2, RGB_G_CH, g);
    __HAL_TIM_SET_COMPARE(&htim2, RGB_B_CH, b);
    return 0;
}

int beep_pattern(const beep_pattern_t *pattern)
{
    if (bsp_is_faulted() || !pattern || !pattern->repeat || !pattern->on_ms ||
        pattern->on_ms > APP_BEEP_MAX_MS || pattern->off_ms > APP_BEEP_MAX_MS)
        return -1;
    return xQueueSend(beeps, pattern, 0) == pdTRUE ? 0 : -1;
}
int beep_ms(uint16_t ms)
{
    beep_pattern_t p = {ms, 0, 1};
    return beep_pattern(&p);
}
int bsp_key_get(key_event_t *event)
{
    return event && xQueueReceive(keys, event, 0) == pdTRUE;
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    BaseType_t wake=pdFALSE;
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
    if(bsp_is_faulted()) { tone.repeat=0; bsp_safe_state(); return; }
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
    if (rgb_phase < 4u) {
        uint8_t phase = (uint8_t)((now-selftest_started)/APP_RGB_STEP_MS);
        if (phase != rgb_phase) {
            rgb_phase = phase > 3u ? 4u : phase;
            rgb_set(rgb_phase==0 ? RGB_ARR:0, rgb_phase==1 ? RGB_ARR:0, rgb_phase==2 ? RGB_ARR:0);
        }
    }
    if (!tone.repeat && xQueueReceive(beeps,&tone,0)==pdTRUE) {
        tone_on=1; tone_deadline=now+tone.on_ms;
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT,BUZZER_PIN,BUZZER_ON_LEVEL);
    }
    if (tone.repeat && (int32_t)(now-tone_deadline)>=0) {
        if (tone_on) {
            HAL_GPIO_WritePin(BUZZER_GPIO_PORT,BUZZER_PIN,BUZZER_OFF_LEVEL);
            tone_on=0; --tone.repeat; tone_deadline=now+tone.off_ms;
        } else {
            tone_on=1; tone_deadline=now+tone.on_ms;
            HAL_GPIO_WritePin(BUZZER_GPIO_PORT,BUZZER_PIN,BUZZER_ON_LEVEL);
        }
    }
}

int bsp_controls_init(void)
{
    keys=xQueueCreate(APP_KEY_QUEUE,sizeof(key_event_t));
    edges=xQueueCreate(APP_KEY_QUEUE,sizeof(uint16_t));
    beeps=xQueueCreate(1,sizeof(beep_pattern_t));
    timer=xTimerCreate("controls",pdMS_TO_TICKS(KEY_POLL_MS),pdTRUE,NULL,controls_tick);
    if (!keys || !edges || !beeps || !timer) return -1;
    MX_TIM2_Init();
    if (HAL_TIM_PWM_Start(&htim2,RGB_R_CH)!=HAL_OK ||
        HAL_TIM_PWM_Start(&htim2,RGB_G_CH)!=HAL_OK ||
        HAL_TIM_PWM_Start(&htim2,RGB_B_CH)!=HAL_OK) return -1;
    selftest_started=HAL_GetTick(); rgb_phase=0;
    rgb_set(RGB_ARR,0,0);
    return xTimerStart(timer,0)==pdPASS ? 0:-1;
}
