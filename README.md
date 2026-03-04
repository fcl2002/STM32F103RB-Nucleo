# STM32F103RB-Nucleo: OneWire & I2C Bit-Banging

This project implements and tests communication protocols on the **STM32F103RB** microcontroller (Nucleo-64 board). It focuses on low-level driver development for **1-Wire (OneWire)** and **I2C** using bit-banging and hardware timers for precise timing.

## 🚀 Overview

The codebase provides a modular approach to interfacing with digital sensors and I/O expanders. It includes a comprehensive test suite in `main.c` to verify protocol integrity, timing, and data validation (CRC8).

## 🛠️ Hardware Requirements

- **MCU:** STM32F103RB (Nucleo-F103RB)
- **1-Wire Device:** DS18B20 Temperature Sensor (connected to `PB4`)
- **I2C Device:** PCF8574 I/O Expander (connected to `PB6/SCL` and `PB7/SDA`)
- **Debug:** USB-Serial (USART2) at 9600 8N1
- **Optional:** Logic Analyzer (e.g., Analog Discovery 2) for observing scope markers on `PB4/PB5`.

## 📦 Project Structure

- **`onewire.c/h`**: 1-Wire protocol implementation using **TIM3** for microsecond-precision timing. Supports:
    - Reset/Presence pulse.
    - Read/Write bits and bytes.
    - Commands: `SKIP_ROM`, `READ_ROM`, `CONVERT_T`, `READ_SCRATCHPAD`.
- **`i2c_bb.c/h`**: Software-based (Bit-Bang) I2C implementation.
- **`pcf8574.c/h`**: Driver for the 8-bit I/O expander via I2C.
- **`serial.c/h`**: USART2 driver for debugging and data logging.
- **`io.c/h`**: Low-level GPIO abstraction (bare-metal configuration of CRL/CRH registers).
- **`delay.c/h`**: Precision delays using SysTick (72 MHz clock).

## 🧪 Testing and Usage

The `main.c` file contains a **Test Selector** macro. To run a specific test:

1. Open `main.c`.
2. Locate the `#define TEST_FUNC` line.
3. Choose one of the available tests:
   - `TEST_RESET`: Basic reset and presence detection.
   - `TEST_READ_ROM`: Read the unique 64-bit ID of the 1-Wire device.
   - `TEST_CONVERT_T`: Trigger temperature conversion and read the result (Celsius).
   - `TEST_READ_SCRATCHPAD`: Read the 9-byte internal memory with CRC8 validation.
   - `TEST_SCOPE_CMDS`: Generate patterns for logic analyzer inspection.
4. Compile and flash using **Keil uVision** (`.uvprojx`).
5. Open a serial terminal (9600 baud) to view the results.

## 🔧 Pin Configuration

| Function | Pin | Description |
|----------|-----|-------------|
| **1-Wire DQ** | `PB4` | Data line (requires pull-up) |
| **Spy/Marker**| `PB5` | Timing markers for oscilloscope |
| **I2C SCL**   | `PB6` | Bit-bang Clock |
| **I2C SDA**   | `PB7` | Bit-bang Data |
| **USART2 TX** | `PA2` | Serial Debug (to ST-Link) |
| **USART2 RX** | `PA3` | Serial Debug |
| **User LED**  | `PA5` | Status indicator (LD2) |

## 📝 License
This project was developed as part of a Microcontrollers Lab
