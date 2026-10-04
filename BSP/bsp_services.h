#ifndef BSP_SERVICES_H
#define BSP_SERVICES_H
#include <stdint.h>
#include <stddef.h>
/** @brief Initialize runtime BSP services before the scheduler; return 0 on success. */
int bsp_services_init(void);
/** @brief Immediately stop actuators without allocation or blocking. */
void bsp_safe_state(void);
/** @brief Latch a fault and reject future actuator requests until reset. */
void bsp_emergency_stop(void);
int bsp_is_faulted(void);
/** @brief Bounded emergency output then safe halt; callable from fault hooks. */
void bsp_panic(const char *reason);
/** @brief Task-only serialized printf log. No floating point formats. */
void bsp_log(const char *level, const char *format, ...);
#define LOGI(...) bsp_log("信息", __VA_ARGS__)
#define LOGW(...) bsp_log("警告", __VA_ARGS__)
#define LOGE(...) bsp_log("错误", __VA_ARGS__)
/** @brief Set RGB PWM duties; reject values outside 0..999. */
int rgb_set(uint16_t r, uint16_t g, uint16_t b);
typedef struct { uint16_t on_ms, off_ms; uint8_t repeat; } beep_pattern_t;
/** @brief Copy a nonblocking beep request; return -1 if invalid or busy. */
int beep_pattern(const beep_pattern_t *pattern);
/** @brief Request one nonblocking beep. */
int beep_ms(uint16_t ms);
typedef enum { KEY_WAKEUP, KEY_BACK, KEY_CONFIRM } key_source_t;
typedef enum { KEY_CLICK, KEY_LONG } key_action_t;
typedef struct { key_source_t source; key_action_t action; uint32_t tick; } key_event_t;
/** @brief Receive a debounced key event without blocking. */
int bsp_key_get(key_event_t *event);
/** @brief Return 0 with temperature x10, or -1 while quality is FAULT. */
int mcu_temp_get(int32_t *temperature_x10);
/** @brief Return estimated VDDA mV; 0 means no valid ADC sample. */
uint32_t vdda_mv_get(void);
/** @brief Read/write RTC counter as Unix seconds; SensorTask is sole set owner. */
uint32_t rtc_get(void);
int rtc_set(uint32_t seconds);
/** @brief Owner-task-only recovery: SensorTask during startup, MenuTask after handoff; never concurrent. */
int bsp_i2c_recover(void);
/** @brief Scan legal 7-bit addresses and print results in task context. */
void i2c_scan(void);
/** @brief Drain DMA ring in NetTask, splitting complete newline-terminated lines. */
void bsp_net_poll(void);
/** @brief Override in App; called in NetTask, buffer valid only during callback. */
void net_on_rx_line(const char *line, size_t length);
/** @brief Copy into an owned TX buffer and start DMA; return -1 if busy/invalid. */
int bsp_net_send(const void *data, size_t length);
/** @brief USART3 IRQ pre-handler; acknowledges IDLE, never parses or prints. */
void bsp_net_idle_irq(void);
/** @brief Start IWDG / refresh after Monitor has checked all heartbeats. */
int bsp_watchdog_start(void);
void bsp_watchdog_feed(void);
/** @brief Start six FreeRTOS tasks; does not return on success. */
void app_start(void);
#endif
