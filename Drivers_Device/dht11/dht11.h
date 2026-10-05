#ifndef DEVICE_DHT11_H
#define DEVICE_DHT11_H
#include "sensor.h"
int dht11_init(sensor_id_t id);
/** @brief 20ms task wait plus bounded interrupt capture; never masks entire frame. */
int dht11_read(sensor_id_t id,sensor_value_t *out);
/** @brief PB12 EXTI callback: capture time/level only; no prints or RTOS calls. */
void dht11_edge_irq(void);
/** @brief Validate 82 alternating response/data widths and checksum. */
int dht11_decode(const uint16_t *widths,size_t count,int32_t *temp_x10,int32_t *humidity_x10);
#endif
