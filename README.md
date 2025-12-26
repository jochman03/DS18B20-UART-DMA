# DS18B20-UART-DMA + SSD1306 (SPI) – STM32

Non-blocking DS18B20 driver implemented as a **standalone library** using **UART + DMA** (1-Wire over UART timing) with temperature display on an **SSD1306 OLED over SPI**.

The DS18B20 logic is fully contained in the library files, while `main.c` only demonstrates **example usage** and application logic.

---

## Project structure


DS18B20-UART-DMA/Core/  
├── Inc/  
│ ├── ds18b20_uart.h // DS18B20 driver API  
│ ├── oled.h // SSD1306 OLED driver  
│ └── main.h  
└── Src/  
&nbsp;&nbsp;&nbsp;├── ds18b20_uart.c // DS18B20 driver implementation  
&nbsp;&nbsp;&nbsp;├── oled.c  
&nbsp;&nbsp;&nbsp;└── main.c // Example usage  

## DS18B20 library

### Files
- `ds18b20_uart.c`
- `ds18b20_uart.h`

The library handles:
- 1-Wire bus emulation using **UART**
  - reset pulse at **9600 baud**
  - data slots at **115200 baud**
- **DMA-based** TX/RX
- Presence detection
- **ROM Search** (multiple sensors on one bus)
- Non-blocking **state machine** for temperature conversion
- Optional resolution configuration (9–12 bit)
- Bus locking to allow multiple sensor instances

### 1-Wire emulation using UART symbols
The driver sends UART bytes as timing symbols:

- `DS_RST = 0xF0` @ **9600** → generates reset pulse
- `DS_ONE = 0xFF` @ **115200** → read/write “1” slot
- `DS_ZERO = 0x00` @ **115200** → write “0” slot

Reads are done by sending `0xFF` and sampling what comes back. Conversion from received symbols to bits happens in `ds_convert()`.

### Non-blocking DS18B20 state machine
Each sensor has its own `DS18B20_t` state, but access to the bus is protected by `ds_bus_owner` so only one instance uses UART at a time.

Measurement flow (simplified):
1. Reset + presence
2. `MATCH ROM` + address
3. `CONVERT T`
4. Wait ~750ms (12-bit worst case)
5. Reset + presence
6. `MATCH ROM` + address
7. `READ SCRATCHPAD` (LSB/MSB)
8. Convert to °C (`raw / 16.0f`)

---

## Application (`main.c`)

`main.c` contains **only example application logic**:
- hardware initialization (CubeMX-generated)
- DS18B20 initialization
- ROM search for connected sensors
- periodic triggering of measurements
- calling the DS18B20 state machine
- updating the OLED when values change

## Notes

- UART must operate in **full-duplex mode**.
  TX and RX pins have to be **physically connected together** (single-wire bus),
  typically through a **~1kΩ resistor** to avoid hard contention.

- A pull-up resistor on the 1-Wire line is required (typically **4.7kΩ to 3.3V**).

- `DS18B20_Handle()` must be called frequently from the main loop
  (no long blocking delays in the application).

- Multiple DS18B20 devices are supported on a single UART bus
  using the ROM Search algorithm.

- OLED refresh is optimized to update only when values change,
  avoiding unnecessary SPI traffic and display flicker.
