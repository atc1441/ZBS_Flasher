#!/usr/bin/env python3
"""ZBS Flasher

   Based on the original implementation by Aaron Christophel atc1441
"""
__author__ = "t.eddy"
__credits__ = ['Aaron Christophel (atc1441)', 't.eddy']

import os.path
import struct
import sys

import click
import serial.tools.list_ports
from serial import SerialException

NUM_RETRIES = 3


class CommunicationError(Exception):
    pass


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
CMD_SAVE_MAC_FROM_FW = 40
CMD_PASS_THROUGH = 50


class ZBSFlasher:
    flash_size = 0x10000
    info_size = 0x400

    def __init__(self, serial_port, slow_spi: bool):
        self._serial = serial_port
        self._slow_spi = slow_spi

    def _send_command(self, command, data=None):
        buffer = int(command).to_bytes(1, byteorder='big', signed=False)
        if data is not None:
            buffer += len(data).to_bytes(1, byteorder='big', signed=False)
            buffer += data
        else:
            buffer += b"\x00"
        crc_val = 0xAB34 + sum(buffer)
        buffer += int(crc_val & 0xffff).to_bytes(2, byteorder='big', signed=False)
        self._serial.write(b"AT" + buffer)

    def _read_response(self, expected_command):
        rx_crc = 0xAB34
        rx_buffer = []

        self._serial.read_until(b"AT")
        buf = self._serial.read(2)
        if len(buf) != 2:
            raise CommunicationError("timeout")
        rx_crc += sum(buf)
        command = buf[0]
        packet_len = int(buf[1])
        if packet_len:
            payload = self._serial.read(packet_len)
            rx_crc += sum(payload)
            rx_buffer.extend(payload)

        crc = self._serial.read(2)
        if rx_crc & 0xffff != crc[0] << 8 | crc[1]:
            raise CommunicationError("CRC Error")
        if expected_command != command:
            raise CommunicationError("command missmatch")
        return bytes(rx_buffer)

    def _query(self, command, data=None):
        for _ in range(NUM_RETRIES):
            self._send_command(command, data)
            return self._read_response(command)
        else:
            raise CommunicationError(f"Failed to communicate with the Flasher (command: {command} data: {data})")

    def flush(self):
        self._serial.flush()
        while self._serial.inWaiting():
            self._serial.read(self._serial.inWaiting())

    def init(self):
        slow_spi = 0
        if self._slow_spi:
            slow_spi = 1
        result = self._query(CMD_ZBS_BEGIN, bytearray([slow_spi]))
        if result[0] != 1:
            raise CommunicationError("Initialisation failed, is a device connected?")

    def version(self):
        return struct.unpack(">I", self._query(CMD_GET_VERSION))[0]

    def select_flash_page(self, page):
        self._query(CMD_SELECT_PAGE, bytearray([page & 1]))

    def read_flash(self, address, length):
        if address + length > self.flash_size:
            raise ValueError(f"Address out of bounds: {address} + {length} ")
        return self._query(
            CMD_READ_FLASH,
            bytearray([
                length,
                (address >> 8) & 0xff, address & 0xff
            ])
        )

    def write_flash(self, address, data):
        length = len(data)
        if length > 250:
            raise ValueError(f"Data is too big {length} > 250")
        if address + length > self.flash_size:
            raise ValueError(f"Address out of bounds: {address} + {length} ")
        response = self._query(
            CMD_WRITE_FLASH, bytearray([
                length,
                (address >> 8) & 0xff, address & 0xff
            ]) + data
        )
        if response[0] != 1:
            raise CommunicationError("Writing command returned an error")

    def erase_flash(self):
        return self._query(CMD_ERASE_FLASH)

    def erase_infopage(self):
        return self._query(CMD_ERASE_INFOBLOCK)

    def copy_mac_from_firmware(self):
        result = self._query(CMD_SAVE_MAC_FROM_FW)
        if result[0] != 1:
            raise CommunicationError("command failed")

    def reset(self):
        self._query(CMD_RESET_ZBS)

    def enter_passthrough_mode(self):
        self._query(CMD_PASS_THROUGH)
        self._serial.timeout = 0.5  # set the timeout to something low, so we don't lag too much.
        while True:
            try:
                data = self._serial.read(128)
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            except SerialException as e:
                return CommunicationError(str(e))


