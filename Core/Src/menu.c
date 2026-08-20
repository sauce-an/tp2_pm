/*
 * menu.c
 *
 *  Created on: Mar 15, 2026
 *      Author: Andrew Jian
 */
#include "menu.h"
#include "connect.h"
//#include "lcd.h"
#include "lcd_new.h"
#include <stdio.h>
#include <string.h>
#include "saveNV.h"
#include "brightness.h"
#include "AC_meas.h"

// --- RTC Handler ---
extern RTC_HandleTypeDef hrtc;

// --- FSM STATES ---
typedef enum {
    STATE_MAIN,
    STATE_DC,
    STATE_AC,
    STATE_TABLE,
    STATE_SETTINGS,
    STATE_EDIT_RATIO,
    STATE_READING
} MenuState_t;

// --- READING TYPES FOR THE UNIFIED READING STATE ---
typedef enum {
    READ_DC_V, READ_DC_I, READ_DC_W,
    READ_AC_V, READ_AC_I, READ_AC_FREQ, READ_AC_PHASE,
    READ_AC_REAL, READ_AC_REACT, READ_AC_APP, READ_AC_PF,
    READ_AC_PP_V, READ_AC_PP_I, READ_THD_V, READ_THD_I
} ReadingType_t;

// --- INTERNAL FSM VARIABLES ---
MenuState_t current_state = STATE_MAIN;
ReadingType_t current_reading = READ_DC_V;

uint8_t cursor_pos = 0;
uint8_t ac_page = 1;

/// Table Storage (these four are defult readings)
ReadingType_t table_selections[4] = {
    READ_DC_V,
    READ_DC_I,
    READ_AC_V,
    READ_AC_I
};

DataMode_t user_data_mode = MODE_USB; // Default to AUTO

// --- EXPORT GETTER FOR MAIN.C ---
DataMode_t Menu_GetDataMode(void) {
    return user_data_mode;
}

// Settings Edit Variables
uint8_t ratio_digits[5] = {0, 1, 0, 0, 0}; // Defaults to 01.000
uint8_t edit_digit_idx = 0;
float step_down_ratio = 1.0f; // Now a float to hold the decimal value

// Button Debounce Tracking
uint8_t btn_down_last = 1;   // Was btn_scroll_last
uint8_t btn_up_last = 1;
uint8_t btn_select_last = 1;
uint32_t last_draw_time = 0;

// --- LIVE DATA VARIABLES ---
float live_dc_v = 0.0, live_dc_i = 0.0, live_dc_w = 0.0;
float live_ac_v = 0.0, live_ac_i = 0.0, live_ac_freq = 0.0, live_ac_phase = 0.0;
float live_ac_real = 0.0, live_ac_react = 0.0, live_ac_app = 0.0, live_ac_pf = 0.0;
float live_ac_pp_v = 0.0, live_ac_pp_i = 0.0, live_ac_thd_v = 0.0, live_ac_thd_i = 0.0;

// The local struct holding all user settings (for non-volatile memory)
typedef struct {
    ReadingType_t table_mem[4];
    float ratio_mem;
    uint8_t digits_mem[5];
    DataMode_t mode_mem;
} DeviceConfig_t;

// --- HELPER for syncing config to flash memory as non-volatile mem
void Sync_Config_To_Flash(void) {
    DeviceConfig_t config;

    config.table_mem[0] = table_selections[0];
    config.table_mem[1] = table_selections[1];
    config.table_mem[2] = table_selections[2];
    config.table_mem[3] = table_selections[3];
    config.ratio_mem = step_down_ratio;
    for(int i=0; i<5; i++) config.digits_mem[i] = ratio_digits[i];

    config.mode_mem = user_data_mode; // pack the selected mode

    NV_Save(&config, sizeof(DeviceConfig_t));
}


// --- HELPER: RENDER CURSOR ---
char GetCursor(uint8_t line_idx) {
    return (cursor_pos == line_idx) ? '>' : ' ';
}

// --- HELPER: ADD TO CUSTOM SCREEN ---
void Log_To_Table(ReadingType_t new_reading) {
    // Shift old selections up
    table_selections[0] = table_selections[1];
    table_selections[1] = table_selections[2];
    table_selections[2] = table_selections[3];
    // Insert new reading at the bottom
    table_selections[3] = new_reading;

    Sync_Config_To_Flash(); // saving the table
}


