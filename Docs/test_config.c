#include "app_config.h"
#define CHECK(name, expr) typedef char check_##name[(expr) ? 1 : -1]
CHECK(timer_clock, BSP_APB1_TIMER_HZ / (TICK_TIM_PSC + 1u) == BSP_US_CLOCK_HZ);
CHECK(timer_tick, BSP_US_CLOCK_HZ / (TICK_TIM_ARR + 1u) == BSP_HAL_TICK_HZ);
CHECK(timer_above_rtos, BSP_IRQ_TIM4_PRIO < APP_RTOS_MAX_SYSCALL_PRIORITY);
CHECK(shared_exti, BSP_IRQ_EXTI15_10_PRIO == APP_RTOS_MAX_SYSCALL_PRIORITY);
CHECK(kv_independent, FLASH_KV_ADDR_B - FLASH_KV_ADDR_A == W25Q_SECTOR_SIZE);
CHECK(kv_slots, KV_SLOTS_PER_BANK == 32u);
CHECK(log_length, FLASH_LOG_SIZE == 65536u);
CHECK(log_end, FLASH_LOG_END_EXCLUSIVE == 0x013000u);
CHECK(spi_divider, W25Q_SPI_BAUD_INIT == SPI_BAUDRATEPRESCALER_8);
CHECK(spi_clock, W25Q_SPI_CLOCK_HZ == 9000000u);
CHECK(cal_disabled, ADC_FACTORY_CAL_ENABLED == 0u);
CHECK(servo_period, BSP_APB2_HZ / (SERVO_PSC + 1u) / (SERVO_ARR + 1u) == 50u);
CHECK(shared_cs_stby, W25Q_CS_PIN == TB_STBY_PIN);
CHECK(shared_miso_pwma, W25Q_MISO_PIN == TB_PWMA_PIN);
