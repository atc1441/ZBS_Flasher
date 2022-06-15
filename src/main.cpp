
/*   Autor: Aaron Christophel ATCnetz.de   */
#include <Arduino.h>
#include <stdio.h>
#include <stdint.h>
#include "zbs_interface.h"
#include "main.h"

#define LED 22

#define ZBS_SS 23
#define ZBS_CLK 18
#define ZBS_MoSi 5
#define ZBS_MiSo 17
#define ZBS_Reset 19
#define ZBS_POWER 16

#define ZBS_RXD 4 // Maybe later used to read UART data from the firmware running on the ZBS, not needed at all

uint32_t FLASHER_VERSION = 0x00000010;

uint16_t addr = 0;
void setup()
{
  Serial.begin(115200);
  // Serial.printf("Welcome\r\n");

  // Serial1.begin(115200, SERIAL_8N1, -1, 12);
  // Serial1.printf("Welcome\r\n");// Used for debugging the code itself but disabled for speed

  pinMode(LED, OUTPUT);
  pinMode(ZBS_RXD, INPUT);
  digitalWrite(LED, LOW);

  while (Serial.available()) // Flushing UART
    Serial.read();
}

void loop()
{
  UART_handler();
}

int UART_rx_state = 0;
uint8_t expected_len = 0;
long last_rx_time = 0;
uint8_t UART_rx_buffer[0x200] = {0};
int curr_data_pos = 0;
uint32_t CRC_calc = 0xAB34;
uint32_t CRC_in = 0;
uint8_t UART_CMD = 0;
void UART_handler()
{
  while (Serial.available())
  {
    last_rx_time = millis();
    uint8_t curr_char = Serial.read();
    switch (UART_rx_state)
    {
    case 0:
      if (curr_char == 'A') // Header
        UART_rx_state++;
      break;
    case 1:
      if (curr_char == 'T') // Header
        UART_rx_state++;
      else
        UART_rx_state = 0;
      break;
    case 2:
      UART_CMD = curr_char; // Receive current CMD
      CRC_calc = 0xAB34;
      CRC_calc += curr_char;
      UART_rx_state++;
      break;
    case 3:
      expected_len = curr_char; // Receive Expected length of data
      CRC_calc += curr_char;
      curr_data_pos = 0;
      if (expected_len == 0)
        UART_rx_state = 5;
      else
        UART_rx_state++;
      break;
    case 4:
      CRC_calc += curr_char; // Read the actual data
      UART_rx_buffer[curr_data_pos++] = curr_char;
      if (curr_data_pos == expected_len)
        UART_rx_state++;
      break;
    case 5:
      CRC_in = curr_char << 8; // Receive high byte of crude CRC
      UART_rx_state++;
      break;
    case 6:
      if ((CRC_calc & 0xffff) == (CRC_in | curr_char)) // Check if CRC is correct
      {
        /*
        Serial1.println("Uart_data_fully received");
        Serial1.printf("The CMD is: %02X , Len: %d\r\n", UART_CMD, expected_len);
        for (int i = 0; i < expected_len; i++)
        {
          Serial1.printf(" %02X", UART_rx_buffer[i]);
        }
        Serial1.printf("\r\n");*/
        digitalWrite(LED, !digitalRead(LED));
        handle_uart_cmd(UART_CMD, UART_rx_buffer, expected_len);
        UART_rx_state = 0;
      }
      break;

    default:
      break;
    }
  }
  if (UART_rx_state && (millis() - last_rx_time >= 10))
  {
    UART_rx_state = 0;
  }
}

typedef enum
{
  CMD_GET_VERSION = 1,
  CMD_RESET_ESP = 2,
  CMD_ZBS_BEGIN = 10,
  CMD_RESET_ZBS = 11,
  CMD_SELECT_PAGE = 12,
  CMD_SET_POWER = 13,
  CMD_READ_RAM = 20,
  CMD_WRITE_RAM = 21,
  CMD_READ_FLASH = 22,
  CMD_WRITE_FLASH = 23,
  CMD_READ_SFR = 24,
  CMD_WRITE_SFR = 25,
  CMD_ERASE_FLASH = 26,
  CMD_ERASE_INFOBLOCK = 27,
} ZBS_UART_PROTO;