// --- HELPER: FORMAT LIVE DATA WITH UNITS ---
void Format_Reading(ReadingType_t type, char* buf) {
    // Helper booleans to check if primary signals are Under Limit (UL)
    uint8_t v_is_ul = (live_ac_v == -999.0f);
    uint8_t i_is_ul = (live_ac_i == -999.0f);

    switch(type) {
        // --- DC MEASUREMENTS ---
        case READ_DC_V:     sprintf(buf, "DC V: %5.2f V   ", live_dc_v); break;
        case READ_DC_I:     sprintf(buf, "DC I: %5.2f A   ", live_dc_i); break;
        case READ_DC_W:     sprintf(buf, "DC W: %5.2f W   ", live_dc_w); break;

        // --- PRIMARY AC MEASUREMENTS ---
        case READ_AC_V:
            if (v_is_ul) sprintf(buf, "AC V: UL        ");
            else sprintf(buf, "AC V: %5.2f V   ", live_ac_v);
            break;

        case READ_AC_I:
        	if (i_is_ul) sprintf(buf, "AC I: UL     [%c]", AC_Meas_GetRangeIsHigh() ? 'H' : 'L');
        	else sprintf(buf, "AC I:%5.2fA [%c]", live_ac_i, AC_Meas_GetRangeIsHigh() ? 'H' : 'L');
            break;

        // --- SECONDARY AC MEASUREMENTS (Dependent on V) ---
        case READ_AC_FREQ:
            if (v_is_ul) sprintf(buf, "Freq: UL        ");
            else sprintf(buf, "Freq: %5.2f Hz  ", live_ac_freq);
            break;

        case READ_AC_PP_V:
            if (v_is_ul) sprintf(buf, "Vpp:  UL        ");
            else sprintf(buf, "Vpp: %5.2f V    ", live_ac_pp_v);
            break;

        case READ_THD_V:
            if (v_is_ul) sprintf(buf, "THDv: UL        ");
            else sprintf(buf, "THDv: %5.2f %%  ", live_ac_thd_v);
            break;

        // --- SECONDARY AC MEASUREMENTS (Dependent on I) ---
        case READ_AC_PP_I:
            if (i_is_ul) sprintf(buf, "Ipp:  UL        ");
            else sprintf(buf, "Ipp: %5.2f A    ", live_ac_pp_i);
            break;

        case READ_THD_I:
            if (i_is_ul) sprintf(buf, "THDi: UL        ");
            else sprintf(buf, "THDi: %5.2f %%  ", live_ac_thd_i);
            break;

        // --- POWER & PHASE MEASUREMENTS (Dependent on BOTH V and I) ---
        case READ_AC_PHASE:
            if (v_is_ul || i_is_ul) sprintf(buf, "Phs:  UL        ");
            else sprintf(buf, "Phs: %5.2f deg  ", live_ac_phase);
            break;

        case READ_AC_PF:
            if (v_is_ul || i_is_ul) sprintf(buf, "PF:   UL        ");
            else sprintf(buf, "PF: %5.2f       ", live_ac_pf);
            break;

        case READ_AC_APP:
            if (v_is_ul || i_is_ul) sprintf(buf, "App:  UL        ");
            else sprintf(buf, "App: %5.2f VA   ", live_ac_app);
            break;

        case READ_AC_REAL:
            if (v_is_ul || i_is_ul) sprintf(buf, "Real: UL        ");
            else sprintf(buf, "Real: %5.2f W   ", live_ac_real);
            break;

        case READ_AC_REACT:
            if (v_is_ul || i_is_ul) sprintf(buf, "Reac: UL        ");
            else sprintf(buf, "Reac: %5.2f VAR ", live_ac_react);
            break;
    }

    // Forcibly cap the string at exactly 16 characters to prevent memory overflow.
    buf[16] = '\0';
}


