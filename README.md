# Composting Temperature Monitor with Raspberry Pi Pico & OLED Display

![Status](https://img.shields.io/badge/Status-Completed-success)
![Language](https://img.shields.io/badge/Language-C/C++-blue)
![Platform](https://img.shields.io/badge/Platform-Raspberry_Pi_Pico-red)

## About the Project

This project was developed for the Electronic Instrumentation (ECA409) and Microcontroller Systems (ECA407) courses. It introduces a complete system for monitoring internal temperatures in composting bins.

The core of the system relies on using a standard 1N4148 diode as the temperature sensing element, exploiting the linear relationship between its forward bias voltage and ambient temperature. The hardware is driven by a Raspberry Pi Pico (RP2040) running custom C/C++ firmware built using the Pico SDK, which handles signal conditioning, data processing, and real-time visualization on an OLED display.

![image](https://github.com/user-attachments/assets/e324176e-713f-46a3-8d78-a148e0cbfb3a) 
![image](https://github.com/user-attachments/assets/7b225a1c-8830-499e-8062-d8e8a5066d73)
![image](https://github.com/user-attachments/assets/fd949ad4-a5ea-4863-9156-2e188ff26bcc)

**For a comprehensive understanding of the project and methodology, reading the academic paper provided in the `artigo_instrumentacao` directory is highly recommended.**

## Key Features

- **Temperature Reading:** Utilizes the voltage variation across a common diode (1N4148) to measure ambient temperature.
- **Moving Average Filter:** Smooths the sensor readings to provide a more stable and accurate value.
- **OLED Display:** Shows the measured voltage and temperature (in °C or °F) on a 128x32 pixel OLED screen.
- **User Interaction:** Allows the user to toggle the temperature unit between Celsius (°C) and Fahrenheit (°F) with a simple button click.
- **LED Indicator:** Visually alerts when the temperature falls below a predefined threshold (set to 40°C in the code).
- **Power Efficiency:** Uses the `sleep` mode (Wait For Interrupt) to minimize energy consumption, waking up only to perform readings or respond to events.

## Hardware Requirements

| Component | Quantity | Notes |
| :--- | :--- | :--- |
| Raspberry Pi Pico | 1 | Main microcontroller. |
| OLED Display 128x32 I2C | 1 | Model with SSD1306 driver. |
| 1N4148 Diode | 1 | Used as the temperature sensor. |
| 10kΩ Resistor | 1 | Current limiting resistor for the diode. |
| Push Button | 1 | To switch the unit of measurement. |
| LED (5mm, any color) | 1 | Visual indicator. |
| 330Ω Resistor | 1 | Current limiting resistor for the LED. |
| 1N4007 Diode | 1 | Reverse polarity protection for the power supply. |
| Breadboard and Jumpers | - | For circuit assembly. |

## Connection Diagram

![image](https://github.com/user-attachments/assets/5b9d6de1-605c-472b-aeaf-6e6ac170f104)

### Pin Mapping

| Component | Pin on Component | Raspberry Pi Pico Pin | Notes |
| :--- | :--- | :--- | :--- |
| **OLED Display** | VCC | 5V (VBUS - Pin 40) | 5V Power supply. |
| | GND | GND - Pin 38 | Ground. |
| | SCL | GP5 (I2C0 SCL) - Pin 7 | I2C Clock. |
| | SDA | GP4 (I2C0 SDA) - Pin 6 | I2C Data. |
| **Sensor (Diode)** | Anode (+) | GP26 (ADC0) - Pin 31 | Connected to R1 resistor. |
| | Cathode (-) | GND | Ground. |
| **Resistor R1 (10kΩ)**| Terminal 1 | 5V (VBUS - Pin 40) | 5V Power supply. |
| | Terminal 2 | GP26 (ADC0) - Pin 31 | Read point for the sensor. |
| **Push Button** | Terminal 1 | GP10 - Pin 14 | |
| | Terminal 2 | GND | Ground. |
| **LED** | Anode (+) | - | Connected to Resistor R2. |
| | Cathode (-) | GND | Ground. |
| **Resistor R2 (330Ω)**| Terminal 1 | GP11 - Pin 15 | Limits current for the LED. |
| | Terminal 2 | - | Connected to the LED Anode (+). |

![image](https://github.com/user-attachments/assets/c70c6e0e-c489-4e90-ba06-1f685e367dfe)

## Software Overview

The project was developed in Visual Studio Code (VS Code) using the Raspberry Pi Pico extension, written in C/C++ based on the Pico SDK. 

- `CMakeLists.txt`: CMake configuration file responsible for defining how the project is compiled, listing the source files, and linking necessary Pico SDK libraries such as `hardware_i2c` and `hardware_adc`.
- `main.c`: Contains the main system logic. It handles the initialization of peripherals (ADC, I2C, GPIO), reads the diode temperature, applies the moving average filter, controls the OLED display, and manages events (button and timer).
- `ssd1306_font.h`: Header file containing the font data (byte array) used to draw alphanumeric characters on the OLED display.
- `raspberry26x32.h`: Header file storing the bitmap data for a 26x32 pixel image of the Raspberry Pi logo.

### Code Operation

The code is structured around a low-power main loop (`__wfi()`) that is awakened by two main interrupts:

1. **Periodic Timer (`adc_timer_callback`):** Every 500ms, the system reads the voltage on the ADC pin, converts it to temperature, updates the moving average filter, and controls the LED.
2. **GPIO Interrupt (`button_isr`):** Triggered when the button is pressed. The interrupt service routine flags the main loop to switch the display unit, implementing a software debounce to prevent multiple triggers.

### Sensor Calibration

The conversion from the read voltage to temperature in Celsius is done using the following formula:

```c
Temperature (°C) = (ADC_Voltage - 0.6264) / (-0.0021)
```

These calibration values (`0.6264` and `-0.0021`) are not arbitrary. They were obtained from experimental data documented in the article *"Termômetro de Alta Sensibilidade Usando Diodo Semicondutor como Elemento Sensor"* (2012).

## References

- Silva, G. V. et al. (2012). *Termômetro de Alta Sensibilidade Usando Diodo Semicondutor como Elemento Sensor*. CEEL 2012. Available at: [https://www.peteletricaufu.com.br/static/ceel/doc/artigos/artigos2012/ceel2012_artigo044_r01.pdf](https://www.peteletricaufu.com.br/static/ceel/doc/artigos/artigos2012/ceel2012_artigo044_r01.pdf)
