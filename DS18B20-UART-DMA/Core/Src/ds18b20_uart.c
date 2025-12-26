/*
 * ds18b20_uart.c
 *
 *  Created on: Nov 30, 2025
 *      Author: Jakub Ochman
 */
#include "ds18b20_uart.h"
#include "main.h"

// UART symbols representing 1-Wire timing
#define DS_ZERO 0x00
#define DS_ONE  0xFF
#define DS_RST  0xF0

// 1-Wire command bytes
#define CMD_SKIP_ROM 0xCC
#define CMD_MATCH_ROM 0x55
#define CMD_CONVERT 0x44
#define CMD_READ 0xBE
#define CMD_SEARCH_ROM 0xF0
#define CMD_WRITE_SCRATCHPAD 0x4E
#define CMD_COPY_SCRATCHPAD 0x48

// Milliseconds tick source
#define GET_MILLIS_F() HAL_GetTick()

// UART DMA transmit buffer (bits expanded to bytes)
uint8_t ds_tx_buf[64];

// UART DMA receive buffer
uint8_t ds_rx_buf[8];

// UART DMA transmission completion flags
volatile uint8_t ds_rx_done = 1;
volatile uint8_t ds_tx_done = 1;

// Raw temperature value from sensor
volatile int16_t temp = 0;


// Last discrepancy position for ROM search
uint8_t last_discrepancy = 0;

// Flag indicating last device found during ROM search
uint8_t last_device_flag  = 0;

// Last discovered ROM address
uint8_t last_rom[8] = {0};

// UART handle used for 1-Wire bus
UART_HandleTypeDef* ds_huart;

// Bus lock to prevent concurrent access
DS18B20_t *ds_bus_owner = NULL;

// Initialize DS18B20 driver and reset ROM search state
void DS18B20_Init(UART_HandleTypeDef *huart){
    ds_huart = huart;
    last_discrepancy = 0;
    last_device_flag = 0;
    for(uint8_t i = 0; i < 8; i++){
    	last_rom[i] = 0;
    }
}

// Reinitialize UART with selected baudrate
static void uart_init(uint32_t baudrate){
    ds_huart->Init.BaudRate = baudrate;

    if (HAL_UART_Init(ds_huart) != HAL_OK) {
        Error_Handler();
    }
}

// Read one byte from 1-Wire bus using read slots
static void ds_read(void){
    for (int i = 0; i < 8; i++) {
        ds_tx_buf[i] = DS_ONE;
    }

    ds_rx_done = 0;
    ds_tx_done = 0;

    HAL_UART_Receive_DMA(ds_huart, ds_rx_buf, 8);
    HAL_UART_Transmit_DMA(ds_huart, ds_tx_buf, 8);
}

// Convert received UART symbols into data byte
static uint8_t ds_convert(void){
    uint8_t value = 0;

    for (int i = 0; i < 8; i++) {
        if (ds_rx_buf[i] == 0xFF) {
            value |= (1 << i);
        }
    }

    return value;
}

// Write one byte to 1-Wire bus
static void ds_write(uint8_t value){
    for (int i = 0; i < 8; i++) {
        if (value & (1 << i)) {
            ds_tx_buf[i] = DS_ONE;
        } else {
            ds_tx_buf[i] = DS_ZERO;
        }
    }

    ds_tx_done = 0;

    HAL_UART_Transmit_DMA(ds_huart, ds_tx_buf, 8);
}

// Generate 1-Wire reset pulse and detect presence
static int8_t ds_reset(){
	uart_init(9600);
	uint8_t data = 0xF0;
	HAL_UART_Transmit(ds_huart, &data, 1, 100);
	if(HAL_UART_Receive(ds_huart, &data, 1, 1000) != HAL_OK){
		return -1;
	}
	if(data == 0xF0){
		return -2;
	}
	uart_init(115200);

	return 1;
}

// Write ROM or data bytes bit-by-bit
static void ds_write_address(const uint8_t *data, uint8_t len){
    for (uint8_t byte = 0; byte < len; byte++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (data[byte] & (1 << bit)) {
                ds_tx_buf[byte * 8 + bit] = DS_ONE;
            } else {
                ds_tx_buf[byte * 8 + bit] = DS_ZERO;
            }
        }
    }

    ds_tx_done = 0;
    HAL_UART_Transmit_DMA(ds_huart, ds_tx_buf, len * 8);
}

