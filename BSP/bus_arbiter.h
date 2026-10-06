#ifndef BUS_ARBITER_H
#define BUS_ARBITER_H
#include <stdint.h>
/** @brief Create priority-inheritance SPI1/PA4-7 mutex before scheduler. */
int spi_bus_init(void);
/** @brief Task-only bounded take; configure SPI pins, deselect Flash safely. */
int spi_bus_take(uint32_t timeout_ms);
/** @brief Return PA4/5/7 low, PWMB zero; release only from owning task. */
void spi_bus_give(void);
#endif
