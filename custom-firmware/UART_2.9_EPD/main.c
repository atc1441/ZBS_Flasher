#define __packed
#include "asmUtil.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "printf.h"
#include "eeprom.h"
#include "screen.h"
//#include "radio.h"
#include "sleep.h"
#include "timer.h"
#include "board.h"
#include "wdt.h"
#include "adc.h"
#include "cpu.h"

uint8_t eeprom_available = 0;
uint16_t cur_eeprom_position = 0;

int8_t __xdata mCurTemperature;
int8_t __xdata battery_voltage;

#define UART_CMD_SIZE 255 // Here we use the MAX since the Master defines later how man
__xdata uint16_t data_len[2] = {0, 0};
__xdata uint16_t cmd_len[2] = {0, 0};
__xdata uint16_t cmd_pos = 0;
__xdata uint16_t cmd_crc[2] = {0, 0};
__xdata uint16_t cmd_crc_in[2] = {0, 0};
__xdata uint16_t cmd_data_pos[2] = {0, 0};
__xdata uint16_t cmd_data_cmd[2] = {0, 0};
__xdata uint16_t endpoint[2] = {0, 0};
__xdata uint8_t uart_cmd_buffer[2][UART_CMD_SIZE + 8];
__xdata uint8_t uart_cmd_answer_buffer[UART_CMD_SIZE + 8];

__bit used_buffer = 0;
__bit new_cmd = 0;

__bit out_enable = 0;
uint16_t out_posi = 0;
uint16_t out_end_len = 0;
uint8_t __xdata uart_cmd_out_buffer[UART_CMD_SIZE + 8];

__bit screen_refresh_started = 0;

uint32_t start_time = 0;


#define BROADCAST_NODE_ID 0xffff
#define NODE_TX_ID 0xfffe

#define CMD_TYPE_NODE_TX 0xAA

#define CMD_TYPE_REFRESH_SEG 0x41
#define CMD_TYPE_REFRESH_SEG_LOAD 0x42
#define CMD_TYPE_REFRESH_SEG_LOADED 0x43
#define CMD_TYPE_REFRESH_SEG_CUSTOM_LUT 0x45
#define CMD_TYPE_REFRESH_SEG_LOADED_CUSTOM_LUT 0x46

#define CMD_TYPE_GET_SENSOR 0xF0
#define CMD_TYPE_PING 0xF1

#define CMD_TYPE_REFRESH_END_PARTIAL 0x13
#define CMD_TYPE_EPD_END_DATA_PARTIAL 0x14

#define CMD_TYPE_REFRESH 0x01
#define CMD_TYPE_REFRESH_INIT 0x02
#define CMD_TYPE_REFRESH_MID 0x03
#define CMD_TYPE_REFRESH_END 0x04
#define CMD_TYPE_SLEEP 0x05
#define CMD_TYPE_EPD_DATA 0x06
#define CMD_TYPE_EPD_INIT_MID_DATA 0x07
#define CMD_TYPE_EPD_INIT_DATA 0x08
#define CMD_TYPE_EPD_END_DATA 0x09

#define CMD_TYPE_ERASE_OTA_AREA 0x10
#define CMD_TYPE_WRITE_EEPROM 0x11
#define CMD_TYPE_DO_UPDATE 0x12


#define CMD_TYPE_WA 0xFF

uint16_t crc16(uint16_t cur_crc, uint8_t data)
{
	cur_crc ^= data;
	for (uint8_t i = 8; i > 0; i--)
	{
		if ((cur_crc & 0x001) != 0)
		{
			cur_crc >>= 1;
			cur_crc ^= 0x8005; // poly
		}
		else
		{
			cur_crc >>= 1;
		}
	}
	return cur_crc;
}

uint8_t check_ota_crc(uint16_t crc_in, uint16_t update_len)
{
	uint16_t crc_checked = 0x0000; // crc start value
	eepromReadStart(0x04000UL);
	for (uint32_t i = 0; i < update_len; i++)
	{
		crc_checked = crc16(crc_checked, eepromByte(0));
	}
	eepromPrvDeselect();
	if (crc_checked == crc_in)
		return 1;
	return 0;
}

void sleep()
{
	screen_refresh_started = 0;
	eepromDeepPowerDown();
	screenSleep();
	powerPortsDownForSleep();
	sleepForMsec(0);
	wdtDeviceReset();
}

