#include "bus_arbiter.h"
#include "app_config.h"
#include "bsp_services.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
static SemaphoreHandle_t mutex;
static TaskHandle_t owner;
static unsigned depth;
static bus_status_t state;
int spi_bus_init(void){mutex=xSemaphoreCreateMutex();return mutex?0:-1;}
static void spi_pins(void)
{
    GPIO_InitTypeDef g={0};
    g.Pin=W25Q_CS_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(W25Q_CS_PORT,&g);
    HAL_GPIO_WritePin(W25Q_SCK_PORT,W25Q_SCK_PIN|W25Q_MOSI_PIN,GPIO_PIN_RESET);
    g.Pin=W25Q_MISO_PIN;g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(W25Q_MISO_PORT,&g);
    g.Pin=W25Q_SCK_PIN|W25Q_MOSI_PIN;g.Mode=GPIO_MODE_AF_PP;g.Speed=GPIO_SPEED_FREQ_HIGH;HAL_GPIO_Init(W25Q_SCK_PORT,&g);
}
static int take(unsigned mode,uint32_t ms)
{
    GPIO_InitTypeDef g={0};
    TaskHandle_t self;
    if(!mutex || __get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || bsp_is_faulted() ||
       xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING)return -1;
    self=xTaskGetCurrentTaskHandle();
    if(owner==self){
        if(mode!=BUS_SPI || state.mode!=BUS_SPI || depth==255u)return -1;
        ++depth;return 0;
    }
    if(xSemaphoreTake(mutex,pdMS_TO_TICKS(ms))!=pdTRUE){
        taskENTER_CRITICAL();
        if(mode==BUS_SPI)++state.spi_waits;else ++state.fan_waits;
        taskEXIT_CRITICAL();return -1;
    }
    /* Short GPIO handoff must not resume after MonitorTask's emergency stop. */
    taskENTER_CRITICAL();
    if(bsp_is_faulted()){taskEXIT_CRITICAL();xSemaphoreGive(mutex);return -1;}
    owner=self;depth=1;state.mode=mode;
    TIM3->CCR4=FAN_SAFE_CCR;
    HAL_GPIO_WritePin(TB_STBY_PORT,TB_STBY_PIN,TB_SAFE_LEVEL);
    __HAL_SPI_DISABLE(&hspi1);
    HAL_GPIO_WritePin(TB_AIN1_PORT,TB_AIN1_PIN|TB_BIN1_PIN,TB_SAFE_LEVEL);
    if(mode==BUS_SPI){
        spi_pins();__HAL_SPI_ENABLE(&hspi1);
        HAL_GPIO_WritePin(W25Q_CS_PORT,W25Q_CS_PIN,GPIO_PIN_SET);
    }else{
        TIM3->EGR=TIM_EGR_UG; /* Flush zero preload before raising STBY. */
        g.Pin=TB_AIN1_PIN|TB_BIN1_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;
        g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(TB_BIN1_PORT,&g);
        g.Pin=TB_PWMA_PIN;g.Mode=GPIO_MODE_INPUT;HAL_GPIO_Init(TB_PWMA_PORT,&g);
        HAL_GPIO_WritePin(TB_BIN2_PORT,TB_BIN2_PIN,TB_SAFE_LEVEL);
        HAL_GPIO_WritePin(TB_BIN1_PORT,TB_BIN1_PIN,GPIO_PIN_SET);
        HAL_GPIO_WritePin(TB_STBY_PORT,TB_STBY_PIN,GPIO_PIN_SET);
    }
    taskEXIT_CRITICAL();return 0;
}
static void give(unsigned mode)
{
    GPIO_InitTypeDef g={0};
    configASSERT(owner==xTaskGetCurrentTaskHandle() && state.mode==mode && depth);
    if(--depth)return;
    taskENTER_CRITICAL();
    TIM3->CCR4=FAN_SAFE_CCR;
    HAL_GPIO_WritePin(TB_STBY_PORT,TB_STBY_PIN,TB_SAFE_LEVEL);
    __HAL_SPI_DISABLE(&hspi1);
    HAL_GPIO_WritePin(TB_AIN1_PORT,TB_AIN1_PIN|TB_BIN1_PIN,TB_SAFE_LEVEL);
    g.Pin=TB_AIN1_PIN|TB_BIN1_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TB_AIN1_PORT,&g);
    if(mode==BUS_FAN)spi_pins();
    owner=NULL;state.mode=BUS_IDLE;
    taskEXIT_CRITICAL();xSemaphoreGive(mutex);
}
int spi_bus_take(uint32_t ms){return take(BUS_SPI,ms);}
void spi_bus_give(void){give(BUS_SPI);}
int fan_bus_take(uint32_t ms){return take(BUS_FAN,ms);}
void fan_bus_give(void){give(BUS_FAN);}
void bus_status(bus_status_t *out){if(out){taskENTER_CRITICAL();*out=state;taskEXIT_CRITICAL();}}
