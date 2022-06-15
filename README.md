# ZBS_Flasher
An Ardunio / C++ Library and interface to flash the ZBS243 / SEM9110 8051 Microcontroller

The ZBS243 is an 8051 Zigbee Enabled SOC with 64KB of Flash and 1KB of EEPROM Like Infopage.

It is used in the Electronic Shelf Labels made by SOLUM and rebranded with the name SEM9110


This Project enables simple Reading and Writing of the Flash/Infopage

It is written for the ESP32 but should run basically on any SOC with ~8 GPIOS and UART.

### You need:

  Virsual Studio code
  
  PlatformIO
  
  Python > 3.0
  
    in Python you need: pyserial ( pip install pyserial )
  


### How to get Started:

  Load the Project folder into Virsual Studio Code, select the Correct COM port in the platformio.ini
  
  Flash it to an ESP32.
  
  After that you can use the python tool zbs_flasher.py(zbs_flasher.exe precompiled) with the following parameter:
  
  'zbs_flasher COMxx read/write/readI/writeI file.bin'<- file from where to read or write the flash or infopage
  
  
  The Pinout on how to connect the ZBS243 SOC is readable in the main.cpp file and can be changed to any wanted one.
  
  The "ZBS_POWER" pin is used to reset the SOC after any debug mode action like flashing etc. the SOC is stuck in debug mode
  
  and can only exit from it via a power cycle so it is adived to connect the power to the SOC via an 
  
  transistor to automaticaly have it exit the debugmode and run the firmware.
  
