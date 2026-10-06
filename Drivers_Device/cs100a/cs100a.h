#ifndef DEVICE_CS100A_H
#define DEVICE_CS100A_H
#include "sensor.h"
int cs100a_init(sensor_id_t id);
/** @brief Trigger12us then task-yield until echo or30ms deadline; no long busy wait. */
int cs100a_read(sensor_id_t id,sensor_value_t *out);
/** @brief PB3/EXTI3 captures unsigned32-bit microsecond timestamps. */
void cs100a_edge_irq(void);
typedef enum {
    ULTRA_ERR_NONE, ULTRA_ERR_IDLE_HIGH, ULTRA_ERR_TRIGGER,
    ULTRA_ERR_NO_RISE, ULTRA_ERR_NO_FALL, ULTRA_ERR_LATE, ULTRA_ERR_WIDTH, ULTRA_ERR_COUNT
} cs100a_error_t;
typedef struct {
    uint32_t errors[ULTRA_ERR_COUNT];
    cs100a_error_t last_error;
    uint32_t width_us, wait_us, rise_irqs, fall_irqs;
} cs100a_debug_t;
/** @brief Task snapshot of last failed attempt plus cumulative pin IRQ counts. */
const cs100a_debug_t *cs100a_debug(void);
#endif
