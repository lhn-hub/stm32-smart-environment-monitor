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
#include "w25q64.h"
#include "tb6612_fan.h"
#include "m5_bench.h"
#include "esp8266.h"
#include "sg90.h"
#include "alarm_engine.h"
#include "app_time.h"
#include "bsp_services.h"
#include "stm32f1xx_hal.h"
int bsp_adc_start(void);
int bsp_rtc_start(void);
int bsp_net_start(void);
int bsp_controls_init(void);
static EventGroupHandle_t heartbeats;
static volatile uint32_t stamps[APP_TASK_COUNT];
static volatile uint32_t ready;
static volatile uint32_t watchdog_tripped;
static TaskHandle_t handles[APP_TASK_COUNT];
static uint8_t reboot_pending;
static uint32_t reboot_at;
static network_command_t saving_command;
static uint32_t saving_sequence;
static uint8_t saving_reply;
#if APP_M8_TEST_ENABLED
volatile unsigned m8_overflow_request;
#endif
#if APP_WATCHDOG_TEST_ENABLED
/* Dedicated bench build only: debugger sets ID 1..5 AFTER watchdog is active.
 * Production build has no injection variable or branch. Never tag this build.
 */
volatile uint32_t m1_stall_task=APP_WATCHDOG_TEST_NONE;
#endif
static const char *names[APP_TASK_COUNT]={"MonitorTask","AlarmTask","NetTask","SensorTask","MenuTask","LogTask"};
static const uint16_t stacks[APP_TASK_COUNT]={APP_MONITOR_STACK,APP_ALARM_STACK,APP_NET_STACK,APP_SENSOR_STACK,APP_MENU_STACK,APP_LOG_STACK};
static const uint8_t priorities[APP_TASK_COUNT]={APP_MONITOR_PRIORITY,APP_ALARM_PRIORITY,APP_NET_PRIORITY,APP_SENSOR_PRIORITY,APP_MENU_PRIORITY,APP_LOG_PRIORITY};

static void network_commands_poll(uint32_t now)
{
    network_command_t command;
    if(saving_reply){
        int32_t fields[CFG_FIELD_COUNT];storage_status_t store;
        if(!storage_get_fields(fields,&store) && (!store.ready || !store.pending)){
            network_command_reply(&saving_command,store.ready && store.config_seq!=saving_sequence &&
                                  fields[saving_command.field]==saving_command.value?
                                  NETWORK_REPLY_DONE:NETWORK_REPLY_REJECTED);
            saving_reply=0;
        }
    }
    if (network_command_pop(&command) == 0) {
        int result = -1;
        unsigned reply = NETWORK_REPLY_REJECTED;
        switch (command.kind) {
        case NETWORK_CMD_ARM:
        case NETWORK_CMD_THRESHOLD:
            {
                int32_t fields[CFG_FIELD_COUNT];storage_status_t store;
                if(!saving_reply && !storage_get_fields(fields,&store)){
                    saving_sequence=store.config_seq;
                    result=command.kind==NETWORK_CMD_ARM?alarm_set_mode((unsigned)command.value):
                           storage_set(command.field,command.value);
                    if(!result){saving_command=command;saving_reply=1;}
                }
            }
            reply = result ? NETWORK_REPLY_REJECTED : NETWORK_REPLY_QUEUED;
            break;
        case NETWORK_CMD_FAN:
            result = alarm_manual_fan((unsigned)command.value);
            reply = result ? NETWORK_REPLY_REJECTED : NETWORK_REPLY_DONE;
            break;
        case NETWORK_CMD_SERVO:
            result = alarm_manual_servo((unsigned)command.value);
            reply = result ? NETWORK_REPLY_REJECTED : NETWORK_REPLY_DONE;
            break;
        case NETWORK_CMD_BUZZER:
            result = alarm_manual_beep((uint16_t)command.value);
            reply = result ? NETWORK_REPLY_REJECTED : NETWORK_REPLY_DONE;
            break;
        case NETWORK_CMD_ACK:
            alarm_acknowledge();result=0;reply=NETWORK_REPLY_DONE;
            break;
        case NETWORK_CMD_TIME:
            result=app_time_request((uint32_t)command.value);
            reply=result?NETWORK_REPLY_REJECTED:NETWORK_REPLY_QUEUED;
            break;
        case NETWORK_CMD_REBOOT:
            reply = NETWORK_REPLY_ACCEPTED;
            reboot_pending = 1;
            reboot_at = now + 1000u;
            result = 0;
            break;
        default:
            break;
        }
        network_command_reply(&command, reply);
    }
}

