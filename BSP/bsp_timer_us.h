#ifndef BSP_TIMER_US_H
#define BSP_TIMER_US_H
#include <stdint.h>
/** @brief Return wrapping 32-bit microseconds from the running TIM4 HAL tick.
 * @pre TIM4 runs at 1MHz with ARR=999 and its priority-4 IRQ enabled.
 * @pre Caller is thread mode or an IRQ with priority numerically >4;
 *      PRIMASK must not mask interrupts. Do not suspend the HAL tick.
 * @note Use unsigned subtraction for intervals across wraparound.
 */
uint32_t bsp_micros(void);
/** @brief Poll for fewer than 1000 microseconds; zero is a no-op.
 * @return 0 on success, -1 for an unsupported interval/context.
 * @note Thread mode only; no busy waiting inside an ISR/critical section.
 */
int bsp_delay_us(uint32_t us);
#endif