// --- SCREEN RENDERING (GRAPHIC MODE) ---
void Draw_Screen(void) {
    char buf1[32], buf2[32], buf3[32], buf4[32];
    extern LCD_Handle_t main_lcd;

    // 1. Wipe the invisible RAM buffer completely clean (zero ghosting!)
    LCD_ClearBuffer(&main_lcd);

    // 2. Pre-fill buffers with blanks just in case a state forgets to write to one
    sprintf(buf1, " "); sprintf(buf2, " ");
    sprintf(buf3, " "); sprintf(buf4, " ");

    // 3. FSM Routing: Fill the 4 buffers based on current state
    switch(current_state) {
        case STATE_MAIN:
            sprintf(buf1, "%cDC", GetCursor(0));
            sprintf(buf2, "%cAC", GetCursor(1));
            sprintf(buf3, "%cTABLE", GetCursor(2));
            sprintf(buf4, "%cSettings", GetCursor(3));
            break;

        case STATE_DC:
            sprintf(buf1, "%cDC voltage(V)", GetCursor(0));
            sprintf(buf2, "%cDC current(A)", GetCursor(1));
            sprintf(buf3, "%cDC power (W)", GetCursor(2));
            sprintf(buf4, "%cHome", GetCursor(3));
            break;

        case STATE_AC:
            if (ac_page == 1) {
                sprintf(buf1, "%cAC voltage(V)", GetCursor(0));
                sprintf(buf2, "%cAC current(A)", GetCursor(1));
                sprintf(buf3, "%c-->", GetCursor(2));
                sprintf(buf4, "%cHome", GetCursor(3));
            } else if (ac_page == 2) {
                sprintf(buf1, "%cAC freq (Hz)", GetCursor(0));
                sprintf(buf2, "%cAC phase(deg)", GetCursor(1));
                sprintf(buf3, "%c-->", GetCursor(2));
                sprintf(buf4, "%cHome", GetCursor(3));
            } else if (ac_page == 3) {
                sprintf(buf1, "%cAC real(W)", GetCursor(0));
                sprintf(buf2, "%cAC react(VAR)", GetCursor(1));
                sprintf(buf3, "%c-->", GetCursor(2));
                sprintf(buf4, "%cHome", GetCursor(3));
            } else if (ac_page == 4) {
                sprintf(buf1, "%cAC app pwr(VA)", GetCursor(0));
                sprintf(buf2, "%cAC PF", GetCursor(1));
                sprintf(buf3, "%c-->", GetCursor(2));
                sprintf(buf4, "%cHome", GetCursor(3));
            } else if (ac_page == 5) {
                sprintf(buf1, "%cAC p-p V (V)", GetCursor(0));
                sprintf(buf2, "%cAC p-p I (A)", GetCursor(1));
                sprintf(buf3, "%c-->", GetCursor(2));
                sprintf(buf4, "%cHome", GetCursor(3));
            } else if (ac_page == 6) {
                sprintf(buf1, "%cTHD volt (%%)", GetCursor(0));
                sprintf(buf2, "%cTHD curr (%%)", GetCursor(1));
                sprintf(buf3, "%cHome", GetCursor(2));
                // buf4 remains blank
            }
            break;

        case STATE_TABLE:
            // Fetch the 4 live readings dynamically!
            Format_Reading(table_selections[0], buf1);
            Format_Reading(table_selections[1], buf2);
            Format_Reading(table_selections[2], buf3);
            Format_Reading(table_selections[3], buf4);
            break;

        case STATE_SETTINGS:
            // Dynamically show the current mode next to the cursor
            sprintf(buf1, "%cMode: %s", GetCursor(0), (user_data_mode == MODE_USB) ? "USB ONLY " : "WIFI ONLY");
            sprintf(buf2, "%cEdit Ratio", GetCursor(1));
            sprintf(buf3, "%cSave (Ratio)", GetCursor(2));
            sprintf(buf4, "%cHome", GetCursor(3));
            break;

        case STATE_EDIT_RATIO:
            sprintf(buf1, " Set Ratio:");
            sprintf(buf2, " Val: %d%d.%d%d%d",
                    ratio_digits[0], ratio_digits[1],
                    ratio_digits[2], ratio_digits[3], ratio_digits[4]);

            // Draw caret underneath the active digit
            if(edit_digit_idx == 0)      sprintf(buf3, "      ^");
            else if(edit_digit_idx == 1) sprintf(buf3, "       ^");
            else if(edit_digit_idx == 2) sprintf(buf3, "         ^");
            else if(edit_digit_idx == 3) sprintf(buf3, "          ^");
            else if(edit_digit_idx == 4) sprintf(buf3, "           ^");
            break;

        case STATE_READING:
            Format_Reading(current_reading, buf1);
            sprintf(buf2, "%cAdd to Table", GetCursor(0));
            // buf3 remains blank for visual spacing
            sprintf(buf4, "%cBack", GetCursor(1));
            break;
    }

    // --- 4. GRAPHIC CANVAS ASSEMBLY ---

    // Top Row (Y=0): Render the Hardware RTC Time
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // Fetch the time and date from hardware
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    char rtc_header[32];

    sprintf(rtc_header, "%02d%02d%04d %02d:%02d:%02d", sDate.Date, sDate.Month, 2000 + sDate.Year, sTime.Hours, sTime.Minutes, sTime.Seconds);
    LCD_PrintString(&main_lcd, 0, 0, rtc_header);

    // Top Row Right (X=118, Y=0): Render Active Connection Icon
    //ConnectionMode_t current_conn = Connect_GetMode();
    if (user_data_mode == MODE_WIFI) {
    	LCD_DrawIcon_WiFi(&main_lcd, 118, 0);
    	Connect_SetMode(CONN_WIFI);
    } else if (user_data_mode == MODE_USB) {
    	if (Connect_GetMode() == CONN_USB) {LCD_DrawIcon_USB(&main_lcd, 118, 0);}
    } // If CONN_NONE, it draws nothing

    // Render Brightness Indicator (Bottom Right)
    LCD_DrawIcon_Brightness(&main_lcd, 116, 53, Brightness_GetLevel());

    // Separator Line (Y=9): Clean visual break under the clock
    LCD_DrawLineHorizontal(&main_lcd, 0, 9, 128, 1);

    // Menu Rows (Y=12, 24, 36, 48): spaced 12 pixels apart
    LCD_PrintString(&main_lcd, 0, 12, buf1);
    LCD_PrintString(&main_lcd, 0, 24, buf2);
    LCD_PrintString(&main_lcd, 0, 36, buf3);
    LCD_PrintString(&main_lcd, 0, 48, buf4);

    // --- 5. PUSH TO HARDWARE ---
    // Blast the completed RAM buffer to the physical ST7920 screen
    LCD_RenderFrame(&main_lcd);
}

