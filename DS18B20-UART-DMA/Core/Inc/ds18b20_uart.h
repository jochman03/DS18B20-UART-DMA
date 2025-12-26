/*
 * ds18b20_uart.h
 *
 *  Created on: Nov 30, 2025
 *      Author: Jakub Ochman
 */

#ifndef INC_DS18B20_UART_H_
#define INC_DS18B20_UART_H_

#include "main.h"

// Configuration register values for DS18B20 resolution
#define PRECISION_9BIT 	0x1F
#define PRECISION_10BIT 0x3F
#define PRECISION_11BIT 0x5F
#define PRECISION_12BIT 0x7F

// Default alarm temperature thresholds
#define DEFAULT_ALARM_TL 0x4B
#define DEFAULT_ALARM_TH 0x46

// Internal state machine states for non-blocking operation
typedef enum {
    DS_IDLE = 0,
	DS_WAIT_ACCESS,
    DS_RESET,
	DS_MATCH_ROM,
	DS_SEND_ADDRESS,
	DS_CONVERT_T,
	DS_WAIT_CONVERSION,
	DS_RESET_2,
	DS_MATCH_ROM_2,
	DS_SEND_ADDRESS_2,
	DS_READ,
	DS_READ_LSB,
	DS_READ_MSB,
	DS_CALC,
	DS_ERROR

} DS18B20_State_t;

// DS18B20 device context and runtime data
typedef struct {
	uint8_t address[8];
	float temperature;
	uint8_t lsb;
	uint8_t msb;
	uint8_t presence;
	DS18B20_State_t state;
	uint32_t last_conv;
} DS18B20_t;

// Run DS18B20 state machine
void DS18B20_Handle(DS18B20_t* thermometer);

// Request temperature measurement
void DS18B20_Measure(DS18B20_t* thermometer);

// Initialize DS18B20 driver with UART handle
void DS18B20_Init(UART_HandleTypeDef *huart);

// UART RX DMA completion hook
void DS18B20_RX_Callback(UART_HandleTypeDef *huart);

// UART TX DMA completion hook
void DS18B20_TX_Callback(UART_HandleTypeDef *huart);

// Search for next DS18B20 device on 1-Wire bus
uint8_t DS18B20_SearchRom(DS18B20_t* thermometer);

// Set sensor resolution and store it to EEPROM
uint8_t DS18B20_SetPrecision(DS18B20_t* thermometer, uint8_t precision);

#endif /* INC_DS18B20_UART_H_ */