void handle_cmd(uint8_t cmd, uint8_t data_len, uint8_t *the_data)
{
	switch (cmd)
	{
	case CMD_TYPE_REFRESH:
		screen_refresh_started = 1;
		screenTxStart();
		for (uint32_t i = 0; i < 4736; i++)
		{
			screenByteTx(the_data[0]);
		}
		screenEndPass();
		for (uint32_t i = 0; i < 4736; i++)
		{
			screenByteTx(the_data[1]);
		}
		screen_refresh_started = 0;
		screenTxEnd(the_data[2] & 1);
		break;
	case CMD_TYPE_REFRESH_INIT:
		screen_refresh_started = 1;
		screenTxStart();
		break;
	case CMD_TYPE_REFRESH_MID:
		if (screen_refresh_started)
		{
			screenEndPass();
		}
		break;
	case CMD_TYPE_REFRESH_END:
		if (screen_refresh_started)
		{
			screen_refresh_started = 0;
			screenTxEnd(0);
		}
		break;
	case CMD_TYPE_REFRESH_END_PARTIAL:
		if (screen_refresh_started)
		{
			screen_refresh_started = 0;
			screenTxEnd(1);
		}
		break;
	case CMD_TYPE_SLEEP:
		sleep();
		break;
	case CMD_TYPE_EPD_INIT_DATA:
		screen_refresh_started = 1;
		screenTxStart();
		for (uint32_t i = 0; i < data_len; i++)
		{
			screenByteTx(the_data[i]);
		}
		break;
	case CMD_TYPE_EPD_INIT_MID_DATA:
		if (screen_refresh_started)
		{
			screenEndPass();
			for (uint32_t i = 0; i < data_len; i++)
			{
				screenByteTx(the_data[i]);
			}
		}
		break;
	case CMD_TYPE_EPD_END_DATA:
		if (screen_refresh_started)
		{
			for (uint32_t i = 0; i < data_len; i++)
			{
				screenByteTx(the_data[i]);
			}
			screen_refresh_started = 0;
			screenTxEnd(0);
		}
		break;
	case CMD_TYPE_EPD_END_DATA_PARTIAL:
		if (screen_refresh_started)
		{
			for (uint32_t i = 0; i < data_len; i++)
			{
				screenByteTx(the_data[i]);
			}
			screen_refresh_started = 0;
			screenTxEnd(1);
		}
		break;
	case CMD_TYPE_EPD_DATA:
		if (screen_refresh_started)
		{
			for (uint32_t i = 0; i < data_len; i++)
			{
				screenByteTx(the_data[i]);
			}
		}
		break;
	case CMD_TYPE_ERASE_OTA_AREA:
		if (eeprom_available)
			eepromErase(0x04000UL, 0x10000UL / 4096);
		cur_eeprom_position = 0;
		break;
	case CMD_TYPE_WRITE_EEPROM:
		if (eeprom_available)
		{
			eepromWrite(0x04000UL + cur_eeprom_position, &the_data[0], data_len);
			cur_eeprom_position += data_len;
		}
		break;
	case CMD_TYPE_DO_UPDATE:
		if (eeprom_available)
		{
			uint16_t crc_in = (the_data[0] << 8) | (the_data[1]);
			uint16_t fw_len = (the_data[2] << 8) | (the_data[3]);
			if (fw_len && check_ota_crc(crc_in, fw_len))
			{
				timerDelay(TIMER_TICKS_PER_SECOND / 10);
				eepromReadStart(0x04000UL);
				selfUpdate();
			}
		}
		break;
	case CMD_TYPE_GET_SENSOR:
	{
		mCurTemperature = adcSampleTemperature();
		uart_cmd_answer_buffer[2] = mCurTemperature >> 8;
		uart_cmd_answer_buffer[3] = mCurTemperature & 0xff;
		uart_cmd_answer_buffer[4] = cur_eeprom_position >> 8;
		uart_cmd_answer_buffer[5] = cur_eeprom_position & 0xff;
		uart_cmd_answer_buffer[6] = eeprom_available;
		uart_cmd_answer_buffer[7] = 0x13; // DISPLAY TYPE 2.9"
	}
	break;
	case CMD_TYPE_PING:
	{
		// Nothing till now but a cmd will be successful
	}
	break;
	case CMD_TYPE_WA:
	{
		// Nothing till now but a cmd will be successful
	}
	break;
	}
}

/////////////////////////////////////

uint8_t tx_free = 0;

void uartInit(void)
{
	// clock it up
	CLKEN |= 0x20;
	// configure
	UARTBRGH = 0x00; // config for 115200
	UARTBRGL = 0x8A;
	UARTSTA = 0x12; // also set the "empty" bit else we wait forever for it to go up
	IEN_UART0 = 1;
}

void send_to_next_node(uint8_t cmd, uint16_t endp, uint8_t data_len, uint8_t cmd_len, uint8_t *cmd_buffer)
{

	uint16_t out_crc = 0x00CA;

	uart_cmd_out_buffer[0] = 0xCA;
	uart_cmd_out_buffer[1] = data_len;
	out_crc += data_len;
	uart_cmd_out_buffer[2] = cmd_len;
	out_crc += cmd_len;
	uart_cmd_out_buffer[3] = cmd;
	out_crc += cmd;
	for (uint8_t i = 0; i < cmd_len; i++)
	{
		uart_cmd_out_buffer[4 + i] = cmd_buffer[i];
		out_crc += cmd_buffer[i];
	}
	for (int i = 0; i < (data_len - cmd_len); i++)
	{
		uart_cmd_out_buffer[4 + cmd_len + i] = 0x00;
	}

	uart_cmd_out_buffer[4 + data_len] = endp >> 8;
	out_crc += endp >> 8;
	uart_cmd_out_buffer[5 + data_len] = endp & 0xff;
	out_crc += endp & 0xff;

	uart_cmd_out_buffer[6 + data_len] = out_crc >> 8;
	uart_cmd_out_buffer[7 + data_len] = out_crc & 0xff;

	out_end_len = 7 + data_len;
	out_posi = 1;
	uartTx(uart_cmd_out_buffer[0]);
	out_enable = 1;
}