// Read single 1-Wire bit
uint8_t ds_read_bit(void){
    ds_rx_done = 0;
    ds_tx_done = 0;

    uint8_t ds_rx_bit = 0;
    uint8_t ds_tx_bit = DS_ONE;

    HAL_UART_Receive_DMA(ds_huart, &ds_rx_bit, 1);
    HAL_UART_Transmit_DMA(ds_huart, &ds_tx_bit, 1);

    while (!ds_rx_done);
    if(ds_rx_bit == 0xFF){
    	return 1;
    }
    return 0;
}

// Write single 1-Wire bit
static void ds_write_bit(uint8_t bit){
	uint8_t b = DS_ZERO;
	if(bit){
		b = DS_ONE;
	}
    HAL_UART_Transmit(ds_huart, &b, 1, 5);
}

// Perform ROM search algorithm to find next device
uint8_t DS18B20_SearchRom(DS18B20_t* thermometer){
    uint8_t bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    uint8_t rom_byte_mask = 1;
    uint8_t rom[8] = {0};

    if (last_device_flag) {
        return 0;
    }

    if (ds_reset() != 1) {
    	last_discrepancy = 0;
    	last_device_flag  = 0;
        return 0;
    }

    ds_write(CMD_SEARCH_ROM);
    while(!ds_tx_done);

    while (rom_byte_number < 8) {
        uint8_t r0 = ds_read_bit();
        uint8_t r1 = ds_read_bit();

        if (r0 == 1 && r1 == 1) {
        	last_discrepancy = 0;
        	last_device_flag  = 0;
            return 0;
        }

        uint8_t chosen_bit;

        if (r0 == 0 && r1 == 0) {
            if (bit_number < last_discrepancy) {
            	if(last_rom[rom_byte_number] & rom_byte_mask){
            		chosen_bit = 1;
            	}
            	else{
            		chosen_bit = 0;
            	}
            } else if (bit_number == last_discrepancy) {
                chosen_bit = 1;
            } else {
                chosen_bit = 0;
                last_zero = bit_number;
            }
        } else {
            chosen_bit = r0;
        }

        ds_write_bit(chosen_bit);

        if (chosen_bit) {
            rom[rom_byte_number] |= rom_byte_mask;
        }

        bit_number++;
        if (rom_byte_mask & 0x80) {
            rom_byte_mask = 1;
            rom_byte_number++;
        } else {
            rom_byte_mask <<= 1;
        }

    }

    last_discrepancy = last_zero;
    if (last_discrepancy == 0) {
    	last_device_flag = 1;
    }

    for (int i = 0; i < 8; i++) {
    	thermometer->address[i] = rom[i];
        last_rom[i] = rom[i];
    }

    return 1;
}

