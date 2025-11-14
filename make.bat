rem avrdude -pm328p -cstk500 -PCOM16 -Uflash:w:"./default/HeatPump.hex":i -Enoreset
rem avrdude -pm328p -cstk500 -PCOM15 -Uflash:w:"../arduino_bootloader.hex":i
avrdude -pm328p -carduino -PCOM16 -b57600 -Uflash:w:"./default/HeatPump.hex":i
REM pause
"C:\Program Files (x86)\teraterm\ttermpro.exe"
