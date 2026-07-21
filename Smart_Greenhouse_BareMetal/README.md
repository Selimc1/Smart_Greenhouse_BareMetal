# Smart Greenhouse BareMetal

![STM32](https://img.shields.io/badge/Microcontroller-STM32F103-blue)
![Architecture](https://img.shields.io/badge/Architecture-Bare%20Metal-orange)

## 📌 Project Overview
An autonomous Smart Greenhouse Control System programmed entirely at the register level for an STM32F1 series microcontroller. This system reads environmental data (temperature and humidity), evaluates the climate conditions, and automatically adjusts climate regulation devices (such as fans) while simultaneously enforcing security protocols against unauthorized entry.

## 🚀 Key Features
- **Custom DHT11 Driver:** Reads the complex 1-wire protocol of the DHT11 temperature and humidity sensor using SysTick for strict microsecond-level timing delays.
- **Dynamic PWM Control:** Utilizes Timer 2 (TIM2) to generate a PWM signal. The duty cycle applied to the fan motor is dynamically calculated based on real-time humidity readings.
- **Hardware Security (EXTI):** Employs an External Interrupt on PB10 to act as a door sensor. If an unauthorized opening is detected, an interrupt halts the fan and triggers an alert, bypassing standard program flow.
- **Telemetry via UART:** Streams current temperature, humidity, and system status (e.g., Door Open/Closed) to a PC or server via UART1.

## 🛠️ Hardware Requirements
- **STM32F1 Series Microcontroller** (e.g., Blue Pill).
- **DHT11** Temperature and Humidity Sensor.
- **DC Fan & Motor Driver** (e.g., L298N).
- **Magnetic Door Switch / Button** (Connected to PB10).
- **USB-to-TTL Serial Converter** (For telemetry).

## ⚙️ How It Works
1. **Data Parsing:** The STM32 sends a start signal to the DHT11 and then reads 40 bits of data, interpreting the precise high/low microsecond timings to extract humidity and temperature.
2. **Climate Logic:** The humidity percentage is mapped to a TIM2 Compare Register (CCR) value, altering the speed of the fan automatically.
3. **Security Halt:** If the door switch breaks contact, the PB10 EXTI triggers. The interrupt routine overrides the PWM output, stopping the fan immediately for safety and security.

## 💻 Getting Started
Open the project in **STM32CubeIDE**, build, and flash. Connect your sensors and motor driver according to the defined pins. Open a serial monitor to view the greenhouse telemetry.