// Main DS18B20 state machine
void DS18B20_Handle(DS18B20_t* thermometer){
    if (ds_bus_owner != NULL && ds_bus_owner != thermometer) {
        return;
    }
	uint32_t now = GET_MILLIS_F();
    switch (thermometer->state) {
    case DS_IDLE:
        break;
    case DS_WAIT_ACCESS:
    	ds_bus_owner = thermometer;
    	thermometer->state = DS_RESET;
        break;
    case DS_RESET:
    	if (!ds_tx_done || !ds_rx_done) {
    		break;
    	}
    	thermometer->presence = ds_reset();
    	if(thermometer->presence != 1){
    		thermometer->state = DS_ERROR;
    		ds_bus_owner = NULL;
    		break;
    	}
    	thermometer->state = DS_MATCH_ROM;
        break;
    case DS_MATCH_ROM:
    	if (!ds_tx_done || !ds_rx_done) break;
        ds_write(CMD_MATCH_ROM);
        thermometer->state = DS_SEND_ADDRESS;
        break;
    case DS_SEND_ADDRESS:
    	if (!ds_tx_done || !ds_rx_done) break;
    	ds_write_address(thermometer->address, 8);
    	thermometer->state = DS_CONVERT_T;
    	break;
    case DS_CONVERT_T:
    	if (!ds_tx_done || !ds_rx_done) break;
        ds_write(CMD_CONVERT);
        thermometer->last_conv = now;
        thermometer->state = DS_WAIT_CONVERSION;
        ds_bus_owner = NULL;
        break;
    case DS_WAIT_CONVERSION:
    	if (!ds_tx_done || !ds_rx_done) {
    		break;
    	}
    	if (now - thermometer->last_conv < 750) {
    		ds_bus_owner = NULL;
            break;
        }
        thermometer->state = DS_RESET_2;
        break;
    case DS_RESET_2:
    	thermometer->presence = ds_reset();
    	if(thermometer->presence != 1){
    		thermometer->state = DS_ERROR;
    		ds_bus_owner = NULL;
    		break;
    	}
    	thermometer->state = DS_MATCH_ROM_2;
        break;

    case DS_MATCH_ROM_2:
    	if (!ds_tx_done || !ds_rx_done) break;
        ds_write(CMD_MATCH_ROM);
        thermometer->state = DS_SEND_ADDRESS_2;
        break;
    case DS_SEND_ADDRESS_2:
    	if (!ds_tx_done || !ds_rx_done) break;
    	ds_write_address(thermometer->address, 8);
    	thermometer->state = DS_READ;
    	break;
    case DS_READ:
    	if (!ds_tx_done || !ds_rx_done) break;
        ds_write(CMD_READ);
        thermometer->state = DS_READ_LSB;
        break;
    case DS_READ_LSB:
    	if (!ds_tx_done || !ds_rx_done) break;
    	ds_read();
    	thermometer->state = DS_READ_MSB;
    	break;
    case DS_READ_MSB:
    	if (!ds_tx_done || !ds_rx_done) break;
    	thermometer->lsb = ds_convert();
    	ds_read();
    	thermometer->state = DS_CALC;
    	break;
    case DS_CALC:
    	if (!ds_tx_done || !ds_rx_done) break;
    	thermometer->msb = ds_convert();
    	temp = (int16_t)((thermometer->msb << 8) | thermometer->lsb);
    	thermometer->temperature = (float)temp / 16.0f;
    	thermometer->state = DS_IDLE;
    	ds_bus_owner = NULL;
		break;

    case DS_ERROR:
    	break;
    default:
        break;
    }
}

// Configure DS18B20 resolution and store it to EEPROM
uint8_t DS18B20_SetPrecision(DS18B20_t* thermometer, uint8_t precision){
    if (ds_bus_owner != NULL && ds_bus_owner != thermometer) {
        return 0;
    }
    ds_bus_owner = thermometer;
	thermometer->presence = ds_reset();

	if(thermometer->presence != 1){
		return 0;
	}
	ds_write(CMD_MATCH_ROM);
	while (!ds_tx_done);
	ds_write_address(thermometer->address, 8);
	while (!ds_tx_done);

	ds_write(CMD_WRITE_SCRATCHPAD);
	while (!ds_tx_done);

	ds_write(DEFAULT_ALARM_TH);
	while (!ds_tx_done);

	ds_write(DEFAULT_ALARM_TL);
	while (!ds_tx_done);

	switch(precision){
	case 9:
		ds_write(PRECISION_9BIT);
		break;
	case 10:
		ds_write(PRECISION_10BIT);
		break;
	case 11:
		ds_write(PRECISION_11BIT);
		break;
	case 12:
		ds_write(PRECISION_12BIT);
		break;
	default:
		return 0;
	}
	while (!ds_tx_done);

	thermometer->presence = ds_reset();
	if(thermometer->presence != 1){
		return 0;
	}
	ds_write(CMD_MATCH_ROM);
	while (!ds_tx_done);
	ds_write_address(thermometer->address, 8);
	while (!ds_tx_done);

	ds_write(CMD_COPY_SCRATCHPAD);
	while (!ds_tx_done);
	HAL_Delay(10);
	ds_bus_owner = NULL;
	return 1;
}

// Start temperature measurement
void DS18B20_Measure(DS18B20_t* thermometer){
	if(thermometer->state == DS_IDLE){
		thermometer->state = DS_WAIT_ACCESS;
	}
}

// UART RX DMA completion callback
void DS18B20_RX_Callback(UART_HandleTypeDef *huart){
    if (huart == ds_huart) {
    	ds_rx_done = 1;
    }

}

// UART TX DMA completion callback
void DS18B20_TX_Callback(UART_HandleTypeDef *huart){
    if (huart == ds_huart) {
    	ds_tx_done = 1;
    }
}
