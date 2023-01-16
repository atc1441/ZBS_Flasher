#include "nfc.h"
#include "asmUtil.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "cpu.h"
#include "printf.h"
#include "timer.h"
#include "board.h"

#define NFC_SCL P1_4
#define NFC_SDA P1_5
#define NFC_IRQ P1_3
#define NFC_POW P1_6

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

uint8_t nfc_handle1()
{
    P1FUNC &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
    P1DIR &= ~((1 << 4) | (1 << 5) | (1 << 6)); // NFC SDA SCL POWER
    P1DIR |= (1 << 3);                          // NFC IRQ
    P1_6 = 0;
    while (1)
    {
        pr("IRQ in: %d\n", P1_3);
    }
}
#define WHO_AM_I 0x04
uint8_t __xdata i2c_address = 0xAA;
uint8_t __xdata nfc_available = 0;

void i2c_pins_init()
{
    P1FUNC &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
    P1DIR &= ~((1 << 4) | (1 << 5) | (1 << 6)); // NFC SDA SCL POWER
    NFC_POW = 0;
    NFC_SCL = 1;
    NFC_SDA = 1;
    timerDelay(500000);
    NFC_POW = 1;
    timerDelay(100000);
}

void i2c_pins_sleep()
{
    timerDelay(50000);
    P1FUNC &= ~((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6));
    P1DIR &= ~((1 << 4) | (1 << 5) | (1 << 6)); // NFC SDA SCL POWER
    NFC_POW = 0;
    NFC_SCL = 0;
    NFC_SDA = 0;
}

void i2c_start()
{
    P1DIR &= ~((1 << 5));
    NFC_SCL = 1;
    NFC_SDA = 1;
    timerDelay(1);
    NFC_SDA = 0;
    timerDelay(1);
    NFC_SCL = 0;
    timerDelay(1);
}

void i2c_stop()
{
    P1DIR &= ~((1 << 5));
    NFC_SCL = 0;
    timerDelay(1);
    NFC_SDA = 0;
    timerDelay(1);
    NFC_SCL = 1;
    timerDelay(1);
    NFC_SDA = 1;
    timerDelay(1);
    NFC_SCL = 0;
    NFC_SCL = 1;
    NFC_SCL = 0;
    NFC_SCL = 1;
}

void i2c_send_byte(char a1)
{
    uint8_t __xdata v2 = 0;
    P1DIR &= ~((1 << 5));
    NFC_SDA = 0;
    do
    {
        NFC_SCL = 0;
        timerDelay(1);
        if ((a1 & 0x80) != 0)
            NFC_SDA = 1;
        else
            NFC_SDA = 0;
        NFC_SCL = 1;
        timerDelay(1);
        a1 *= 2;
        v2++;
    } while (v2 < 8);
    NFC_SCL = 0;
    NFC_SDA = 0;
    P1DIR |= (1 << 5);
    timerDelay(1);
    NFC_SCL = 1;
    timerDelay(1);
    P1DIR &= ~((1 << 5));
    NFC_SDA = 0;
}

uint8_t i2c_receive_byte(int a1)
{
    uint8_t __xdata v2 = 0;
    uint8_t __xdata v3 = 0;
    P1DIR |= (1 << 5);
    do
    {
        NFC_SCL = 0;
        timerDelay(1);
        NFC_SCL = 1;
        timerDelay(1);
        v2 *= 2;
        if (NFC_SDA)
            v2 |= 1u;
        v3++;
    } while (v3 < 8);
    NFC_SCL = 0;
    P1DIR &= ~((1 << 5));
    NFC_SDA = 0;
    timerDelay(1);
    if (a1)
        NFC_SDA = 0;
    else
        NFC_SDA = 1;
    NFC_SCL = 1;
    timerDelay(1);
    NFC_SCL = 0;
    NFC_SDA = 0;
    P1DIR |= (1 << 5);
    return v2;
}

void i2c_send_buffer(uint8_t buffer[], uint8_t addr, uint8_t reg)
{
    uint8_t __xdata v7 = 0;
    timerDelay(1);
    i2c_start();
    i2c_send_byte(addr);
    i2c_send_byte(reg);
    do
    {
        i2c_send_byte(buffer[v7]);
        v7++;
    } while (v7 < 16);
    i2c_stop();
}