static void worker(void *argument)
{
    unsigned id=(unsigned)(uintptr_t)argument;
    uint32_t last_status=0,boot=HAL_GetTick();
    int started=0,wd=0,ui_started=0;
#if APP_CRITICAL_AUDIT
    if(id==0)bsp_audit_start();
#endif
    for(;;) {
        uint32_t now=HAL_GetTick();
#if APP_WATCHDOG_TEST_ENABLED
        if(id!=0 && m1_stall_task==id) vTaskSuspend(NULL);
#endif
        if(!started) {
            if(id==5 && !(ready&(1u<<3))){vTaskDelay(pdMS_TO_TICKS(APP_MONITOR_PERIOD_MS));continue;}
            if(id==5)storage_start();
            if(id==2) {
                network_init();
                if(bsp_net_start()) bsp_panic("串口3初始化失败");
            }
            if(id==3) {
                if(bsp_adc_start()) bsp_panic("ADC初始化失败");
                if(bsp_rtc_start()) LOGE("RTC初始化失败");
                else app_time_start();
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
#if APP_M8_TEST_ENABLED
        if(id==0 && m8_overflow_request==1u){
            TaskStatus_t info;volatile StackType_t *base;unsigned j;
            m8_overflow_request=0;
            vTaskGetInfo(NULL,&info,pdFALSE,eInvalid);base=info.pxStackBase;
            /* Corrupt only the real kernel guard; PendSV must detect it. */
            for(j=0;j<4u;j++)base[j]=0;
            taskYIELD();
        }
#endif
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
            network_poll(now);
            network_commands_poll(now);
            if (reboot_pending && (network_reboot_response_sent() ||
                                   (int32_t)(now - reboot_at) >= 0)) {
                reboot_pending = 0;
                NVIC_SystemReset();
            }
        }
        if(id==4) {
            key_event_t e;
            if(!ui_started && (ready&(1u<<3))) { menu_init(); ui_started=1; }
            while(bsp_key_get(&e)) {
                if(ui_started) menu_key(&e);
            }
        }
        if(id==5)storage_poll();
        if(id==1 && (ready&((1u<<4)|(1u<<5)))==((1u<<4)|(1u<<5))){
            alarm_poll(now);
            fan_poll(now);
#if APP_M5_STRESS_ENABLED
            m5_bench_poll();
#endif
        }
        if(id==3){app_time_poll();sensors_poll();}
        if(id==4 && ui_started) menu_poll();
        if(now-last_status>=APP_STATUS_MS) {
            unsigned free=(unsigned)uxTaskGetStackHighWaterMark(NULL);
            last_status=now;
            LOGI("运行正常 栈剩余=%u/%u字 (%u%%)",free,stacks[id],free*100u/stacks[id]);
            if(id==3)sensors_log_table();
            if(id==0) LOGI("堆剩余=%u字节 看门狗故障=%lu",(unsigned)xPortGetFreeHeapSize(),(unsigned long)watchdog_tripped);
        }
        if(id==0){
            static uint32_t report_at;
            if(now-report_at>=APP_BURN_REPORT_MS){
                network_status_t net;unsigned resets,boots;
                report_at=now;network_get_status(&net);bsp_boot_stats(&resets,&boots);
                LOGI("BURN ms=%lu wd=%u boots=%u reconnect=%lu heap=%u min=%u mqtt=%u drop=%lu",
                     (unsigned long)now,resets,boots,(unsigned long)net.reconnects,
                     (unsigned)xPortGetFreeHeapSize(),(unsigned)xPortGetMinimumEverFreeHeapSize(),
                     (unsigned)net.mqtt,(unsigned long)net.dropped);
            }
#if APP_CRITICAL_AUDIT
            bsp_audit_report();
#endif
        }
        if(id==5)w25q_wear_report(now);
        vTaskDelay(pdMS_TO_TICKS(id==1?APP_ACTUATOR_POLL_MS:id==2?APP_NET_POLL_MS:id==4?APP_MENU_PERIOD_MS:id==3?APP_SENSOR_POLL_MS:APP_MONITOR_PERIOD_MS));
    }
}
void app_start(void)
{
    unsigned i;
    if(bsp_services_init()) bsp_panic("板级服务初始化失败");
    if(datahub_init())bsp_panic("DataHub初始化失败");
    alarm_init();
    if(storage_setup())bsp_panic("存储队列初始化失败");
    heartbeats=xEventGroupCreate();
    if(!heartbeats) bsp_panic("心跳内存分配失败");
    for(i=0;i<APP_TASK_COUNT;i++)
        if(xTaskCreate(worker,names[i],stacks[i],(void *)(uintptr_t)i,priorities[i],&handles[i])!=pdPASS)
            bsp_panic("任务创建失败");
    vTaskStartScheduler(); bsp_panic("调度器启动失败");
}
void vApplicationMallocFailedHook(void) { bsp_fault_record(3u,0);bsp_panic("内存分配失败"); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name)
{
    (void)name;
    bsp_fault_record(2u,0);bsp_emergency_stop();
    bsp_fault_hex("STACK task=",(uint32_t)(uintptr_t)task);
    bsp_panic("任务栈溢出");
}
