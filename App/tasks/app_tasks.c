#include "bsp_services.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <stdio.h>
int bsp_adc_start(void);
int bsp_rtc_start(void);
int bsp_net_start(void);
int bsp_controls_init(void);
static EventGroupHandle_t heartbeats;
static volatile uint32_t stamps[APP_TASK_COUNT];
static volatile uint32_t ready;
static volatile uint32_t watchdog_tripped;
static TaskHandle_t handles[APP_TASK_COUNT];
#if APP_WATCHDOG_TEST_ENABLED
/* Dedicated bench build only: debugger sets ID 1..5 AFTER watchdog is active.
 * Production build has no injection variable or branch. Never tag this build.
 */
volatile uint32_t m1_stall_task=APP_WATCHDOG_TEST_NONE;
#endif
static const char *names[APP_TASK_COUNT]={"MonitorTask","AlarmTask","NetTask","SensorTask","MenuTask","LogTask"};
static const uint16_t stacks[APP_TASK_COUNT]={APP_MONITOR_STACK,APP_ALARM_STACK,APP_NET_STACK,APP_SENSOR_STACK,APP_MENU_STACK,APP_LOG_STACK};
static const uint8_t priorities[APP_TASK_COUNT]={APP_MONITOR_PRIORITY,APP_ALARM_PRIORITY,APP_NET_PRIORITY,APP_SENSOR_PRIORITY,APP_MENU_PRIORITY,APP_LOG_PRIORITY};

static void worker(void *argument)
{
    unsigned id=(unsigned)(uintptr_t)argument;
    uint32_t last_status=0,last_tx=0,boot=HAL_GetTick();
    int started=0,wd=0;
    for(;;) {
        uint32_t now=HAL_GetTick();
#if APP_WATCHDOG_TEST_ENABLED
        if(id!=0 && m1_stall_task==id) vTaskSuspend(NULL);
#endif
        if(!started) {
            if(id==2 && bsp_net_start()) bsp_panic("USART3 init");
            if(id==3) {
                if(bsp_adc_start()) bsp_panic("ADC init");
                if(bsp_rtc_start()) LOGE("RTC init failed");
                i2c_scan();
            }
            if(id==4 && bsp_controls_init()) bsp_panic("controls init");
            started=1;
            /* Only short RAM bookkeeping in critical sections. BASEPRI masks
             * priority>=5; TIM4=4 still runs. No IO/delay/printf while masked.
             */
            taskENTER_CRITICAL(); ready|=1u<<id; taskEXIT_CRITICAL();
        }
        stamps[id]=HAL_GetTick();
        xEventGroupSetBits(heartbeats,1u<<id);
        if(id==0) {
            const EventBits_t all=(1u<<APP_TASK_COUNT)-1u;
            unsigned i; int healthy=1;
            if(ready!=all && now-boot>APP_STARTUP_LIMIT_MS) bsp_panic("startup timeout");
            if(!wd && ready==all) {
                if(bsp_watchdog_start()) bsp_panic("IWDG init");
                wd=1;
            }
            if(wd) {
                for(i=0;i<APP_TASK_COUNT;i++) if(now-stamps[i]>APP_HEARTBEAT_MS) healthy=0;
                if(!healthy) { watchdog_tripped=1; bsp_emergency_stop(); }
                if(!watchdog_tripped && (xEventGroupGetBits(heartbeats)&all)==all) {
                    xEventGroupClearBits(heartbeats,all); bsp_watchdog_feed();
                }
            }
        }
        if(id==2) {
            bsp_net_poll();
            if(now-last_tx>=APP_NET_TEST_MS) {
                last_tx=now;
                if(bsp_net_send(APP_NET_TEST_LINE,sizeof(APP_NET_TEST_LINE)-1u)) LOGW("USART3 TX busy");
            }
        }
        if(id==4) {
            key_event_t e;
            while(bsp_key_get(&e)) {
                LOGI("KEY src=%u action=%s tick=%lu",(unsigned)e.source,
                     e.action==KEY_LONG?"LONG":"CLICK",(unsigned long)e.tick);
                if(e.source==KEY_CONFIRM && e.action==KEY_CLICK) beep_ms(APP_KEY_DEBOUNCE_MS);
            }
        }
        if(now-last_status>=APP_STATUS_MS) {
            unsigned free=(unsigned)uxTaskGetStackHighWaterMark(NULL);
            last_status=now;
            LOGI("alive stack_free=%u/%u (%u%%)",free,stacks[id],free*100u/stacks[id]);
            if(id==3) LOGI("MCU_TEMP=FAULT calibration=disabled VDDA=%lumV nominal RTC=%lu",
                (unsigned long)vdda_mv_get(),(unsigned long)rtc_get());
            if(id==0) LOGI("heap_free=%u watchdog_trip=%lu",(unsigned)xPortGetFreeHeapSize(),(unsigned long)watchdog_tripped);
        }
        vTaskDelay(pdMS_TO_TICKS(id==2?APP_NET_POLL_MS:id==4?APP_MENU_PERIOD_MS:APP_MONITOR_PERIOD_MS));
    }
}
void app_start(void)
{
    unsigned i;
    if(bsp_services_init()) bsp_panic("services init");
    heartbeats=xEventGroupCreate();
    if(!heartbeats) bsp_panic("heartbeat allocation");
    for(i=0;i<APP_TASK_COUNT;i++)
        if(xTaskCreate(worker,names[i],stacks[i],(void *)(uintptr_t)i,priorities[i],&handles[i])!=pdPASS)
            bsp_panic("task allocation");
    vTaskStartScheduler(); bsp_panic("scheduler returned");
}
void vApplicationMallocFailedHook(void) { bsp_panic("malloc failed"); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name)
{ (void)task; (void)name; bsp_panic("stack overflow"); }
