# E-Paper Station

The CC2531 USB Stick is used together with the Python E-Paper station from [danielkucera](https://github.com/danielkucera) and can be found here: [epaper-station](https://github.com/danielkucera/epaper-station)

The TiMac Firmware is needed to be flashed on the CC2531 USB Stick and can be found as a bin file in this folder.

The TiMac firmware is used as a ZigBee interface and will enumerate as a UART connection to the PC to communicate with the Python script.

The folder [EPaperStation_cc13x2_cc26x2_Driver](./EPaperStation_cc13x2_cc26x2_Driver) contains a firmware for the CC1352(P) or CC2652(P) SoC and acts as a CC2531 USB Stick to be directly compatible with the E-Paper station.

To flash it with an Arduino Board use this [CC.Flash](https://github.com/atc1441/CC.Flash) tool (Windows only)

The CC.Flash tool should be compatible with any possible Arduino board as only UART and 3 GPIOs are needed.

The folder [epaper_station_websocket](./epaper_station_websocket) contains a modified version of the epaper-station which does add a simple "online_viewer.html" which can be simply opened after starting the station.py script, it communicates via WebSocket and will display each connected E-Paper display with its image and infos.