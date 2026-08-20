/*
 * AC_meas.h
 *
 *  Created on: May 9, 2026
 *      Author: Andrew Jian
 */

#ifndef INC_AC_MEAS_H_
#define INC_AC_MEAS_H_

#include "main.h"

uint8_t AC_Meas_GetRangeIsHigh(void);
void AC_Meas_Init(void);

// ADC3 for Voltage
// ADC1 for Current
void AC_Meas_Update(ADC_HandleTypeDef* hadc_ac_volt, ADC_HandleTypeDef* hadc_ac_curr);

#endif
