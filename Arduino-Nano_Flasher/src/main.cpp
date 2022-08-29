
/*   Author: Aaron Christophel ATCnetz.de   */
/*   -mods for Arduino nano by Jelmer Bruijn       */

#include "main.h"

#include <Arduino.h>

#include "zbs_interface.h"

// hardware SPI is possible through these pins, but currently neither implemented nor needed
#define ZBS_SS 10
#define ZBS_CLK 13
#define ZBS_MoSi 11
#define ZBS_MiSo 12

#define ZBS_Reset 7

// This version of the flasher expects entire PORTC (Arduino pin 14-19)
// to be connected to the VCC rail for soft power control.
#define ZBS_POWER 16

// optional pins for debugging
#define ZBS_TXD 9
#define ZBS_TEST 6
#define ZBS_RXD 8

#define MAX_WRITE_ATTEMPTS 1

uint32_t FLASHER_VERSION = 0x00000010;

char *buff = nullptr;

void setup() {
    Serial.begin(115200);
    buff = (char *)calloc(100, 1);
    while (Serial.available())  // Flushing UART
        Serial.read();
    pinMode(ZBS_TXD, INPUT);
    pinMode(ZBS_RXD, INPUT);
    zbs.init(ZBS_SS, ZBS_CLK, ZBS_MoSi, ZBS_MiSo, ZBS_Reset, ZBS_POWER, ZBS_TEST);
}

uint8_t step = 0;

void loop() {
    UART_handler();
}

uint8_t UART_rx_state = 0;
uint8_t expected_len = 0;
long last_rx_time = 0;
int curr_data_pos = 0;
uint16_t CRC_calc = 0xAB34;
uint16_t CRC_in = 0;
uint8_t UART_CMD = 0;
uint8_t *UART_rx_buffer = nullptr;
bool debug_disabled = false;

void UART_handler() {
    while (Serial.available()) {
        last_rx_time = millis();
        uint8_t curr_char = Serial.read();
        switch (UART_rx_state) {
            case 0:
                if (debug_disabled) {
                    if (curr_char == 'A') {
                        UART_rx_state++;
                    }
                } else {
                    switch (curr_char) {
                        case 'A':
                            UART_rx_state++;
                            break;
                        case '?':
                            zbs.display_debug_menu();
                            break;
                        case '|':
                            do_passthrough();
                            break;
                        case 'B':
                        case 'b':
                            zbs.powerup();
                            break;
                        case 'T':
                        case 't':
                            zbs.toggle_testpin();
                            break;
                        case 'P':
                        case 'p':
                            zbs.toggle_power();
                            break;
                        case 'R':
                        case 'r':
                            zbs.toggle_reset();
                            break;
                        case 'C':
                        case 'c':
                            zbs.clear_screen(14000);
                            break;
                        case 'F':
                        case 'f':
                            zbs.clear_screen(7000);
                            break;
                        case 'S':
                        case 's':
                            zbs.softreset();
                            break;
                        case 'H':
                        case 'h':
                            zbs.hardreset();
                            break;
                        case 'V':
                        case 'v':
                            dump_ram();
                            break;
                        default:
                            break;
                    }
                }
                break;
            case 1:
                if (curr_char == 'T')  // Header
                    UART_rx_state++;
                else
                    UART_rx_state = 0;
                break;
            case 2:
                UART_CMD = curr_char;  // Receive current CMD
                CRC_calc = 0xAB34;
                CRC_calc += curr_char;
                UART_rx_state++;
                break;
            case 3:
                expected_len = curr_char;  // Receive Expected length of data
                UART_rx_buffer = (uint8_t *)calloc(expected_len + 4, 1);
                if (UART_rx_buffer == nullptr) {
                    while (true) {
                        digitalWrite(13, 0);
                        delay(500);
                        digitalWrite(13, 1);
                        delay(150);
                    }
                }
                CRC_calc += curr_char;
                curr_data_pos = 0;
                if (expected_len == 0)
                    UART_rx_state = 5;
                else
                    UART_rx_state++;
                break;
            case 4:
                CRC_calc += curr_char;  // Read the actual data
                UART_rx_buffer[curr_data_pos++] = curr_char;
                if (curr_data_pos == expected_len)
                    UART_rx_state++;
                break;
            case 5:
                CRC_in = curr_char << 8;  // Receive high byte of crude CRC
                UART_rx_state++;
                break;
            case 6:
                if ((CRC_calc & 0xffff) == (CRC_in | curr_char)) {
                    if (debug_disabled == false) {
                        debug_disabled = true;
                        free(buff);
                    }
                    /*
                    Serial1.println("Uart_data_fully received");
                    Serial1.printf("The CMD is: %02X , Len: %d\r\n", UART_CMD, expected_len);
                    for (int i = 0; i < expected_len; i++)
                    {
                      Serial1.printf(" %02X", UART_rx_buffer[i]);
                    }
                    Serial1.printf("\r\n");*/
                    handle_uart_cmd(UART_CMD, UART_rx_buffer, expected_len);
                    UART_rx_state = 0;
                }
                if (UART_rx_buffer != nullptr) {
                    free(UART_rx_buffer);
                    UART_rx_buffer = nullptr;
                }
                break;

            default:
                break;
        }
    }
    if (UART_rx_state && (millis() - last_rx_time >= 10)) {
        UART_rx_state = 0;
        if (UART_rx_buffer != nullptr) {
            free(UART_rx_buffer);
            UART_rx_buffer = nullptr;
        }
    }
}