def guess_port():
    """Guesses the serial port, the programmer might be connected to.

    this is based on the vendor-id of the usb device
    """
    for port in serial.tools.list_ports.comports():
        if port.vid in (0x1a86,):
            return port.device


@click.group(help="Utility to flash ZBS243 / SEM9110 8051 microcontrollers", chain=True)
@click.option('-p', '--port', 'port', type=str, help="Serial Port")
@click.option('-b', '--baudrate', 'baudrate', default=115200, type=int, help="serial baudrate, default: 115200")
@click.option('--slow', is_flag=True, help="Use slower SPI speed, helpful if you use long cables")
@click.pass_context
def main(ctx, port, baudrate, slow):
    click.echo("Welcome to the ZBS-Flasher Utility")
    if port is None:
        click.echo("No Serial port given, trying to guess...")
        port = guess_port()
        if port is None:
            raise click.ClickException("No suitable port found, please specify one and try again")

    click.echo(f"Using port {port}")
    try:
        serial_port = serial.Serial(
            port,
            baudrate,
            serial.EIGHTBITS,
            serial.PARITY_NONE,
            serial.STOPBITS_ONE,
            timeout=2
        )
    except SerialException as e:
        raise click.ClickException(f"Could not open serial port: {str(e)}")
    ctx.obj = ZBSFlasher(serial_port, slow)
    ctx.obj.flush()

    @ctx.call_on_close
    def close_serial():
        serial_port.close()


@main.command(help="Show the Flasher version")
@click.pass_obj
def version(obj: ZBSFlasher):
    click.echo(f"Flasher Version: {obj.version()}")


@main.command(help='Sets or copies the MAC address')
@click.option(
    '-m', '--mac',
    'mac_address',
    type=str,
    help='MAC Address to set. '
         'Must be an 8-byte value. '
         'If empty, the MAC-address of the original firmware will be copied.'
)
@click.option('-d', '--backup-directory', 'backup_directory', type=str, default='mac_backups')
@click.pass_obj
@click.pass_context
def mac(ctx, obj: ZBSFlasher, mac_address: str, backup_directory):
    os.makedirs(backup_directory, exist_ok=True)
    try:
        obj.init()
    except CommunicationError as e:
        raise click.ClickException(str(e))

    if mac_address:
        try:
            mac_address = mac_address.replace('-', '')
            mac_address = bytes.fromhex(mac_address)
        except ValueError as e:
            raise click.ClickException(f"Invalid MAC address '{mac_address}': {str(e)}")

        if len(mac_address) != 8:
            raise click.ClickException("MAC address must be 8 bytes long.")

        click.echo(f"Writing MAC address '{mac_address.hex('-')}' to infopage")

        filename = os.path.join(backup_directory, mac_address.hex() + ".bin")

        with open(filename, 'wb') as fh:
            ctx.invoke(read_infopage, dest=fh)
            fh.seek(0x10)
            fh.write(mac_address[::-1])

        with open(filename, 'rb') as fh:
            ctx.invoke(write_infopage, src=fh)
    else:
        obj.copy_mac_from_firmware()


@main.command(help="Reads the flash memory")
@click.argument('dest', type=click.File('wb'))
@click.pass_obj
def read(obj: ZBSFlasher, dest):
    try:
        obj.init()
    except CommunicationError as e:
        raise click.ClickException(str(e))

    try:
        obj.select_flash_page(0)
    except CommunicationError as e:
        raise click.ClickException(f"Error selecting flash page {str(e)}")
    with click.progressbar(range(0, obj.flash_size, 0xff), label=f"Reading into {dest.name}") as bar:
        for position in bar:
            read_length = min(0xff, obj.flash_size - position)
            dump_buffer = obj.read_flash(position, read_length)
            if len(dump_buffer) != read_length:
                raise click.ClickException(
                    f"Reading flash failed at 0x{position:05x} "
                    f"tried to read {read_length} bytes, only {len(dump_buffer)} bytes read"
                )
            dest.write(dump_buffer)
    click.echo("All done!")


