#include "sensor.h"
#include "dht11.h"
#include "cs100a.h"
#include "mq_gas.h"
#include "radar_rd03.h"
#include "mcu_internal.h"
#include "datahub.h"
#include "bsp_services.h"
#include "app_config.h"
#include <string.h>
static const sensor_ops_t registry[SENSOR_COUNT]={
 {dht11_init,dht11_read,NULL,"DHT11"},{cs100a_init,cs100a_read,NULL,"CS100A"},
 {mq_init,mq_read,mq_ioctl,"MQ2"},{mq_init,mq_read,mq_ioctl,"MQ135"},
 {radar_init,radar_read,NULL,"Rd-03"},{mcu_sensor_init,mcu_sensor_read,NULL,"MCU"}};
static sensor_value_t cached[SENSOR_COUNT];
static sensor_diag_t diag[SENSOR_COUNT];
static uint32_t due[SENSOR_COUNT],publish_errors;
static uint32_t period(unsigned i){return i==SENSOR_DHT11?DHT11_PERIOD_MS:i==SENSOR_DISTANCE?ULTRA_PERIOD_MS:APP_SENSOR_REGULAR_MS;}
const sensor_ops_t *sensor_registry(size_t *n){if(n)*n=SENSOR_COUNT;return registry;}
int sensor_read(sensor_id_t id,sensor_value_t *out)
{if((unsigned)id>=SENSOR_COUNT || !out)return -1;*out=cached[id];return 0;}
int sensor_ioctl(sensor_id_t id,unsigned cmd,void *arg)
{
    if((unsigned)id>=SENSOR_COUNT || !arg)return -1;
    if(cmd==SENSOR_IOCTL_DIAG){*(sensor_diag_t*)arg=diag[id];return 0;}
    return registry[id].ioctl?registry[id].ioctl(id,cmd,arg):-1;
}
void sensors_init(void)
{
    unsigned i;memset(diag,0,sizeof(diag));memset(cached,0,sizeof(cached));
    for(i=0;i<SENSOR_COUNT;i++){
        cached[i].value=cached[i].auxiliary=APP_SENSOR_UNKNOWN;
        cached[i].quality=cached[i].auxiliary_quality=SENSOR_FAULT;
        if(registry[i].init((sensor_id_t)i))++diag[i].failures;
        due[i]=HAL_GetTick();
        /* Power-up stabilization is not a failed communication attempt. */
        if(i==SENSOR_DHT11 && due[i]<DHT11_POWERUP_MS)due[i]=DHT11_POWERUP_MS;
    }
    LOGW("传感器：MQ电平未处理，默认禁用；DO通道无开路自检；MCU温度校准禁用");
}
void sensors_poll(void)
{
    unsigned i;uint32_t smoke,air;
    if(!datahub_get_gas_limits(&smoke,&air)){
        sensor_limits_t l={smoke,DEF_ALARM_RELEASE_MS};sensor_ioctl(SENSOR_SMOKE,SENSOR_IOCTL_LIMITS,&l);
        l.confirm_ms=air;sensor_ioctl(SENSOR_AIR,SENSOR_IOCTL_LIMITS,&l);
    }
    for(i=0;i<SENSOR_COUNT;i++){
        sensor_value_t value;uint32_t now=HAL_GetTick();
        if((int32_t)(now-due[i])<0)continue;
        due[i]=now+period(i);memset(&value,0,sizeof(value));
        value.quality=value.auxiliary_quality=SENSOR_FAULT;value.value=value.auxiliary=APP_SENSOR_UNKNOWN;
        ++diag[i].attempts;
        if(!registry[i].read((sensor_id_t)i,&value)){
            cached[i]=value;diag[i].consecutive_failures=0;
            if(!(value.flags&SENSOR_DISABLED) && (value.quality==SENSOR_GOOD || value.auxiliary_quality==SENSOR_GOOD))++diag[i].successes;
        }else{
            unsigned limit=i==SENSOR_DHT11?DHT11_FAULT_LIMIT:ULTRA_FAILURE_LIMIT;
            ++diag[i].failures;
            if(diag[i].consecutive_failures<limit)++diag[i].consecutive_failures;
            if(diag[i].consecutive_failures>=limit){cached[i].quality=cached[i].auxiliary_quality=SENSOR_FAULT;}
            else{
                if(cached[i].quality==SENSOR_GOOD)cached[i].quality=SENSOR_STALE;
                if(cached[i].auxiliary_quality==SENSOR_GOOD)cached[i].auxiliary_quality=SENSOR_STALE;
            }
        }
        /* DataHub and serial/UI all consume the same public read contract. */
        if(!sensor_read((sensor_id_t)i,&value) && datahub_publish((sensor_id_t)i,&value))++publish_errors;
    }
}
void sensors_log_table(void)
{
    static const char *quality[]={"GOOD","STALE","FAULT"};unsigned i;
    for(i=0;i<SENSOR_COUNT;i++){
        sensor_value_t v;if(sensor_read((sensor_id_t)i,&v))continue;
        LOGI("传感器 %s 值=%ld/%s 附=%ld/%s 时间=%lu 标志=0x%lX 预热=%lus",
            registry[i].name,(long)v.value,quality[v.quality],(long)v.auxiliary,quality[v.auxiliary_quality],
            (unsigned long)v.timestamp,(unsigned long)v.flags,(unsigned long)(v.warmup_remaining_ms/1000u));
    }
    LOGI("采集统计 DHT=%lu/%lu 连败=%lu 超声失败=%lu 连败=%lu 发布失败=%lu",
        (unsigned long)diag[SENSOR_DHT11].successes,(unsigned long)diag[SENSOR_DHT11].attempts,
        (unsigned long)diag[SENSOR_DHT11].consecutive_failures,(unsigned long)diag[SENSOR_DISTANCE].failures,
        (unsigned long)diag[SENSOR_DISTANCE].consecutive_failures,(unsigned long)publish_errors);
    {
        const dht11_debug_t *d=dht11_debug();
        const cs100a_debug_t *u=cs100a_debug();
        LOGI("DHT fail=%u low=%lums rx=%luus n=%lu bad=%lu/%luus level=%u/%u/%u IRQ=%lu",
            (unsigned)d->last_error,(unsigned long)d->start_ms,(unsigned long)d->capture_us,
            (unsigned long)d->captured_widths,(unsigned long)d->bad_index,(unsigned long)d->bad_width_us,
            (unsigned)d->low_level,(unsigned)d->released_level,(unsigned)d->end_level,(unsigned long)d->irqs);
        LOGI("DHT errors low=%lu silent=%lu short=%lu edge=%lu response=%lu bit=%lu crc=%lu range=%lu",
            (unsigned long)d->errors[DHT_ERR_START_LONG],(unsigned long)d->errors[DHT_ERR_NO_RESPONSE],
            (unsigned long)d->errors[DHT_ERR_INCOMPLETE],(unsigned long)d->errors[DHT_ERR_EDGE_GAP],
            (unsigned long)d->errors[DHT_ERR_RESPONSE_WIDTH],(unsigned long)d->errors[DHT_ERR_BIT_WIDTH],
            (unsigned long)d->errors[DHT_ERR_CHECKSUM],(unsigned long)d->errors[DHT_ERR_RANGE]);
        LOGI("ULTRA fail=%u wait=%luus pulse=%luus rise=%lu fall=%lu ECHO=%u",
            (unsigned)u->last_error,(unsigned long)u->wait_us,(unsigned long)u->width_us,
            (unsigned long)u->rise_irqs,(unsigned long)u->fall_irqs,
            (unsigned)HAL_GPIO_ReadPin(ULTRA_ECHO_PORT,ULTRA_ECHO_PIN));
    }
}
