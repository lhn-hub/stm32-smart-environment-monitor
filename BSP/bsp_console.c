#include "bsp_services.h"
#include "app_config.h"
#include "usart.h"
#include "esp8266.h"
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
/* Independent MSP for exceptions with a damaged PSP/MSP. */
__attribute__((aligned(8))) uint32_t bsp_fault_stack[32];
volatile uint32_t bsp_fault_frame[14];
static uint16_t boot_watchdogs,boot_count;
static char console_input[104];
static volatile uint8_t console_state,console_length,console_bad,console_cr;

void bsp_backup_restore(void)
{
    BKP->DR1=0xb809u;BKP->DR2=boot_watchdogs;BKP->DR3=boot_count;
}
void bsp_boot_stats(unsigned *watchdogs,unsigned *boots)
{*watchdogs=boot_watchdogs;*boots=boot_count;}
void bsp_fault_record(unsigned cause,uint32_t pc)
{
    if((RCC->APB1ENR & RCC_APB1ENR_BKPEN) && (PWR->CR & PWR_CR_DBP)){
        BKP->DR4=cause;BKP->DR5=pc&0xffffu;BKP->DR6=pc>>16;
    }
}
void bsp_boot_record(void)
{
    unsigned watchdog=__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)?1u:0u;
    __HAL_RCC_PWR_CLK_ENABLE();__HAL_RCC_BKP_CLK_ENABLE();HAL_PWR_EnableBkUpAccess();
    if(BKP->DR1==0xb809u){boot_watchdogs=(uint16_t)BKP->DR2;boot_count=(uint16_t)BKP->DR3;}
    else {BKP->DR4=0;BKP->DR5=0;BKP->DR6=0;}
    if(watchdog && boot_watchdogs<65535u)++boot_watchdogs;
    if(boot_count<65535u)++boot_count;
    LOGI("BOOT wd=%u count=%u boots=%u cause=%u pc=%08lX",watchdog,
         (unsigned)boot_watchdogs,(unsigned)boot_count,(unsigned)BKP->DR4,
         (unsigned long)((BKP->DR6<<16)|BKP->DR5));
    BKP->DR4=0;BKP->DR5=0;BKP->DR6=0;bsp_backup_restore();
}
static void fault_text(const char *text)
{
    if(!(USART1->CR1 & USART_CR1_UE))return;
    while(*text){
        uint32_t left=APP_PANIC_UART_SPINS;
        while(!(USART1->SR & USART_SR_TXE) && --left){}
        if(!left)return;
        USART1->DR=(uint8_t)*text++;
    }
}
void bsp_fault_hex(const char *label,uint32_t value)
{
    char text[11];unsigned i;
    for(i=0;i<8u;i++)text[i]="0123456789ABCDEF"[(value>>(28u-i*4u))&15u];
    text[8]='\r';text[9]='\n';text[10]=0;fault_text(label);fault_text(text);
}
void bsp_fault_dump(const uint32_t *frame,uint32_t exception_return)
{
    static const char *labels[]={"R0=","R1=","R2=","R3=","R12=","LR=","PC=","xPSR=",
                                "SP=","EXC_RETURN=","CFSR=","HFSR=","MMFAR=","BFAR="};
    uintptr_t address=(uintptr_t)frame;unsigned i;
    bsp_emergency_stop();
    bsp_fault_reason="HardFault";
    /* Never dereference a failed stacking operation or an invalid SRAM frame. */
    bsp_fault_frame[10]=SCB->CFSR;
    for(i=0;i<8u;i++)bsp_fault_frame[i]=
        address>=0x20000000u && address<=0x20005000u-32u && !(address&3u) &&
        !(bsp_fault_frame[10]&0x1818u)?frame[i]:0xffffffffu;
    bsp_fault_frame[8]=(uint32_t)address;bsp_fault_frame[9]=exception_return;
    bsp_fault_frame[11]=SCB->HFSR;bsp_fault_frame[12]=SCB->MMFAR;bsp_fault_frame[13]=SCB->BFAR;
    bsp_fault_record(1u,bsp_fault_frame[6]);
    fault_text("\r\nHardFault\r\n");
    for(i=0;i<14u;i++)bsp_fault_hex(labels[i],bsp_fault_frame[i]);
    for(;;){__NOP();}
}

