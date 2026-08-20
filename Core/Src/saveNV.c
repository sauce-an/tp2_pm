/*
 * saveNV.c
 *
 *  Created on: Apr 24, 2026
 *      Author: Andrew Jian
 */


#include "saveNV.h"
#include "main.h"
#include <string.h>

bool NV_Save(void *data, uint16_t size_in_bytes) {
    uint32_t double_words_to_write = (size_in_bytes + 7) / 8;
    uint64_t write_buffer[double_words_to_write];

    memset(write_buffer, 0xFF, sizeof(write_buffer));
    memcpy(write_buffer, data, size_in_bytes);

    // stop background interrupts from crashing the CPU during Flash write
    __disable_irq();

    HAL_FLASH_Unlock();
    // clear errors from debugger
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    // Erase the target page
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Banks     = FLASH_CONFIG_BANK;
    EraseInitStruct.Page      = FLASH_CONFIG_PAGE; // page 127
    EraseInitStruct.NbPages   = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        HAL_FLASH_Lock();
        __enable_irq();
        return false;
    }

    // write bugger -> silicon
    for (uint32_t i = 0; i < double_words_to_write; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                              FLASH_CONFIG_ADDR + (i * 8),
                              write_buffer[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            __enable_irq();
            return false;
        }
    }

    HAL_FLASH_Lock();

    // CRITICAL: Flush the CPU Data Cache so immediate reads see the new data!
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET();
    __HAL_FLASH_DATA_CACHE_ENABLE();

    __enable_irq();
    return true;
}

bool NV_Load(void *data, uint16_t size_in_bytes) {
    // Point directly to the flash address
    uint32_t *flash_ptr = (uint32_t *)FLASH_CONFIG_ADDR;

    // If the very first 32-bits are 0xFFFFFFFF, the page has never been
    // written to (it is factory blank).
    if (*flash_ptr == 0xFFFFFFFF) {
        return false;
    }

    // copy the raw bytes from flash into the user's struct
    memcpy(data, (void*)FLASH_CONFIG_ADDR, size_in_bytes);
    return true;
}