void i2c_receive_buffer(uint8_t buffer[], uint8_t addr, uint8_t reg, uint8_t len, uint8_t a5)
{
    uint8_t __xdata v8 = 0;
    timerDelay(1);
    i2c_start();
    i2c_send_byte(addr);
    i2c_send_byte(reg);
    if (a5 == 1)
        i2c_send_byte(0);
    i2c_stop();
    i2c_start();
    i2c_send_byte(addr + 1);
    while (v8 < len)
    {
        buffer[v8++] = i2c_receive_byte(1);
    }
    i2c_stop();
}

void i2c_send_one_buffer(uint8_t new_address, uint8_t a1)
{
    uint8_t __xdata v1[16] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEE, 0x10, 0xE1, 0x00};
    if (a1 == 1)
    {
        v1[10] = 0xFF;
        v1[11] = 0xFF;
    }
    i2c_send_buffer(v1, new_address, 0);
    timerDelay(1560);
}

void i2c_receive_one_byte(uint8_t *test_byte, uint8_t addr, uint8_t reg, uint8_t a5)
{
    i2c_start();
    i2c_send_byte(addr);
    i2c_send_byte(reg);
    if (a5 == 1)
        i2c_send_byte(0);
    i2c_stop();
    i2c_start();
    i2c_send_byte(addr + 1);
    *test_byte = i2c_receive_byte(1);
    i2c_stop();
}

void nfc_send_16_byte_blocks(uint8_t buffer[], uint8_t len)
{
    uint8_t __xdata v2 = 0;
    uint8_t __xdata v5 = 1;
    uint8_t __xdata v6 = 0;

    i2c_pins_init();
    if (len << 28)
        v6 = (len >> 4) + 1;
    else
        v6 = len >> 4;
    while (v2 < v6)
    {
        i2c_send_buffer(&buffer[16 * v2], i2c_address, v5++);
        v2++;
    }
    i2c_pins_sleep();
}

