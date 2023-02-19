make clean

make BUILD=zbs29v033 CPU=8051 SOC=zbs243

zbs_flasher.exe -p COM17 write main.bin reset monitor
