# HeatPump
Main control unit for water to water heat pump
Unit is based on Arduino Nano clone with ATmega328p
Inputs:
- 6 DS18B20 temperature sensors
  Primary side inlet and outlet temperature
  Secondary side inlet and outlet temperature
  Upper and lower tank temperatures
- 2 flow meters
  Primary and secondary side flow measurement
Output:
- 3 Relays
  Primary and secondary side circulating pump
  Heat pump compressor

Issues:
- When serial port is not connected to PC, AVR restarts after pump check

Missing features:
- check if temperature sensors are ok



CAN library (MCP2515):
https://github.com/dergraaf/avr-can-lib
