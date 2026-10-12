#ifndef SG90_H
#define SG90_H
#include <stdint.h>
int servo_init(void);
/** @brief Nonblocking target 0..180; timer advances <=1 degree per 50ms. */
int servo_write(unsigned angle);
unsigned servo_angle(void);
void servo_poll(uint32_t now);
#endif
