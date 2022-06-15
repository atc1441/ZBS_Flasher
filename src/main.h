
/*   Autor: Aaron Christophel ATCnetz.de   */
#pragma once

void UART_handler();
void handle_uart_cmd(uint8_t cmd, uint8_t *cmd_buff, uint8_t len);
void send_uart_answer(uint8_t answer_cmd, uint8_t *ans_buff, uint8_t len);