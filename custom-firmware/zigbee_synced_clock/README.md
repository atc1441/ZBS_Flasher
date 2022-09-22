# This is a simple E-Paper Clock
The time is synced via a zigbee station

The time is displayed every Minute via a partial refresh and synced every hour.
The current consumption is currently at 200uA and way too high, but usable.


Demo video (click on the picture to watch it):

[![YoutubeVideo](https://img.youtube.com/vi/9TVta7L0C74/0.jpg)](https://www.youtube.com/watch?v=9TVta7L0C74)

## Compiling
Use this manual for [Compiling](../)

## Running
After flashing the firmware as described in the compiling manual start the station via "python station.py" it may be needed to change the "station.py" to the correct COM port.
The python tool is based on Daniels work: https://github.com/danielkucera/epaper-station/
