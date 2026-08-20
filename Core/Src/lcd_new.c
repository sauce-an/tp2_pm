/*
 * lcd_new.c
 *
 *  Created on: Mar 20, 2026
 *      Author: Andrew Jian
 *
 *  	   REF: https://github.com/cbm80amiga/ST7920_SPI
 *  	   REF: https://www.buydisplay.com/download/democode/ERM12864-6_Serial_DemoCode.txt
 */


#include "lcd_new.h"
#include <string.h>

// --- ST7920 Hardware Commands ---
#define CMD_BASIC_INSTR       0x30
#define CMD_EXTENDED_INSTR    0x34
#define CMD_GRAPHIC_ON        0x36
#define CMD_TURN_ON_DISPLAY   0x0C
#define CMD_CLEAR_SCREEN      0x01
#define CMD_ENTRY_MODE        0x06

// --- Internal Hardware Abstraction Helpers ---
static void Transmit_Raw_SPI(LCD_Handle_t* lcd, uint8_t sync_byte, uint8_t payload) {
    uint8_t spi_packet[3];
    spi_packet[0] = sync_byte;
    spi_packet[1] = payload & 0xF0;
    spi_packet[2] = (payload << 4) & 0xF0;

    HAL_GPIO_WritePin(lcd->cs_port, lcd->cs_pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(lcd->spi_handle, spi_packet, 3, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(lcd->cs_port, lcd->cs_pin, GPIO_PIN_RESET);
}

static void Send_Command(LCD_Handle_t* lcd, uint8_t cmd) {
    Transmit_Raw_SPI(lcd, 0xF8, cmd);
    HAL_Delay(1); // ST7920 commands require slight delay
}

static void Send_Data(LCD_Handle_t* lcd, uint8_t data) {
    Transmit_Raw_SPI(lcd, 0xFA, data);
}

static void Set_Graphic_Address(LCD_Handle_t* lcd, uint8_t word_x, uint8_t row_y) {
    // Talk directly to SPI, skipping the HAL_Delay(1) bottleneck!
    if (row_y < 32) {
        Transmit_Raw_SPI(lcd, 0xF8, 0x80 | row_y);
        Transmit_Raw_SPI(lcd, 0xF8, 0x80 | word_x);
    } else {
        Transmit_Raw_SPI(lcd, 0xF8, 0x80 | (row_y - 32));
        Transmit_Raw_SPI(lcd, 0xF8, 0x88 | word_x);
    }
}
// --- Public Core Functions ---
void LCD_Init(LCD_Handle_t* lcd, SPI_HandleTypeDef* hspi, GPIO_TypeDef* cs_port, uint16_t cs_pin) {
    lcd->spi_handle = hspi;
    lcd->cs_port = cs_port;
    lcd->cs_pin = cs_pin;

    // clear local RAM buffer
    memset(lcd->frame_buffer, 0, LCD_BUFFER_SIZE);

    // Force CS pin LOW (ST7920 CS is Active-High, so resting state must be LOW)
    HAL_GPIO_WritePin(lcd->cs_port, lcd->cs_pin, GPIO_PIN_RESET);

    // Hardware Reset (Recovers the LCD from frozen states)
    // PC6 is physically wired to the LCD's RST pin (updated)
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_Delay(50); // Hold reset low
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(50); // Wait for the LCD processor to wake up

    // Software Initialization Sequence
    Send_Command(lcd, CMD_BASIC_INSTR);     // 0x30: Wake up in 8-bit text mode
    HAL_Delay(2);
    Send_Command(lcd, CMD_BASIC_INSTR);     // Send again to ensure synchronization
    HAL_Delay(2);
    Send_Command(lcd, CMD_TURN_ON_DISPLAY); // 0x0C: Display ON, cursor OFF
    HAL_Delay(2);
    Send_Command(lcd, CMD_CLEAR_SCREEN);    // 0x01: Wipe the DDRAM
    HAL_Delay(20);                          // Clearing is the slowest operation, wait 20ms
    Send_Command(lcd, CMD_ENTRY_MODE);      // 0x06: Auto-increment address
    HAL_Delay(2);

    // Switch to Graphic Mode
    Send_Command(lcd, CMD_EXTENDED_INSTR);  // 0x34: Unlock extended instruction set
    HAL_Delay(2);
    Send_Command(lcd, CMD_GRAPHIC_ON);      // 0x36: Turn on graphic rendering engine
    HAL_Delay(10);                          // Give the graphic engine time to boot

    // Flush our clean, empty RAM buffer to the physical screen
    LCD_ClearBuffer(lcd);
    LCD_RenderFrame(lcd);
}

void LCD_ClearBuffer(LCD_Handle_t* lcd) {
    memset(lcd->frame_buffer, 0, LCD_BUFFER_SIZE);
}

// --- DMA utilization ---
void LCD_RenderFrame(LCD_Handle_t* lcd) {
    // 1. If DMA is still sending the last frame, wait here so we don't corrupt the data
    while(lcd->is_dma_busy) {
        // CPU waits here for microseconds (or you can just 'return;' to drop the frame)
    }

    // 2. Rapidly format the 1,024-byte frame buffer into the 3,456-byte ST7920 SPI protocol
    uint16_t ptr = 0;
    for (uint8_t y = 0; y < 64; y++) {
        // Format Y Address Command
        uint8_t addr_y = (y < 32) ? (0x80 | y) : (0x80 | (y - 32));
        lcd->dma_tx_buffer[ptr++] = 0xF8;
        lcd->dma_tx_buffer[ptr++] = addr_y & 0xF0;
        lcd->dma_tx_buffer[ptr++] = (addr_y << 4) & 0xF0;

        // Format X Address Command (Always 0 for the start of the row)
        uint8_t addr_x = (y < 32) ? 0x80 : 0x88;
        lcd->dma_tx_buffer[ptr++] = 0xF8;
        lcd->dma_tx_buffer[ptr++] = addr_x & 0xF0;
        lcd->dma_tx_buffer[ptr++] = (addr_x << 4) & 0xF0;

        // Format 16 bytes of pixel data for this row
        for (uint8_t x_word = 0; x_word < 16; x_word++) {
            uint8_t pixel_byte = lcd->frame_buffer[y * 16 + x_word];
            lcd->dma_tx_buffer[ptr++] = 0xFA;
            lcd->dma_tx_buffer[ptr++] = pixel_byte & 0xF0;
            lcd->dma_tx_buffer[ptr++] = (pixel_byte << 4) & 0xF0;
        }
    }

    // 3. Lock the busy flag
    lcd->is_dma_busy = 1;

    // 4. Pull CS High to wake up the screen
    HAL_GPIO_WritePin(lcd->cs_port, lcd->cs_pin, GPIO_PIN_SET);

    // 5. Fire DMA and free CPU
    HAL_SPI_Transmit_DMA(lcd->spi_handle, lcd->dma_tx_buffer, 3456);
}

// --- Geometry Functions ---
void LCD_SetPixel(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t draw_pixel) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;

    // --- 180 DEGREE FLIP MATH ---
    #if LCD_FLIP_180
        x = (LCD_WIDTH - 1) - x;
        y = (LCD_HEIGHT - 1) - y;
    #endif

    uint16_t byte_index = (y * 16) + (x / 8);
    uint8_t bit_mask = 0x80 >> (x % 8);

    if (draw_pixel) lcd->frame_buffer[byte_index] |= bit_mask;
    else            lcd->frame_buffer[byte_index] &= ~bit_mask;
}

void LCD_DrawLineHorizontal(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t width, uint8_t draw_pixel) {
    for (uint8_t i = 0; i < width; i++) LCD_SetPixel(lcd, x + i, y, draw_pixel);
}

void LCD_DrawLineVertical(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t height, uint8_t draw_pixel) {
    for (uint8_t i = 0; i < height; i++) LCD_SetPixel(lcd, x, y + i, draw_pixel);
}

void LCD_DrawRectangle(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t draw_pixel) {
    if (width == 0 || height == 0) return;
    LCD_DrawLineHorizontal(lcd, x, y, width, draw_pixel);
    LCD_DrawLineHorizontal(lcd, x, y + height - 1, width, draw_pixel);
    LCD_DrawLineVertical(lcd, x, y, height, draw_pixel);
    LCD_DrawLineVertical(lcd, x + width - 1, y, height, draw_pixel);
}

void LCD_FillRectangle(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t draw_pixel) {
    for (uint8_t i = 0; i < height; i++) LCD_DrawLineHorizontal(lcd, x, y + i, width, draw_pixel);
}

// --- Text & Font System ---
static const uint8_t Font_Numbers[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static const uint8_t Font_Alphabet[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43}
};

static const uint8_t Sym_Colon[5]   = {0x00,0x36,0x36,0x00,0x00};
static const uint8_t Sym_Dot[5]     = {0x00,0x60,0x60,0x00,0x00};
static const uint8_t Sym_Dash[5]    = {0x08,0x08,0x08,0x08,0x08};
static const uint8_t Sym_Space[5]   = {0x00,0x00,0x00,0x00,0x00};

// --- NEW MISSING PIXEL ART ---
static const uint8_t Sym_Greater[5] = {0x41,0x22,0x14,0x08,0x00}; // >
static const uint8_t Sym_Slash[5]   = {0x20,0x10,0x08,0x04,0x02}; // /
static const uint8_t Sym_Percent[5] = {0x63,0x13,0x08,0x64,0x63}; // %
static const uint8_t Sym_LParen[5]  = {0x00,0x3E,0x41,0x00,0x00}; // (
static const uint8_t Sym_RParen[5]  = {0x00,0x00,0x41,0x3E,0x00}; // )
static const uint8_t Sym_Caret[5]   = {0x04, 0x02, 0x01, 0x02, 0x04}; // ^

static const uint8_t* Lookup_Glyph(char c) {
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A'; // Auto capitalize
    if (c >= '0' && c <= '9') return Font_Numbers[c - '0'];
    if (c >= 'A' && c <= 'Z') return Font_Alphabet[c - 'A'];

    // Map the characters to the new pixel art!
    switch (c) {
        case ':': return Sym_Colon;
        case '.': return Sym_Dot;
        case '-': return Sym_Dash;
        case '>': return Sym_Greater;
        case '/': return Sym_Slash;
        case '%': return Sym_Percent;
        case '(': return Sym_LParen;
        case ')': return Sym_RParen;
        case '^': return Sym_Caret;
        default:  return Sym_Space;
    }
}

// --- USB icon bit-mapping ---
void LCD_DrawIcon_USB(LCD_Handle_t* lcd, uint8_t x, uint8_t y) {
    // Draws a Lightning Bolt (↯)
    const uint8_t bolt[7] = {
        0b00011000,
        0b00110000,
        0b01100000,
        0b01111110,
        0b00011000,
        0b00110000,
        0b01000000
    };
    for(uint8_t r=0; r<7; r++) {
        for(uint8_t c=0; c<8; c++) {
            if(bolt[r] & (1 << (7-c))) LCD_SetPixel(lcd, x+c, y+r, 1);
        }
    }
}


// --- Wifi icon bit-mapping ---
void LCD_DrawIcon_WiFi(LCD_Handle_t* lcd, uint8_t x, uint8_t y) {
    // Draws a Wi-Fi Arc
    const uint8_t wifi[7] = {
        0b01111110,
        0b10000001,
        0b00111100,
        0b01000010,
        0b00011000,
        0b00000000,
        0b00011000
    };
    for(uint8_t r=0; r<7; r++) {
        for(uint8_t c=0; c<8; c++) {
            if(wifi[r] & (1 << (7-c))) LCD_SetPixel(lcd, x+c, y+r, 1);
        }
    }
}


// --- Brightness indicator icon bit-mapping ---
void LCD_DrawIcon_Brightness(LCD_Handle_t* lcd, uint8_t x, uint8_t y, uint8_t level) {
    // 1. Draw the 10x10 hollow outline so the user knows where the box is even at 0%
    LCD_DrawRectangle(lcd, x, y, 10, 10, 1);

    // 2. Clear out the inside (an 8x8 area) to wipe the previous frame's level
    LCD_FillRectangle(lcd, x + 1, y + 1, 8, 8, 0);

    // 3. Fill it from the bottom upwards based on the level!
    if (level > 0) {
        // level 1 = 2 pixels high, level 4 = 8 pixels high
        uint8_t fill_height = level * 2;

        // Calculate the starting Y coordinate so it anchors to the bottom of the box
        uint8_t start_y = (y + 9) - fill_height;

        // Fill the solid block
        LCD_FillRectangle(lcd, x + 1, start_y, 8, fill_height, 1);
    }
}


void LCD_PrintChar(LCD_Handle_t* lcd, uint8_t x, uint8_t y, char character) {
    const uint8_t* glyph = Lookup_Glyph(character);
    for (uint8_t col = 0; col < 5; col++) {
        for (uint8_t row = 0; row < 7; row++) {
            if (glyph[col] & (1 << row)) {
                LCD_SetPixel(lcd, x + col, y + row, 1);
            }
        }
    }
}

void LCD_PrintString(LCD_Handle_t* lcd, uint8_t x, uint8_t y, const char* text) {
    while (*text != '\0') {
        LCD_PrintChar(lcd, x, y, *text);
        x += 6; // 5 pixels for the character + 1 blank pixel for spacing
        if (x > (LCD_WIDTH - 6)) break; // Stop if we hit the edge of the screen
        text++;
    }
}
