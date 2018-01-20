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
- temperature readings freeze from time to time
- When serial port is not connected to PC, AVR restarts after pump check
- Outputs are switched on in random times when CAN is connected
- CAN is not initialized correctly every boot
