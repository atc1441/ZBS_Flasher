

# CC2531 Zigbee station infos are here:
[CC2531_station](../CC2531_station/)


# Precompiled custom firmware
## For a simple starting point and direct testing of the labels use this precompiled firmwares for the 2.9" ST-GR29000 Label with a few modifications from @atc1441

V1.5:
- No greyscale but faster refresh with OTP LUT

[demo_firmware_2.9_33_V1.5.0.0.bin](demo_firmware_2.9_33_V1.5.0.0.bin)

V1.7:
- Better sleep, also while updating the screen. ~2,5uA
- Showing a black line instead of the "ASSOCIATE READY" screen when offline to leave image showing

[demo_firmware_2.9_33_V1.7.0.0.bin](demo_firmware_2.9_33_V1.7.0.0.bin)

V1.8:
- After a new Pairing it will show the last valid image again instead of erasing everything
- HW_Upair option via P1.0 Pin added

[demo_firmware_2.9_33_V1.8.0.0.bin](demo_firmware_2.9_33_V1.8.0.0.bin)

V1.9:
- Better connection status showing on display

[demo_firmware_2.9_33_V1.9.0.0.bin](demo_firmware_2.9_33_V1.9.0.0.bin)

The source of the newest firmware is from this repo

## Adding a MAC
Since the custom firmware expects a 8 byte MAC address in the Infopage of the SOC at 0x10 offset it is needed write it there, here is how:

The flashing of a MAC can happen before or after flashing the firmware.

Step 1. Dump the current infopage with the zbs_flasher via "zbs_flasher COMxx readI dump_infopage.bin"

Step 2. Use a tool like "HxD" on windows to edit the "dump_infopage.bin" and put in a random 8 byte MAC at the second line / offset 0x10 and save the file

Step 3. Flash the modified file back via "zbs_flasher COMxx writeI dump_infopage.bin"


# Compiling on Windows
To get started under windows downlaod and install MinGW as descripted here: https://www.ics.uci.edu/~pattis/common/handouts/mingweclipse/mingw.html

Add both the "C:\MinGW\msys\1.0\bin" and "C:\MinGW\bin" folder to your system PATH!, you may also want to add the ZBS_Flasher.exe folder to it for simplicity.

after having installed MinGW restart your Mashine and test it via a CMD line by entering "make" and it should not show any errors.

Clone this repository and CD into the "firmware_ch11_low_power" folder and execude the "compile.bat" file it will now build the firmware and will try to flash it via the "zbs_flasher" you may want to put your correct COM port into the "compile.bat" file. It will use the included SDCC to build the firmware.
(to build the other custom firmwares copy SDCC into it)

### The now created "main.bin" is your own compiled version which can be flashed.


# Compiling on Linux
### This is a step by step guide on how to build the firmware for Solum SEM9110 alias ZBS243 Tags under Linux, that is talked about in [Dmitrys blog](http://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags).


## Download
Use an Arch Linux or set up a docker container with the archlinux image to build the firmware.
Other Distros can cause problems, e.g. the firmware does not find the MAC address.

Download the files with the following commands or get it from [dmitry.gr](http://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags):

>curl -LO http://dmitry.gr/images/einkTags_0001.zip
>
>curl -LO http://dmitry.gr/images/einkTags_0002_8051.zip

Install the "unzip" package with the following command, so that you will be able to unzip the downloaded files:
>pacman -Syy unzip

Unzip the files with the following commands:
>unzip einkTags_0001.zip
>
>unzip einkTags_0002_8051.zip 

## Preparing
After unpacking `einkTags_0002_8051.zip`, change the "#include" of the "proto.h" in the files **comms.c** and **main.c**.

For example:
>#include "../../../arm/einkTags/patchedfw/proto.h"

to

>#include "../dmitrygr-eink/solum_BW_4.2_fw/proto.h"

> **_NOTE:_**  The new **proto.h** is found in the unzipped folder of `einkTags_0001.zip`

Comment out the line `#include "datamatrix.h"` in the **drawing.c**. _Yes, really!_

Install the "binutils" package with the following command, so that you will be able to use the `make` command for the **Makefile**:
>pacman -Syy binutils

Sometimes the following commands are needed:

>pacman-key --init
>
>pacman -Syy base-devel

Install the "sdcc" package with the following command, so that you will be able to compile the code:
>pacman -Syy sdcc

## Build
Go into the **/firmware** folder of the unzipped `einkTags_0002_8051.zip` folder and enter the following commands to build your **main.bin**, which can be used to flash it to your Solum SEM9110 alias ZBS243 Tag:

>make clean
>
>make BUILD=zbs29v026 CPU=8051 SOC=zbs243

For the CH11 Low Power Version 2.9" use:

>make clean
>
>make BUILD=zbs29v033 CPU=8051 SOC=zbs243

For the 1.54" Version:

>make BUILD=zbs154v033 CPU=8051 SOC=zbs243

## Flashing
Now you are able to flash the **main.bin** for example with the [ZBS_Flasher by atc1441](https://github.com/atc1441/ZBS_Flasher)

> **_NOTE:_**  Before flashing, please BACKUP THE INFOPAGE!

The infopage-binary contains calibration data.
Starting from byte 0x10 of the infopage-binary, 8 bytes of a mac address can be set.

# Credits:
- https://twitter.com/atc1441
- https://twitter.com/dmitrygr
- Many more !