// --- INPUT HANDLERS ---
void Handle_Scroll_Down(void) {
    if (current_state == STATE_MAIN || current_state == STATE_DC) {
        cursor_pos++; if(cursor_pos > 3) cursor_pos = 0;
    }
    else if (current_state == STATE_AC) {
        if (ac_page <= 5) {
            cursor_pos++; if(cursor_pos > 3) cursor_pos = 0;
        } else {
            cursor_pos++; if(cursor_pos > 2) cursor_pos = 0; // Page 6 only has 3 items
        }
    }
    else if (current_state == STATE_SETTINGS) {
        cursor_pos++; if(cursor_pos > 3) cursor_pos = 0;
    }
    else if (current_state == STATE_READING) {
        cursor_pos++; if(cursor_pos > 1) cursor_pos = 0;
    }
    else if (current_state == STATE_TABLE) {
        current_state = STATE_MAIN; cursor_pos = 0;
    }
    else if (current_state == STATE_EDIT_RATIO) {
        // Down button decreases the digit!
        if(ratio_digits[edit_digit_idx] == 0) ratio_digits[edit_digit_idx] = 9;
        else ratio_digits[edit_digit_idx]--;
    }

}

void Handle_Scroll_Up(void) {
    if (current_state == STATE_MAIN || current_state == STATE_DC) {
        if(cursor_pos == 0) cursor_pos = 3; else cursor_pos--;
    }
    else if (current_state == STATE_AC) {
        if (ac_page <= 5) {
            if(cursor_pos == 0) cursor_pos = 3; else cursor_pos--;
        } else {
            if(cursor_pos == 0) cursor_pos = 2; else cursor_pos--; // Page 6 only has 3 items
        }
    }
    else if (current_state == STATE_SETTINGS) {
        if(cursor_pos == 0) cursor_pos = 3; else cursor_pos--;
    }
    else if (current_state == STATE_READING) {
        if(cursor_pos == 0) cursor_pos = 1; else cursor_pos--;
    }
    else if (current_state == STATE_TABLE) {
        current_state = STATE_MAIN; cursor_pos = 0;
    }
    else if (current_state == STATE_EDIT_RATIO) {
        // Up button increases the digit!
        ratio_digits[edit_digit_idx]++;
        if(ratio_digits[edit_digit_idx] > 9) ratio_digits[edit_digit_idx] = 0;
    }
}
void Handle_Select(void) {
    switch(current_state) {
        case STATE_MAIN:
            if(cursor_pos == 0) { current_state = STATE_DC; }
            else if(cursor_pos == 1) { current_state = STATE_AC; ac_page = 1; }
            else if(cursor_pos == 2) { current_state = STATE_TABLE; }
            else if(cursor_pos == 3) { current_state = STATE_SETTINGS; }
            cursor_pos = 0;
            break;

        case STATE_DC:
            if(cursor_pos == 0) { current_state = STATE_READING; current_reading = READ_DC_V; }
            else if(cursor_pos == 1) { current_state = STATE_READING; current_reading = READ_DC_I; }
            else if(cursor_pos == 2) { current_state = STATE_READING; current_reading = READ_DC_W; }
            else if(cursor_pos == 3) { current_state = STATE_MAIN; }
            cursor_pos = 0;
            break;

        case STATE_AC:
            if (cursor_pos == 3 && ac_page <= 5) { current_state = STATE_MAIN; cursor_pos = 0; break; }
            if (cursor_pos == 2 && ac_page == 6) { current_state = STATE_MAIN; cursor_pos = 0; break; }

            if (cursor_pos == 2 && ac_page <= 5) {
                ac_page++; cursor_pos = 0; // The "-->" next page button
            } else {
                // Route to the correct reading based on page and cursor
                current_state = STATE_READING;
                if(ac_page == 1 && cursor_pos == 0) current_reading = READ_AC_V;
                else if(ac_page == 1 && cursor_pos == 1) current_reading = READ_AC_I;
                else if(ac_page == 2 && cursor_pos == 0) current_reading = READ_AC_FREQ;
                else if(ac_page == 2 && cursor_pos == 1) current_reading = READ_AC_PHASE;
                else if(ac_page == 3 && cursor_pos == 0) current_reading = READ_AC_REAL;
                else if(ac_page == 3 && cursor_pos == 1) current_reading = READ_AC_REACT;
                else if(ac_page == 4 && cursor_pos == 0) current_reading = READ_AC_APP;
                else if(ac_page == 4 && cursor_pos == 1) current_reading = READ_AC_PF;
                else if(ac_page == 5 && cursor_pos == 0) current_reading = READ_AC_PP_V;
                else if(ac_page == 5 && cursor_pos == 1) current_reading = READ_AC_PP_I;
                else if(ac_page == 6 && cursor_pos == 0) current_reading = READ_THD_V;
                else if(ac_page == 6 && cursor_pos == 1) current_reading = READ_THD_I;

                cursor_pos = 0;
            }
            break;

        case STATE_TABLE:
            current_state = STATE_MAIN;
            cursor_pos = 0;
            break;

        case STATE_SETTINGS:
        	if(cursor_pos == 0) {
        	    // TOGGLE THE DATA MODE!
        	    user_data_mode = (user_data_mode == MODE_USB) ? MODE_WIFI : MODE_USB;
        	    Sync_Config_To_Flash();
        	    if (user_data_mode == MODE_USB) {
        	        Connect_SetMode(CONN_USB);  // Attempts to wake UART if cable is present
        	    } else {
        	        Connect_SetMode(CONN_NONE); // Forces UART back to sleep/analog mode
        	    }
        	    break; // Do NOT reset cursor_pos here, so the user can see the text change!
        	}
        	else if(cursor_pos == 1) {
        	    current_state = STATE_EDIT_RATIO;
        	    edit_digit_idx = 0;
        	}
            else if(cursor_pos == 2) {
            // Save the 5 digits into a single floating-point decimal (XX.XXX)
            step_down_ratio = (ratio_digits[0] * 10.0f) +
                                  (ratio_digits[1] * 1.0f) +
                                  (ratio_digits[2] * 0.1f) +
                                 (ratio_digits[3] * 0.01f) +
                                  (ratio_digits[4] * 0.001f);

            Sync_Config_To_Flash(); // saving step down ratio
            current_state = STATE_MAIN;
            }
            else if(cursor_pos == 3) { current_state = STATE_MAIN; }
            cursor_pos = 0;
            break;

        case STATE_EDIT_RATIO:
             edit_digit_idx++;
             // We now have 5 digits to click through (indexes 0 to 4)
             if(edit_digit_idx > 4) {
                // Finished editing all 5 digits, go back to settings menu
                current_state = STATE_SETTINGS;
                cursor_pos = 1; // Hover over "Enter (Save)"
             }
             break;


        case STATE_READING:
            if(cursor_pos == 0) {
                // Just pass the Enum to the queue
                Log_To_Table(current_reading);
            } else if (cursor_pos == 1) {
                // Back button logic
                if (current_reading <= READ_DC_W) current_state = STATE_DC;
                else current_state = STATE_AC;
            }
            cursor_pos = 0;
            break;
    }
}

