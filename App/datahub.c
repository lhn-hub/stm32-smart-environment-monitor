#include "datahub.h"
#include "app_config.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>
static datahub_snapshot_t hub;
static SemaphoreHandle_t lock;
static uint32_t smoke_confirm=DEF_SMOKE_CONFIRM_MS,air_confirm=DEF_AIR_CONFIRM_MS;
int datahub_init(void)
{
    unsigned i;lock=xSemaphoreCreateMutex();if(!lock)return -1;
    memset(&hub,0,sizeof(hub));
    for(i=0;i<SENSOR_COUNT;i++){hub.sensors[i].value=hub.sensors[i].auxiliary=APP_SENSOR_UNKNOWN;hub.sensors[i].quality=hub.sensors[i].auxiliary_quality=SENSOR_FAULT;}
    return 0;
}
int datahub_publish(sensor_id_t id,const sensor_value_t *v)
{
    if(!lock || !v || (unsigned)id>=SENSOR_COUNT || xSemaphoreTake(lock,pdMS_TO_TICKS(APP_DATAHUB_LOCK_MS))!=pdTRUE)return -1;
    hub.sensors[id]=*v;xSemaphoreGive(lock);return 0;
}
int datahub_snapshot(datahub_snapshot_t *out)
{
    unsigned i;uint32_t now=HAL_GetTick();
    if(!lock || !out || xSemaphoreTake(lock,pdMS_TO_TICKS(APP_DATAHUB_LOCK_MS))!=pdTRUE)return -1;
    *out=hub;xSemaphoreGive(lock);out->captured_at=now;
    for(i=0;i<SENSOR_COUNT;i++){
        uint32_t period=i==SENSOR_DHT11?DHT11_PERIOD_MS:i==SENSOR_DISTANCE?ULTRA_PERIOD_MS:APP_SENSOR_REGULAR_MS;
        if(now-out->sensors[i].timestamp>period*APP_SENSOR_STALE_MULTIPLIER){
            if(out->sensors[i].quality!=SENSOR_FAULT)out->sensors[i].quality=SENSOR_STALE;
            if(out->sensors[i].auxiliary_quality!=SENSOR_FAULT)out->sensors[i].auxiliary_quality=SENSOR_STALE;
        }
    }
    return 0;
}
int datahub_set_gas_limits(uint32_t smoke,uint32_t air)
{
    if(!lock || smoke>APP_EDIT_MAX_CONFIRM_MS || air>APP_EDIT_MAX_CONFIRM_MS ||
       xSemaphoreTake(lock,pdMS_TO_TICKS(APP_DATAHUB_LOCK_MS))!=pdTRUE)return -1;
    smoke_confirm=smoke;air_confirm=air;xSemaphoreGive(lock);return 0;
}
int datahub_get_gas_limits(uint32_t *smoke,uint32_t *air)
{
    if(!lock || !smoke || !air || xSemaphoreTake(lock,pdMS_TO_TICKS(APP_DATAHUB_LOCK_MS))!=pdTRUE)return -1;
    *smoke=smoke_confirm;*air=air_confirm;xSemaphoreGive(lock);return 0;
}
