/*
 * comm.c
 *
 * Created on: Mar 17, 2026
 * Author: Andrew Jian
 */

#include "comm.h"
#include "menu.h"
#include "connect.h"
#include "lcd_new.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart3;
extern RTC_HandleTypeDef hrtc;
extern SPI_HandleTypeDef hspi3;
extern LCD_Handle_t main_lcd; // for LCD_Init

uint8_t rx_byte;
uint8_t rtc_just_synced = 0;

#define RX_CMD_MAX_LEN 16
char rx_cmd_buffer[RX_CMD_MAX_LEN];
uint8_t rx_cmd_index = 0;
uint8_t is_receiving_cmd = 0;

uint8_t fake_v_spec[500];
uint8_t fake_i_spec[500];

// --- HELPER: Fake data generator for spectra-graph (50Hz)
void Generate_Fake_Spectrum(void) {
    for (int i = 0; i < 500; i++) {
        // 1. Generate a low random noise floor (0 to 5)
        fake_v_spec[i] = rand() % 5;
        fake_i_spec[i] = rand() % 5;

        // 2. Inject a massive Fundamental Frequency spike at 50Hz
        if (i == 50) {
            fake_v_spec[i] = 250;
            fake_i_spec[i] = 200;
        }
        // 3. Inject smaller Harmonic spikes (150Hz, 250Hz, etc)
        else if (i == 150) {
            fake_v_spec[i] = 80;
            fake_i_spec[i] = 60;
        }
        else if (i == 250) {
            fake_v_spec[i] = 30;
            fake_i_spec[i] = 25;
        }
    }
}

void Comm_Init(void) {
    //HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

void Comm_SendTelemetry(void) {
    ConnectionMode_t current_mode = Connect_GetMode();

    // --- STATE CHANGE: JUST UNPLUGGED ---
    if (current_mode != CONN_USB) {
    	return;
    }

    // 1. Refresh the fake spectrum data array
    Generate_Fake_Spectrum();

    // --- PROCEED WITH TRANSMISSION ---
    char tx_buf[256];
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // 2. Format basic telemetry (Removed \r\n at the end so it stays on one line!)
    sprintf(tx_buf, "%02d:%02d:%02d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
            sTime.Hours, sTime.Minutes, sTime.Seconds,
            live_dc_v, live_dc_i, live_dc_w, live_ac_v, live_ac_i, live_ac_freq,
            live_ac_phase, live_ac_real, live_ac_react, live_ac_app, live_ac_pf,
            live_ac_pp_v, live_ac_pp_i, live_ac_thd_v, live_ac_thd_i);

    // Send the first ~100 characters of standard data (time + measurements)
    HAL_UART_Transmit(&huart3, (uint8_t*)tx_buf, strlen(tx_buf), 100);

    // 3. The Spectrum Header (Tells PC that Voltage array is starting)
    char spec_header[] = "|SPEC_V:";
    HAL_UART_Transmit(&huart3, (uint8_t*)spec_header, strlen(spec_header), 10);

    // 4. Stream the Voltage Spectrum (RAM Safe Hex Chunking!)
    char hex_chunk[3];
    for (int i = 0; i < 500; i++) {
        sprintf(hex_chunk, "%02X", fake_v_spec[i]);
        HAL_UART_Transmit(&huart3, (uint8_t*)hex_chunk, 2, 10);
    }

    // 5. The Spectrum Header for Current
    char spec_mid[] = "|SPEC_I:";
    HAL_UART_Transmit(&huart3, (uint8_t*)spec_mid, strlen(spec_mid), 10);

    // 6. Stream the Current Spectrum
    for (int i = 0; i < 500; i++) {
        sprintf(hex_chunk, "%02X", fake_i_spec[i]);
        HAL_UART_Transmit(&huart3, (uint8_t*)hex_chunk, 2, 10);
    }

    // 7. End of transmission line break (Now the PC knows the packet is complete)
    char tail[] = "\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t*)tail, 2, 10);
}

void Sync_RTC_Time(const char* time_str) {
    if (strlen(time_str) >= 14) {
        uint8_t day   = (time_str[0] - '0') * 10 + (time_str[1] - '0');
        uint8_t month = (time_str[2] - '0') * 10 + (time_str[3] - '0');
        uint8_t year  = (time_str[6] - '0') * 10 + (time_str[7] - '0');

        uint8_t hours   = (time_str[8] - '0') * 10 + (time_str[9] - '0');
        uint8_t minutes = (time_str[10] - '0') * 10 + (time_str[11] - '0');
        uint8_t seconds = (time_str[12] - '0') * 10 + (time_str[13] - '0');

        RTC_TimeTypeDef sTime = {0};
        sTime.Hours = hours;
        sTime.Minutes = minutes;
        sTime.Seconds = seconds;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

        RTC_DateTypeDef sDate = {0};
        sDate.WeekDay = RTC_WEEKDAY_MONDAY;
        sDate.Date = day;
        sDate.Month = month;
        sDate.Year = year;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        rtc_just_synced = 1;
    }
}

void Comm_ReceiveCommand(void) {
    if (rx_byte == 'T') {
        is_receiving_cmd = 1;
        rx_cmd_index = 0;
        memset(rx_cmd_buffer, 0, RX_CMD_MAX_LEN);
        return;
    }

    if (is_receiving_cmd == 1) {
        if (rx_byte == '\0') {
            rx_cmd_buffer[rx_cmd_index] = '\0';
            Sync_RTC_Time(rx_cmd_buffer);
            is_receiving_cmd = 0;
        }
        else {
            if (rx_cmd_index < (RX_CMD_MAX_LEN - 1)) {
                rx_cmd_buffer[rx_cmd_index++] = (char)rx_byte;
            } else {
                is_receiving_cmd = 0;
            }
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        Comm_ReceiveCommand();
        Connect_SetMode(CONN_USB);
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}

// --- STANDARD RECOVERY ---
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        // If a garbage byte hits the line, do NOT call Abort.
        // Just quietly wipe the error flags and keep listening.
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);

        huart->RxState = HAL_UART_STATE_READY;
        huart->Lock = HAL_UNLOCKED;
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}