// --- PUBLIC FUNCTIONS ---
void Menu_Init(void) {
    DeviceConfig_t config;

    // If NV_Load returns true, it found data! Unpack it.
    if (NV_Load(&config, sizeof(DeviceConfig_t))) {
        table_selections[0] = config.table_mem[0];
        table_selections[1] = config.table_mem[1];
        table_selections[2] = config.table_mem[2];
        table_selections[3] = config.table_mem[3];
        step_down_ratio = config.ratio_mem;
        for(int i=0; i<5; i++) ratio_digits[i] = config.digits_mem[i];
        user_data_mode = config.mode_mem;
    }
    // If NV_Load returns false, it does nothing and leaves your defaults alone.

    Draw_Screen();
}


void Menu_Update(void) {
    // Read the current physical state of the pins
    uint8_t down_now   = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7); // PA7 ↓
    uint8_t select_now = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0); // PB0
    uint8_t up_now     = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1); // PB1 ↑

    // --- DOWN BUTTON DEBOUNCE (PA6) ---
    if(btn_down_last == 1 && down_now == 0) {
        HAL_Delay(10); // shrink debounce because of hardware deboucne
        if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == 0) {
            Handle_Scroll_Down();
            Draw_Screen();
        }
    }

    // --- UP BUTTON DEBOUNCE (PB1) ---
    if(btn_up_last == 1 && up_now == 0) {
        HAL_Delay(10);
        if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == 0) {
            Handle_Scroll_Up();
            Draw_Screen();
        }
    }

    // --- SELECT BUTTON DEBOUNCE (PB0) ---
    if(btn_select_last == 1 && select_now == 0) {
        HAL_Delay(10);
        if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == 0) {
            Handle_Select();
            Draw_Screen();
        }
    }

    // Save the current state for the next loop
    btn_down_last   = down_now;
    btn_up_last     = up_now;
    btn_select_last = select_now;

    // --- BRIGHTNESS BUTTONS (PB4 & PB5) ---
    static uint8_t btn_bright_up_last = 1;
    static uint8_t btn_bright_down_last = 1;

   uint8_t bright_up_now = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);
   uint8_t bright_down_now = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);

   // Brightness + (PA6)
   if (btn_bright_up_last == 1 && bright_up_now == 0) {
        HAL_Delay(10);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == 0) {
            Brightness_Increase();
            Draw_Screen(); // Force an immediate screen redraw to update the icon!
        }
    }

    // Brightness - (PA5)
    if (btn_bright_down_last == 1 && bright_down_now == 0) {
        HAL_Delay(10);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == 0) {
            Brightness_Decrease();
            Draw_Screen();
        }
    }

    btn_bright_up_last = bright_up_now;
    btn_bright_down_last = bright_down_now;

}

