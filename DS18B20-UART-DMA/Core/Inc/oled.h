/*
 * oled.h
 *
 *  Created on: Nov 24, 2025
 *      Author: Jakub Ochman
 */

#ifndef INC_OLED_H_
#define INC_OLED_H_

#include "main.h"

// I2C address of OLED display
#define OLED_ADDRESS 0x7A

// OLED display size in pixels
#define OLED_WIDTH 128
#define OLED_HEIGHT 64


// Draw single character at given position
void OLED_DrawChar(uint8_t x, uint8_t y, char c, uint8_t size);

// Draw single pixel on display buffer
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);

// Send display buffer to OLED
void OLED_Update();

// Clear display buffer
void OLED_Clear();

// Fill display buffer with pixels
void OLED_Fill();

// Hardware reset of OLED display
void OLED_Reset();

// Initialize OLED controller and interface
void OLED_Init();

// Draw null-terminated text string
void OLED_WriteText(char* txt, uint8_t x, uint8_t y, uint8_t fontSize);

// SPI transfer completion callback
void OLED_SPI_Callback(SPI_HandleTypeDef *hspi);

#endif /* INC_OLED_H_ */
