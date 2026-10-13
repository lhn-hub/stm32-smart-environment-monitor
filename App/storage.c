#include "storage.h"
#include "w25q64.h"
#include "bus_arbiter.h"
#include "bsp_services.h"
#include "datahub.h"
#include "m5_bench.h"
#include "esp8266.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>
typedef struct {uint32_t kind,time;int32_t first,second;} request_t;
static QueueHandle_t requests;
static SemaphoreHandle_t view_lock;
static config_t current_config; /* Sole writer LogTask; never on a256-word task stack. */
static event_record_t recent[APP_STORAGE_RECENT];
static unsigned recent_count;
static int32_t fields[CFG_FIELD_COUNT];
static storage_status_t state;
#if APP_STORAGE_STRESS_ENABLED
/* Bench-only: debugger requests1=500 KV writes,2=2304 events. One op per
 * LogTask iteration keeps normal heartbeat supervision. Never tag test binary. */
volatile uint32_t storage_bench_request;
static uint32_t bench_left,bench_mode;
static int32_t bench_saved_temp;
#endif
static void take_view(void){configASSERT(xSemaphoreTake(view_lock,portMAX_DELAY)==pdTRUE);}
static void publish_fields(void)
{
    take_view();fields[CFG_TEMP]=current_config.temp_x10/10;fields[CFG_DISTANCE]=(int32_t)current_config.distance_cm;
    fields[CFG_SMOKE]=(int32_t)current_config.smoke_ms;fields[CFG_AIR]=(int32_t)current_config.air_ms;fields[CFG_MODE]=(int32_t)current_config.mode;
    state.config_seq=current_config.seq;state.erases=w25q_erases();xSemaphoreGive(view_lock);
}
static void offline(void)
{take_view();state.ready=0;++state.errors;state.pending=0;state.erases=w25q_erases();xSemaphoreGive(view_lock);LOGE("存储故障：保留已确认配置，停止写入，请复位后重扫");}
static int load_recent(void)
{
    size_t n;
    /* Startup keeps recent_count zero until the complete scan is published. */
    if(log_read_last(APP_STORAGE_RECENT,recent,&n))return -1;
    take_view();recent_count=(unsigned)n;state.erases=w25q_erases();xSemaphoreGive(view_lock);
    return 0;
}
static int append_recent(uint32_t time,uint32_t type,int32_t parameter,int32_t auxiliary)
{
    event_record_t record;size_t n;
    if(log_append(time,type,parameter,auxiliary) || log_read_last(1,&record,&n) || n!=1u)return -1;
    take_view();
    if(recent_count<APP_STORAGE_RECENT)++recent_count;
    if(recent_count>eventlog_count())recent_count=(unsigned)eventlog_count();
    memmove(recent+1,recent,(recent_count-1u)*sizeof(*recent));recent[0]=record;
    state.erases=w25q_erases();xSemaphoreGive(view_lock);
    return 0;
}
int storage_setup(void)
{
    requests=xQueueCreate(APP_STORAGE_QUEUE,sizeof(request_t));view_lock=xSemaphoreCreateMutex();
    if(!requests || !view_lock || spi_bus_init())return -1;
    config_defaults(&current_config);publish_fields();return 0;
}
void storage_start(void)
{
    take_view();state.ready=0;recent_count=0;xSemaphoreGive(view_lock);
    LOGI("存储初始化：开始SPI/JEDEC");
    if(w25q_init()){LOGE("Flash JEDEC阶段失败：读到=0x%06lX 期望=0x%06lX",(unsigned long)w25q_id(),(unsigned long)W25Q_JEDEC_ID);offline();return;}
    LOGI("Flash JEDEC=0x%06lX SPI=9MHz",(unsigned long)w25q_id());
    {
        int result=kv_load(&current_config);
        LOGI("存储初始化：KV扫描结果=%d 序号=%lu",(long)result,(unsigned long)current_config.seq);
        if(result<0){LOGE("KV扫描/默认配置写入失败");offline();return;}
    }
#if APP_NETWORK_PROVISION
    /* Explicit local provisioning preserves all non-network KV fields. */
    if(strcmp(current_config.ssid,APP_WIFI_SSID) || strcmp(current_config.wifi_password,APP_WIFI_PASSWORD) ||
       strcmp(current_config.mqtt_host,APP_MQTT_HOST) || strcmp(current_config.mqtt_user,APP_MQTT_USER) ||
       strcmp(current_config.mqtt_password,APP_MQTT_PASSWORD) || strcmp(current_config.device,APP_DEVICE_DEFAULT) ||
       current_config.mqtt_port!=APP_MQTT_PORT || current_config.mqtt_keepalive!=APP_MQTT_KEEPALIVE){
        strcpy(current_config.ssid,APP_WIFI_SSID);strcpy(current_config.wifi_password,APP_WIFI_PASSWORD);
        strcpy(current_config.mqtt_host,APP_MQTT_HOST);strcpy(current_config.mqtt_user,APP_MQTT_USER);
        strcpy(current_config.mqtt_password,APP_MQTT_PASSWORD);strcpy(current_config.device,APP_DEVICE_DEFAULT);
        current_config.mqtt_port=APP_MQTT_PORT;current_config.mqtt_keepalive=APP_MQTT_KEEPALIVE;
        if(kv_save(&current_config)){LOGE("网络配置提交失败");offline();return;}
        LOGI("网络配置已保存 序号=%lu 设备=%s",(unsigned long)current_config.seq,current_config.device);
    }
#endif
    {
        int result=eventlog_init();
        LOGI("存储初始化：事件日志扫描结果=%d 下一序号=%lu",(long)result,(unsigned long)eventlog_next_seq());
        if(result){LOGE("事件日志扫描失败");offline();return;}
    }
    if(datahub_set_gas_limits(current_config.smoke_ms,current_config.air_ms)){LOGE("存储初始化：阈值热加载失败");offline();return;}
    publish_fields();
    if(log_append(rtc_get(),EVENT_BOOT,0,0)){LOGE("存储初始化：启动事件写入失败");offline();return;}
    if(load_recent()){LOGE("存储初始化：最近事件读取失败");offline();return;}
    take_view();state.ready=1;xSemaphoreGive(view_lock);
    LOGI("存储就绪 KV序号=%lu 最近事件=%u 擦除=%lu",(unsigned long)current_config.seq,recent_count,(unsigned long)w25q_erases());
}
int storage_get_fields(int32_t v[CFG_FIELD_COUNT],storage_status_t *s)
{
    if(!view_lock || !v || !s || xSemaphoreTake(view_lock,pdMS_TO_TICKS(APP_STORAGE_LOCK_MS))!=pdTRUE)return -1;
    memcpy(v,fields,sizeof(fields));*s=state;xSemaphoreGive(view_lock);return 0;
}
static int enqueue(const request_t *r,int config)
{
    int result=-1;
    if(!requests || __get_IPSR() || xSemaphoreTake(view_lock,pdMS_TO_TICKS(APP_STORAGE_LOCK_MS))!=pdTRUE)return -1;
    if(state.ready && (!config || !state.pending)){
        if(xQueueSend(requests,r,0)==pdTRUE){if(config)state.pending=1;result=0;}else ++state.dropped;
    }
    xSemaphoreGive(view_lock);return result;
}
int storage_set(unsigned field,int32_t value)
{
    request_t r={0,rtc_get(),(int32_t)field,value};
    if(field>=CFG_FIELD_COUNT || value<0 || (field==CFG_TEMP&&value>APP_EDIT_MAX_TEMP) ||
       (field==CFG_DISTANCE&&value>APP_EDIT_MAX_DISTANCE) ||
       ((field==CFG_SMOKE||field==CFG_AIR)&&value>APP_EDIT_MAX_CONFIRM_MS) || (field==CFG_MODE&&value>1))return -1;
    return enqueue(&r,1);
}
int storage_event(uint32_t type,int32_t param,int32_t aux)
{request_t r={type,rtc_get(),param,aux};return type?enqueue(&r,0):-1;}
int storage_recent(unsigned index,event_record_t *r,unsigned *count)
{
    int result=-1;
    if(!view_lock || !r || !count || xSemaphoreTake(view_lock,pdMS_TO_TICKS(APP_STORAGE_LOCK_MS))!=pdTRUE)return -1;
    *count=recent_count;if(index<recent_count){*r=recent[index];result=0;}xSemaphoreGive(view_lock);return result;
}
int storage_get_network_text(unsigned field,char *out,size_t capacity)
{
    const char *src=NULL;
    if(!out || capacity<2u || !view_lock ||
       xSemaphoreTake(view_lock,pdMS_TO_TICKS(APP_STORAGE_LOCK_MS))!=pdTRUE)return -1;
    if(!state.ready){xSemaphoreGive(view_lock);return -1;}
    switch(field){
    case STORAGE_NET_SSID:src=current_config.ssid;break;
    case STORAGE_NET_PASSWORD:src=current_config.wifi_password;break;
    case STORAGE_NET_HOST:src=current_config.mqtt_host;break;
    case STORAGE_NET_USER:src=current_config.mqtt_user;break;
    case STORAGE_NET_MQTT_PASSWORD:src=current_config.mqtt_password;break;
    case STORAGE_NET_DEVICE:src=current_config.device;break;
    default:xSemaphoreGive(view_lock);return -1;
    }
    if(strlen(src)>=capacity){xSemaphoreGive(view_lock);return -1;}
    strcpy(out,src);xSemaphoreGive(view_lock);return 0;
}
int storage_get_network_numbers(uint32_t *port,uint32_t *keepalive)
{
    if(!port || !keepalive || !view_lock ||
       xSemaphoreTake(view_lock,pdMS_TO_TICKS(APP_STORAGE_LOCK_MS))!=pdTRUE)return -1;
    if(!state.ready){xSemaphoreGive(view_lock);return -1;}
    *port=current_config.mqtt_port;*keepalive=current_config.mqtt_keepalive;
    xSemaphoreGive(view_lock);return 0;
}
void storage_poll(void)
{
    request_t r;int ready;
    take_view();ready=state.ready;xSemaphoreGive(view_lock);
    if(!ready)return;
    /* No idle pin churn; a queued request remains queued until the bus is ours. */
#if !APP_STORAGE_STRESS_ENABLED
    if(!uxQueueMessagesWaiting(requests))return;
#endif
    if(spi_bus_take(APP_SPI_LOCK_MS))return;
#if APP_STORAGE_STRESS_ENABLED
    if(!bench_left && (storage_bench_request==1 || storage_bench_request==2)){
        bench_mode=storage_bench_request;storage_bench_request=0;
        bench_left=bench_mode==1?APP_STORAGE_STRESS_WRITES:APP_STORAGE_EVENT_STRESS;
        bench_saved_temp=current_config.temp_x10;
    }
    if(bench_left){
        if(bench_mode==1){
            current_config.temp_x10=bench_left==1?bench_saved_temp:(int32_t)(bench_left%APP_EDIT_MAX_TEMP)*10;
            if(kv_save(&current_config)){offline();bench_left=0;goto done;}
            publish_fields();
        }else if(append_recent(rtc_get(),EVENT_MANUAL,(int32_t)bench_left,0)){offline();bench_left=0;goto done;}
        if(--bench_left==0)LOGI("存储压力测试结束 模式=%lu",(unsigned long)bench_mode);
        goto done;
    }
#endif
    if(xQueueReceive(requests,&r,0)!=pdTRUE)goto done;
#if APP_M5_STRESS_ENABLED
    if(r.kind==EVENT_BUS_TEST)m5_bench_storage_hook((unsigned)r.second);
#endif
    if(!r.kind){
        switch(r.first){case CFG_TEMP:current_config.temp_x10=r.second*10;break;case CFG_DISTANCE:current_config.distance_cm=(uint32_t)r.second;break;
        case CFG_SMOKE:current_config.smoke_ms=(uint32_t)r.second;break;case CFG_AIR:current_config.air_ms=(uint32_t)r.second;break;
        case CFG_MODE:current_config.mode=(uint32_t)r.second;break;default:offline();goto done;}
        if(kv_save(&current_config)){offline();goto done;}
        if(datahub_set_gas_limits(current_config.smoke_ms,current_config.air_ms)){offline();goto done;}
        publish_fields();
        LOGI("配置已保存 序号=%lu 字段=%ld 值=%ld",(unsigned long)current_config.seq,(long)r.first,(long)r.second);
        take_view();state.pending=0;xSemaphoreGive(view_lock);
        r.kind=EVENT_CONFIG;
    }
    if(append_recent(r.time,r.kind,r.first,r.second))offline();
    else network_event_enqueue(r.kind,r.first,r.second,r.time);
done:
    spi_bus_give();
}
