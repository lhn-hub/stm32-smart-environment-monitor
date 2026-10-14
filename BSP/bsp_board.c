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
void bsp_boot_record(void);
static volatile uint32_t fault_latched;
int bsp_is_faulted(void) { return fault_latched!=0; }
void bsp_emergency_stop(void) { fault_latched=1; bsp_safe_state(); }

void bsp_safe_state(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(TB_STBY_PORT,TB_STBY_PIN,TB_SAFE_LEVEL);
    HAL_GPIO_WritePin(TB_BIN1_PORT,TB_BIN1_PIN,TB_SAFE_LEVEL);
    HAL_GPIO_WritePin(TB_BIN2_PORT,TB_BIN2_PIN,TB_SAFE_LEVEL);
    TIM3->CCR4=FAN_SAFE_CCR;
    SPI1->CR1 &= ~SPI_CR1_SPE;
    TIM1->CCR1=SERVO_CCR_MIN;
    TIM1->BDTR &= ~SERVO_MOE_MASK;
    HAL_GPIO_WritePin(RGB_R_PORT,RGB_R_PIN|RGB_G_PIN|RGB_B_PIN,RGB_OFF_LEVEL);
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
    if(HAL_RCC_GetHCLKFreq()!=BSP_SYSCLK_HZ) bsp_panic("系统时钟不是72MHz");
    MX_GPIO_Init();
    /* No PWM on A channel. Keep PA5/PA7 low until an owner takes SPI. */
    HAL_GPIO_WritePin(TB_AIN1_PORT,TB_AIN1_PIN|TB_BIN1_PIN,GPIO_PIN_RESET);
    gpio.Pin=TB_AIN1_PIN|TB_BIN1_PIN; gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Speed=GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(TB_AIN1_PORT,&gpio);
    bsp_safe_state();
    HAL_GPIO_WritePin(SERVO_PORT,SERVO_PIN,GPIO_PIN_RESET);
    gpio.Pin=SERVO_PIN;gpio.Mode=GPIO_MODE_OUTPUT_PP;gpio.Pull=GPIO_NOPULL;
    gpio.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(SERVO_PORT,&gpio);
    /* Active-low LEDs: high GPIO is off before PWM takes ownership. */
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN | RGB_G_PIN | RGB_B_PIN,
                      RGB_OFF_LEVEL);
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
    LOGI("sms-hub %s",APP_FW_VERSION);
    bsp_boot_record();
    __HAL_RCC_CLEAR_RESET_FLAGS();
    LOGW("ADC校准禁用：候选地址未验证；温度不可用，VDDA为标称估算");
    return 0;
}
