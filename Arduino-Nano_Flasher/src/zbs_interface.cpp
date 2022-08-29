
/*   Autor: Aaron Christophel ATCnetz.de   */
#include "zbs_interface.h"

#include <Arduino.h>

void ZBS_interface::init(uint8_t SS, uint8_t CLK, uint8_t MOSI, uint8_t MISO, uint8_t RESET, uint8_t POWER, uint8_t TEST) {
    _SS_PIN = SS;
    _CLK_PIN = CLK;
    _MOSI_PIN = MOSI;
    _MISO_PIN = MISO;
    _RESET_PIN = RESET;
    _POWER_PIN = POWER;
    _TEST_PIN = TEST;
    pinMode(_MOSI_PIN, INPUT);
    pinMode(_MISO_PIN, INPUT);
    pinMode(_SS_PIN, INPUT);
    pinMode(_CLK_PIN, INPUT);
    set_power(ZBS_OFF);
    set_reset(PINSTATE_HIGHZ);
    set_testpin(PINSTATE_HIGHZ);
}

uint8_t ZBS_interface::begin() {
    pinMode(_SS_PIN, OUTPUT);
    pinMode(_CLK_PIN, OUTPUT);
    pinMode(_MOSI_PIN, OUTPUT);
    pinMode(_MISO_PIN, INPUT);
    pinMode(_RESET_PIN, OUTPUT);
    digitalWrite(_SS_PIN, HIGH);
    digitalWrite(_CLK_PIN, LOW);
    digitalWrite(_MOSI_PIN, HIGH);
    digitalWrite(_RESET_PIN, HIGH);
    set_power(ZBS_ON);

    enable_debug();
    return check_connection();
}

void ZBS_interface::set_power(uint8_t state) {
    if (_POWER_PIN != -1) {
        DDRC = 0xFF;
        if (state) {
            PORTC = 0xFF;
            power_on = true;
        } else {
            PORTC = 0;
            power_on = false;
        }
    }
}

void ZBS_interface::powerup() {
    set_reset(PINSTATE_HIGH);
    set_testpin(PINSTATE_HIGHZ);
    set_power(ZBS_ON);
    sprintf_P(buff, PSTR("Powering up!\r\n"));
    Serial.print(buff);
}

void ZBS_interface::set_reset(pinstate state) {
    switch (state) {
        case PINSTATE_HIGHZ:
            pinMode(_RESET_PIN, INPUT);
            digitalWrite(_RESET_PIN, 0);
            break;
        case PINSTATE_HIGH:
            pinMode(_RESET_PIN, OUTPUT);
            digitalWrite(_RESET_PIN, 1);
            break;
        case PINSTATE_LOW:
            pinMode(_RESET_PIN, OUTPUT);
            digitalWrite(_RESET_PIN, 0);
            break;
    }
    resetpinstate = state;
}

void ZBS_interface::set_testpin(pinstate state) {
    switch (state) {
        case PINSTATE_HIGHZ:
            pinMode(_TEST_PIN, INPUT);
            digitalWrite(_TEST_PIN, 0);
            break;
        case PINSTATE_HIGH:
            pinMode(_TEST_PIN, OUTPUT);
            digitalWrite(_TEST_PIN, 1);
            break;
        case PINSTATE_LOW:
            pinMode(_TEST_PIN, OUTPUT);
            digitalWrite(_TEST_PIN, 0);
            break;
    }
    testpinstate = state;
}

void ZBS_interface::toggle_power() {
    if (power_on) {
        set_power(ZBS_OFF);
    } else {
        set_power(ZBS_ON);
    }
    display_debug_menu();
}

void ZBS_interface::toggle_reset() {
    switch (resetpinstate) {
        case PINSTATE_HIGHZ:
            set_reset(PINSTATE_LOW);
            break;
        case PINSTATE_LOW:
            set_reset(PINSTATE_HIGH);
            break;
        case PINSTATE_HIGH:
            set_reset(PINSTATE_HIGHZ);
            break;
    }
    display_debug_menu();
}

