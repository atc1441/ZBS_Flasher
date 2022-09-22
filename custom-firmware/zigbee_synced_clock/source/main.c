#define __packed
#include "asmUtil.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "drawing.h"
#include "printf.h"
#include "screen.h"
#include "radio.h"
#include "sleep.h"
#include "timer.h"
#include "eeprom.h"
#include "board.h"
#include "chars.h"
#include "wdt.h"
#include "adc.h"

#define COMMS_RX_ERR_NO_PACKETS			(-1)
#define COMMS_RX_ERR_INVALID_PACKET		(-2)

#define COMMS_MAX_PACKET_SZ				(127 /* max phy len */ - 21 /* max mac frame with panID compression */ - 2 /* FCS len */)

static uint8_t __xdata mCommsBuf[127];
static uint8_t __xdata mRxBuf[COMMS_MAX_PACKET_SZ];
uint8_t __xdata mSelfMac[8];
int8_t __xdata current_temperature = 0;
uint16_t __xdata battery_voltage = 0;
static uint8_t __xdata mLastLqi;
static int8_t __xdata mLastRSSI;

const char __xdata *timeString(uint32_t t)
{
	static char __xdata time_string[32];

	if (!time_string[0])
	{
		uint16_t h = 0;
		uint8_t m = 0;
		uint8_t s = 0;

		s = t % 60;
		t = (t - s) / 60;
		m = t % 60;
		t = (t - m) / 60;
		h = t % 24;
		spr(time_string, " %02d:%02d:%02d", h, m, s);
	}

	return time_string;
}

int8_t commsRxUnencrypted(void __xdata *data)
{
	uint8_t __xdata *dstData = (uint8_t __xdata *)data;
	uint8_t __xdata *__xdata rxedBuf;
	int8_t ret = COMMS_RX_ERR_INVALID_PACKET;

	int8_t rxedLen = radioRxDequeuePktGet((void __xdata *__xdata) & rxedBuf, &mLastLqi, &mLastRSSI);

	if (rxedLen < 0)
		return COMMS_RX_ERR_NO_PACKETS;

	xMemCopyShort(dstData, rxedBuf, rxedLen);
	radioRxDequeuedPktRelease();
	return rxedLen;
}

bool commsTxUnencrypted(const void __xdata *packetP, uint8_t len)
{
	const uint8_t __xdata *packet = (const uint8_t __xdata *)packetP;

	if (len > COMMS_MAX_PACKET_SZ)
		return false;
	memset(mCommsBuf, 0, COMMS_MAX_PACKET_SZ);
	xMemCopyShort(mCommsBuf + 1, packet, len);

	mCommsBuf[0] = len + RADIO_PAD_LEN_BY;

	radioTx(mCommsBuf);

	return true;
}

uint32_t current_unix_time = 0;
uint32_t one_second_trimmed = TIMER_TICKS_PER_SECOND;
uint32_t last_clock_increase = 0;
__xdata uint8_t last_minute = 0;
__xdata uint8_t last_hour = 0;
uint8_t was_never_synced = 0;

__xdata uint32_t last_synced_time = 0;
__xdata int32_t last_time_offset = 0;
__xdata uint32_t seconds_since_last_sync = 0;

char __xdata secondLine[64];

