#!/usr/bin/python
###    Autor: Aaron Christophel ATCnetz.de    ###
# arg1 = COMPORT  arg2 = read(I) or write(I) arg3 = filename to read or write

import sys
from sys import exit
import time
import serial.tools.list_ports

CMD_GET_VERSION = 1
CMD_RESET_ESP = 2
CMD_ZBS_BEGIN = 10
CMD_RESET_ZBS = 11
CMD_SELECT_PAGE = 12
CMD_SET_POWER = 13
CMD_READ_RAM = 20
CMD_WRITE_RAM = 21
CMD_READ_FLASH = 22
CMD_WRITE_FLASH = 23
CMD_READ_SFR = 24
CMD_WRITE_SFR = 25
CMD_ERASE_FLASH = 26
CMD_ERASE_INFOBLOCK = 27

if(len(sys.argv) != 4):
    print("Example: COM1 read/readI file.bin, or COM1 write/writeI file.bin")
    print("Not the right arguments but here are the... please wait...")
    ports_list = "possible UART ports: "
    for port in serial.tools.list_ports.comports():
        ports_list += port.device + " "
    print(ports_list)
    exit()

usedCom = sys.argv[1]  # "COM5"
read_or_write = sys.argv[2]
file = sys.argv[3]
usedBaud = 115200
serialPort = serial.Serial(usedCom, usedBaud, serial.EIGHTBITS,
                           serial.PARITY_NONE, serial.STOPBITS_ONE, timeout=2)
print('Using port: {}'.format(usedCom))


def millis():
    return round(time.time() * 1000)


def to_byte(input, number=4):
    return input.to_bytes(number, byteorder='big')


def send_cmd(cmd, data):
    return_data = to_byte(cmd, 1) + to_byte(len(data), 1) + data
    crc_val = 0xAB34
    for x in return_data:
        crc_val += x
    return_data = b"AT" + return_data + to_byte(crc_val & 0xffff, 2)
    #print(' '.join(format(x, '02x') for x in return_data))
    serialPort.write(return_data)


def uart_flush():
    time.sleep(1)  # Flush serial data
    while(serialPort.inWaiting() > 0):
        data_str = serialPort.read(serialPort.inWaiting())
        #print(' '.join(format(x, '02x') for x in data_str))


def uart_receive_handler():
    start_time = millis()
    rx_state = 0
    expected_len = 0
    rx_crc = 0xAB34
    CRC_in = 0
    rx_position = 0
    rx_buffer = []
    while(1 == 1):
        while(serialPort.inWaiting() > 0):
            current_char = serialPort.read(1)
            current_char_int = current_char[0]
            if rx_state == 0:
                if current_char == b'A':
                    rx_state += 1
            elif rx_state == 1:
                if current_char == b'T':
                    rx_state += 1
                else:
                    rx_state = 0
            elif rx_state == 2:
                rx_buffer += current_char
                rx_crc += current_char_int
                rx_state += 1
            elif rx_state == 3:
                expected_len = current_char_int
                rx_crc += current_char_int
                if expected_len == 0:
                    rx_state = 5
                else:
                    rx_state += 1
            elif rx_state == 4:
                rx_buffer += current_char
                rx_crc += current_char_int
                rx_position += 1
                if rx_position >= expected_len:
                    rx_state += 1
            elif rx_state == 5:
                CRC_in = current_char_int
                rx_state += 1
            elif rx_state == 6:
                if rx_crc & 0xffff == CRC_in << 8 | current_char_int:
                    return [0]+rx_buffer
                else:
                    return [1]
        if millis() - start_time > 2000:
            return [2]


def zbs_read_version():
    retry = 3
    while(retry):
        send_cmd(CMD_GET_VERSION, bytearray([]))
        answer_array = uart_receive_handler()
        #print(' '.join(format(x, '02x') for x in answer_array))
        if answer_array[0] == 0:
            return [0] + answer_array[1:]
        retry -= 1
    return [1]

def zbs_init():
    retry = 3
    while(retry):
        send_cmd(CMD_ZBS_BEGIN, bytearray([]))
        answer_array = uart_receive_handler()
        #print(' '.join(format(x, '02x') for x in answer_array))
        if answer_array[0] == 0 and answer_array[2] == 1:
            return [0]
        retry -= 1
    return [1]


