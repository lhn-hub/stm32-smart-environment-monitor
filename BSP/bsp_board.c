#include "bsp_pin.h"

void bsp_board_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    /* M0: static GPIO level only. Confirm RGB polarity on the real board. */
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN | RGB_G_PIN | RGB_B_PIN,
                      GPIO_PIN_RESET);
    gpio.Pin = RGB_R_PIN | RGB_G_PIN | RGB_B_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RGB_R_PORT, &gpio);
}
