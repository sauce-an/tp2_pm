/*
 * lcd_new.h
 *
 *  Created on: Mar 20, 2026
 *      Author: Andrew Jian
 */

#ifndef INC_LCD_NEW_H_
#define INC_LCD_NEW_H_

#include "main.h"
#include <stdint.h>

// Screen Dimensions
#define LCD_WIDTH       128
#define LCD_HEIGHT      64
#define LCD_BUFFER_SIZE (LCD_WIDTH * LCD_HEIGHT / 8)

// --- PCB ROUTING OPTIMIZATION ---
// 1 -> 180 flip
// 0 -> Original
#define LCD_FLIP_180    1


// LCD Hardware Configuration Struct
typedef struct {
    SPI_HandleTypeDef* spi_handle;
    GPIO_TypeDef* cs_port;
    uint16_t           cs_pin;
    uint8_t            frame_buffer[LCD_BUFFER_SIZE];
    uint8_t            dma_tx_buffer[3456]; // for DMA: 64 rows * 54 bytes per row
    volatile uint8_t   is_dma_busy;         // for DMA: Flag to prevent CPU overwrites
} LCD_Handle_t;

// --- Core Initialization & Control ---
void LCD_Init(LCD_Handle_t* lcd, SPI_HandleTypeDef* hspi, GPIO_TypeDef* cs_port, uint16_t cs_pin);
void LCD_ClearBuffer(LCD_Handle_t* lcd);
void LCD_RenderFrame(LCD_Handle_t* lcd); // Pushes the RAM buffer to the physical screen

// --- Geometry Drawing ---
void LCD_SetPixel(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t draw_pixel);
void LCD_DrawLineHorizontal(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t width, uint8_t draw_pixel);
void LCD_DrawLineVertical(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t height, uint8_t draw_pixel);
void LCD_DrawRectangle(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t draw_pixel);
void LCD_FillRectangle(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t draw_pixel);

// --- Text Drawing (5x7 Font) ---
void LCD_PrintChar(LCD_Handle_t* lcd, uint8_t x, uint8_t y, char character);
void LCD_PrintString(LCD_Handle_t* lcd, uint8_t x, uint8_t y, const char* text);

// --- Icon Bit-mapping for USB & WiFi ---
void LCD_DrawIcon_USB(LCD_Handle_t* lcd, uint8_t x, uint8_t y);
void LCD_DrawIcon_WiFi(LCD_Handle_t* lcd, uint8_t x, uint8_t y);

// --- Icon Bit-mapping for brightness indicator ---
void LCD_DrawIcon_Brightness(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t level);

#endif /* INC_LCD_NEW_H_ */
