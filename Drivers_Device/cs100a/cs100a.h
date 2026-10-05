#ifndef DEVICE_CS100A_H
#define DEVICE_CS100A_H
#include "sensor.h"
int cs100a_init(sensor_id_t id);
/** @brief Trigger12us then task-yield until echo or30ms deadline; no long busy wait. */
int cs100a_read(sensor_id_t id,sensor_value_t *out);
/** @brief PA15 ISR captures unsigned32-bit microsecond timestamps. */
void cs100a_edge_irq(void);
#endif
