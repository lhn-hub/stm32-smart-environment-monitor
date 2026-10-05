#ifndef DEVICE_MCU_INTERNAL_H
#define DEVICE_MCU_INTERNAL_H
#include "sensor.h"
/** @brief Wrap M1 ADC; temp staysFAULT without trusted calibration, VDDA is auxiliary. */
int mcu_sensor_init(sensor_id_t id);
int mcu_sensor_read(sensor_id_t id,sensor_value_t *out);
#endif
