/*
 * menu.h
 *
 *  Created on: Mar 15, 2026
 *      Author: Andrew Jian
 */

/*
 * PA0: Cursor Move (Down/Wrap)
 * PA1: Select / Confirm
 * */

#ifndef INC_MENU_H_
#define INC_MENU_H_

#include "main.h"

// Initialize the menu system (Call before while(1))
void Menu_Init(void);

// The core FSM engine (Call inside while(1))
void Menu_Update(void);
void Draw_Screen(void);

// --- GLOBAL LIVE READING VARIABLES ---
// Update these variables from ADC/Calculation code in main.c
// The menu system will automatically read them to display live values!
extern float live_dc_v;
extern float live_dc_i;
extern float live_dc_w;

extern float live_ac_v;
extern float live_ac_i;
extern float live_ac_freq;
extern float live_ac_phase;
extern float live_ac_real;
extern float live_ac_react;
extern float live_ac_app;
extern float live_ac_pf;
extern float live_ac_pp_v;
extern float live_ac_pp_i;
extern float live_ac_thd_v;
extern float live_ac_thd_i;

extern float step_down_ratio; // The result of your settings screen

// --- DATA MODE SELECTION ---
typedef enum {
    MODE_USB,
    MODE_WIFI   // Forces Wi-Fi telemetry
} DataMode_t;

// --- EXPORT GETTER FOR MAIN.C ---
DataMode_t Menu_GetDataMode(void);

#endif /* INC_MENU_H_ */