@main.command(help="Reads the infopage")
@click.argument('dest', type=click.File('wb'))
@click.pass_obj
def read_infopage(obj: ZBSFlasher, dest):
    try:
        obj.init()
    except CommunicationError as e:
        raise click.ClickException(str(e))

    try:
        obj.select_flash_page(1)
    except CommunicationError as e:
        raise click.ClickException(f"Error selecting flash page {str(e)}")

    with click.progressbar(range(0, 0x400, 0xff), label=f"Reading into {dest.name}") as bar:
        for position in bar:
            read_length = min(0xff, 0x400 - position)
            dump_buffer = obj.read_flash(position, read_length)
            if len(dump_buffer) != read_length:
                raise click.ClickException(
                    f"Reading infopage failed at 0x{position:03x} "
                    f"tried to read {read_length} bytes, only {len(dump_buffer)} bytes read"
                )
            dest.write(dump_buffer)
    click.echo("All done!")


@main.command(help="Writes the file to flash")
@click.argument('src', type=click.File('rb'))
@click.pass_obj
def write(obj: ZBSFlasher, src):
    try:
        obj.init()
    except CommunicationError as e:
        raise click.ClickException(str(e))

    try:
        obj.select_flash_page(0)
    except CommunicationError as e:
        raise click.ClickException(f"Error selecting flash page {str(e)}")

    click.echo("Erasing flash... ", nl=False)

    try:
        obj.erase_flash()
    except CommunicationError as e:
        raise click.ClickException(f"Error: {str(e)}")

    click.echo("Done.")
    data = src.read()
    image_size = len(data)
    if image_size > obj.flash_size:
        raise click.ClickException(f"Image is to big {image_size:05x} > {obj.flash_size:05x}")

    chunk_length = 250

    with click.progressbar(range(0, image_size, chunk_length), label=f"Writing {src.name} to Flash") as bar:
        for position in bar:
            write_length = min(chunk_length, image_size - position)
            try:
                obj.write_flash(
                    address=position,
                    data=data[position:position + write_length]
                )
            except CommunicationError as e:
                raise click.ClickException(
                    f"Writing to flash failed at 0x{position:05x} tried to write {write_length} bytes: {str(e)}"
                )

    click.echo("All done!")


@main.command(help="Writes the file to the infopage")
@click.argument('src', type=click.File('rb'))
@click.pass_obj
def write_infopage(obj: ZBSFlasher, src):
    try:
        obj.init()
    except CommunicationError as e:
        raise click.ClickException(str(e))

    try:
        obj.select_flash_page(1)
    except CommunicationError as e:
        raise click.ClickException(f"Error selecting flash page {str(e)}")

    click.echo("Erasing infopage... ", nl=False)

    try:
        obj.erase_infopage()
    except CommunicationError as e:
        raise click.ClickException(f"Error: {str(e)}")

    click.echo("Done.")

    data = src.read()
    image_size = len(data)
    if image_size > obj.info_size:
        raise click.ClickException(f"Image is to big {image_size:05x} > {obj.info_size:05x}")

    chunk_length = 250

    with click.progressbar(range(0, image_size, chunk_length), label=f"Writing {src.name} to infopage") as bar:
        for position in bar:
            write_length = min(chunk_length, image_size - position)
            try:
                obj.write_flash(
                    address=position,
                    data=data[position:position + write_length]
                )
            except CommunicationError as e:
                raise click.ClickException(
                    f"Writing to infopage failed at 0x{position:05x} tried to write {write_length} bytes: {str(e)}"
                )

    click.echo("All done!")


@main.command(help="Resets the target processor")
@click.pass_obj
def reset(obj: ZBSFlasher):
    click.echo("Resetting target... ", nl=False)
    try:
        obj.reset()
    except CommunicationError as e:
        raise click.ClickException(f"Error: {str(e)}")

    click.echo("Done.")


@main.command(help="Enters passthrough mode")
@click.pass_obj
def monitor(obj: ZBSFlasher):
    click.echo("Entering passthrough mode")
    try:
        obj.enter_passthrough_mode()
    except CommunicationError as e:
        raise click.ClickException(f"Error reading from device {str(e)}")


if __name__ == '__main__':
    main()
