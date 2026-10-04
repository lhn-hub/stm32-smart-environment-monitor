#include "bsp_pin.h"
#include "bsp_services.h"
#include "app_config.h"
#include "gpio.h"
#include "tim.h"
#include "spi.h"
#include "i2c.h"
#include "dma.h"

int bsp_console_init(void);
int bsp_controls_init(void);
static volatile uint32_t fault_latched;
int bsp_is_faulted(void) { return fault_latched!=0; }
void bsp_emergency_stop(void) { fault_latched=1; bsp_safe_state(); }

void bsp_safe_state(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(TB_STBY_PORT,TB_STBY_PIN,TB_SAFE_LEVEL);
    HAL_GPIO_WritePin(TB_BIN1_PORT,TB_BIN1_PIN,TB_SAFE_LEVEL);
    HAL_GPIO_WritePin(TB_BIN2_PORT,TB_BIN2_PIN,TB_SAFE_LEVEL);
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT,BUZZER_PIN,BUZZER_OFF_LEVEL);
    TIM3->CCR4=FAN_SAFE_CCR;
    TIM2->CCR2=0; TIM2->CCR3=0; TIM2->CCR4=0;
}

void bsp_board_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    if(HAL_RCC_GetHCLKFreq()!=BSP_SYSCLK_HZ) bsp_panic("clock !=72MHz");
    MX_GPIO_Init();
    /* No PWM on A channel. Keep PA5/PA7 low until an owner takes SPI. */
    HAL_GPIO_WritePin(TB_AIN1_PORT,TB_AIN1_PIN|TB_BIN1_PIN,GPIO_PIN_RESET);
    gpio.Pin=TB_AIN1_PIN|TB_BIN1_PIN; gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Speed=GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(TB_AIN1_PORT,&gpio);
    bsp_safe_state();
    /* M0: static GPIO level only. Confirm RGB polarity on the real board. */
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN | RGB_G_PIN | RGB_B_PIN,
                      GPIO_PIN_RESET);
    gpio.Pin = RGB_R_PIN | RGB_G_PIN | RGB_B_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RGB_R_PORT, &gpio);
}

int bsp_services_init(void)
{
    MX_DMA_Init();
    if(bsp_console_init()) return -1;
    MX_TIM3_Init(); /* CCR4 remains zero; PWM not started. */
    MX_I2C1_Init();
    MX_SPI1_Init();
    /* SPI initialized for M4, but no transfer and STBY stays low. */
    __HAL_SPI_DISABLE(&hspi1);
    {
        GPIO_InitTypeDef gpio={0};
        HAL_GPIO_WritePin(TB_BIN1_PORT,TB_BIN1_PIN|TB_AIN1_PIN,TB_SAFE_LEVEL);
        gpio.Pin=TB_BIN1_PIN|TB_AIN1_PIN; gpio.Mode=GPIO_MODE_OUTPUT_PP;
        gpio.Speed=GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(TB_BIN1_PORT,&gpio);
    }
    bsp_safe_state();
    LOGI("sms-hub %s reset_IWDG=%u",APP_FW_VERSION,
         __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)?1u:0u);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    LOGW("ADC calibration DISABLED: candidate addresses unverified; TEMP=FAULT VDDA=nominal");
    return 0;
}
