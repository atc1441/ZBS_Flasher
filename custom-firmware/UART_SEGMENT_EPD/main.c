#define __packed
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sleep.h"
#include "timer.h"
#include "board.h"
#include "wdt.h"
#include "zbs243.h"
#include "flash.h"
#include "main.h"
#include "screenSegmented.h"

#define FLASH_ADDR_CUSTOM_LUT 0xf000

#define UART_CMD_SIZE 255 // Here we use the MAX since the Master defines later how man
uint16_t data_len[2] = {0, 0};
uint16_t cmd_len[2] = {0, 0};
uint16_t cmd_pos = 0;
uint16_t cmd_crc[2] = {0, 0};
uint16_t cmd_crc_in[2] = {0, 0};
uint16_t cmd_data_pos[2] = {0, 0};
uint16_t cmd_data_cmd[2] = {0, 0};
uint16_t endpoint[2] = {0, 0};
__xdata uint8_t uart_cmd_buffer[2][UART_CMD_SIZE + 8];
__xdata uint8_t display_loaded_buffer[200];

__bit used_buffer = 0;
__bit new_cmd = 0;

__bit out_enable = 0;
uint16_t out_posi = 0;
uint16_t out_end_len = 0;
uint8_t __xdata uart_cmd_out_buffer[UART_CMD_SIZE + 8];

uint32_t start_time = 0;
uint32_t stay_awake_time = (uint32_t)((uint64_t)TIMER_TICKS_PER_SECOND / 10);

#define BROADCAST_NODE_ID 0xffff
#define NODE_TX_ID 0xfffe

#define CMD_TYPE_NODE_TX 0xAA
#define CMD_TYPE_STAY_AWAKE 0xAB
#define CMD_TYPE_REFRESH_SEG 0x41
#define CMD_TYPE_REFRESH_SEG_LOAD 0x42
#define CMD_TYPE_REFRESH_SEG_LOADED 0x43
#define CMD_TYPE_REFRESH_SEG_CUSTOM_LUT 0x45
#define CMD_TYPE_REFRESH_SEG_LOADED_CUSTOM_LUT 0x46
#define CMD_TYPE_REFRESH_PIXEL 0x47
#define CMD_TYPE_REFRESH_PIXEL_CUSTOM_LUT 0x48

void sleep()
{
	powerPortsDownForSleep();
	sleepForMsec(0);
	wdtDeviceReset();
}

void make_pixel_refresh(uint16_t our_id, uint8_t *the_data, __bit custom_lut)
{

	uint16_t our_byte = our_id / 8;

	uint8_t used_bit = our_id % 8;

	uint8_t our_bit = (the_data[our_byte] >> used_bit) & 1;

	if (custom_lut)
	{
		flashRead(FLASH_ADDR_CUSTOM_LUT, &display_loaded_buffer[0], 30);
		screenDraw(&display_loaded_buffer[0], our_bit, 1);
	}
	else
	{
		for (uint8_t i = 0; i < 14; i++)
		{
			display_loaded_buffer[i] = 0x00;
		}
		screenDraw(&display_loaded_buffer[0], our_bit, 0);
	}
}

void handle_cmd(uint8_t cmd, uint8_t data_len, uint8_t *the_data)
{
	switch (cmd)
	{
	case CMD_TYPE_REFRESH_SEG:
		screenDraw(&the_data[1], the_data[0] & 1, 0);
		break;
	case CMD_TYPE_REFRESH_SEG_LOAD:
		flashWrite(FLASH_ADDR_CUSTOM_LUT, &the_data[0], data_len, 1);
		break;
	case CMD_TYPE_REFRESH_SEG_LOADED:
		screenDraw(&the_data[1], the_data[0] & 1, 0);
		break;
	case CMD_TYPE_REFRESH_SEG_CUSTOM_LUT:
		screenDraw(&the_data[0], 0, 1);
		break;
	case CMD_TYPE_REFRESH_SEG_LOADED_CUSTOM_LUT:
		flashRead(FLASH_ADDR_CUSTOM_LUT, &display_loaded_buffer[0], 30);
		screenDraw(&display_loaded_buffer[0], 0, 1);
		break;
	case CMD_TYPE_REFRESH_PIXEL:
		make_pixel_refresh(0, &the_data[0], 0);
		break;
	case CMD_TYPE_REFRESH_PIXEL_CUSTOM_LUT:
		make_pixel_refresh(0, &the_data[0], 1);
		break;
	case CMD_TYPE_STAY_AWAKE:
		stay_awake_time = (TIMER_TICKS_PER_SECOND / 10) * the_data[0];
		break;
	}
}

__bit tx_free = 0;

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

	IEN_EA = 1;

	flashRead(0xe000, &display_loaded_buffer[0], 1);
	if (display_loaded_buffer[0] != 0xAC)
	{
		display_loaded_buffer[3] = 0b01101000; /*number small last*/
		display_loaded_buffer[4] = 0b11101000; /*number small after point*/
		display_loaded_buffer[2] = 0b11111100; /*number small before point*/
		screenDraw(&display_loaded_buffer[1], 0, 0);
		while (is_drawing())
		{
		}
		display_loaded_buffer[0] = 0xAC;
		flashWrite(0xe000, &display_loaded_buffer[0], 1, 1);
	}

	while (is_drawing() || (timerGet() - start_time < stay_awake_time))
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
				send_to_next_node(CMD_TYPE_NODE_TX, NODE_TX_ID, data_len[!used_buffer], 0, uart_cmd_buffer[!used_buffer]);
				handle_cmd(cmd_data_cmd[!used_buffer], cmd_len[!used_buffer], uart_cmd_buffer[!used_buffer]);
			}
			else // The CMD is on its way to its node so substract one node and send further
			{
				send_to_next_node(cmd_data_cmd[!used_buffer], endpoint[!used_buffer] - 1, data_len[!used_buffer], cmd_len[!used_buffer], uart_cmd_buffer[!used_buffer]);
				if (cmd_data_cmd[!used_buffer] == CMD_TYPE_REFRESH_PIXEL) // Special CMD so that we can use one TX for all pixels
					make_pixel_refresh(endpoint[!used_buffer], uart_cmd_buffer[!used_buffer], 0);
				else if (cmd_data_cmd[!used_buffer] == CMD_TYPE_REFRESH_PIXEL_CUSTOM_LUT)
					make_pixel_refresh(endpoint[!used_buffer], uart_cmd_buffer[!used_buffer], 1);
			}
		}
	}
	sleep();
}