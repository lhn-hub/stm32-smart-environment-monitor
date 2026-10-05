#include "bsp_pin.h"
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BSP_GPIO_PORT_A, BSP_GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BSP_GPIO_PORT_B, BSP_GPIO_PIN_13|BSP_GPIO_PIN_3|BSP_GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BSP_GPIO_PORT_B, BSP_GPIO_PIN_8|BSP_GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BSP_GPIO_PORT_C, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BSP_GPIO_PORT_A, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BSP_GPIO_PORT_A, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB12 PB14 PB15 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_0|BSP_GPIO_PIN_12|BSP_GPIO_PIN_14|BSP_GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BSP_GPIO_PORT_B, &GPIO_InitStruct);

  /*Configure GPIO pins : PB13 PB3 PB5 PB8
                           PB9 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_13|BSP_GPIO_PIN_3|BSP_GPIO_PIN_5|BSP_GPIO_PIN_8
                          |BSP_GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BSP_GPIO_PORT_B, &GPIO_InitStruct);

  /* EC11 removed: PA11/PA12 unused inputs, no EXTI. */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_11|BSP_GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BSP_GPIO_PORT_A, &GPIO_InitStruct);

  /*Configure GPIO pin : PA15 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BSP_GPIO_PORT_A, &GPIO_InitStruct);

  /*Configure GPIO pin : PB4 */
  GPIO_InitStruct.Pin = BSP_GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BSP_GPIO_PORT_B, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, BSP_IRQ_EXTI0_PRIO, BSP_IRQ_SUBPRIO);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, BSP_IRQ_EXTI4_PRIO, BSP_IRQ_SUBPRIO);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, BSP_IRQ_EXTI15_10_PRIO, BSP_IRQ_SUBPRIO);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
