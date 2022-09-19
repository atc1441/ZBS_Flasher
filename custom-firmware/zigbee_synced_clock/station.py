import timaccop
import time
import logging

logging.basicConfig(format='%(asctime)s %(message)s')
logger = logging.getLogger(__name__)

dsn = 0

def print(*args):
    msg = ""
    for arg in args:
        msg += str(arg) + " "
    logger.warning(msg)

PORT = "COM38"
EXTENDED_ADDRESS = [0xAB, 0xFF, 0xFF, 0xFF, 0x34, 0x56, 0x78, 0xFF]
PANID = [ 0x43, 0x34 ]
CHANNEL = 11

dsn = 0
def getDSN():
    global dsn
    dsn += 1
    if dsn > 255:
        dsn = 0
    return dsn

def process_pkt(pkt):
    #print(pkt)
    #print(bytes(pkt['data']).hex())
    
    if len(pkt['src_add']) < 8:
        #print("Mac too short")
        return

    src_mac = bytes(pkt['src_add']).hex().upper()

    if pkt['data'][0] == 0xC8 and pkt['data'][1] == 0xBC:
        time_now = int(time.time())+ 60 * 60 * 2;
        out_data = [0xC9, 0xCA,(time_now>>24)&0xff,(time_now>>16)&0xff,(time_now>>8)&0xff,time_now&0xff]
        timaccop.mac_data_req(pkt['src_add'], PANID, 12, getDSN(), out_data,1)
        print("Time request from " + src_mac + " send time: " + str(time_now))
        display_time = 0
        if len(pkt['data']) >= 5:
            display_time = pkt['data'][2]<<24|pkt['data'][3]<<16|pkt['data'][4]<<8|pkt['data'][5]
            print("Time was wrong by " + str(time_now - display_time) + " seconds")
        if len(pkt['data']) >= 6:
            print("Temperature: " + str(pkt['data'][6]) + "°C")
        return

timaccop.init(PORT, PANID, CHANNEL, EXTENDED_ADDRESS, process_pkt)
print("ZigBee Server started")

timaccop.run()
