/*
 * brightness.h
 *
 * Driver for LCD Backlight PWM Control
 */

#ifndef INC_BRIGHTNESS_H_
#define INC_BRIGHTNESS_H_

#include "main.h"

// Public Functions
void Brightness_Init(TIM_HandleTypeDef* htim);
void Brightness_Increase(void);
void Brightness_Decrease(void);
uint8_t Brightness_GetLevel(void);

#endif /* INC_BRIGHTNESS_H_ */
