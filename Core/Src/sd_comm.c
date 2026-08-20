/*
 * sd_comm.c
 *
 *  Created on: Mar 30, 2026
 *      Author: Lauren Moffatt
 */

#include "sd_comm.h"

// RTC Handler
extern RTC_HandleTypeDef hrtc;

// FatFs Variables
FATFS FatFs; // Fatfs handle
FIL fil;	 // File handle

// Variables for
static bool logging_enabled = false;
char filename[256];

// Set up communication with SD card
void SD_start_logging(void)
{
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	// 'Refresh' FatFS
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
	FATFS_UnLinkDriver(USERPath);
	HAL_Delay(50);
	FATFS_LinkDriver(&USER_Driver, USERPath);

	f_mount(&FatFs, "", 0); // Mount SD card

	// Create filename with date and time stamp
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); // Fetch the time and date from hardware

	sprintf(filename, "RECORD_%02d%02d%04d_%02d%02d%02d.csv",
			sDate.Date, sDate.Month, sDate.Year,
			sTime.Hours, sTime.Minutes, sTime.Seconds);

	// Open file
	f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS);

	// CSV header
	char header[] = "Date, Time, DC Voltage (V), DC Current (A), DC Power (W), "
					"AC Voltage (V), AC Current (A), AC Frequency (Hz), AC Phase Difference "
					"(degrees), AC Real Power (W), AC Reactive Power (VAR), AC Apparent Power (VA),"
					"AC Power Factor, AC Peak-to-Peak Voltage (V), AC Peak-to-Peak Current (A)\n";

	f_write(&fil, header, strlen(header), NULL);

	return;
}

// Stop communication with SD card
void SD_stop_logging(void)
{
	f_close(&fil);
	f_mount(NULL, "", 0);
	HAL_Delay(50);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
}

// Toggle between SD logging enabled and disabled
void SD_toggle_logging(void)
{
	if (logging_enabled)
	{ // disable logging
		logging_enabled = false;
		SD_stop_logging();
	}
	else
	{ // enable logging
		logging_enabled = true;
		SD_start_logging();
	}
}

// Write to SD card if logging enabled
void SD_log_measurement(void)
{
	// Get time from RTC
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	// Create date/time buffer
	char datetime[32];
	sprintf(datetime, "%02d/%02d/%02d, %02d:%02d:%02d",
			sDate.Date, sDate.Month, sDate.Year,		// Date
			sTime.Hours, sTime.Minutes, sTime.Seconds); // Time

	// Get data in string format and change to UL if value is
	float data_array[13] = {live_dc_v, live_dc_i, live_dc_w, live_ac_v, live_ac_i, live_ac_freq, live_ac_phase,
							live_ac_real, live_ac_react, live_ac_app, live_ac_pf, live_ac_pp_v, live_ac_pp_i};

	char formatted_data[13][16];
	for (int i = 0; i < 13; i++)
	{
		if (data_array[i] == -999.0)
		{
			sprintf(formatted_data[i], "UL");
		}
		else
		{
			sprintf(formatted_data[i], "%.2f", data_array[i]);
		}
	}

	// Fill buffer that will be written to the sd card
	char buffer[256];
	UINT bytes_written;

	sprintf(buffer, "%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s\n",
			datetime,
			formatted_data[0],	 // live_dc_v
			formatted_data[1],	 // live_dc_i
			formatted_data[2],	 // live_dc_w
			formatted_data[3],	 // live_ac_v
			formatted_data[4],	 // live_ac_i
			formatted_data[5],	 // live_ac_freq
			formatted_data[6],	 // live_ac_phase
			formatted_data[7],	 // live_ac_real
			formatted_data[8],	 // live_ac_react
			formatted_data[9],	 // live_ac_app
			formatted_data[10],	 // live_ac_pf
			formatted_data[11],	 // live_ac_pp_v
			formatted_data[12]); // live_ac_pp_i

	// Write data to SD card
	if (logging_enabled)
	{
		f_write(&fil, buffer, strlen(buffer), &bytes_written);
	}

	return;
}
