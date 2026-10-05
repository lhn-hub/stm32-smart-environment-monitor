#ifndef DATAHUB_H
#define DATAHUB_H
#include "sensor.h"
typedef struct { sensor_value_t sensors[SENSOR_COUNT]; uint32_t captured_at; } datahub_snapshot_t;
/** @brief Create the single bounded DataHub mutex before scheduler starts. */
int datahub_init(void);
/** @brief Copy-only mutex regions; no logging, hardware waits, or ISR calls. */
int datahub_publish(sensor_id_t id,const sensor_value_t *value);
int datahub_snapshot(datahub_snapshot_t *out);
/** @brief RAM gas confirmation settings shared between menu and SensorTask. */
int datahub_set_gas_limits(uint32_t smoke_ms,uint32_t air_ms);
int datahub_get_gas_limits(uint32_t *smoke_ms,uint32_t *air_ms);
#endif
