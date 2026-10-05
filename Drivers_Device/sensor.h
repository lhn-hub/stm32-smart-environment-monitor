#ifndef SENSOR_H
#define SENSOR_H
#include <stdint.h>
#include <stddef.h>
typedef enum { SENSOR_GOOD, SENSOR_STALE, SENSOR_FAULT } sensor_quality_t;
enum { SENSOR_WARMUP=1u, SENSOR_DISABLED=2u, SENSOR_NO_OPEN_WIRE_TEST=4u, SENSOR_ESTIMATED=8u };
typedef struct {
    int32_t value;
    sensor_quality_t quality;
    uint32_t timestamp; /* HAL milliseconds of last accepted sample, not last poll. */
    int32_t auxiliary; /* DHT humidity x10, MCU VDDA mV, MQ raw DO (-1 unknown). */
    sensor_quality_t auxiliary_quality;
    uint32_t flags, warmup_remaining_ms;
} sensor_value_t;
typedef enum { SENSOR_DHT11, SENSOR_DISTANCE, SENSOR_SMOKE, SENSOR_AIR, SENSOR_RADAR, SENSOR_MCU, SENSOR_COUNT } sensor_id_t;
typedef struct { uint32_t attempts, successes, failures, consecutive_failures; } sensor_diag_t;
typedef struct { uint32_t confirm_ms, release_ms; } sensor_limits_t;
enum { SENSOR_IOCTL_DIAG, SENSOR_IOCTL_LIMITS };
typedef struct {
    int (*init)(sensor_id_t id);
    int (*read)(sensor_id_t id,sensor_value_t *out);
    int (*ioctl)(sensor_id_t id,unsigned command,void *arg);
    const char *name;
} sensor_ops_t;
/** @brief Read a cached registered value; task-only, SensorTask owns registry. */
int sensor_read(sensor_id_t id,sensor_value_t *out);
int sensor_ioctl(sensor_id_t id,unsigned command,void *arg);
const sensor_ops_t *sensor_registry(size_t *count);
/** @brief Initialize/enumerate all drivers; errors become quality FAULT. */
void sensors_init(void);
/** @brief Poll only due drivers, publish under DataHub mutex; SensorTask only. */
void sensors_poll(void);
void sensors_log_table(void);
#endif
