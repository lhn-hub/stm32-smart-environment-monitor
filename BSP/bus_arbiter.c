#include "bus_arbiter.h"
#include "app_config.h"
#include "bsp_services.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
static SemaphoreHandle_t mutex;
static TaskHandle_t owner;
int spi_bus_init(void){mutex=xSemaphoreCreateMutex();return mutex?0:-1;}
int spi_bus_take(uint32_t ms)
{
    GPIO_InitTypeDef g={0};
    if(!mutex || __get_IPSR() || __get_PRIMASK() || __get_BASEPRI() || bsp_is_faulted() ||
       xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING)return -1;
    if(xSemaphoreTake(mutex,pdMS_TO_TICKS(ms))!=pdTRUE)return -1;
    if(bsp_is_faulted()){xSemaphoreGive(mutex);return -1;}
    owner=xTaskGetCurrentTaskHandle();
    __HAL_SPI_DISABLE(&hspi1);TIM3->CCR4=FAN_SAFE_CCR;
    HAL_GPIO_WritePin(W25Q_CS_PORT,W25Q_CS_PIN,GPIO_PIN_RESET);
    g.Pin=W25Q_CS_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(W25Q_CS_PORT,&g);
    HAL_GPIO_WritePin(W25Q_SCK_PORT,W25Q_SCK_PIN|W25Q_MOSI_PIN,GPIO_PIN_RESET);
    g.Pin=W25Q_MISO_PIN;g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_NOPULL;HAL_GPIO_Init(W25Q_MISO_PORT,&g);
    g.Pin=W25Q_SCK_PIN|W25Q_MOSI_PIN;g.Mode=GPIO_MODE_AF_PP;g.Speed=GPIO_SPEED_FREQ_HIGH;HAL_GPIO_Init(W25Q_SCK_PORT,&g);
    __HAL_SPI_ENABLE(&hspi1);
    HAL_GPIO_WritePin(W25Q_CS_PORT,W25Q_CS_PIN,GPIO_PIN_SET);
    return 0;
}
void spi_bus_give(void)
{
    GPIO_InitTypeDef g={0};configASSERT(owner==xTaskGetCurrentTaskHandle());
    /* Disable bridge before SPI/pins; never drive MISO/PWMA. */
    HAL_GPIO_WritePin(TB_STBY_PORT,TB_STBY_PIN,TB_SAFE_LEVEL);TIM3->CCR4=FAN_SAFE_CCR;
    __HAL_SPI_DISABLE(&hspi1);
    HAL_GPIO_WritePin(TB_AIN1_PORT,TB_AIN1_PIN|TB_BIN1_PIN,TB_SAFE_LEVEL);
    g.Pin=TB_AIN1_PIN|TB_BIN1_PIN;g.Mode=GPIO_MODE_OUTPUT_PP;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TB_AIN1_PORT,&g);owner=NULL;xSemaphoreGive(mutex);
}
