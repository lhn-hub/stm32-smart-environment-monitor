#include "mcu_internal.h"
#include "bsp_services.h"
#include "app_config.h"
int mcu_sensor_init(sensor_id_t id){(void)id;return 0;}
int mcu_sensor_read(sensor_id_t id,sensor_value_t *out)
{
    uint32_t stamp,voltage;int32_t t;(void)id;if(!out)return -1;
    out->quality=mcu_temp_get(&t)?SENSOR_FAULT:SENSOR_GOOD;out->value=t;
    bsp_adc_snapshot(&voltage,&stamp);
    out->auxiliary=(int32_t)voltage;out->auxiliary_quality=!voltage?SENSOR_FAULT:
        HAL_GetTick()-stamp>APP_MCU_STALE_MS?SENSOR_STALE:SENSOR_GOOD;
    out->timestamp=stamp;out->flags=SENSOR_ESTIMATED;return 0;
}
