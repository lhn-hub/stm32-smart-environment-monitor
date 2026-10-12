#ifndef M5_BENCH_H
#define M5_BENCH_H
#include "app_config.h"
#if APP_M5_STRESS_ENABLED
/** @brief Bench-only debugger commands: 1=fan->log, 2=Flash->fan, 3=abort. */
extern volatile uint32_t m5_bench_request;
void m5_bench_poll(void);
void m5_bench_storage_hook(unsigned mode);
#endif
#endif
