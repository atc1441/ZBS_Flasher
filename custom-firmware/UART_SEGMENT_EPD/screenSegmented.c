#include <stdbool.h>
#include "screenSegmented.h"
#include "zbs243.h"
#include "timer.h"
#include "spi.h"

__bit display_is_drawing = 0;
uint32_t screen_start_time = 0;

#pragma callee_saves screenPrvTimedWait
static __bit screenPrvTimedWait(uint32_t maxTicks)
{
	uint32_t startTicks = timerGet();

	while (timerGet() - startTicks < maxTicks)
	{
		if (!P2_0)
			return true;
	}

	return false;
}

#pragma callee_saves screenPrvRegWriteGuts
static __bit screenPrvRegWriteGuts(uint32_t val, uint8_t reg) // order because sdcc sucks
{
	if (!screenPrvTimedWait(TIMER_TICKS_PER_SECOND / 1000))
		return false;

	P2_1 = 0;
	spiByte((uint8_t)0x80 + (uint8_t)(reg << 1));
	spiByte(val >> 16);
	spiByte(val >> 8);
	spiByte(val);
	P2_1 = 1;

	return true;
}

#define screenPrvRegWrite(_reg, _val) screenPrvRegWriteGuts(_val, _reg)

__bit screenDraw(const uint8_t __xdata *data, __bit inverted, __bit custom_lut)
{
	display_is_drawing = 0;
	P2_1 = 1;
	P2_2 = 1; // power it up
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);
	P1_7 = 0; // assert reset
	timerDelay(TIMER_TICKS_PER_SECOND / 100);
	P1_7 = 1; // release reset
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);

	P1FUNC |= 0x40;
	// wait for it
	if (!screenPrvTimedWait(TIMER_TICKS_PER_SECOND))
	{
		return false;
	}

	// internal oscillator init?
	if (!screenPrvRegWrite(0x03, 0x3a0000))
		return false;
	// iface init?
	if (!screenPrvRegWrite(0x01, 0x070000))
		return false;
	if (!screenPrvRegWrite(0x03, 0x400000))
		return false;
	if (!screenPrvRegWrite(0x04, 0xfc0000))
		return false;

	// data (doesnt agree with spec but makes sense)
	if (!screenPrvRegWrite(0x0d, *(uint32_t __xdata *)(data + 0)))
		return false;
	if (!screenPrvRegWrite(0x0e, *(uint32_t __xdata *)(data + 3)))
		return false;
	if (!screenPrvRegWrite(0x0f, *(uint32_t __xdata *)(data + 6)))
		return false;
	if (!screenPrvRegWrite(0x10, *(uint32_t __xdata *)(data + 9)))
		return false;
	if (custom_lut)
	{
		// LUTs
		if (!screenPrvRegWrite(0x14, *(uint32_t __xdata *)(data + 12)))
			return false;
		if (!screenPrvRegWrite(0x15, *(uint32_t __xdata *)(data + 15)))
			return false;
		if (!screenPrvRegWrite(0x16, *(uint32_t __xdata *)(data + 18)))
			return false;

		if (!screenPrvRegWrite(0x19, *(uint32_t __xdata *)(data + 21)))
			return false;
		if (!screenPrvRegWrite(0x1a, *(uint32_t __xdata *)(data + 24)))
			return false;

		// update!
		if (!screenPrvRegWrite(0x00, *(uint32_t __xdata *)(data + 27) | (inverted ? 0x200000 : 0x000000)))
			return false;
	}
	else
	{
		// LUTs
		if (!screenPrvRegWrite(0x14, 0x440000))
			return false;
		if (!screenPrvRegWrite(0x15, inverted ? 0x680001 : 0x860000))
			return false;
		if (!screenPrvRegWrite(0x16, inverted ? 0x240000 : 0x040000))
			return false;

		if (!screenPrvRegWrite(0x19, 0x082514))
			return false;
		if (!screenPrvRegWrite(0x1a, 0xa02000))
			return false;

		// update!
		if (!screenPrvRegWrite(0x00, inverted ? 0xa0001c : 0x80001c))
			return false;
	}
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);

	screen_start_time = timerGet();
	display_is_drawing = 1;
	return true;
}

void display_end()
{
	P1_7 = 0; // assert reset
	timerDelay(TIMER_TICKS_PER_SECOND / 100);
	P2_2 = 0; // power it dowm

	P1FUNC &= ~0x40;
}

uint8_t is_drawing()
{
	if (display_is_drawing)
	{
		if (!P2_0 || (timerGet() - screen_start_time > (TIMER_TICKS_PER_SECOND*3)))
		{
			display_end();
			display_is_drawing = 0;
			return 0;
		}
		return 1;
	}
	return 0;
}