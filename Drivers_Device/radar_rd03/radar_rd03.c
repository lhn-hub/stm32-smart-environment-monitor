#include "radar_rd03.h"
#include "sensor_filter.h"
static sensor_switch_t state;
static uint32_t started;
int radar_init(sensor_id_t id)
{(void)id;started=HAL_GetTick();sensor_switch_init(&state,0,started);return 0;}
int radar_read(sensor_id_t id,sensor_value_t *out)
{
    uint32_t now=HAL_GetTick();int raw=HAL_GPIO_ReadPin(RADAR_PORT,RADAR_PIN)==RADAR_ACTIVE_LEVEL;(void)id;
    if(!out)return -1;
    out->value=sensor_switch_update(&state,raw,now,RADAR_DEBOUNCE_MS,RADAR_DEBOUNCE_MS);
    out->quality=now-started>=APP_RADAR_SETTLE_MS?SENSOR_GOOD:SENSOR_STALE;
    out->timestamp=now;out->flags=SENSOR_NO_OPEN_WIRE_TEST;return 0;
}