def zbs_reset():
    retry = 3
    while(retry):
        send_cmd(CMD_RESET_ZBS, bytearray([]))
        answer_array = uart_receive_handler()
        if answer_array[0] == 0:
            return [0]
        retry -= 1
    return [1]


def zbs_erase_flash():
    retry = 3
    while(retry):
        send_cmd(CMD_ERASE_FLASH, bytearray([]))
        answer_array = uart_receive_handler()
        if answer_array[0] == 0:
            return [0]
        retry -= 1
    return [1]


def zbs_erase_infopage():
    retry = 3
    while(retry):
        send_cmd(CMD_ERASE_INFOBLOCK, bytearray([]))
        answer_array = uart_receive_handler()
        if answer_array[0] == 0:
            return [0]
        retry -= 1
    return [1]


def zbs_select_flash_page(page):
    retry = 3
    while(retry):
        send_cmd(CMD_SELECT_PAGE, bytearray([page & 1]))
        answer_array = uart_receive_handler()
        if answer_array[0] == 0:
            return [0]
        retry -= 1
    return [1]


def zbs_read_flash(addr, len):
    retry = 3
    #print("Reading flash at " + str(addr) + " len " + str(len))
    while(retry):
        if len > 0xff:
            print("error len to long")
            return [2]
        if addr + len > 0x10000:
            print("error addr to high")
            return [3]
        send_cmd(CMD_READ_FLASH, bytearray(
            [len, (addr >> 8) & 0xff, addr & 0xff]))
        answer_array = uart_receive_handler()
        if answer_array[0] == 0:
            return [0] + answer_array[2:]
        retry -= 1
    return [1]


def zbs_write_flash(addr, len, data):
    retry = 3
    #print("Writing flash at " + str(addr) + " len " + str(len))
    #print("Len: " + str(len) + " : "+' '.join(format(x, '02x') for x in data))
    while(retry):
        if len > 250:
            print("error len to long")
            return [2]
        if addr + len > 0x10000:
            print("error addr to high Addr: " + str(addr) +
                  " : Len: " + str(len) + " Position: " + str(addr+len))
            return [3]
        send_cmd(CMD_WRITE_FLASH, bytearray(
            [len, (addr >> 8) & 0xff, addr & 0xff]) + data)
        answer_array = uart_receive_handler()
        if answer_array[0] == 0 and answer_array[2] == 1:
            return [0]
        if answer_array[0] == 0 and answer_array[2] == 0:
            return [1]
        retry -= 1
    return [1]


uart_flush()

zbs_version_answer = zbs_read_version()
if zbs_version_answer[0] == 0 and len(zbs_version_answer) == 6:
    print("ZBS Flasher version: " + str(zbs_version_answer[2]<<24|zbs_version_answer[3]<<16|zbs_version_answer[4]<<8|zbs_version_answer[5]))
else:
    print("Failed to read ZBS Flasher version")

if zbs_init()[0] != 0:
    print("Some Error in init")
    exit()

if(read_or_write == 'read'):
    if zbs_select_flash_page(0)[0] != 0:
        print("error selecting flash page")
        exit()
    print("Reading flash now")
    len_left = 0x10000
    position = 0
    curr_len = 0
    dump_buffer = []
    reading_start_time = millis()
    while(len_left > 0):
        if len_left > 255:
            curr_len = 255
        else:
            curr_len = len_left
        answer = zbs_read_flash(position, curr_len)
        if answer[0] == 0:
            dump_buffer += answer[1:]
            #print(' '.join(format(x, '02x') for x in answer))
        else:
            print("Error dumping flash")
            exit()
        position += curr_len
        len_left -= curr_len
        print(str(position) + " /   " + str(0x10000) + "  " +
              str(int((position/0x10000)*100)) + "% " + str(int((millis() - reading_start_time) / 1000)), end='\r', flush=True)
    print("")
    print("Reading flash done, now saving the file")
    file = open(file, "wb")
    file.write(bytearray(dump_buffer))
    file.close()
    print("Saving file done, it took " +
          str(int((millis()-reading_start_time)/1000)))

