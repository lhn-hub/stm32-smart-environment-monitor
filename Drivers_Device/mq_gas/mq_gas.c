#include "mq_gas.h"
#include "sensor_filter.h"
static sensor_switch_t states[2];
static sensor_limits_t limits[2];
static uint8_t warmed[2];
static unsigned index_of(sensor_id_t id){return id==SENSOR_AIR?1u:0u;}
int mq_init(sensor_id_t id)
{
    unsigned i=index_of(id);warmed[i]=0;sensor_switch_init(&states[i],0,HAL_GetTick());
    limits[i].confirm_ms=i?DEF_AIR_CONFIRM_MS:DEF_SMOKE_CONFIRM_MS;
    limits[i].release_ms=DEF_ALARM_RELEASE_MS;return 0;
}
int mq_ioctl(sensor_id_t id,unsigned command,void *arg)
{
    sensor_limits_t *v=(sensor_limits_t *)arg;
    if(!arg || command!=SENSOR_IOCTL_LIMITS || (id!=SENSOR_SMOKE && id!=SENSOR_AIR))return -1;
    if(v->confirm_ms>APP_EDIT_MAX_CONFIRM_MS || v->release_ms>APP_EDIT_MAX_CONFIRM_MS)return -1;
    if(limits[index_of(id)].confirm_ms!=v->confirm_ms || limits[index_of(id)].release_ms!=v->release_ms)
        sensor_switch_init(&states[index_of(id)],0,HAL_GetTick());
    limits[index_of(id)]=*v;return 0;
}
int mq_read(sensor_id_t id,sensor_value_t *out)
{
    unsigned i=index_of(id);uint32_t now=HAL_GetTick();int raw;
    if(!out)return -1;
    out->flags=SENSOR_NO_OPEN_WIRE_TEST;
    if(now>=APP_MQ_WARMUP_MS)warmed[i]=1;
    out->timestamp=now;out->warmup_remaining_ms=warmed[i]?0:APP_MQ_WARMUP_MS-now;
    if(out->warmup_remaining_ms)out->flags|=SENSOR_WARMUP;
    /* This compile-time gate is a wiring prerequisite, not a5V voltage clamp.
     * Do not enable until a real level-safe interface has been approved. */
    if(!APP_MQ_DO_ENABLED){out->flags|=SENSOR_DISABLED;out->value=out->auxiliary=APP_SENSOR_UNKNOWN;out->quality=out->auxiliary_quality=SENSOR_FAULT;return 0;}
    raw=(int)HAL_GPIO_ReadPin(i?MQ_AIR_PORT:MQ_SMOKE_PORT,i?MQ_AIR_PIN:MQ_SMOKE_PIN);
    out->auxiliary=raw;out->auxiliary_quality=SENSOR_GOOD;
    if(out->warmup_remaining_ms){sensor_switch_init(&states[i],0,now);out->value=0;out->quality=SENSOR_STALE;return 0;}
    out->value=sensor_switch_update(&states[i],raw==(int)MQ_ALARM_LEVEL,now,limits[i].confirm_ms,limits[i].release_ms);
    out->quality=SENSOR_GOOD;return 0;
}
