/*   Autor: Aaron Christophel ATCnetz.de   */
#include <Arduino.h>
#pragma once

void UART_handler();
void handle_uart_cmd(uint8_t cmd, uint8_t *cmd_buff, uint8_t len);
void send_uart_answer(uint8_t answer_cmd, uint8_t *ans_buff, uint8_t len);
void do_passthrough();
void display_debug_menu();
void toggle_test_pin();
void clear_screen_slow();
void clear_screen_fast();
void dump_ram();
//extern char* buff;