#pragma callee_saves uartTx
void uartTx(uint8_t val)
{
	while (tx_free)
		;
	UARTBUF = val;
	tx_free = 1;
}
////////////////////////////////////////////////////////

void uart_cmd(uint8_t rx_cur)
{
	switch (cmd_pos)
	{
	case 0:
		if (rx_cur == 0xCA)
		{
			cmd_crc[used_buffer] = 0x00CA;
			P0FUNC |= 0b01000000; // Enable Uart TXD only now to wake up the next node
			cmd_pos++;
		}
		break;
	case 1:
		data_len[used_buffer] = rx_cur;
		cmd_crc[used_buffer] += rx_cur;
		cmd_pos++;
		break;
	case 2:
		if (rx_cur <= data_len[used_buffer])
		{
			cmd_len[used_buffer] = rx_cur;
			cmd_data_pos[used_buffer] = 0;
			cmd_crc[used_buffer] += rx_cur;
			cmd_pos++;
		}
		else
		{
			cmd_pos = 0;
		}
		break;
	case 3:
		cmd_data_cmd[used_buffer] = rx_cur;
		cmd_crc[used_buffer] += rx_cur;
		cmd_pos++;
		break;
	case 4:
		uart_cmd_buffer[used_buffer][cmd_data_pos[used_buffer]++] = rx_cur;
		cmd_crc[used_buffer] += rx_cur;
		if (cmd_data_pos[used_buffer] >= data_len[used_buffer])
		{
			cmd_pos++;
		}
		break;
	case 5:
		endpoint[used_buffer] = rx_cur << 8;
		cmd_crc[used_buffer] += rx_cur;
		cmd_pos++;
		break;
	case 6:
		endpoint[used_buffer] += rx_cur;
		cmd_crc[used_buffer] += rx_cur;
		cmd_pos++;
		break;
	case 7:
		cmd_crc_in[used_buffer] = rx_cur << 8;
		cmd_pos++;
		break;
	case 8:
		if ((cmd_crc_in[used_buffer] | rx_cur) == cmd_crc[used_buffer])
		{
			used_buffer = !used_buffer;
			new_cmd = 1;
		}
		cmd_pos = 0;
		break;
	}
}

void UART_IRQ1(void) __interrupt(0)
{
	P1_0 = 1;
	if (UARTSTA & 1)
	{
		UARTSTA &= 0xfe;
		uart_cmd(UARTBUF);
	}
	if (UARTSTA & 2)
	{
		UARTSTA &= 0xfd;
		if (out_enable)
		{
			if (out_posi <= out_end_len)
			{
				UARTBUF = uart_cmd_out_buffer[out_posi++];
			}
			else
			{
				out_enable = 0;
			}
		}
		tx_free = 0;
	}
	P1_0 = 0;
}

void main(void)
{
	P0CHSTA = 0x00;
	clockingAndIntsInit();
	timerInit();
	boardInit();
	if (eepromInit())
	{
		eeprom_available = 1;
	}
	irqsOn();
	while (is_drawing() || (timerGet() - start_time < (uint32_t)((uint64_t)TIMER_TICKS_PER_SECOND)))
	{
		if (new_cmd)
		{
			new_cmd = 0;
			start_time = timerGet();
			if (endpoint[!used_buffer] == BROADCAST_NODE_ID) // The CMD is a broadcast cmd so for every node
			{
				send_to_next_node(cmd_data_cmd[!used_buffer], endpoint[!used_buffer], data_len[!used_buffer], cmd_len[!used_buffer], uart_cmd_buffer[!used_buffer]);
				handle_cmd(cmd_data_cmd[!used_buffer], cmd_len[!used_buffer], uart_cmd_buffer[!used_buffer]);
			}
			else if (endpoint[!used_buffer] == 0) // The CMD was meant for us
			{
				uart_cmd_answer_buffer[0] = cmd_data_cmd[!used_buffer];
				uart_cmd_answer_buffer[1] = data_len[!used_buffer];
				send_to_next_node(CMD_TYPE_NODE_TX, NODE_TX_ID, data_len[!used_buffer], 20, uart_cmd_answer_buffer);
				handle_cmd(cmd_data_cmd[!used_buffer], cmd_len[!used_buffer], uart_cmd_buffer[!used_buffer]);
			}
			else // The CMD is on its way to its node so substract one node and send further
			{
				send_to_next_node(cmd_data_cmd[!used_buffer], endpoint[!used_buffer] - 1, data_len[!used_buffer], cmd_len[!used_buffer], uart_cmd_buffer[!used_buffer]);
			}
		}
	}
	sleep();
}