#if APP_CRITICAL_AUDIT
typedef struct {uint32_t key;uint16_t maximum_us,count;} audit_record_t;
static audit_record_t audit[96];
static struct {uint32_t begin;unsigned slot;} audit_stack[8];
static unsigned audit_depth,audit_used,audit_lost,audit_started;
void bsp_audit_start(void)
{
    CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;
    DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;audit_started=1;
}
void bsp_audit_enter(unsigned site)
{
    uint32_t begin=DWT->CYCCNT;unsigned i;
    if(!audit_started)return;
    for(i=0;i<audit_used;i++)if(audit[i].key==site)break;
    if(i==audit_used){
        if(i>=96u){++audit_lost;i=96u;}
        else{audit[i].key=site;++audit_used;}
    }
    if(audit_depth<8u){audit_stack[audit_depth].begin=begin;audit_stack[audit_depth].slot=i;}
    else ++audit_lost;
    ++audit_depth;
}
unsigned bsp_audit_isr_enter(unsigned mask,unsigned site)
{bsp_audit_enter(site);return mask;}
void bsp_audit_switch_enter(void){bsp_audit_enter(0xfffffff1u);}
void bsp_audit_exit(void)
{
    uint32_t end=DWT->CYCCNT,us;unsigned slot;
    if(!audit_started)return;
    if(!audit_depth){++audit_lost;return;}
    if(--audit_depth>=8u)return;
    slot=audit_stack[audit_depth].slot;if(slot>=96u)return;
    /* Include lookup/nested-hook overhead and a conservative exit allowance. */
    us=(end-audit_stack[audit_depth].begin+200u+71u)/72u;
    if(us>65535u)us=65535u;
    if(us>audit[slot].maximum_us)audit[slot].maximum_us=(uint16_t)us;
    if(audit[slot].count<65535u)++audit[slot].count;
}
void bsp_audit_report(void)
{
    static uint32_t report_at;static unsigned index;
    audit_record_t record;unsigned count,lost;uint32_t now=HAL_GetTick();
    if(now-report_at<100u)return;
    report_at=now;
    /* Copy under the real port lock; printing happens after it is released. */
    vPortEnterCritical();
    count=audit_used;lost=audit_lost;
    if(index>=count)index=0;
    record=audit[index];
    vPortExitCritical();
    if(!count)return;
    LOGI("CRIT pc=%08lX n=%u max_us=%u lost=%u",(unsigned long)record.key,
         (unsigned)record.count,(unsigned)record.maximum_us,lost);
    index++;
}
#endif

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
    if(!log_lock)return -1;
    /* No RTOS calls: priority 4 keeps RX alive through BASEPRI critical locks. */
    HAL_NVIC_SetPriority(USART1_IRQn,4u,0u);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    USART1->CR1|=USART_CR1_RXNEIE;
    return 0;
}
void bsp_console_line(const char *text,size_t length)
{
    if(!text || bsp_log_quiet_begin(APP_LOG_LOCK_MS))return;
    HAL_UART_Transmit(&huart1,(uint8_t *)"@WIFI ",6u,APP_UART_TIMEOUT_MS);
    HAL_UART_Transmit(&huart1,(uint8_t *)text,(uint16_t)length,APP_UART_TIMEOUT_MS);
    HAL_UART_Transmit(&huart1,(uint8_t *)"\r\n",2u,APP_UART_TIMEOUT_MS);
    bsp_log_quiet_end();
}
int bsp_console_wifi_text(char *ssid,char *password)
{
    const char *p=console_input+5;const char *sep=strchr(p,'\t');
    if((console_state!=1u && console_state!=3u) || strncmp(console_input,"JOIN\t",5) ||
       !sep || sep==p || (size_t)(sep-p)>=APP_CFG_SSID ||
       strlen(sep+1)>=APP_CFG_PASSWORD || strchr(sep+1,'\t'))return -1;
    /* AT quoted parameters cannot contain unescaped quotes/backslashes. */
    if(strpbrk(p,"\"\\,"))return -1;
    memcpy(ssid,p,(size_t)(sep-p));ssid[sep-p]=0;strcpy(password,sep+1);return 0;
}
void bsp_console_release(void)
{
    memset(console_input,0,sizeof(console_input));
    console_length=console_bad=console_cr=0;
    console_state=0;
}
/* One complete line is frozen until the owner releases it. No parsing in ISR. */
void bsp_console_rx_irq(void)
{
    uint32_t sr=USART1->SR;
    uint8_t byte=(uint8_t)USART1->DR;
    if(console_state)return;
    if(sr&(USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE))console_bad=1;
    if(!(sr&USART_SR_RXNE))return;
    if(byte=='\n'){
        console_input[console_length]=0;
        console_state=console_bad?2u:1u;
    }else if(byte=='\r')console_cr=1;
    else if(console_cr || (byte<32u && byte!='\t') || byte==127u ||
            console_length+1u>=sizeof(console_input))console_bad=1;
    else console_input[console_length++]=(char)byte;
}
void bsp_console_poll(void)
{
    if(!console_state || console_state==3u)return;
    if(console_state==2u)bsp_console_line("REJECTED",8u);
    else if(!strcmp(console_input,"SCAN")){
        int result=network_provision_scan();
        if(!result)console_state=3u;
        bsp_console_line(result?"BUSY":"SCAN_QUEUED",result?4u:11u);
    }else if(!strncmp(console_input,"JOIN\t",5)){
        if(!network_provision_join())
            {console_state=3u;bsp_console_line("JOIN_QUEUED",11u);return;}
        bsp_console_line("REJECTED",8u);
    }
    else bsp_console_line("REJECTED",8u);
    bsp_console_release();
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
    bsp_fault_reason=reason;
    bsp_emergency_stop();
    fault_text(reason);fault_text("\r\n");
    for (;;) { __NOP(); }
}