typedef enum {
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

void dump_ram() {
    zbs.begin();
    delay(100);
    Serial.println("Reading from RAM...");
    sprintf_P(buff, PSTR(" Address: | 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  | ---------------- |\r\n"));
    Serial.print(buff);
    for (uint8_t high = 0; high < 0x10; high++) {
        sprintf_P(buff, PSTR(" 0x%0X-     | "), high);
        Serial.write(buff);
        char string[20] = {0};
        for (uint8_t low = 0; low < 0x10; low++) {
            uint8_t temp = zbs.read_ram((high << 4) | low);
            if((temp >= 0x20)&&(temp<0x7F)) {
                string[low] = (char)temp;
            } else {
                string[low] = '.';
            }
            sprintf_P(buff, PSTR("%02X "), temp);
            Serial.write(buff);
        }
        string[16] = 0x00;
        sprintf_P(buff, PSTR("| "));
        Serial.write(buff);
        Serial.write(string);
        sprintf_P(buff, PSTR(" |\r\n"));
        Serial.write(buff);
    }
    zbs.reset();
}

void do_passthrough() {
    sprintf_P(buff, PSTR("This feature is experimental! Data sent across this passthrough may appear mangled/corrupted!\r\n"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("Hard resetting tag and entering serial passthrough in\r\n"));
    Serial.print(buff);
    Serial.print("3");
    for (int8_t c = 2; c >= 0; c--) {
        delay(1000);
        Serial.print("\r");
        Serial.print(c);
    }
    sprintf_P(buff, PSTR("\n\r----------------------------\r\n"));
    Serial.print(buff);
    delay(15);
    Serial.end();

    pinMode(1, OUTPUT);  // TX out
    pinMode(0, INPUT);   // RX in
    pinMode(ZBS_TXD, INPUT);
    pinMode(ZBS_RXD, OUTPUT);
    zbs.set_power(0);
    delay(150);
    zbs.set_power(1);

    while (1) {  // only way outta here is a reset
        if (PINB & (1 << PINB1)) {
            PORTD |= (1 << PORTD1);
        } else {
            PORTD &= ~(1 << PORTD1);
        }
        if (PIND & (1 << PIND0)) {
            PORTB |= (1 << PORTB0);
        } else {
            PORTB &= ~(1 << PORTB0);
        }
    }
}

uint8_t temp_buff[0x08] = {0};
void handle_uart_cmd(uint8_t cmd, uint8_t *cmd_buff, uint8_t len) {
    uint8_t *temp = nullptr;
    switch (cmd) {
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
            // ESP.restart();
            break;
        case CMD_ZBS_BEGIN:
            temp_buff[0] = zbs.begin();
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
            temp = (uint8_t *)calloc(cmd_buff[0] + 6, 1);
            if (temp == nullptr) {
                while (true) {
                    digitalWrite(13, 0);
                    delay(150);
                    digitalWrite(13, 1);
                    delay(150);
                }
            }
            for (int i = 0; i < cmd_buff[0]; i++) {
                temp[i] = zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i);
            }
            send_uart_answer(cmd, temp, cmd_buff[0]);
            if (temp != nullptr) free(temp);
            break;
        case CMD_WRITE_FLASH:
            // cmd_buff[0] = len
            // cmd_buff[1] << 8 | cmd_buff[2] = position
            // cmd_buff[3+i] = data
            if (cmd_buff[0] >= (0xff - 3)) {  // Len too high, only 0xFF - header len possible
                temp_buff[0] = 0xEE;
                send_uart_answer(cmd, temp_buff, 1);
                break;
            }
            for (int i = 0; i < cmd_buff[0]; i++) {
                if (cmd_buff[3 + i] != 0xff) {
                    uint8_t attempts = 0;
                    for (attempts = 0; attempts < MAX_WRITE_ATTEMPTS; attempts++) {
                        zbs.write_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i, cmd_buff[3 + i]);
                        if (zbs.read_flash((cmd_buff[1] << 8 | cmd_buff[2]) + i) == cmd_buff[3 + i]) {
                            break;
                        }
                    }
                    if (attempts == MAX_WRITE_ATTEMPTS) {
                        temp_buff[0] = 0;
                        send_uart_answer(cmd, temp_buff, 1);
                        break;
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

void send_uart_answer(uint8_t answer_cmd, uint8_t *ans_buff, uint8_t len) {
    uint8_t *answer_buffer = nullptr;
    answer_buffer = (uint8_t *)calloc(len + 10, 1);
    if (answer_buffer == nullptr) {
        while (true) {
            digitalWrite(13, 0);
            delay(50);
            digitalWrite(13, 1);
            delay(50);
        }
    }
    uint32_t CRC_value = 0xAB34;
    answer_buffer[0] = 'A';
    answer_buffer[1] = 'T';
    answer_buffer[2] = answer_cmd;
    CRC_value += answer_cmd;
    answer_buffer[3] = len;
    CRC_value += len;
    for (int i = 0; i < len; i++) {
        answer_buffer[4 + i] = ans_buff[i];
        CRC_value += ans_buff[i];
    }
    answer_buffer[2 + 2 + len] = CRC_value >> 8;
    answer_buffer[2 + 2 + len + 1] = CRC_value;
    Serial.write(answer_buffer, 2 + 2 + len + 2);
    if (answer_buffer != nullptr) {
        free(answer_buffer);
        answer_buffer = nullptr;
    }
}