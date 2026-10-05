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
typedef enum {
    DHT_ERR_NONE, DHT_ERR_START_LONG, DHT_ERR_NO_RESPONSE, DHT_ERR_INCOMPLETE,
    DHT_ERR_EDGE_GAP, DHT_ERR_RESPONSE_WIDTH, DHT_ERR_BIT_WIDTH,
    DHT_ERR_CHECKSUM, DHT_ERR_RANGE, DHT_ERR_COUNT
} dht11_error_t;
typedef struct {
    uint32_t errors[DHT_ERR_COUNT];
    dht11_error_t last_error;
    uint32_t start_ms, capture_us, captured_widths, bad_index, bad_width_us;
} dht11_debug_t;
/** @brief SensorTask-owned last FAILED attempt; success does not erase evidence. */
const dht11_debug_t *dht11_debug(void);
#endif
