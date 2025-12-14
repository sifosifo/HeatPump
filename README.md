# HeatPump
Main control unit for earth (ground loop) to water heat pump
Unit is based on Arduino Nano clone with ATmega328p

Inputs:
- 6 DS18B20 temperature sensors
  Primary side inlet and outlet temperature
  Secondary side inlet and outlet temperature
  Upper and lower tank temperatures
- 2 flow meters
  Primary and secondary side flow measurement

Output:
- 4 Relays
  Primary and secondary side circulating pump
  Heat pump compressor
  AUX - for example pump and fan pushing heat from water tank to house


CAN library (MCP2515):
https://github.com/dergraaf/avr-can-lib