uint8_t temp_buff[0x200] = {0};
void handle_uart_cmd(uint8_t cmd, uint8_t *cmd_buff, uint8_t len)
{
  switch (cmd)
  {
  case CMD_GET_VERSION:
    temp_buff[0] = FLASHER_VERSION >> 24;
    temp_buff[1] = FLASHER_VERSION >> 16;
    temp_buff[2] = FLASHER_VERSION >> 8;
    temp_buff[3] = FLASHER_VERSION;
    send_uart_answer(cmd, temp_buff, 4);
    break;
  case CMD_RESET_ESP:
    send_uart_answer(cmd, NULL, 0);
    delay(100);
    ESP.restart();
    break;
  case CMD_ZBS_BEGIN:
    temp_buff[0] = zbs.begin(ZBS_SS, ZBS_CLK, ZBS_MoSi, ZBS_MiSo, ZBS_Reset, ZBS_POWER);
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_RESET_ZBS:
    zbs.reset();
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_SELECT_PAGE:
    temp_buff[0] = zbs.select_flash(cmd_buff[0] ? 1 : 0);
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_SET_POWER:
    zbs.set_power(cmd_buff[0] ? 1 : 0);
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_READ_RAM:
    temp_buff[0] = zbs.read_ram(cmd_buff[0]);
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_WRITE_RAM:
    zbs.write_ram(cmd_buff[0], cmd_buff[1]);
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_READ_FLASH:
    // cmd_buff[0] = len
    // cmd_buff[1] << 8 | cmd_buff[2] = position
    for (int i = 0; i < cmd_buff[0]; i++)
    {
      temp_buff[i] = zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i);
    }
    send_uart_answer(cmd, temp_buff, cmd_buff[0]);
    break;
  case CMD_WRITE_FLASH:
    // cmd_buff[0] = len
    // cmd_buff[1] << 8 | cmd_buff[2] = position
    // cmd_buff[3+i] = data
    if (cmd_buff[0] >= (0xff - 3))
    { // Len too high, only 0xFF - header len possible
      temp_buff[0] = 0xEE;
      send_uart_answer(cmd, temp_buff, 1);
      break;
    }
    for (int i = 0; i < cmd_buff[0]; i++)
    {
      if (cmd_buff[3 + i] != 0xff)
      {
        zbs.write_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i, cmd_buff[3 + i]);
        if (zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i) != cmd_buff[3 + i])
        {
          zbs.write_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i, cmd_buff[3 + i]);
          if (zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i) != cmd_buff[3 + i])
          {
            zbs.write_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i, cmd_buff[3 + i]);
            if (zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i) != cmd_buff[3 + i])
            {
              zbs.write_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i, cmd_buff[3 + i]);
              if (zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i) != cmd_buff[3 + i])
              {
                temp_buff[0] = 0;
                send_uart_answer(cmd, temp_buff, 1);
                break;
              }
            }
          }
        }
      }
    }
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_READ_SFR:
    temp_buff[0] = zbs.read_sfr(cmd_buff[0]);
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_WRITE_SFR:
    zbs.write_sfr(cmd_buff[0], cmd_buff[1]);
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_ERASE_FLASH:
    zbs.erase_flash();
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  case CMD_ERASE_INFOBLOCK:
    zbs.erase_infoblock();
    temp_buff[0] = 1;
    send_uart_answer(cmd, temp_buff, 1);
    break;
  }
}

uint8_t answer_buffer[0x200] = {0};
void send_uart_answer(uint8_t answer_cmd, uint8_t *ans_buff, uint8_t len)
{
  uint32_t CRC_value = 0xAB34;
  answer_buffer[0] = 'A';
  answer_buffer[1] = 'T';
  answer_buffer[2] = answer_cmd;
  CRC_value += answer_cmd;
  answer_buffer[3] = len;
  CRC_value += len;
  for (int i = 0; i < len; i++)
  {
    answer_buffer[4 + i] = ans_buff[i];
    CRC_value += ans_buff[i];
  }
  answer_buffer[2 + 2 + len] = CRC_value >> 8;
  answer_buffer[2 + 2 + len + 1] = CRC_value;
  Serial.write(answer_buffer, 2 + 2 + len + 2);
  /*
    Serial1.println("Uart_answer now sending");
    Serial1.printf("The CMD is: %02X , Len: %d\r\n", answer_cmd, len);
    for (int i = 0; i < (2 + 2 + len + 1); i++)
    {
      Serial1.printf(" %02X", answer_buffer[i]);
    }
    Serial1.printf("\r\n");*/
}
