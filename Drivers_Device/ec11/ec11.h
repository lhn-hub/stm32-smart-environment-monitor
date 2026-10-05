#ifndef EC11_H
#define EC11_H
#include <stdint.h>
/** @brief Reset quadrature decoder to sampled AB; no allocation. */
void ec11_reset(uint8_t ab);
/** @brief Feed ordered snapshots; returns +/-1 only on full detent, else0. */
int ec11_feed(uint8_t ab);
/** @brief Capture sampled AB in EXTI and notify MenuTask, no decoding/IO. */
void ec11_irq(void);
/** @brief Bind MenuTask after its start and sample initial GPIO. */
void ec11_start(void);
/** @brief Drain ordered ISR snapshots in MenuTask; return net detents. */
int ec11_poll(void);
uint32_t ec11_overflows(void);
/** @brief Task-context diagnostics: pin levels, captured IRQs, invalid jumps. */
uint8_t ec11_levels(void);
uint32_t ec11_irq_count(void);
uint32_t ec11_invalid_count(void);
#endif
