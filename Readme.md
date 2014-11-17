STM32F0-Discovery-LIS3DSH-example-code
==============================

This is my example code for the STM32F4 Discovers using the RTOS ChibiOS.

requirements
------------
* Chibios 2.5.0+ (or a recent development snapshot)
* arm toolchain (e.g. arm-none-eabi from summon-arm)

features
--------
* serial console
* ADC measuring, continuous scan
* PWM leds
* LIS3DSH 

usage
-----
* edit the Makefile and point "CHIBIOS = ../../chibios" to your ChibiOS folder
* make
* connect the STM32F0 Discovery with TTL/RS232 adapter, PA2(TX) and PA3(RX) are routed to USART2 38400-8-N-1.
* connect PC1 to 3.3v(warning! 5v can damage your board) and PC2 to GND for analog measurements.
* flash the STM32F4
* use your favorite terminal programm to connect to the Serial Port 38400-8n1 
* turn on LIS3DSH by typing "accel" command

console commands
----------------
* help
* temp
* volt
* ledon
* ledoff
* accel
