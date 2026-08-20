/*
 * DC_meas.h
 *
 * Driver for DC Current, Voltage, and Power measurements.
 */

#ifndef INC_DC_MEAS_H_
#define INC_DC_MEAS_H_

#include "main.h"

void DC_Meas_Init(ADC_HandleTypeDef* hadc_dc);
void DC_Meas_Update(ADC_HandleTypeDef* hadc_dc);

#endif /* INC_DC_MEAS_H_ */
