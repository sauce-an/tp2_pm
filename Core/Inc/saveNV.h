/*
 * saveNV.h
 *
 *  Created on: Apr 24, 2026
 *      Author: Andrew Jian
 */



#ifndef INC_SAVENV_H_
#define INC_SAVENV_H_

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

// The very last 2KB page of the 512KB STM32L4 flash (Dual Bank Mode)
#define FLASH_CONFIG_ADDR 0x0807F800
#define FLASH_CONFIG_PAGE 127          // Changed from 255
#define FLASH_CONFIG_BANK FLASH_BANK_2 // Explicitly target Bank 2

// Public Functions
bool NV_Save(void *data, uint16_t size_in_bytes);
bool NV_Load(void *data, uint16_t size_in_bytes);

#endif /* INC_SAVENV_H_ */
