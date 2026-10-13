#ifndef ALARM_ENGINE_H
#define ALARM_ENGINE_H
#include "datahub.h"
#include "bsp_services.h"
typedef enum { ALARM_SELF_TEST, ALARM_FAULT, ALARM_DISARMED,
               ALARM_WARMUP, ALARM_ARMED, ALARM_ACTIVE } alarm_state_t;
typedef enum { ALARM_NONE, ALARM_AIR, ALARM_INTRUSION, ALARM_FIRE } alarm_kind_t;
enum {
    FAULT_OLED=1u, FAULT_DHT=2u, FAULT_DISTANCE=4u, FAULT_SMOKE=8u,
    FAULT_AIR=16u, FAULT_RADAR=32u, FAULT_FLASH=64u, FAULT_ESP=128u,
    FAULT_DATAHUB=256u, FAULT_STORAGE=512u, FAULT_OUTPUT=1024u
};
#define ALARM_DEVICE_TESTS (FAULT_OLED|FAULT_FLASH|FAULT_ESP)
typedef struct {
    int32_t temp_x10;
    uint32_t distance_cm, smoke_ms, air_ms;
    unsigned armed;
} alarm_config_t;
typedef struct {
    uint32_t entered, since[3], cleared, faults, transitions;
    uint8_t state, kind, tracking, clearing, armed;
} alarm_engine_t;
typedef struct {
    uint32_t faults, transitions, warmup_remaining_ms;
    uint8_t state, kind;
} alarm_status_t;
/* Pure evaluation: no IO, heap, driver reads or wall-clock waits. */
void alarm_engine_init(alarm_engine_t *engine,uint32_t now);
void alarm_engine_step(alarm_engine_t *engine,const datahub_snapshot_t *snapshot,
                       const alarm_config_t *config,uint32_t faults,
                       int self_test_done,uint32_t now,int acknowledge);
void alarm_init(void);
void alarm_poll(uint32_t now);
void alarm_get_status(alarm_status_t *out);
void alarm_selftest_report(uint32_t mask,int passed);
void alarm_acknowledge(void);
int alarm_set_mode(unsigned armed);
int alarm_manual_fan(unsigned percent);
int alarm_manual_servo(unsigned angle);
int alarm_manual_effect(effect_t effect);
int alarm_manual_beep(uint16_t ms);
int alarm_manual_rgb(uint16_t r,uint16_t g,uint16_t b);
#if APP_M7_TEST_ENABLED
extern volatile uint32_t m7_test_request,m7_test_cycles,m7_test_failures;
void alarm_bench_poll(datahub_snapshot_t *scratch);
#endif
#endif
