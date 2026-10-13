"""Actual fault/backup/audit C and FreeRTOS stack guard; no board claims."""
from pathlib import Path
import os
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tmp/m8-test"
OUT.mkdir(parents=True, exist_ok=True)
ENV = dict(os.environ, PATH="D:/msys64/ucrt64/bin;" + os.environ.get("PATH", ""))


def write(name, body):
    (OUT / name).write_text(textwrap.dedent(body), encoding="utf-8")


write("stm32f1xx_hal.h", r"""
    #ifndef TEST_HAL_H
    #define TEST_HAL_H
    #include <stdint.h>
    typedef struct { int unused; } TIM_HandleTypeDef;
    typedef struct { uint32_t DR1,DR2,DR3,DR4,DR5,DR6; } Backup;
    typedef struct { uint32_t CR; } Power;
    typedef struct { uint32_t APB1ENR; } Clock;
    typedef struct { uint32_t SR,DR,CR1; } Uart;
    typedef struct { uint32_t CFSR,HFSR,MMFAR,BFAR; } Fault;
    typedef struct { uint32_t CTRL,CYCCNT; } Cycle;
    typedef struct { uint32_t DEMCR; } Debug;
    extern Backup backup;extern Power power;extern Clock clock_regs;
    extern Uart uart;extern Fault fault_regs;extern Cycle cycle;extern Debug debug_regs;
    #define BKP (&backup)
    #define PWR (&power)
    #define RCC (&clock_regs)
    #define USART1 (&uart)
    #define SCB (&fault_regs)
    #define DWT (&cycle)
    #define CoreDebug (&debug_regs)
    #define CoreDebug_DEMCR_TRCENA_Msk 1u
    #define DWT_CTRL_CYCCNTENA_Msk 1u
    #define RCC_APB1ENR_BKPEN 1u
    #define PWR_CR_DBP 1u
    #define USART_CR1_UE 1u
    #define USART_SR_TXE 1u
    #define RCC_FLAG_IWDGRST 1u
    #define HAL_OK 0
    extern unsigned wd_flag;
    #define __HAL_RCC_GET_FLAG(f) (wd_flag)
    #define __HAL_RCC_PWR_CLK_ENABLE() ((void)0)
    #define __HAL_RCC_BKP_CLK_ENABLE() (clock_regs.APB1ENR|=1u)
    void HAL_PWR_EnableBkUpAccess(void);
    uint32_t HAL_GetTick(void);
    unsigned __get_IPSR(void);
    unsigned __get_PRIMASK(void);
    unsigned __get_BASEPRI(void);
    void test_stop(void);
    #define __NOP() test_stop()
    int HAL_UART_Transmit(void *,void *,unsigned,unsigned);
    void MX_USART1_UART_Init(void);
    #endif
""")
write("usart.h", "extern int huart1;\n")
write("FreeRTOS.h", """
    #define pdTRUE 1
    #define taskSCHEDULER_RUNNING 2
    #define pdMS_TO_TICKS(x) (x)
    #define configCHECK_FOR_STACK_OVERFLOW 2
    #define portSTACK_GROWTH -1
""")
write("task.h", """
    typedef void *TaskHandle_t;
    int xTaskGetSchedulerState(void);
    const char *pcTaskGetName(void *);
    void vPortEnterCritical(void);
    void vPortExitCritical(void);
""")
write("semphr.h", """
    typedef void *SemaphoreHandle_t;
    void *xSemaphoreCreateMutex(void);
    int xSemaphoreTake(void *,unsigned);
    int xSemaphoreGive(void *);
""")
hooks = (ROOT / "App/tasks/app_tasks.c").read_text(encoding="utf-8")
write("hooks.h", hooks[hooks.index("void vApplicationMallocFailedHook"):])
write("test.c", r"""
    #include <windows.h>
    #include <assert.h>
    #include <setjmp.h>
    #include <stdio.h>
    #include "stm32f1xx_hal.h"
    Backup backup;Power power;Clock clock_regs;Uart uart;Fault fault_regs;Cycle cycle;Debug debug_regs;
    int huart1;unsigned wd_flag;static unsigned safe,stops,critical;
    static jmp_buf terminal;
    uint32_t HAL_GetTick(void){return 60000u;}
    void HAL_PWR_EnableBkUpAccess(void){power.CR|=1u;}
    unsigned __get_IPSR(void){return 0;}
    unsigned __get_PRIMASK(void){return 0;}
    unsigned __get_BASEPRI(void){return critical;}
    void test_stop(void){++stops;longjmp(terminal,1);}
    void bsp_emergency_stop(void){++safe;}
    void MX_USART1_UART_Init(void){}
    int HAL_UART_Transmit(void *u,void *p,unsigned n,unsigned t){(void)u;(void)p;(void)n;(void)t;return 0;}
    void *xSemaphoreCreateMutex(void){return (void*)1;}
    int xSemaphoreTake(void *m,unsigned t){(void)m;(void)t;return 1;}
    int xSemaphoreGive(void *m){(void)m;return 1;}
    int xTaskGetSchedulerState(void){return 0;}
    const char *pcTaskGetName(void *t){(void)t;return "test";}
    void vPortEnterCritical(void){++critical;}
    void vPortExitCritical(void){assert(critical);--critical;}
    #include "../../BSP/bsp_console.c"
    #include "hooks.h"
    typedef struct { uint32_t *pxStack;char pcTaskName[16]; } TCB;
    static TCB tcb,*pxCurrentTCB=&tcb;
    #include "../../Middleware/FreeRTOS/include/stack_macros.h"
    int main(void){
        uint32_t *frame;unsigned i,resets,boots;
        uart.CR1=uart.SR=1;clock_regs.APB1ENR=power.CR=1;
        frame=VirtualAlloc((void*)0x20000000u,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        assert(frame==(void*)0x20000000u);
        for(i=0;i<8;i++)frame[i]=0x100u+i;
        if(!setjmp(terminal))bsp_fault_dump(frame,0xfffffffdu);
        assert(safe && bsp_fault_frame[6]==0x106u && bsp_fault_frame[5]==0x105u);
        assert(bsp_fault_frame[9]==0xfffffffdu && backup.DR4==1 && backup.DR5==0x106u);
        for(i=0;i<4;i++){
            fault_regs.CFSR=(uint32_t[]){8,16,0x800,0x1000}[i];
            if(!setjmp(terminal))bsp_fault_dump(frame,0xfffffffdu);
            assert(bsp_fault_frame[6]==0xffffffffu);
        }
        fault_regs.CFSR=0;
        if(!setjmp(terminal))bsp_fault_dump((void*)0x20004fe4u,0);
        assert(bsp_fault_frame[0]==0xffffffffu);
        if(!setjmp(terminal))bsp_fault_dump((void*)0x20000001u,0);
        assert(bsp_fault_frame[0]==0xffffffffu);
        puts("PASS actual fault frame: PC/LR/R0-R3, stacking errors, alignment and SRAM bounds");
        tcb.pxStack=frame;
        for(i=0;i<4;i++)frame[i]=0xa5a5a5a5u;
        taskCHECK_FOR_STACK_OVERFLOW();assert(backup.DR4==1);
        for(i=0;i<4;i++){
            frame[i]=0;
            if(!setjmp(terminal)){taskCHECK_FOR_STACK_OVERFLOW();assert(!"guard missed");}
            assert(backup.DR4==2 && safe && !strcmp((const char*)bsp_fault_reason,"任务栈溢出"));
            frame[i]=0xa5a5a5a5u;
        }
        if(!setjmp(terminal))vApplicationMallocFailedHook();
        assert(backup.DR4==3 && safe);
        puts("PASS real FreeRTOS mode2 guard -> actual overflow hook -> recorded safe halt; malloc hook");
        backup.DR1=0;bsp_boot_record();bsp_boot_stats(&resets,&boots);assert(!resets&&boots==1);
        wd_flag=1;bsp_fault_record(2,0x08001234u);bsp_boot_record();
        bsp_boot_stats(&resets,&boots);assert(resets==1&&boots==2&&!backup.DR4);
        memset(&backup,0,sizeof(backup));bsp_backup_restore();assert(backup.DR2==1&&backup.DR3==2);
        puts("PASS watchdog cause captured before reset-flag clearing; backup source reset preserves counters");
    #if APP_CRITICAL_AUDIT
        bsp_audit_start();cycle.CYCCNT=0xfffffff0u;
        bsp_audit_enter(0x08001234u);
        cycle.CYCCNT+=72;bsp_audit_enter(0x08001334u);
        cycle.CYCCNT+=720;bsp_audit_exit();cycle.CYCCNT+=72;bsp_audit_exit();
        assert(!audit_depth&&audit_used==2&&audit[0].maximum_us>=12u&&audit[1].maximum_us>=10u);
        for(i=0;i<110;i++){bsp_audit_enter(0x08002000u+i);cycle.CYCCNT+=72;bsp_audit_exit();}
        assert(audit_used==96&&audit_lost>0&&!audit_depth);
        bsp_audit_exit();assert(audit_lost>1);
        puts("PASS actual DWT audit: nested sections, uint32 wrap, saturation/capacity/unbalanced exits");
    #endif
        VirtualFree(frame,0,MEM_RELEASE);return 0;
    }
""")
for audit in (0, 1):
    args = ["D:/msys64/ucrt64/bin/gcc.exe", "-std=c99", "-Wall", "-Wextra", "-Werror",
            "-DAPP_NO_LOCAL_SECRETS", f"-DAPP_CRITICAL_AUDIT={audit}",
            "-I", "tmp/m8-test", "-I", "App", "-I", "BSP", "tmp/m8-test/test.c",
            "-o", f"tmp/m8-test/test{audit}.exe"]
    subprocess.run(args, cwd=ROOT, env=ENV, check=True)
    subprocess.run([f"tmp/m8-test/test{audit}.exe"], cwd=ROOT, env=ENV, check=True)