void nfc_erase()
{
    uint8_t __xdata dowrite = 0;
    uint8_t __xdata count_erased = 0;
    uint8_t __xdata empty_buffer[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t __xdata read_temp_buffer[16];
    for (int i = 0x01; i < 0x37; i++)
    {
        i2c_receive_buffer(read_temp_buffer, i2c_address, i, 16, 0);
        for (int b = 0; b < 16; b++)
        {
            if (read_temp_buffer[b] != 0x00)
            {
                dowrite = 1;
                break;
            }
        }
        if (dowrite)
        {
            dowrite = 0;
            count_erased++;
            i2c_send_buffer(empty_buffer, i2c_address, i);
        }
    }
    pr("NFC Erase counter: %i\r\n", count_erased);
}

void nfc_set_default_config()
{
    // Set default Config
    uint8_t temp_config[8];
    uint8_t expected_config[16] = {0x19, 0x00, 0xF8, 0x48, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    i2c_receive_buffer(temp_config, i2c_address, 0x3A, 8, 0);
    if (memcmp(temp_config, expected_config, 8) != 0)
    {
        pr("Config rewriting\r\n");
        i2c_send_buffer(expected_config, i2c_address, 0x3A);
        timerDelay(15600);
    }
    // Set default Config END
}

void nfc_dump_register()
{
    uint8_t __xdata temp_dump[16];

    pr("Dumping all regs now:\r\n");
    for (int i = 0; i < 0xff; i++)
    {
        if (i != 0x39 && (i < 0x3B || i > 0xF7) && i != 0xFC && i != 0xFD && i != 0xFE)
        {
            i2c_receive_buffer(temp_dump, i2c_address, i, 16, 0);
            pr("REG 0x%02X :", i);
            for (int i = 0; i < 16; i++)
            {
                pr(" %02X", temp_dump[i]);
            }
            pr("\r\n");
        }
    }
}

void nfc_write_session(uint8_t index, uint8_t mask, uint8_t data)
{
    i2c_start();
    i2c_send_byte(i2c_address);
    i2c_send_byte(0xFE);
    i2c_send_byte(index);
    i2c_send_byte(mask);
    i2c_send_byte(data);
    i2c_stop();
}

uint8_t nfc_read_session(uint8_t index)
{
    uint8_t __xdata read_data = 0;
    i2c_start();
    i2c_send_byte(i2c_address);
    i2c_send_byte(0xFE);
    i2c_send_byte(index);
    i2c_stop();
    i2c_start();
    i2c_send_byte(i2c_address + 1);
    read_data = i2c_receive_byte(1);
    i2c_stop();
    return read_data;
}
/*
volatile uint8_t gpio7_triggered = 0;
void irq_gpio7(void)
{
    // pr("GPIO_IRQ\r\n");
    if ((nfc_read_session(0) & 0x40) != 1)
        nfc_write_session(0, 0b01000001, 0b01000001);
    gpio7_triggered = 1;
}
*/
/*
void __attribute__((interrupt)) ExtPin7_IRQHandler(void)
{
    NVIC_ClearPendingIRQ(ExtPin7_IRQn);
    GPIO_IntMask(NFC_IRQ, MASK);
    NVIC_some_IRQ1(ExtPin7_IRQn);
    PMU_ClearWakeupExtpin(PMU_GPIO7_INT);
    NVIC_ClearPendingIRQ(ExtPin7_IRQn);
}*/

void nfc_i2c_init()
{
    uint8_t __xdata test_byte = 0;
    i2c_pins_init();
    i2c_receive_one_byte(&test_byte, i2c_address, 0, 0);
    pr("I2C_CheckForNfcConnection = %d test_buffer = 0x%x \r\n", nfc_available, test_byte);
    if (test_byte != WHO_AM_I)
    {
        pr("NFC device was not found at: %02X restoring now.\r\n", i2c_address);
        for (int i = 0; i < 127; i++)
        {
            i2c_receive_one_byte(&test_byte, i << 1, 0, 0);
            if (test_byte == WHO_AM_I)
            {
                pr("Found NFC device at: %02X restoring default now.\r\n", i << 1);
                timerDelay(50);
                i2c_send_one_buffer(i << 1, 0);
                break;
            }
        }
    }
    i2c_receive_one_byte(&test_byte, i2c_address, 0, 0);
    nfc_available = test_byte == WHO_AM_I;
    pr("I2C_CheckForNfcConnection = %d test_buffer = 0x%x \r\n", i2c_address, test_byte);
    if (nfc_available)
    {
        nfc_erase();
        nfc_set_default_config();
    }

    /*if (0) // Test SRAM HERE
    {

        pr("Writing now to SRAM of the NFC Chip\r\n");

        // GPIO Interrupt
        GPIO_IntConfig_Type gpio7_intConfg;
        gpio7_intConfg.gpioIntFunc = ENABLE;
        gpio7_intConfg.intLevel = GPIO_INT_EDGE_SENS;
        gpio7_intConfg.intPol = GPIO_INT_ACTIVE_LOW;
        GPIO_PinModeConfig(NFC_IRQ, PINMODE_PULLUP);
        GPIO_PinMuxFun(NFC_IRQ, GPIO7_GPIO7);
        GPIO_SetPinDir(NFC_IRQ, GPIO_INPUT);
        GPIO_IntConfig(NFC_IRQ, &gpio7_intConfg);
        GPIO_IntMask(NFC_IRQ, UNMASK);
        GPIO_IntClr(NFC_IRQ);
        install_int_callback(INT_GPIO, NFC_IRQ, irq_gpio7);
        NVIC_ClearPendingIRQ(GPIO_IRQn);
        NVIC_EnableIRQ(GPIO_IRQn);
        // GPIO Interrupt END

        nfc_dump_register();

        pr("Enabling Pass-through mode now\r\n");
        for (int i = 0; i < 8; i++)
        {
            pr(" %02X", nfc_read_session(i));
        }
        pr("\r\n");

        pr("Writing now to SRAM of the NFC Chip\r\n");
        uint8_t s_buffer[64];
        for (int i = 0; i < sizeof(s_buffer); i++)
        {
            s_buffer[i] = i;
        }
        i2c_send_buffer(&s_buffer[0], i2c_address, 0xF8);
        i2c_send_buffer(&s_buffer[16], i2c_address, 0xF9);
        i2c_send_buffer(&s_buffer[32], i2c_address, 0xFA);
        i2c_send_buffer(&s_buffer[48], i2c_address, 0xFB);
        while (0)
        {
            if (gpio7_triggered)
            {
                gpio7_triggered = 0;

                pr("Received the following data via NFC:\r\n");
                for (int i = 0; i < sizeof(s_buffer); i++)
                {
                    s_buffer[i] = i;
                }
                i2c_send_buffer(&s_buffer[0], i2c_address, 0xF8);
                i2c_send_buffer(&s_buffer[16], i2c_address, 0xF9);
                i2c_send_buffer(&s_buffer[32], i2c_address, 0xFA);
                i2c_send_buffer(&s_buffer[48], i2c_address, 0xFB);
                i2c_receive_buffer(s_buffer, i2c_address, 0xF8, 16, 0);
                for (int i = 0; i < 16; i++)
                {
                    pr(" %02X", s_buffer[i]);
                }
                pr("\r\n");
            }
            WDT_RestartCounter();
        }
    }*/
    i2c_pins_sleep();
}

// extern void uiPrvDrawLatestImage(const struct EepromContentsInfo *eci);
// extern void prvWriteNewHeader(struct EepromImageHeader *eih, uint32_t addr, uint32_t eeSize, uint64_t ver, uint32_t size);
// extern void prvEepromIndex(struct EepromContentsInfo *eci);
//  This will get allready checked cmds buffer and can directly being used

/*
CMD 0x10:
Erase IMAGE AREA. EEPROM_IMG_START to EEPROM_IMG_EACH
expected_size = (buff_int[5] << 16) | (buff_int[6] << 8) | buff_int[7]

CMD 0x11:
Write to IMAGE AREA. EEPROM_IMG_START + address
address (buff[1] << 16) |(buff[2] << 8) | buff[3]
data buff[4] up to len

CMD 0x12:
Read from IMAGE AREA. EEPROM_IMG_START + address
address (buff[1] << 16) |(buff[2] << 8) | buff[3]
len buff[4], MAX 32-5

CMD 0x13:
Finish upload and display image


*/
void reply_nfc_cmd(uint8_t buff_out[], int len_out)
{
    uint8_t __xdata buffer_out[32];
    uint16_t __xdata crc_checked = 0x0000;
    for (uint32_t i = 0; i < sizeof(buffer_out) - 2; i++)
    {
        if (i < len_out)
            buffer_out[i] = buff_out[i];
        else
            buff_out[i] = 0x00;
        crc_checked = crc16(crc_checked, buffer_out[i]);
    }
    buffer_out[sizeof(buffer_out) - 2] = crc_checked >> 8;
    buffer_out[sizeof(buffer_out) - 1] = crc_checked & 0xff;

    /*pr("BUFFER_OUT_DUMP:\r\n");
    for (int i = 0; i < sizeof(buffer_out); i++)
    {
        pr(" %02X", buffer_out[i]);
    }
    pr("\r\n");*/

    nfc_write_session(0, 0b00000001, 0b00000000);
    i2c_send_buffer(&buffer_out[0], i2c_address, 0xF8);
    i2c_send_buffer(&buffer_out[16], i2c_address, 0xF9);
    nfc_write_session(0, 0b00000001, 0b00000001);
}

uint8_t __xdata should_finish_it = 0;
void handle_nfc_cmd(uint8_t buff_int[], uint8_t len_in)
{
    struct EepromImageHeader __xdata eih;
    uint8_t __xdata buff_out[30];
    uint8_t __xdata cmd = buff_int[0];
    uint32_t __xdata address = ((uint32_t)buff_int[1] << 16) | ((uint32_t)buff_int[2] << 8) | buff_int[3];
    uint8_t __xdata len = buff_int[4];
    uint8_t __xdata *data_buff = &buff_int[5];
    buff_out[0] = 0xFE; // NACK 0xFA = ACK
    if (len > (len_in - 5))
    {
        pr("Something wrong, len to big %i\r\n", len);
        reply_nfc_cmd(buff_out, 1);
        return;
    }
    pr("HANDLING NFC CMD NOW\r\n");
    switch (cmd)
    {
    case 0x10:
    {
        buff_out[0] = 0xFA;
        reply_nfc_cmd(buff_out, 1);
        // Here lets write a new header into the image area. lets set the current version plus one so RF can upload a new one
        uint32_t __xdata expected_size = ((uint32_t)data_buff[0] << 16) | ((uint32_t)data_buff[1] << 8) | data_buff[2];
        pr("Expected size: %i\r\n", expected_size);
        if (expected_size > EEPROM_IMG_EACH)
        {
            reply_nfc_cmd(buff_out, 1);
            return;
        }
        // FLASH_Read(0, EEPROM_IMG_START, (uint8_t *)&eih, sizeof(struct EepromImageHeader));
        // prvWriteNewHeader(&eih, EEPROM_IMG_START, EEPROM_IMG_EACH, eih.version + 1, expected_size);
        break;
    }
    /*case 0x11:
    {
        if ((address + len) < (EEPROM_IMG_EACH - sizeof(struct EepromImageHeader)))
        {
            buff_out[0] = 0xFA;
            reply_nfc_cmd(buff_out, 1);
            pr("Wrinting to flash at: %08X with len %i\r\n", EEPROM_IMG_START + address + sizeof(struct EepromImageHeader), len);
            // FLASH_Write(false, EEPROM_IMG_START + address + sizeof(struct EepromImageHeader), data_buff, len);
        }
        break;
    }
    case 0x12:
    {
        if (address + len < EEPROM_IMG_EACH)
        {
            if (len < (sizeof(buff_out) - 1))
            {
                buff_out[0] = 0xFA;
                reply_nfc_cmd(buff_out, 1);
                // FLASH_Read(false, EEPROM_IMG_START + address, &buff_out[1], len);
            }
        }
        break;
    }
    case 0x13:
    {
        buff_out[0] = 0xFA;
        reply_nfc_cmd(buff_out, 1);
        struct EepromImageHeader eih;
        // FLASH_Read(0, EEPROM_IMG_START, (uint8_t *)&eih, sizeof(struct EepromImageHeader));
        eih.validMarker = EEPROM_IMG_VALID;
        // FLASH_Write(false, EEPROM_IMG_START + offsetof(struct EepromImageHeader, validMarker), (uint8_t *)&eih.validMarker, sizeof(eih.validMarker));
        should_finish_it = 1;
        break;
    }*/
    default:
    {
        reply_nfc_cmd(buff_out, 1);
        break;
    }
    }
}

uint8_t nfc_handle()
{
    i2c_pins_init();
    P1FUNC &= ~((1 << 3));
    P1DIR |= (1 << 3); // NFC IRQ
    if (NFC_IRQ == 1)  // Do nothing no NFC connection notified
        return 0;
    pr("Doing NFC NOW\r\n");
    nfc_i2c_init();
    uint32_t __xdata last_comms = 0;
    uint8_t __xdata temp_pinstate = 0;
    while (1)
    {
        if (NFC_IRQ == 0)
        {
            if (temp_pinstate == 0)
            {
                temp_pinstate = 1;
                uint8_t __xdata sess_reg0 = nfc_read_session(0);
                if ((sess_reg0 & 0x40) != 1)
                    nfc_write_session(0, 0b01000001, 0b01000001);
            }
        }
        else
        {
            temp_pinstate = 0;
        }
        uint8_t __xdata ns_reg = nfc_read_session(6);
        if (ns_reg == 0x51)
        {
            last_comms = timerGet();
            // pr("NS_REG %i %i %i %i %i %i %i %i : ", (ns_reg >> 7) & 1, (ns_reg >> 6) & 1, (ns_reg >> 5) & 1, (ns_reg >> 4) & 1, (ns_reg >> 3) & 1, (ns_reg >> 2) & 1, (ns_reg >> 1) & 1, ns_reg & 1);
            uint8_t __xdata buffer_in[64];
            i2c_receive_buffer(&buffer_in[0], i2c_address, 0xF8, 16, 0);
            i2c_receive_buffer(&buffer_in[16], i2c_address, 0xF9, 16, 0);
            i2c_receive_buffer(&buffer_in[32], i2c_address, 0xFA, 16, 0);
            i2c_receive_buffer(&buffer_in[48], i2c_address, 0xFB, 16, 0);
            uint16_t __xdata crc_checked = 0x0000;
            uint16_t __xdata crc_in = (buffer_in[sizeof(buffer_in) - 2] << 8) | buffer_in[sizeof(buffer_in) - 1];
            for (uint8_t i = 0; i < sizeof(buffer_in) - 2; i++)
            {
                crc_checked = crc16(crc_checked, buffer_in[i]);
            }
            if (crc_checked == crc_in)
            {
                handle_nfc_cmd(buffer_in, 62);
            }
            /*for (int i = 0; i < 64; i++)
            {
                pr(" %02X", buffer_in[i]);
            }
            pr("\r\n");*/
        }
        if (timerGet() - last_comms > TIMER_TICKS_PER_SECOND * 6)
        {
            pr("NFC Timeout!\r\n");
            break;
        }
        if (should_finish_it && timerGet() - last_comms > TIMER_TICKS_PER_SECOND)
            break;
    }
    i2c_pins_sleep();
    if (should_finish_it)
    {
        should_finish_it = 0;
        return 1;
    }
    return 0;
}