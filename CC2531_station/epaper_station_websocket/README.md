# epaper-station

Contains PC based controll software for epaper display protocol by @dmitrygr (http://dmitry.gr/index.php?r=05.Projects&proj=29.%20eInk%20Price%20Tags)

## Dongle firmware
- 802.15.4 CC2531 dongle needs TIMAC firmware from: https://www.ti.com/tool/TIMAC
- after install, you should see following files:

![](https://user-images.githubusercontent.com/1734256/184469490-7287bd56-6ff9-4716-8b1d-2c8783fb2065.jpg)
- you can use following methods to flash the dongle: https://www.zigbee2mqtt.io/guide/adapters/flashing/alternative_flashing_methods.html

## Display firmware
- https://github.com/danielkucera/epaper-firmware

## Install
```
apt install python3-serial python3-pycryptodome python3-pil
```

## Run
```
python3 station.py
```

## Docker
```
docker build -t epaper-station:latest .
docker run --device=/dev/<tty>:/dev/<tty> -e EPS_PORT=/dev/<tty> epaper-station
```

## Usage

- to pair a display, it has to be really really close (touching the adapter with left edge)
- when the display "checks in", it will check the presence of <DISPLAY_MAC>.bmp(24bit With RED) or <DISPLAY_MAC>.png(Only Black White) in current dir, convert it to bmp and send to display
  - if the image doesn't change, the display will stay as is and checks back again in defined interval (`checkinDelay`)

## Possible improvements

- replace TIMACCoP with low level radio driver https://github.com/srhg/zag-bridge-firmware
- user python library to decode received frames e.g. https://github.com/andrewdodd/pyCCSniffer/blob/master/ieee15dot4.py

## License

Copyright (c) 2022 Daniel Kucera

THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.

Permission is hereby granted to use or copy this program
for any purpose,  provided the above notices are retained on all copies.
Permission to modify the code and to distribute modified code is granted,
provided the above notices are retained, and a notice that the code was
modified is included with the above copyright notice.
