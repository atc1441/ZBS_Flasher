#include <stdbool.h>
#include "board.h"
#include "spi.h"
#include "zbs243.h"
#include "wdt.h"
#include "main.h"

void clockingAndIntsInit(void)
{
	IEN0 = 0;
	CLKEN = 0x00;	 // timers only for now
	CLKSPEED = 0x01; // fast crystal
}

void powerPortsDownForSleep(void)
{
	P0FUNC = 0;
	P1FUNC = 0;
	P2FUNC = 0;
	P0 = 0;
	P1 = 2;
	P2 = 1;
	P0PULL = 0;
	P1PULL = 0;
	P2PULL = 0;
	P0DIR = 0x80;
	P1DIR = 0;
	P2DIR = 0b11111001;

	// UART RX GPIO Pin Int enable
	P0LVLSEL = 0x00;
	P0CHSTA = 0x00;
	P0INTEN = 0x80;
}

void boardInit(void)
{
	wdtOff();

	P0FUNC = 0b10000111;// No UART TXD pim since we want to wake up later
	P1FUNC = 0b00000000;
	P2FUNC = 0b00000000;
	P0 = 0;
	P1 = 0;
	P2 = 0;
	P0PULL = 0x00;
	P1PULL = 0x00;
	P2PULL = 0x00;
	P0DIR = 0b01111100;
	P1DIR = 0b00111011;
	P2DIR = 0b11111001;

	// raise chip select(s)
	P1_1 = 1;
	P1_7 = 1;

	// BS1 = low
	P1_2 = 0;

	uartInit();
	spiInit();
}