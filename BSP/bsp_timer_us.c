#include "bsp_pin.h"
#include "bsp_timer_us.h"

uint32_t bsp_micros(void)
{
    uint32_t before, after, count, pending;
    do {
        before = HAL_GetTick();
        count = BSP_US_TIM->CNT;
        pending = BSP_US_TIM->SR & TIM_SR_UIF;
        after = HAL_GetTick();
        /* If CNT rolled before its IRQ ran, wait for the IRQ and retry.
         * No IRQ masking: priority-4 update preempts allowed callers.
         */
    } while ((before != after) || (pending != 0u));
    return before * BSP_US_PER_MS + count;
}

int bsp_delay_us(uint32_t us)
{
    uint32_t start;
    if ((us >= BSP_US_BUSYWAIT_LIMIT) || (__get_IPSR() != 0u) ||
        (__get_PRIMASK() != 0u) || (__get_BASEPRI() != 0u)) {
        return -1;
    }
    start = bsp_micros();
    while ((uint32_t)(bsp_micros() - start) < us) {
        __NOP();
    }
    return 0;
}