uint8_t dsn = 0;
uint8_t __xdata packetp[128];
void request_time_via_zigbee()
{
	if (!boardGetOwnMac(mSelfMac))
	{
		pr("failed to get MAC. Aborting\n");
		return;
	}
	radioInit();

	radioRxFilterCfg(mSelfMac, 0x10000, 0x3443);
	radioSetChannel(RADIO_FIRST_CHANNEL);
	radioSetTxPower(10);
	radioRxEnable(true, true);

	pr("Sending Time request to AP now\r\n");
	packetp[0] = 0x01;
	packetp[1] = 0xC8;
	packetp[2] = dsn;

	packetp[3] = 0xFF;
	packetp[4] = 0xFF;

	packetp[5] = 0xFF;
	packetp[6] = 0xFF;

	packetp[7] = 0x34;
	packetp[8] = 0x43;

	packetp[9] = mSelfMac[0];
	packetp[10] = mSelfMac[1];
	packetp[11] = mSelfMac[2];
	packetp[12] = mSelfMac[3];
	packetp[13] = mSelfMac[4];
	packetp[14] = mSelfMac[5];
	packetp[15] = mSelfMac[6];
	packetp[16] = mSelfMac[7];
	packetp[17] = 0xC8;
	packetp[18] = 0xBC;
	packetp[19] = current_unix_time >> 24;
	packetp[20] = current_unix_time >> 16;
	packetp[21] = current_unix_time >> 8;
	packetp[22] = current_unix_time;
	packetp[23] = current_temperature;
	dsn++;
	commsTxUnencrypted(packetp, 24/*LEN*/);
	uint32_t start_time = timerGet();
	while (timerGet() - start_time < (TIMER_TICKS_PER_SECOND / 10))// 100ms timeout waiting for an answer from AP
	{
		int8_t ret = commsRxUnencrypted(mRxBuf);
		if (ret > 1)
		{
			pr("RX Len: %d Data:", ret);
			for (uint8_t len = 0; len < ret; len++)
			{
				pr(" 0x%02X", mRxBuf[len]);
			}
			pr("\r\nradio off\r\n");
			radioRxEnable(false, true);
			if (mRxBuf[21] == 0xC9 && mRxBuf[22] == 0xCA)// random magic bytes
			{
				seconds_since_last_sync = current_unix_time - last_synced_time;
				last_synced_time = current_unix_time;
				current_unix_time = (uint32_t)mRxBuf[23] << 24 | (uint32_t)mRxBuf[24] << 16 | (uint32_t)mRxBuf[25] << 8 | (uint32_t)mRxBuf[26];
				pr("Setting the new time now: %lu\n", current_unix_time);
				if (was_never_synced >= 2)// only the second sync can be used to trim the time for now.
				{
					last_time_offset = current_unix_time - last_synced_time;
					pr("My offset was: %ld in %lu seconds\n", last_time_offset, seconds_since_last_sync);
					if (last_time_offset > 0 || last_time_offset < 0)// if the time was not correct try to align it
					{
						int32_t temp_off = last_time_offset * 1000;
						temp_off = temp_off / seconds_since_last_sync;
						pr("Wrong time: %ld\n", temp_off);
						one_second_trimmed = one_second_trimmed + (TIMER_TICKS_PER_SECOND / temp_off);
					}
				}
				was_never_synced++;
				if (was_never_synced >= 2)
					was_never_synced = 2;
			}
			return;
		}
	}
	pr("\r\nTIMEOUT radio off\r\n");
	radioRxEnable(false, true);
}

void handle_time()
{
	while (timerGet() - last_clock_increase > one_second_trimmed)
	{
		last_clock_increase += one_second_trimmed;
		current_unix_time++;
	}
}

void printRam()
{
	pr("RAM:\n");
	for (uint16_t posi = 0; posi <= 255;)
	{
		pr("%02X: ", posi);
		for (uint8_t posiInternal = 0; posiInternal < 16; posiInternal++)
		{
			pr("%02X ", *(__idata char *)posi);
			posi++;
		}
		pr("\n");
	}
}

void main(void)
{
	uint32_t __xdata sleepDuration;

	clockingAndIntsInit();
	timerInit();
	boardInit();

	pr("booted at 0x%04x\n", (uintptr_near_t)&main);

	irqsOn();
	set_partial_enable(1);
	drawFullscreenMsg((const __xdata char *)" Boot", "V1.3");
	screenShutdown();

	while (1)
	{
		handle_time();
		uint16_t h = 0;
		uint8_t m = 0;
		uint8_t s = 0;
		uint32_t t = 0;
		__bit do_refresh = 0;

		t = current_unix_time;
		s = t % 60;
		t = (t - s) / 60;
		m = t % 60;
		t = (t - m) / 60;
		h = t % 24;

		pr("Sleeping now %lu   %02d:%02d:%02d  LastMinute: %d\n", current_unix_time, h, m, s, last_minute);
		if (last_hour != h || !was_never_synced)// Only do this every hour
		{
			battery_voltage = adcSampleBattery() / 100;
			current_temperature = adcSampleTemperature();
			request_time_via_zigbee();
			t = current_unix_time;
			s = t % 60;
			t = (t - s) / 60;
			m = t % 60;
			t = (t - m) / 60;
			h = t % 24;
			last_hour = h;
			last_minute = m;
			do_refresh = 1;
		}
		else if (last_minute != m)// refresh the time every minute
		{
			last_minute = m;
			do_refresh = 1;
		}
		if (do_refresh)
		{
			do_refresh = 0;
			set_partial_enable(1);

			pr("temp: %d %dV Offset: %lu Trimmed: %lu\n", current_temperature, battery_voltage, last_time_offset, one_second_trimmed);
			spr(secondLine, "%d'C %dV W:%ld", current_temperature, battery_voltage, last_time_offset);
			drawFullscreenMsg(timeString(current_unix_time), secondLine);
		}
		uint32_t seconds_till_next_minute = 61 - (current_unix_time % 60);// Try to leep until one minute has passt
		pr("Sleeping now: %lu\n", seconds_till_next_minute);
		screenSleep();
		// printRam();
		powerPortsDownForSleep();
		if (!seconds_till_next_minute) // prevent endless sleeping this way
			seconds_till_next_minute = 60;
		sleepForMsec(seconds_till_next_minute * 1000); // Attention always also increase the unix time!
		current_unix_time += seconds_till_next_minute;
		// printRam();
		boardInit();
	}
	while (1)
		;
}
