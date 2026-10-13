#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H
#include "app_config.h"
void bsp_panic(const char *reason);
#if APP_CRITICAL_AUDIT
void bsp_audit_enter(unsigned site);
void bsp_audit_exit(void);
unsigned bsp_audit_isr_enter(unsigned mask,unsigned site);
#endif
#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ BSP_SYSCLK_HZ
#define configTICK_RATE_HZ APP_RTOS_TICK_HZ
#define configMAX_PRIORITIES APP_RTOS_MAX_PRIORITIES
#define configMINIMAL_STACK_SIZE APP_IDLE_STACK
#define configTOTAL_HEAP_SIZE APP_RTOS_HEAP_BYTES
#define configMAX_TASK_NAME_LEN APP_RTOS_TASK_NAME_LENGTH
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 0
#define configUSE_COUNTING_SEMAPHORES 0
#define configUSE_TASK_NOTIFICATIONS 1
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY APP_TIMER_PRIORITY
#define configTIMER_QUEUE_LENGTH APP_TIMER_QUEUE
#define configTIMER_TASK_STACK_DEPTH APP_TIMER_STACK
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configUSE_TRACE_FACILITY APP_M8_TEST_ENABLED
#define configUSE_NEWLIB_REENTRANT 0
#define configPRIO_BITS APP_RTOS_PRIORITY_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY BSP_IRQ_SYSTICK_PRIO
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY APP_RTOS_MAX_SYSCALL_PRIORITY
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8-configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8-configPRIO_BITS))
#define configASSERT(x) do { if (!(x)) bsp_panic("assert"); } while (0)
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
#endif
