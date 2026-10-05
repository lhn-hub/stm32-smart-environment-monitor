#ifndef SENSOR_FILTER_H
#define SENSOR_FILTER_H
#include <stdint.h>
#include "app_config.h"
typedef struct { int32_t values[APP_FILTER_WINDOW]; unsigned count,next; } sensor_filter_t;
typedef struct { uint32_t since; uint8_t candidate,stable; } sensor_switch_t;
int32_t sensor_median(sensor_filter_t *f,int32_t sample);
int32_t sensor_mean(sensor_filter_t *f,int32_t sample);
void sensor_switch_init(sensor_switch_t *s,int value,uint32_t now);
/** @brief Confirm/clear using separate elapsed times, wrap-safe unsigned subtraction. */
int sensor_switch_update(sensor_switch_t *s,int raw,uint32_t now,uint32_t on_ms,uint32_t off_ms);
#endif
