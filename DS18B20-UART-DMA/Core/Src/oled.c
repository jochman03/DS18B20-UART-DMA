/*
 * oled.c
 *
 *  Created on: Nov 24, 2025
 *      Author: Jakub Ochman
 */

#include "main.h"
#include "oled.h"
#include "fonts.h"

// SSD1306 command bytes used by init sequence
#define CMD_DISPLAY_OFF 0xAE
#define CMD_CLOCK_RATIO 0xD5
#define CMD_MULTIPLEX_RATIO 0xA8
#define CMD_DISPLAY_OFFSET 0xD3
#define CMD_START_LINE 0x40
#define CMD_CHARGE_PUMP 0x8D
#define CMD_MEMORY_ADDRESSING 0x20
#define CMD_SEGMENT_REMAP 0xA1
#define CMD_COLUMN_RANGE 0x21
#define CMD_PAGE_RANGE 0x22
#define CMD_COM_OUTPUT_SCAN_DIRECTION 0xC8
#define CMD_COM_PINS_CONF 0xDA
#define CMD_CONTRAST_CONTROL 0x7F
#define CMD_DISPLAY_RESUME 0xA4
#define CMD_NORMAL 0xA6
#define CMD_INVERT 0xA6
#define CMD_DISPLAY_ON 0xAF
#define CMD_PAGE_START 0xB0

// SPI handle used by OLED driver
SPI_HandleTypeDef* oled_spi;

// Framebuffer in SSD1306 page format (1 bit per pixel)
uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

// DMA transfer busy flag
uint8_t oled_tx_done = 0;

// Send single SSD1306 command byte
static void oled_write_command(uint8_t data){
	HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(oled_spi, &data, 1, HAL_MAX_DELAY);
}

// Start DMA transfer of framebuffer to display
void OLED_Update(void) {
	if(oled_tx_done){
		return;
	}
	HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET);
	oled_tx_done = 1;
	HAL_SPI_Transmit_DMA(oled_spi, oled_buffer, OLED_HEIGHT * OLED_WIDTH / 8);
}

// Clear framebuffer to black
void OLED_Clear(){
	for(uint8_t x = 0; x < OLED_WIDTH; x++){
		for(uint8_t y = 0; y < OLED_HEIGHT; y++){
			uint16_t index = x + (y / 8) * OLED_WIDTH;
	    	oled_buffer[index] &= ~(1 << (y % 8));
		}
	}
}

// Fill framebuffer to white
void OLED_Fill(){
	for(uint8_t x = 0; x < OLED_WIDTH; x++){
		for(uint8_t y = 0; y < OLED_HEIGHT; y++){
			uint16_t index = x + (y / 8) * OLED_WIDTH;
	        oled_buffer[index] |= (1 << (y % 8));
		}
	}
}

// Set or clear a single pixel in framebuffer
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT){
    	return;
    }

    uint16_t index = x + (y / 8) * OLED_WIDTH;

    if (color){
        oled_buffer[index] |= (1 << (y % 8));
    }
    else{
    	oled_buffer[index] &= ~(1 << (y % 8));
    }
}

// Draw one character using selected font size
void OLED_DrawChar(uint8_t x, uint8_t y, char c, uint8_t size){
	if(c < 32){
		return;
	}
	if(size == 6){
			uint8_t height = 8;
			uint8_t width = 6;
			const uint8_t* ch = &(ssd1306xled_font6x8[4 + 6 * (c - 32)]);
			for(uint8_t i = 0; i < width; i++){
				for(uint8_t j = 0; j < height; j++){
					if(*(ch + i) & (1 << j)){
						OLED_DrawPixel(x + i, y + j, 1);
					}
					else{
						OLED_DrawPixel(x + i, y + j, 0);
					}
				}
			}
	}
	else if (size == 8) {
	    uint8_t width = 8;

	    const uint8_t* ch = &(ssd1306xled_font8x16[4 + 16 * (c - 32)]);

	    const uint8_t* top = ch;
	    const uint8_t* bottom = ch + 8;

	    for (uint8_t col = 0; col < width; col++) {

	        uint8_t upper = top[col];
	        uint8_t lower = bottom[col];

	        for (uint8_t row = 0; row < 8; row++) {
	            uint8_t pixel = (upper & (1 << row)) ? 1 : 0;
	            OLED_DrawPixel(x + col, y + row, pixel);
	        }

	        for (uint8_t row = 0; row < 8; row++) {
	            uint8_t pixel = (lower & (1 << row)) ? 1 : 0;
	            OLED_DrawPixel(x + col, y + 8 + row, pixel);
	        }
	    }
	}
}

// Draw text starting at x,y
void OLED_WriteText(char* txt, uint8_t x, uint8_t y, uint8_t fontSize){
    uint8_t cursorX = x;
    uint8_t cursorY = y;

    while (*txt != '\0') {

        if (*txt == '\n') {
            cursorY += fontSize;
            cursorX = x;
        }
        else {
            OLED_DrawChar(cursorX, cursorY, *txt, fontSize);
            cursorX += fontSize;
        }

        txt++;
    }
}

// Toggle OLED reset pin
void OLED_Reset(){
	HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(OLED_RST_GPIO_Port, OLED_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(100);
}

// Initialize OLED controller and store SPI handle
void OLED_Init(SPI_HandleTypeDef* hspi){
	oled_spi = hspi;

	OLED_Reset();
	// Turn display off during configuration
	oled_write_command(CMD_DISPLAY_OFF);

	// Set display clock divide ratio
	oled_write_command(CMD_CLOCK_RATIO);
	oled_write_command(0x80);

	// Set multiplex ratio (display height)
	oled_write_command(CMD_MULTIPLEX_RATIO);
	oled_write_command(OLED_HEIGHT - 1);

	// Set display offset
	oled_write_command(CMD_DISPLAY_OFFSET);
	oled_write_command(0x00);

	// Set display start line
	oled_write_command(CMD_START_LINE);

	// Enable internal charge pump
	oled_write_command(CMD_CHARGE_PUMP);
	oled_write_command(0x14);

	// Set horizontal memory addressing mode
	oled_write_command(CMD_MEMORY_ADDRESSING);
	oled_write_command(0x00);

	// Set column address range
	oled_write_command(CMD_COLUMN_RANGE);
	oled_write_command(0x00);
	oled_write_command(0x7F);

	// Set page address range
	oled_write_command(CMD_PAGE_RANGE);
	oled_write_command(0x00);
	oled_write_command(0x07);

	// Map segment 0 to column 127
	oled_write_command(CMD_SEGMENT_REMAP);

	// Set COM scan direction (remapped mode)
	oled_write_command(CMD_COM_OUTPUT_SCAN_DIRECTION);

	// Configure COM pins hardware
	oled_write_command(CMD_COM_PINS_CONF);
	oled_write_command(0x12);

	// Set display contrast
	oled_write_command(CMD_CONTRAST_CONTROL);
	oled_write_command(0x7F);

	// Resume display from RAM content
	oled_write_command(CMD_DISPLAY_RESUME);

	// Set normal (non-inverted) display mode
	oled_write_command(CMD_NORMAL);

	// Turn display on
	oled_write_command(CMD_DISPLAY_ON);

	HAL_Delay(100);
}


// Clear DMA busy flag when SPI transfer completes
void OLED_SPI_Callback(SPI_HandleTypeDef *hspi){
    if (hspi == oled_spi) {
    	oled_tx_done = 0;
    }
}
