# CC2531 E-Paper Station

The CC2531 USB Stick is used together with the Python E-Paper station from [danielkucera](https://github.com/danielkucera) and can be found here: [epaper-station](https://github.com/danielkucera/epaper-station)

The TiMac Firmware is needed to be flashed on the CC2531 USB Stick and can be found as a bin file in this folder.

The TiMac firmware is used as a ZigBee interface and will enumerate as a UART connection to the PC to communicate with the Python script.

To flash it with an Arduino Board use this [CC.Flash](https://github.com/atc1441/CC.Flash) tool (Windows only)

The CC.Flash tool should be compatible with any possible Arduino board as only UART and 3 GPIOs are needed.
