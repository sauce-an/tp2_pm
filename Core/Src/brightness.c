/*
 * brightness.c
 *
 *  Created on: Apr 29, 2026
 *      Author: Andrew Jian
 */


#include "brightness.h"

// Internal State
static TIM_HandleTypeDef* pwm_timer;
static uint8_t current_level = 4; // Default to Max Brightness (4)

// Helper function to blast the duty cycle to the silicon
static void Set_Hardware_PWM(void) {
    uint32_t ccr_val = 0;

    // Map our 0-4 levels to the 0-1000 hardware counter
    switch(current_level) {
        case 0: ccr_val = 0;    break; // 0% (Off)
        case 1: ccr_val = 250;  break; // 25%
        case 2: ccr_val = 500;  break; // 50%
        case 3: ccr_val = 750;  break; // 75%
        case 4: ccr_val = 1000; break; // 100% (Max)
    }

    // Update the Timer's Capture/Compare Register (CCR2 for Channel 2)
    __HAL_TIM_SET_COMPARE(pwm_timer, TIM_CHANNEL_2, ccr_val);
}

void Brightness_Init(TIM_HandleTypeDef* htim) {
    pwm_timer = htim;
    // Turn on the physical PWM signal generator!
    HAL_TIM_PWM_Start(pwm_timer, TIM_CHANNEL_2);
    Set_Hardware_PWM();
}

void Brightness_Increase(void) {
    if (current_level < 4) {
        current_level++;
        Set_Hardware_PWM();
    }
}

void Brightness_Decrease(void) {
    if (current_level > 0) {
        current_level--;
        Set_Hardware_PWM();
    }
}

uint8_t Brightness_GetLevel(void) {
    return current_level;
}
