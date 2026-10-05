#include "bsp_services.h"
#include "app_config.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static SemaphoreHandle_t log_lock;
/* Fault hooks may run with an unusable task stack / RTOS critical section.
 * Preserve the reason for the debugger even when UART output is forbidden. */
volatile const char *bsp_fault_reason;

int fputc(int c, FILE *stream)
{
    uint8_t byte = (uint8_t)c;
    (void)stream;
    if (HAL_UART_Transmit(&huart1, &byte, 1u, APP_UART_TIMEOUT_MS) != HAL_OK)
        return EOF;
    return c;
}

int bsp_console_init(void)
{
    MX_USART1_UART_Init();
    log_lock = xSemaphoreCreateMutex();
    return log_lock ? 0 : -1;
}

int bsp_log_quiet_begin(uint32_t timeout_ms)
{
    if(!log_lock || __get_IPSR() || __get_PRIMASK() || __get_BASEPRI() ||
       xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING)return -1;
    return xSemaphoreTake(log_lock,pdMS_TO_TICKS(timeout_ms))==pdTRUE?0:-1;
}
void bsp_log_quiet_end(void){xSemaphoreGive(log_lock);}

static const char *display_task(void)
{
    const char *name=pcTaskGetName(NULL);
    if(!strcmp(name,"MonitorTask")) return "监控任务";
    if(!strcmp(name,"AlarmTask")) return "告警任务";
    if(!strcmp(name,"NetTask")) return "网络任务";
    if(!strcmp(name,"SensorTask")) return "采集任务";
    if(!strcmp(name,"MenuTask")) return "菜单任务";
    if(!strcmp(name,"LogTask")) return "日志任务";
    return name;
}

void bsp_log(const char *level, const char *format, ...)
{
    va_list args;
    int running = xTaskGetSchedulerState() == taskSCHEDULER_RUNNING;
    if (__get_IPSR() || __get_PRIMASK() || __get_BASEPRI()) return;
    if (running && xSemaphoreTake(log_lock, pdMS_TO_TICKS(APP_LOG_LOCK_MS)) != pdTRUE)
        return;
    printf("[%lu][%s][%s] ", (unsigned long)HAL_GetTick(),
           running ? display_task() : "启动", level);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\r\n");
    if (running) xSemaphoreGive(log_lock);
}

/* No printf/RTOS/stack-heavy formatting in a corrupted-stack or ISR hook.
 * Bounded register polling is emergency output, never inside a critical section.
 * Safety precedes diagnostics. Watchdog is intentionally not fed afterward.
 */
void bsp_panic(const char *reason)
{
    const char *p = reason;
    bsp_fault_reason=reason;
    bsp_emergency_stop();
    if (!__get_IPSR() && !__get_PRIMASK() && !__get_BASEPRI() &&
        (USART1->CR1 & USART_CR1_UE)) {
        while (*p) {
            uint32_t left = APP_PANIC_UART_SPINS;
            while (!(USART1->SR & USART_SR_TXE) && --left) { }
            if (!left) break;
            USART1->DR = (uint8_t)*p++;
        }
    }
    for (;;) { __NOP(); }
}
