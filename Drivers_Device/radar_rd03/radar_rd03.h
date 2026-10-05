#ifndef DEVICE_RADAR_RD03_H
#define DEVICE_RADAR_RD03_H
#include "sensor.h"
/** @brief Rd-03 OT2 PB4 high=detection. Binary GPIO cannot prove wire continuity. */
int radar_init(sensor_id_t id);
int radar_read(sensor_id_t id,sensor_value_t *out);
#endif