if(read_or_write == 'readI'):
    if zbs_select_flash_page(1)[0] != 0:
        print("error selecting infopage page")
        exit()
    print("Reading infopage now")
    len_left = 0x400
    position = 0
    curr_len = 0
    dump_buffer = []
    reading_start_time = millis()
    while(len_left > 0):
        if len_left > 255:
            curr_len = 255
        else:
            curr_len = len_left
        answer = zbs_read_flash(position, curr_len)
        if answer[0] == 0:
            dump_buffer += answer[1:]
            #print(' '.join(format(x, '02x') for x in answer))
        else:
            print("Error dumping infopage")
            exit()
        position += curr_len
        len_left -= curr_len
        print(str(position) + " /   " + str(0x400) + "  " +
              str(int((position/0x400)*100)) + "% " + str(int((millis() - reading_start_time) / 1000)), end='\r', flush=True)
    print("")
    print("Reading infopage done, now saving the file")
    file = open(file, "wb")
    file.write(bytearray(dump_buffer))
    file.close()
    print("Saving file done, it took " +
          str(int((millis()-reading_start_time)/1000)))


elif(read_or_write == 'write'):
    print("Erasing flash now")
    if zbs_select_flash_page(0)[0] != 0:
        print("error selecting flash page")
        exit()
    if zbs_erase_flash()[0] != 0:
        print("Some Error erasing")
        exit()
    print("Flashing file: "+file)
    in_file = open(file, "rb")
    data = bytearray(in_file.read())
    in_file.close()
    file_size = len(data)
    if file_size > 0x10000:
        print("File is too big for flash " + str(file_size))
        exit()
    len_left = file_size
    print("File size : " + str(len_left))
    curr_len = 0
    position = 0
    write_start_time = millis()
    while(len_left):
        if len_left > 250:
            curr_len = 250
        else:
            curr_len = len_left
        should_write = 0
        for i in range(curr_len):
            if data[position + i] != 0xff:
                should_write = 1
                break
        if should_write == 1:
            if zbs_write_flash(position, curr_len, data[position:position+curr_len])[0] != 0:
                print("error writing flash at " + str(position) + " /   " + str(file_size) + "  " +
                      str(int((position/file_size)*100)) + "% " + str(int((millis() - write_start_time) / 1000)))
                exit()
        position += curr_len
        len_left -= curr_len
        print(str(position) + " /   " + str(file_size) + "  " +
              str(int((position/file_size)*100)) + "% " + str(int((millis() - write_start_time) / 1000)), end='\r', flush=True)
    print("")
    print("Writing done, it took " + str(int((millis()-write_start_time)/1000)))
    print("Verfiy done and OK")


elif(read_or_write == 'writeI'):
    print("Erasing infopage now")
    if zbs_select_flash_page(1)[0] != 0:
        print("error selecting infopage page")
        exit()
    if zbs_erase_infopage()[0] != 0:
        print("Some Error erasing")
        exit()
    print("Flashing file: "+file)
    in_file = open(file, "rb")
    data = bytearray(in_file.read())
    in_file.close()
    file_size = len(data)
    if file_size > 0x400:
        print("File is too big for infopage " + str(file_size))
        exit()
    len_left = file_size
    print("File size : " + str(len_left))
    curr_len = 0
    position = 0
    write_start_time = millis()
    while(len_left):
        if len_left > 250:
            curr_len = 250
        else:
            curr_len = len_left
        should_write = 0
        for i in range(curr_len):
            if data[position + i] != 0xff:
                should_write = 1
                break
        if should_write == 1:
            if zbs_write_flash(position, curr_len, data[position:position+curr_len])[0] != 0:
                print("error writing infopage at " + str(position) + " /   " + str(file_size) + "  " +
                      str(int((position/file_size)*100)) + "% " + str(int((millis() - write_start_time) / 1000)))
                exit()
        position += curr_len
        len_left -= curr_len
        print(str(position) + " /   " + str(file_size) + "  " +
              str(int((position/file_size)*100)) + "% " + str(int((millis() - write_start_time) / 1000)), end='\r', flush=True)
    print("")
    print("Writing done, it took " + str(int((millis()-write_start_time)/1000)))
    print("Verfiy done and OK")

if zbs_reset()[0] == 0:
    print("ZBS Reset")
else:
    print("error while Reseting")

serialPort.close()