void ZBS_interface::toggle_testpin() {
    switch (testpinstate) {
        case PINSTATE_HIGHZ:
            set_testpin(PINSTATE_LOW);
            break;
        case PINSTATE_LOW:
            set_testpin(PINSTATE_HIGH);
            break;
        case PINSTATE_HIGH:
            set_testpin(PINSTATE_HIGHZ);
            break;
    }
    display_debug_menu();
}

void ZBS_interface::enable_debug() {
    digitalWrite(_RESET_PIN, HIGH);
    digitalWrite(_SS_PIN, HIGH);
    digitalWrite(_CLK_PIN, LOW);
    digitalWrite(_MOSI_PIN, HIGH);
    delay(30);
    digitalWrite(_RESET_PIN, LOW);
    delay(33);
    digitalWrite(_CLK_PIN, HIGH);
    delay(1);
    digitalWrite(_CLK_PIN, LOW);
    delay(1);
    digitalWrite(_CLK_PIN, HIGH);
    delay(1);
    digitalWrite(_CLK_PIN, LOW);
    delay(1);
    digitalWrite(_CLK_PIN, HIGH);
    delay(1);
    digitalWrite(_CLK_PIN, LOW);
    delay(1);
    digitalWrite(_CLK_PIN, HIGH);
    delay(1);
    digitalWrite(_CLK_PIN, LOW);
    delay(9);
    digitalWrite(_RESET_PIN, HIGH);
    delay(100);
}

void ZBS_interface::softreset() {
    set_reset(PINSTATE_LOW);
    delay(100);
    set_reset(PINSTATE_HIGH);
    sprintf_P(buff, PSTR("Soft reset performed\r\n"));
    Serial.print(buff);
}

void ZBS_interface::hardreset() {
    set_reset(PINSTATE_LOW);
    set_power(ZBS_OFF);
    delay(400);
    set_reset(PINSTATE_HIGH);
    set_power(ZBS_ON);
    sprintf_P(buff, PSTR("Hard reset performed\r\n"));
    Serial.print(buff);
}

void ZBS_interface::reset() {
    pinMode(_SS_PIN, INPUT);
    pinMode(_CLK_PIN, INPUT);
    pinMode(_MOSI_PIN, INPUT);
    pinMode(_MISO_PIN, INPUT);
    digitalWrite(_RESET_PIN, LOW);
    set_power(ZBS_OFF);
    delay(50);
    digitalWrite(_RESET_PIN, HIGH);
    set_power(ZBS_ON);
    pinMode(_RESET_PIN, INPUT);
}
void ZBS_interface::clear_screen(uint16_t time) {
    powerup();
    delay(time);
    set_power(ZBS_OFF);
    sprintf_P(buff, PSTR("Clear screen complete\r\n"));
    Serial.print(buff);
}

void ZBS_interface::send_byte(uint8_t data) {
    digitalWrite(_SS_PIN, LOW);
    delayMicroseconds(1);  // was 5
    for (int i = 0; i < 8; i++) {
        if (data & 0x80)
            digitalWrite(_MOSI_PIN, HIGH);
        else
            digitalWrite(_MOSI_PIN, LOW);
        delayMicroseconds(ZBS_spi_delay);
        digitalWrite(_CLK_PIN, HIGH);
        delayMicroseconds(ZBS_spi_delay);
        digitalWrite(_CLK_PIN, LOW);
        data <<= 1;
    }
    digitalWrite(_SS_PIN, HIGH);
}

uint8_t ZBS_interface::read_byte() {
    uint8_t data = 0x00;
    digitalWrite(_SS_PIN, LOW);
    delayMicroseconds(1);  // 5
    for (int i = 0; i < 8; i++) {
        data <<= 1;
        if (digitalRead(_MISO_PIN))
            data |= 1;
        delayMicroseconds(ZBS_spi_delay);
        digitalWrite(_CLK_PIN, HIGH);
        delayMicroseconds(ZBS_spi_delay);
        digitalWrite(_CLK_PIN, LOW);
    }
    digitalWrite(_SS_PIN, HIGH);
    return data;
}

void ZBS_interface::write_byte(uint8_t cmd, uint8_t addr, uint8_t data) {
    send_byte(cmd);
    send_byte(addr);
    send_byte(data);
}

uint8_t ZBS_interface::read_byte(uint8_t cmd, uint8_t addr) {
    uint8_t data = 0x00;
    send_byte(cmd);
    send_byte(addr);
    data = read_byte();
    return data;
}

