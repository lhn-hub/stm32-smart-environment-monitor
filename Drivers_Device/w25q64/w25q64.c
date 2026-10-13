#include "w25q64.h"
#include "bus_arbiter.h"
#include "bsp_services.h"
#include "app_config.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
static uint32_t jedec,erases;
static int identified;
#if !APP_CRITICAL_AUDIT
static uint32_t log_writes[FLASH_LOG_SECTORS],log_erases[FLASH_LOG_SECTORS];
static void wear(uint32_t address,int erase)
{
    if(address>=FLASH_LOG_ADDR && address<FLASH_LOG_ADDR+FLASH_LOG_SIZE){
        unsigned sector=(address-FLASH_LOG_ADDR)/W25Q_SECTOR_SIZE;
        if(erase)++log_erases[sector];else ++log_writes[sector];
    }
}
#else
#define wear(address,erase) ((void)0)
#endif
void w25q_wear_report(uint32_t now)
{
#if !APP_CRITICAL_AUDIT
    static uint32_t report_at;static unsigned sector;
    /* Sole Flash owner LogTask prints one sector per second, without Flash IO. */
    if(now-report_at>=1000u){
        report_at=now;
        LOGI("WEAR sector=%u addr=%06lX writes=%lu erases=%lu",sector,
             (unsigned long)(FLASH_LOG_ADDR+sector*W25Q_SECTOR_SIZE),
             (unsigned long)log_writes[sector],(unsigned long)log_erases[sector]);
        sector=(sector+1u)%FLASH_LOG_SECTORS;
    }
#else
    (void)now;
#endif
}
/* W25Q protocol opcodes/status bits. */
enum { CMD_ID=0x9f,CMD_READ=3,CMD_PP=2,CMD_ERASE=0x20,CMD_WREN=6,CMD_SR=5,SR_BUSY=1,SR_WEL=2 };
/* Higher-priority polling tasks can exhaust the HAL wall-clock timeout. */
static int tx(const void *p,size_t n)
{
    HAL_StatusTypeDef r;
    vTaskSuspendAll();
    r=HAL_SPI_Transmit(&hspi1,(uint8_t*)p,(uint16_t)n,APP_FLASH_SPI_TIMEOUT_MS);
    (void)xTaskResumeAll();
    return r==HAL_OK?0:-1;
}
static int rx(void *p,size_t n)
{
    HAL_StatusTypeDef r;
    /* STM32F1 master2-line HAL receive transmits this same buffer as dummy TX. */
    memset(p,0xff,n);
    vTaskSuspendAll();
    r=HAL_SPI_Receive(&hspi1,p,(uint16_t)n,APP_FLASH_SPI_TIMEOUT_MS);
    (void)xTaskResumeAll();
    return r==HAL_OK?0:-1;
}
static void select_flash(int active)
{
    taskENTER_CRITICAL();
    HAL_GPIO_WritePin(W25Q_CS_PORT,W25Q_CS_PIN,
        active || bsp_is_faulted()?GPIO_PIN_RESET:GPIO_PIN_SET);
    taskEXIT_CRITICAL();
}
static int status(uint8_t *s)
{uint8_t c=CMD_SR;int r;select_flash(1);r=tx(&c,1);if(!r)r=rx(s,1);select_flash(0);return r;}
static int wait_ready(uint32_t limit)
{
    uint32_t begin=HAL_GetTick();uint8_t s;
    for(;;){if(status(&s)||s==0xff)return -1;if(!(s&SR_BUSY))return 0;
        if(HAL_GetTick()-begin>=limit)return -1;
        vTaskDelay(pdMS_TO_TICKS(APP_FLASH_POLL_MS));}
}
static int write_enable(void)
{uint8_t c=CMD_WREN,s;int r;select_flash(1);r=tx(&c,1);select_flash(0);return r||status(&s)||!(s&SR_WEL)?-1:0;}
static int header(uint8_t cmd,uint32_t a)
{uint8_t h[4]={cmd,(uint8_t)(a>>16),(uint8_t)(a>>8),(uint8_t)a};return tx(h,sizeof(h));}
static int raw_read(uint32_t a,void *p,size_t n)
{int r;select_flash(1);r=header(CMD_READ,a);if(!r)r=rx(p,n);select_flash(0);return r;}
static int range(uint32_t a,size_t n){return n && a<W25Q_CAPACITY_BYTES && n<=W25Q_CAPACITY_BYTES-a;}
int w25q_init(void)
{
    uint8_t c=CMD_ID,id[3];int r;identified=0;jedec=0;
    if(spi_bus_take(APP_SPI_LOCK_MS))return -1;
    r=wait_ready(APP_FLASH_BOOT_BUSY_MS);
    if(!r){select_flash(1);r=tx(&c,1);if(!r)r=rx(id,3);select_flash(0);}
    if(!r)jedec=((uint32_t)id[0]<<16)|((uint32_t)id[1]<<8)|id[2];
    spi_bus_give();identified=!r && jedec==W25Q_JEDEC_ID;
    /* Fail-closed identity assertion, never erase a mismatched/absent part. */
    return identified?0:-1;
}
uint32_t w25q_id(void){return jedec;}
int w25q_probe(uint32_t *id)
{
    uint8_t c=CMD_ID,b[3];int r;
    if(!identified || !id || spi_bus_take(APP_SPI_LOCK_MS))return -1;
    r=wait_ready(APP_FLASH_PROGRAM_MS);
    if(!r){select_flash(1);r=tx(&c,1);if(!r)r=rx(b,3);select_flash(0);}
    if(!r)*id=((uint32_t)b[0]<<16)|((uint32_t)b[1]<<8)|b[2];
    spi_bus_give();return r;
}
uint32_t w25q_erases(void){return erases;}
int w25q_read(uint32_t a,void *p,size_t n)
{
    int r;if(!identified || !p || !range(a,n) || n>W25Q_PAGE_SIZE)return -1;
    if(spi_bus_take(APP_SPI_LOCK_MS))return -1;
    r=wait_ready(APP_FLASH_PROGRAM_MS);if(!r)r=raw_read(a,p,n);
    spi_bus_give();return r;
}
int w25q_program(uint32_t a,const void *data,size_t n)
{
    uint8_t verify[APP_FLASH_VERIFY_CHUNK];const uint8_t *p=data;size_t off,k,i;int r=0;
    if(!identified || !p || !range(a,n) || n>W25Q_PAGE_SIZE-(a%W25Q_PAGE_SIZE))return -1;
    if(spi_bus_take(APP_SPI_LOCK_MS))return -1;
    r=wait_ready(APP_FLASH_PROGRAM_MS);
    for(off=0;!r&&off<n;off+=k){k=n-off;if(k>sizeof(verify))k=sizeof(verify);
        r=raw_read(a+(uint32_t)off,verify,k);for(i=0;!r&&i<k;i++)if(verify[i]!=0xff)r=-1;}
    if(!r)r=write_enable();
    if(!r){select_flash(1);r=header(CMD_PP,a);if(!r)r=tx(p,n);select_flash(0);if(!r)r=wait_ready(APP_FLASH_PROGRAM_MS);}
    for(off=0;!r&&off<n;off+=k){k=n-off;if(k>sizeof(verify))k=sizeof(verify);
        r=raw_read(a+(uint32_t)off,verify,k);if(!r&&memcmp(verify,p+off,k))r=-1;}
    if(!r)wear(a,0);
    spi_bus_give();return r;
}
int w25q_erase(uint32_t a)
{
    uint8_t b[APP_FLASH_VERIFY_CHUNK];uint32_t off;unsigned i;int r;
    if(!identified || a%W25Q_SECTOR_SIZE || !range(a,W25Q_SECTOR_SIZE))return -1;
    if(spi_bus_take(APP_SPI_LOCK_MS))return -1;
    r=wait_ready(APP_FLASH_PROGRAM_MS);if(!r)r=write_enable();
    if(!r){select_flash(1);r=header(CMD_ERASE,a);select_flash(0);}
    /* ~45ms typical is NOT a deadline.600ms bound is below nominal4s IWDG;
     * polling yields; only MonitorTask may feed after ALL task heartbeats. */
    if(!r)r=wait_ready(APP_FLASH_ERASE_MS);
    for(off=0;!r&&off<W25Q_SECTOR_SIZE;off+=sizeof(b)){
        r=raw_read(a+off,b,sizeof(b));for(i=0;!r&&i<sizeof(b);i++)if(b[i]!=0xff)r=-1;}
    if(!r){++erases;wear(a,1);}
    spi_bus_give();return r;
}
