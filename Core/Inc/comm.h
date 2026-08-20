/*
 * comm.h
 *
 *  Created on: Mar 17, 2026
 *      Author: Andrew Jian
 */

#ifndef INC_COMM_H_
#define INC_COMM_H_

#include "main.h"

// Public functions for PC communication

extern uint8_t fake_v_spec[500];
extern uint8_t fake_i_spec[500];

void Comm_Init(void);
void Comm_SendTelemetry(void);
void Comm_ReceiveCommand(void);
void Sync_RTC_Time(const char* time_str);

void Generate_Fake_Spectrum(void);

#endif /* INC_COMM_H_ */
