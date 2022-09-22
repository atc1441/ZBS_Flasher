
CC = SDCC\bin\sdcc
#file containing main() must be first!
SOURCES	+= main.c drawing.c chars.c board.c screen.c
SOURCES += soc.c wdt.c sleep.c spi.c uart.c timer.c radio.c
SOURCES += flash.c temperature.c random.c printf.c asmUtil.c eeprom.c

all:	#make sure it is the first target

FLAGS += --code-size 0xfc00 -I
FLAGS += -DSOC_ZBS243 --xram-loc 0xe000 --xram-size 0x2000 --model-large
FLAGS += -mmcs51 --std-c2x --opt-code-size --peep-file peep.def --fomit-frame-pointer


TARGETS	= main.ihx main.bin
OBJFILEEXT = rel

FLAGS += -I.

OBJS = $(patsubst %.c,%.$(OBJFILEEXT),$(SOURCES))

all: $(TARGETS)

%.$(OBJFILEEXT): %.c
	$(CC) -c $^ $(FLAGS) -o $@

main.ihx: $(OBJS)
	rm -f $@
	$(CC) $^ $(FLAGS)

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
	rm -f $(patsubst %.c,%.bin,$(SOURCES))
	rm -f $(patsubst %.c,%.lk,$(SOURCES))
	rm -f $(OBJS)
