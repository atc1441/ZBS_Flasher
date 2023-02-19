
make rembin all
make clean

zbs_flasher.exe -p COM17 write main.bin reset monitor
rem zbs_flasher.exe -p COM17 write main.bin reset
