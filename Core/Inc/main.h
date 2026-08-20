/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AC_HIGH_CURRENT_MEAS_Pin GPIO_PIN_0
#define AC_HIGH_CURRENT_MEAS_GPIO_Port GPIOC
#define AC_LOW_CURRENT_MEAS_Pin GPIO_PIN_1
#define AC_LOW_CURRENT_MEAS_GPIO_Port GPIOC
#define RANGE_SELECT_Pin GPIO_PIN_2
#define RANGE_SELECT_GPIO_Port GPIOC
#define AC_VOLTAGE_Pin GPIO_PIN_3
#define AC_VOLTAGE_GPIO_Port GPIOC
#define DC_VOLTAGE_Pin GPIO_PIN_0
#define DC_VOLTAGE_GPIO_Port GPIOA
#define DC_CURRENT_Pin GPIO_PIN_1
#define DC_CURRENT_GPIO_Port GPIOA
#define SD_CARD_SELECT_Pin GPIO_PIN_12
#define SD_CARD_SELECT_GPIO_Port GPIOB
#define LCD_RESET_Pin GPIO_PIN_6
#define LCD_RESET_GPIO_Port GPIOC
#define LCD_SELECT_Pin GPIO_PIN_2
#define LCD_SELECT_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */
#define SD_SPI_HANDLE hspi2
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