void ZBS_interface::write_flash(uint16_t addr, uint8_t data) {
    send_byte(ZBS_CMD_W_FLASH);
    send_byte(addr >> 8);
    send_byte(addr);
    send_byte(data);
}

uint8_t ZBS_interface::read_flash(uint16_t addr) {
    uint8_t data = 0x00;
    send_byte(ZBS_CMD_R_FLASH);
    send_byte(addr >> 8);
    send_byte(addr);
    data = read_byte();
    return data;
}

void ZBS_interface::write_ram(uint8_t addr, uint8_t data) {
    write_byte(ZBS_CMD_W_RAM, addr, data);
}

uint8_t ZBS_interface::read_ram(uint8_t addr) {
    return read_byte(ZBS_CMD_R_RAM, addr);
}

void ZBS_interface::write_sfr(uint8_t addr, uint8_t data) {
    write_byte(ZBS_CMD_W_SFR, addr, data);
}

uint8_t ZBS_interface::read_sfr(uint8_t addr) {
    return read_byte(ZBS_CMD_R_SFR, addr);
}

uint8_t ZBS_interface::check_connection() {
    uint8_t test_byte = 0xA5;
    write_ram(0xba, test_byte);
    delay(1);
    return read_ram(0xba) == test_byte;
}

uint8_t ZBS_interface::select_flash(uint8_t page) {
    uint8_t sfr_low_bank = page ? 0x80 : 0x00;
    write_sfr(0xd8, sfr_low_bank);
    delay(1);
    return read_sfr(0xd8) == sfr_low_bank;
}

void ZBS_interface::erase_flash() {
    send_byte(ZBS_CMD_ERASE_FLASH);
    send_byte(0x00);
    send_byte(0x00);
    send_byte(0x00);
    delay(100);
}

void ZBS_interface::erase_infoblock() {
    send_byte(ZBS_CMD_ERASE_INFOBLOCK);
    send_byte(0x00);
    send_byte(0x00);
    send_byte(0x00);
    delay(100);
}

void ZBS_interface::display_debug_menu() {
    Serial.write(27);  // Print "esc"
    Serial.print("[2J");
    sprintf_P(buff, PSTR("+---------- DEBUG OPTIONS -------------+\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| ? - Displays this list               |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| B - Power on and boot tag            |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| T - Toggle the test pin (P1.0)       |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| P - Toggle power                     |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| R - Toggle reset pin                 |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| S - Soft reset                       |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| H - Hard reset                       |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| | - Hard reset into passthrough mode |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| C - Clear the screen (on stock fw)   |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| F - Clear the screen (on fast hw)    |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| V - Dump RAM contents (experimental) |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("+--------------------------------------+\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("| POWER: "));
    Serial.print(buff);
    if (power_on) {
        sprintf_P(buff, PSTR(" ON"));
    } else {
        sprintf_P(buff, PSTR("OFF"));
    }
    Serial.print(buff);
    sprintf_P(buff, PSTR("   _RESET: "));
    Serial.print(buff);
    switch (resetpinstate) {
        case PINSTATE_HIGHZ:
            sprintf_P(buff, PSTR("   Z"));
            break;
        case PINSTATE_HIGH:
            sprintf_P(buff, PSTR("HIGH"));
            break;
        case PINSTATE_LOW:
            sprintf_P(buff, PSTR(" LOW"));
            break;
    }
    Serial.print(buff);
    sprintf_P(buff, PSTR(" TEST: "));
    Serial.print(buff);
    switch (testpinstate) {
        case PINSTATE_HIGHZ:
            sprintf_P(buff, PSTR("   Z"));
            break;
        case PINSTATE_HIGH:
            sprintf_P(buff, PSTR("HIGH"));
            break;
        case PINSTATE_LOW:
            sprintf_P(buff, PSTR(" LOW"));
            break;
    }
    Serial.print(buff);
    sprintf_P(buff, PSTR(" |\n\r"));
    Serial.print(buff);
    sprintf_P(buff, PSTR("+--------------------------------------+\n\r"));
    Serial.print(buff);
}

ZBS_interface zbs;