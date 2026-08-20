/*
 * connect.c
 *
 *  Created on: Mar 30, 2026
 *      Author: Andrew Jian
 */


#include "connect.h"
#include "comm.h"
#include "main.h"
#include "menu.h"
#include "lcd_new.h"


extern UART_HandleTypeDef huart3;
extern uint8_t rx_byte;

// Global state variables
static ConnectionMode_t current_mode = CONN_NONE;
uint32_t health_check_start = 0;

// Helper: Safely restores UART when USB is plugged in
void Safe_UART_Restore(void) {
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;

    // Initialize the hardware with the manually injected settings
    HAL_UART_Init(&huart3);

    // Safely restart the receive interrupt
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
}

void Connect_Init(void) {
    // Force actual pin reading on boot to synchronize the hardware state machine
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_RESET) {
        current_mode = CONN_NONE;
        __HAL_UART_DISABLE(&huart3);
        HAL_UART_DeInit(&huart3);

        // Ensure pins are in Analog mode right from boot if unplugged
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    } else {
        current_mode = CONN_USB;
        //Safe_UART_Restore();
    }
}

void Connect_SetMode(ConnectionMode_t mode) {
    // 1. IGNORE REDUNDANT OVERWRITES: If the UI tries to set the same mode, ignore it
    if (current_mode == mode) {
        return;
    }

    // 2. HARDWARE RE-EVALUATION GATEWAY
    if (mode == CONN_USB) {
        // Only allow switching to USB mode if the cable is physically plugged in!
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET) {
            current_mode = CONN_USB;
            Safe_UART_Restore(); // Instantly spin up UART clocks and interrupts
        }
    }
    else if (mode == CONN_NONE) {
        // If switching away from USB (to WiFi/None), cleanly put hardware to sleep
        current_mode = CONN_NONE;
        __HAL_UART_DISABLE(&huart3);
        HAL_UART_DeInit(&huart3);

        // Turn TX/RX pins back to Analog to completely guard against LCD glitching noise
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11; // Your USART3 Pins
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    }
}

ConnectionMode_t Connect_GetMode(void) {
    return current_mode;
/*
    if (current_mode == CONN_USB) {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2) == GPIO_PIN_SET) {
            Safe_UART_Restore();
        }
    } */
}

// ======================================================================
// HARDWARE INTERRUPT CALLBACK
// ======================================================================
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_2) { // USB Detection Pin (PB2)

        // --- EDGE DETECTION LOGIC ---
        GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2);

        if (pin_state == GPIO_PIN_SET) {
            // -> RISING EDGE: USB PLUGGED IN (3.3V)

        	Comm_Init();
            current_mode = CONN_USB;
            health_check_start = HAL_GetTick();
            Safe_UART_Restore();
            Comm_SendTelemetry();

        }
        if (pin_state == GPIO_PIN_RESET){
            // -> FALLING EDGE: USB UNPLUGGED (0V)
            if (current_mode != CONN_NONE) {current_mode = CONN_NONE;}
            //current_mode = CONN_NONE;
            __HAL_UART_DISABLE(&huart3);
            HAL_UART_DeInit(&huart3);


            //Connect_Init();

            // PB10 (SEEDUINO TX) | PB11 (SEEDUINO RX)
            GPIO_InitTypeDef GPIO_InitStruct = {0};
            GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
            GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
            GPIO_InitStruct.Pull = GPIO_PULLDOWN;
            HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
        }
    }
}
