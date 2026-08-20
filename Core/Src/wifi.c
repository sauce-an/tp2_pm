/*
 * wifi.c
 *
 *  Created on: Mar 30, 2026
 *  Author: Lauren Moffatt
 */

#include "wifi.h"
#include "menu.h"
#include "main.h"
#include "connect.h"
#include "comm.h"
#include "stdio.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>

#define RX_BUFFER_SIZE 512
#define ESP_RX_SIZE 512

extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef hrtc; // Link to the real hardware RTC in main.c

uint8_t esp_rx_buffer[ESP_RX_SIZE];
uint8_t esp_msg_buffer[ESP_RX_SIZE];
volatile uint16_t esp_msg_len = 0;
volatile uint8_t esp_msg_ready = 0;

/* Initialise connection as publisher with ESP32 module and as subscriber to RTC topic */
void Wifi_CreateConnection(void)
{
	char cmd[256];

	// Start listening
	HAL_UARTEx_ReceiveToIdle_IT(&huart1, esp_rx_buffer, 512);

	/* Set a MQTT configuration */
	sprintf(cmd, "AT+MQTTUSERCFG=0,7,\"firmware\",\"team14\",\"1be2118ccf0f01fdf95bd335bcd28df3\",0,0,\"ws\"\r\n");
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
	HAL_Delay(500);

	/* Connect to MQTT brokers (with automatic reconnection) */
	sprintf(cmd, "AT+MQTTCONN=0,\"tp-mqtt.uqcloud.net\",443,1\r\n");
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
	HAL_Delay(2000);

	/* subscribe to RTC topic */
	sprintf(cmd, "AT+MQTTSUB=0,\"2026s1/team14/time\",1\r\n");
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
	HAL_Delay(500);

	return;
}

void Wifi_SendTelemetry(void)
{
	char cmd[256];
	char transmitted_data[256];

	// get time from RTC
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0}; // Required to unlock shadow registers
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	// load data
	sprintf(transmitted_data,
			"%02d:%02d:%02d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
			sTime.Hours, sTime.Minutes, sTime.Seconds, // time
			live_dc_v,
			live_dc_i,
			live_dc_w,
			live_ac_v,
			live_ac_i,
			live_ac_freq,
			live_ac_phase,
			live_ac_real,
			live_ac_react,
			live_ac_app,
			live_ac_pf,
			live_ac_pp_v,
			live_ac_pp_i,
			live_ac_thd_v,
			live_ac_thd_i);

	/* Publish MQTT message */
	sprintf(cmd, "AT+MQTTPUBRAW=0,\"2026s1/team14/data\",%d,1,0\r\n", strlen(transmitted_data));
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
	HAL_Delay(50);
	snprintf(cmd, sizeof(cmd), "%s", transmitted_data); // send data
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
	HAL_Delay(50);

	return;
}

void Wifi_ReceiveRTC(char* msg)
{
	if (strstr(msg, "+MQTTSUBRECV:0,\"2026s1/team14/time\"") != NULL)
	{
		char *payload = strrchr(msg, 'T'); // find beginning of date/time string
		payload++;						   // Move past 'T'
		Sync_RTC_Time(payload);			   // Sync the received date/time string
	}

	return;
}

void Wifi_CloseConnection(void)
{
	char cmd[256];

	sprintf(cmd, "AT+MQTTCLEAN=0"); // close connection
	HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
	HAL_Delay(50);

	return;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if (huart->Instance == USART1) // change if huart1 uses different USART
	{
		memset(esp_msg_buffer, 0, ESP_RX_SIZE);

		if (Size < ESP_RX_SIZE)
		{
			memcpy(esp_msg_buffer, esp_rx_buffer, Size);
			esp_msg_len = Size;
			esp_msg_ready = 1;
		}

		// Handle if RTC time was received
		char *msg = (char *)esp_msg_buffer;
		Wifi_ReceiveRTC(msg);

		// Restart reception
		HAL_UARTEx_ReceiveToIdle_IT(&huart1, esp_rx_buffer, ESP_RX_SIZE);
	}
}
