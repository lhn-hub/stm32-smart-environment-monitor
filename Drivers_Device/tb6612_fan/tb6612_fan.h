#ifndef TB6612_FAN_H
#define TB6612_FAN_H
#include <stdint.h>
int fan_init(void);
/** @brief Task-only nonblocking target 0..100; zero cancels pending start. */
int fan_set(unsigned percent);
unsigned fan_percent(void);
/** @brief AlarmTask sole owner of take/ramp/stop/give; bounded lock wait. */
void fan_poll(uint32_t now);
#endif
