
CC = SDCC\bin\sdcc

SOURCES		+= main.c
SOURCES		+= flash.c i2c.c sleep.c soc.c spi.c temperature.c timer.c uart.c wdt.c #radio.c 
SOURCES		+= asmUtil.c random.c printf.c board.c screen.c eeprom.c #aes.c 

FLAGS += --xram-loc 0xe000 --xram-size 0x2000 --model-large
FLAGS += -mmcs51 --std-c2x --opt-code-size --fomit-frame-pointer
FLAGS += --code-size 0xfc00
FLAGS += -I.


TARGETS	= main.ihx main.bin
OBJFILEEXT = rel

all:	#make sure it is the first target


OBJS = $(patsubst %.c,%.$(OBJFILEEXT),$(SOURCES))

all: $(TARGETS)


%.$(OBJFILEEXT): %.c
	$(CC) -c $^ $(FLAGS) -o $@

main.ihx: $(OBJS)
	rm -f $@
	$(CC) $^ $(FLAGS) -o $@

main.elf: $(OBJS)
	$(CC) $(FLAGS) -o $@ $^

%.bin: %.ihx
	objcopy $^ $@ -I ihex -O binary

.PHONY: clean
clean:
	rm -f $(patsubst %.c,%.asm,$(SOURCES))
	rm -f $(patsubst %.c,%.lst,$(SOURCES))
	rm -f $(patsubst %.c,%.rst,$(SOURCES))
	rm -f $(patsubst %.c,%.sym,$(SOURCES))
	rm -f $(patsubst %.c,%.map,$(SOURCES))
	rm -f $(patsubst %.c,%.mem,$(SOURCES))
	rm -f $(patsubst %.c,%.ihx,$(SOURCES))
	rm -f $(patsubst %.c,%.adb,$(SOURCES))
	rm -f $(patsubst %.c,%.adb,$(SOURCES))
	rm -f $(patsubst %.c,%.omf,$(SOURCES))
	#rm -f $(patsubst %.c,%.bin,$(SOURCES))
	rm -f $(patsubst %.c,%.lk,$(SOURCES))
	rm -f $(OBJS)

.PHONY: rembin
rembin:
	rm -f $(patsubst %.c,%.bin,$(SOURCES))
