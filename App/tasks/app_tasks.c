#include "bsp_services.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include <stdio.h>
#include "menu.h"
#include "sensor.h"
#include "datahub.h"
#include "storage.h"
#include "tb6612_fan.h"
#include "m5_bench.h"
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
    int started=0,wd=0,ui_started=0;
    for(;;) {
        uint32_t now=HAL_GetTick();
#if APP_WATCHDOG_TEST_ENABLED
        if(id!=0 && m1_stall_task==id) vTaskSuspend(NULL);
#endif
        if(!started) {
            if(id==5 && !(ready&(1u<<3))){vTaskDelay(pdMS_TO_TICKS(APP_MONITOR_PERIOD_MS));continue;}
            if(id==5)storage_start();
            if(id==2 && bsp_net_start()) bsp_panic("串口3初始化失败");
            if(id==3) {
                if(bsp_adc_start()) bsp_panic("ADC初始化失败");
                if(bsp_rtc_start()) LOGE("RTC初始化失败");
                i2c_scan();
                sensors_init();
            }
            if(id==4 && bsp_controls_init()) bsp_panic("控制服务初始化失败");
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
            if(ready!=all && now-boot>APP_STARTUP_LIMIT_MS) bsp_panic("启动超时");
            if(!wd && ready==all) {
                if(bsp_watchdog_start()) bsp_panic("看门狗初始化失败");
                wd=1;
            }
            if(wd) {
                /* Sample AFTER initialization/heartbeat writes; stale loop 'now'
                 * can precede a stamp and underflow into a false watchdog fault. */
                uint32_t checked=HAL_GetTick();
                for(i=0;i<APP_TASK_COUNT;i++) if(checked-stamps[i]>APP_HEARTBEAT_MS) healthy=0;
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
                if(bsp_net_send(APP_NET_TEST_LINE,sizeof(APP_NET_TEST_LINE)-1u)) LOGW("串口3发送忙");
            }
        }
        if(id==4) {
            key_event_t e;
            if(!ui_started && (ready&(1u<<3))) { menu_init(); ui_started=1; }
            while(bsp_key_get(&e)) {
                LOGI("按键=%u 动作=%s 时刻=%lu",(unsigned)e.source,
                     e.action==KEY_LONG?"长按":"单击",(unsigned long)e.tick);
                if(ui_started) menu_key(&e);
            }
        }
        if(id==5)storage_poll();
        if(id==1 && (ready&((1u<<4)|(1u<<5)))==((1u<<4)|(1u<<5))){
            fan_poll(now);
#if APP_M5_STRESS_ENABLED
            m5_bench_poll();
#endif
        }
        if(id==3)sensors_poll();
        if(id==4 && ui_started) menu_poll();
        if(now-last_status>=APP_STATUS_MS) {
            unsigned free=(unsigned)uxTaskGetStackHighWaterMark(NULL);
            last_status=now;
            LOGI("运行正常 栈剩余=%u/%u字 (%u%%)",free,stacks[id],free*100u/stacks[id]);
            if(id==3)sensors_log_table();
            if(id==0) LOGI("堆剩余=%u字节 看门狗故障=%lu",(unsigned)xPortGetFreeHeapSize(),(unsigned long)watchdog_tripped);
        }
        vTaskDelay(pdMS_TO_TICKS(id==1?APP_ACTUATOR_POLL_MS:id==2?APP_NET_POLL_MS:id==4?APP_MENU_PERIOD_MS:id==3?APP_SENSOR_POLL_MS:APP_MONITOR_PERIOD_MS));
    }
}
void app_start(void)
{
    unsigned i;
    if(bsp_services_init()) bsp_panic("板级服务初始化失败");
    if(datahub_init())bsp_panic("DataHub初始化失败");
    if(storage_setup())bsp_panic("存储队列初始化失败");
    heartbeats=xEventGroupCreate();
    if(!heartbeats) bsp_panic("心跳内存分配失败");
    for(i=0;i<APP_TASK_COUNT;i++)
        if(xTaskCreate(worker,names[i],stacks[i],(void *)(uintptr_t)i,priorities[i],&handles[i])!=pdPASS)
            bsp_panic("任务创建失败");
    vTaskStartScheduler(); bsp_panic("调度器启动失败");
}
void vApplicationMallocFailedHook(void) { bsp_panic("内存分配失败"); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name)
{ (void)task; (void)name; bsp_panic("任务栈溢出"); }
