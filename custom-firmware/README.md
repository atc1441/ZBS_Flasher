# Intro
### This is a step by step guide on how to build the firmware for Solum SEM9110 alias ZBS243 Tags under Linux or Windows (Docker), that is talked about in [Dmitrys blog](http://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags).

> **_NOTE:_**  If you use Docker, you should know how to use it, especially how to get the copied file from the Docker container afterwards. Dealing with Docker is not covered here.

### Credits:
- https://twitter.com/atc1441
- https://twitter.com/dmitrygr

# For a simple starting point and direct testing of the labels use this precompiled firmware for the 2.9" ST-GR29000 Label with a few modifications from @atc1441

If you are on windows you need to install MinGW and add it to the Windows PATH, SDCC is included in the folder.

Manual: https://www.ics.uci.edu/~pattis/common/handouts/mingweclipse/mingw.html


Precompiled firmware:

V1.5:
- No greyscale but faster refresh with OTP LUT

[demo_firmware_2.9_33_V1.5.0.0.bin](demo_firmware_2.9_33_V1.5.0.0.bin)

V1.7:
- Better sleep, also while updating the screen. ~2,5uA
- Showing a black line instead of the "ASSOCIATE READY" screen when offline to leave image showing

[demo_firmware_2.9_33_V1.7.0.0.bin](demo_firmware_2.9_33_V1.7.0.0.bin)

The source code including all the modifications can be found in this folder on GitHub


You still need to dump the infopage once and add a MAC to 0x10 otherwise it will be just 8 * 0xFF for every label, it will still work but can disrupt things.

It is advised to compile your own version to have a felxible firmware setup.

# Download
Use an Arch Linux or set up a docker container with the archlinux image to build the firmware.
Other Distros can cause problems, e.g. the firmware does not find the MAC address.

Download the files with the following commands or get it from [dmitry.gr](http://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags):

>curl -LO http://dmitry.gr/images/einkTags_0001.zip
>curl -LO http://dmitry.gr/images/einkTags_0002_8051.zip

Install the "unzip" package with the following command, so that you will be able to unzip the downloaded files:
>pacman -Syy unzip

Unzip the files with the following commands:
>unzip einkTags_0001.zip 
>unzip einkTags_0002_8051.zip 

# Preparing
After unpacking `einkTags_0002_8051.zip`, change the "#include" of the "proto.h" in the files **comms.c** and **main.c**.

For example:
>#include "../../../arm/einkTags/patchedfw/proto.h"

to

>#include "../dmitrygr-eink/solum_BW_4.2_fw/proto.h"

> **_NOTE:_**  The new **proto.h** is found in the unzipped folder of `einkTags_0001.zip`

Comment out the line `#include "datamatrix.h"` in the **drawing.c**. _Yes, really!_

Install the "binutils" package with the following command, so that you will be able to use the `make` command for the **Makefile**:
>pacman -Syy binutils

Install the "sdcc" package with the following command, so that you will be able to compile the code:
>pacman -Syy sdcc

# Build
Go into the **/firmware** folder of the unzipped `einkTags_0002_8051.zip` folder and enter the following commands to build your **main.bin**, which can be used to flash it to your Solum SEM9110 alias ZBS243 Tag:

>make clean
>make BUILD=zbs29v026 CPU=8051 SOC=zbs243

# Flashing
Now you are able to flash the **main.bin** for example with the [ZBS_Flasher by atc1441](https://github.com/atc1441/ZBS_Flasher)

> **_NOTE:_**  Before flashing, please BACKUP THE INFOPAGE!

The infopage-binary contains calibration data.
Starting from byte 0x10 of the infopage-binary, 8 bytes of a mac address can be set.
