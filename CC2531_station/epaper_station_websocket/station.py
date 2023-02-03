import timaccop 
import bmp2grays 
from Cryptodome.Cipher import AES
from collections import namedtuple
import struct
import os
import logging
from PIL import Image
import time
import gzip
import websockets
import websockets.server
import queue
import asyncio
import time
import websock

masterkey = bytearray.fromhex("D306D9348E29E5E358BF2934812002C1")

PORT = os.environ.get("EPS_PORT", default="COM13")
EXTENDED_ADDRESS = [ int(addr.strip(), 16) for addr in os.environ.get("EPS_EXTENDED_ADDRESS", default="0x00, 0x12, 0x4B, 0x00, 0x14, 0xD9, 0x49, 0x35").split(",") ]
PANID = [ int(panid.strip(), 16) for panid in os.environ.get("EPS_PANID", default="0x47, 0x44").split(",") ]
CHANNEL = int(os.environ.get("EPS_CHANNEL", default="11"))
IMAGE_DIR = os.environ.get("EPS_IMAGE_DIR", default="./input_img")
IMAGE_WORKDIR = os.environ.get("EPS_IMAGE_WORKDIR", default="tmp/")

CHECKIN_DELAY = int(os.environ.get("EPS_CHECKIN_DELAY", default="300000")) # 900s
RETRY_DELAY = int(os.environ.get("EPS_RETRY_DELAY", default="1000")) # 1s
FAILED_CHECKINS_TILL_BLANK = int(os.environ.get("EPS_FAILED_CHECKINS_TILL_BLANK", default="5"))
FAILED_CHECKINS_TILL_DISSOC = int(os.environ.get("EPS_FAILED_CHECKINS_TILL_DISSOC", default="5"))

PKT_ASSOC_REQ			= (0xF0)
PKT_ASSOC_RESP			= (0xF1)
PKT_CHECKIN				= (0xF2)
PKT_CHECKOUT			= (0xF3)
PKT_CHUNK_REQ			= (0xF4)
PKT_CHUNK_RESP			= (0xF5)

VERSION_SIGNIFICANT_MASK				= (0x0000ffffffffffff)

HW_TYPE_42_INCH_SAMSUNG					= (1)
HW_TYPE_42_INCH_SAMSUNG_ROM_VER_OFST	= (0xEFF8)
HW_TYPE_74_INCH_DISPDATA				= (2)
HW_TYPE_74_INCH_DISPDATA_FRAME_MODE		= (3)
HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST	= (0x008b)
HW_TYPE_ZBD_EPOP50						= (4)
HW_TYPE_ZBD_EPOP50_ROM_VER_OFST			= (0x008b)
HW_TYPE_ZBD_EPOP900						= (5)
HW_TYPE_ZBD_EPOP900_ROM_VER_OFST		= (0x008b)
HW_TYPE_29_INCH_DISPDATA				= (6)
HW_TYPE_29_INCH_DISPDATA_FRAME_MODE		= (7)
HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST	= (0x008b)
HW_TYPE_29_INCH_ZBS_026					= (8)
HW_TYPE_29_INCH_ZBS_026_FRAME_MODE		= (9)
HW_TYPE_29_INCH_ZBS_025					= (10)
HW_TYPE_29_INCH_ZBS_025_FRAME_MODE		= (11)
HW_TYPE_29_INCH_ZBS_033_BW				= (12)
HW_TYPE_29_INCH_ZBS_033_BW_FRAME_MODE	= (13)
HW_TYPE_154_INCH_ZBS_033				= (18)
HW_TYPE_154_INCH_ZBS_033_FRAME_MODE		= (19)
HW_TYPE_42_INCH_ZBS_026					= (28)
HW_TYPE_42_INCH_ZBS_026_FRAME_MODE		= (29)
HW_TYPE_29_INCH_ZBS_ROM_VER_OFST		= (0x008b)

HW_TYPE_74_INCH_BWR					= (40)
HW_TYPE_74_INCH_BWR_ROM_VER_OFST	= (0x0160)
HW_TYPE_58_INCH_BWR					= (41)
HW_TYPE_58_INCH_BWR_ROM_VER_OFST	= (0x0160)
HW_TYPE_42_INCH_BWR					= (42)
HW_TYPE_42_INCH_BWR_ROM_VER_OFST	= (0x0160)


TagInfo = namedtuple('TagInfo', """
protoVer,
swVer,
hwType,
batteryMv,
rfu1,
screenPixWidth,
screenPixHeight,
screenMmWidth,
screenMmHeight,
compressionsSupported,
maxWaitMsec,
screenType,
rfu
""")

AssocInfo = namedtuple('AssocInfo', """
checkinDelay,
retryDelay,
failedCheckinsTillBlank,
failedCheckinsTillDissoc,
newKey,
rfu
""")

CheckinInfo = namedtuple('CheckinInfo', """
swVer,
hwType,
batteryMv,
lastPacketLQI,
lastPacketRSSI,
temperature,
rfu,
""")

PendingInfo = namedtuple('PendingInfo', """
imgUpdateVer,
imgUpdateSize,
osUpdateVer,
osUpdateSize,
nextCheckinDelay,
rfu
""")

ChunkReqInfo = namedtuple('ChunkReqInfo', """
versionRequested,
offset,
len,
osUpdatePlz,
rfu,
""")

ChunkInfo = namedtuple('ChunkInfo', """
offset,
osUpdatePlz,
rfu,
""")

logging.basicConfig(format='%(asctime)s %(message)s')
logger = logging.getLogger(__name__)

dsn = 0

def print(*args):
    msg = ""
    for arg in args:
        msg += str(arg) + " "
    logger.warning(msg)

def decrypt(hdr, enc, tag, nonce):
    cipher = AES.new(masterkey, AES.MODE_CCM, nonce, mac_len=4)
    cipher.update(hdr)
    plaintext = cipher.decrypt(enc)
    #print("rcvd_packet:", plaintext.hex())
    #print("rcvhdr:", hdr.hex())
    try:
        cipher.verify(tag)
        return plaintext
    except:
        return None

def send_data(dst, data):
    global dsn
    dsn += 1
    if dsn > 255:
        dsn = 0
    hdr = bytearray.fromhex("41cc")
    hdr.append(dsn)
    hdr.extend(PANID)
    hdr.extend(reversed(dst))
    hdr.extend(EXTENDED_ADDRESS)
    #print("hdr:", hdr.hex())

    cntr = int(time.time())
    cntrb = struct.pack('<L', cntr)

    nonce = bytearray(cntrb)
    nonce.extend(EXTENDED_ADDRESS)
    nonce.append(0)
    #print("nonce:", nonce.hex())

    cipher = AES.new(masterkey, AES.MODE_CCM, nonce, mac_len=4)
    cipher.update(hdr)
    ciphertext, tag = cipher.encrypt_and_digest(data)

    out = ciphertext+tag+cntrb
    timaccop.mac_data_req(dst, PANID, 12, dsn, out)

def process_assoc(pkt, data):
    ti = TagInfo._make(struct.unpack('<BQHHBHHHHHHB11s',data))
    print(ti)

    ai = AssocInfo(
	    checkinDelay=CHECKIN_DELAY,
	    retryDelay=RETRY_DELAY,
	    failedCheckinsTillBlank=FAILED_CHECKINS_TILL_BLANK,
	    failedCheckinsTillDissoc=FAILED_CHECKINS_TILL_DISSOC,
	    newKey=masterkey,
	    rfu=bytearray(8*[0])
    )
    print(ai)
    ai_pkt = bytearray([ PKT_ASSOC_RESP ]) + bytearray(struct.pack('<LLHH16s8s', *ai))

    send_data(pkt['src_add'], ai_pkt)

def prepare_image(client, compressionSupported):
    is_bmp = False
    base_name = os.path.join(IMAGE_DIR, bytes(client).hex())
    filename = base_name + ".png"
    print("Reading image file:" + base_name + ".bmp/.png")        
    if os.path.isfile(filename):
        print("Using .png file")
    elif os.path.isfile(base_name + ".bmp"):
        is_bmp = True
        filename = base_name + ".bmp"
        print("Using .bmp file")
    else:
        print("No Image file available")
        return (0,0)

    modification_time = os.path.getmtime(filename)
    creation_time = os.path.getctime(filename)
    imgVer = int(modification_time)<<32|int(creation_time) # This uses the mofidication time of the image to look for the newest one

    file_conv = os.path.join(IMAGE_WORKDIR, bytes(client).hex().upper() + "_" + str(imgVer) + ".bmp") # also use the MAC in case 1 images are created within 1 second

    if not os.path.isfile(file_conv):
        if is_bmp:
            bmp2grays.convertImage(1, "1bppR", filename, file_conv)
        else:
            Image.open(filename).convert("RGB").save(os.path.join(IMAGE_WORKDIR, "tempConvert.bmp"))
            bmp2grays.convertImage(1, "1bppR", os.path.join(IMAGE_WORKDIR, "tempConvert.bmp"), file_conv)
            
        if compressionSupported == 1:
            file = open(file_conv,"rb")
            data = file.read()
            file.close()
            compressed_data = gzip.compress(data)
            file = open(file_conv,"wb")
            file.write(compressed_data)
            file.close()
            print("Size before compression: " + str(len(data)) + " compressed: " + str(len(compressed_data)))
        

    imgLen = os.path.getsize(file_conv)

    return (imgVer, imgLen)

def get_firmware_offset(hwType):
    if hwType == HW_TYPE_74_INCH_BWR:
        return HW_TYPE_74_INCH_BWR_ROM_VER_OFST
    if hwType == HW_TYPE_58_INCH_BWR:
        return HW_TYPE_58_INCH_BWR_ROM_VER_OFST
    if hwType == HW_TYPE_42_INCH_SAMSUNG:
        return HW_TYPE_42_INCH_SAMSUNG_ROM_VER_OFST
    if hwType == HW_TYPE_74_INCH_DISPDATA:
        return HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST   
    if hwType == HW_TYPE_74_INCH_DISPDATA_FRAME_MODE:
        return HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST
    if hwType == HW_TYPE_29_INCH_DISPDATA:
        return HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST
    if hwType == HW_TYPE_29_INCH_DISPDATA_FRAME_MODE:
        return HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST
    if hwType == HW_TYPE_ZBD_EPOP50:
        return HW_TYPE_ZBD_EPOP50_ROM_VER_OFST
    if hwType == HW_TYPE_ZBD_EPOP900:
        return HW_TYPE_ZBD_EPOP900_ROM_VER_OFST
    if hwType == HW_TYPE_29_INCH_ZBS_026 or hwType == HW_TYPE_29_INCH_ZBS_026_FRAME_MODE or hwType == HW_TYPE_29_INCH_ZBS_025 or hwType == HW_TYPE_29_INCH_ZBS_025_FRAME_MODE or hwType == HW_TYPE_154_INCH_ZBS_033 or hwType == HW_TYPE_154_INCH_ZBS_033_FRAME_MODE or hwType == HW_TYPE_42_INCH_ZBS_026 or hwType == HW_TYPE_42_INCH_ZBS_026_FRAME_MODE:
        return HW_TYPE_29_INCH_ZBS_ROM_VER_OFST

def prepare_firmware(hwType):
    filename = 'UPDT{0:0{1}X}.BIN'.format(hwType,4)
    
    print("Reading firmware file:", filename)

    if not os.path.isfile(filename):
        print("No Firmware file available")
        return (0,0)

    f = open(filename,mode='rb')
    f.seek(get_firmware_offset(hwType))
    firmwareVersionData = f.read(8)
    f.close()

    osVer = int.from_bytes(firmwareVersionData, "little") & VERSION_SIGNIFICANT_MASK | hwType << 48
    osLen = os.path.getsize(filename)

    return (osVer, osLen)

def get_image_data(imgVer, offset, length):
    filename = os.path.join(IMAGE_WORKDIR, imgVer + ".bmp")
    print("Reading image file:", filename)

    f = open(filename,mode='rb')
    f.seek(offset)
    image_data = f.read(length)
    f.close()

    return image_data

def get_fw_data(hwType, offset, length):
    filename = 'UPDT{0:0{1}X}.BIN'.format(hwType,4)
    print("Reading firmware file:", filename)

    f = open(filename,mode='rb')
    f.seek(offset)
    fw_data = f.read(length)
    f.close()

    return fw_data

def process_checkin(pkt, data):
    ci = CheckinInfo._make(struct.unpack('<QHHBBB6s',data))
    
    print(ci)

    imgVer = 0
    imgLen = 0

    try:
        imgVer, imgLen = prepare_image(pkt['src_add'],ci.rfu[0])
    except Exception as e :
        print("Unable to prepare image data for client", pkt['src_add'])
        print(e)

    osVer = 0
    osLen = 0

    try:
        osVer, osLen = prepare_firmware(ci.hwType)
    except Exception as e :
        print("Unable to prepare firmware data for client", pkt['src_add'])
        print(e)

    pi = PendingInfo(
        imgUpdateVer = imgVer,
        imgUpdateSize = imgLen,
        osUpdateVer = osVer,
        osUpdateSize = osLen,
        nextCheckinDelay = 0,
        rfu=bytearray(4*[0])
    )
    print(pi)

    pi_pkt = bytearray([ PKT_CHECKOUT ]) + bytearray(struct.pack('<QLQLL4s', *pi))

    send_data(pkt['src_add'], pi_pkt)

def process_download(pkt, data):
    cri = ChunkReqInfo._make(struct.unpack('<QLBB6s',data))
    print(cri)

    ci = ChunkInfo(
        offset = cri.offset,
        osUpdatePlz = cri.osUpdatePlz,
        rfu = 0,
    )
    print(ci)

    try:
        if ci.osUpdatePlz:
            fdata = get_fw_data(cri.versionRequested>>48, cri.offset, cri.len)
        else:
            fdata = get_image_data(bytes(pkt['src_add']).hex().upper() + "_" + str(cri.versionRequested), cri.offset, cri.len)
    except Exception as e :
        print("Unable to get data for version", cri.versionRequested)
        print(e)
        return

    outpkt = bytearray([ PKT_CHUNK_RESP ]) + bytearray(struct.pack('<LBB', *ci)) + bytearray(fdata)

    print("sending chunk", len(outpkt), outpkt[:10].hex() ,"...")

    send_data(pkt['src_add'], outpkt)

def generate_pkt_header(pkt): #hacky- timaccop cannot provide header data
    bcast = True
    if pkt['dst_add'] == b'\xff\xff': #broadcast assoc
        hdr = bytearray.fromhex("01c8")
    else:
        hdr = bytearray.fromhex("41cc")
        bcast = False
    hdr.append(pkt['dsn'])
    hdr.extend(pkt['dst_pan_id'])
    hdr.extend(pkt['dst_add'])
    if bcast:
        hdr.extend(pkt['src_pan_id'])
    hdr.extend(reversed(pkt['src_add']))

    return hdr


def process_pkt(pkt):
    hdr = generate_pkt_header(pkt)
    
    if len(pkt['data']) < 10:
        print("Received a too short paket")
        print("data", pkt['data'].hex())
        return

    nonce = bytearray(pkt['data'][-4:])
    nonce.extend(reversed(pkt['src_add']))
    nonce.extend(b'\x00')

    tag = pkt['data'][-8:-4]

    ciphertext = pkt['data'][:-8]

    plaintext = decrypt(hdr, ciphertext, tag, nonce)
    if not plaintext:
        print("data", pkt['data'].hex())
        print("hdr", hdr.hex())
        print("ciph", ciphertext.hex())
        print("nonce", nonce.hex())
        print("tag", tag.hex())
        print("packet is NOT authentic")
        return
        
    if len(pkt['src_add']) == 8:
        websock.broadcast( "".join("{:02x}".format(num) for num in pkt['src_add']).upper() + plaintext.hex().upper())
    
    typ = plaintext[0]

    if typ == PKT_ASSOC_REQ:
        print("Got assoc request")
        process_assoc(pkt, plaintext[1:])
    elif typ == PKT_CHECKIN:
        print("Got checkin request")
        process_checkin(pkt, plaintext[1:])
    elif typ == PKT_CHUNK_REQ:
        print("Got chunk request")
        process_download(pkt, plaintext[1:])
    else:
        print("Unknown request", typ)


websock.init()
print("WebSocket started")
timaccop.init(PORT, PANID, CHANNEL, EXTENDED_ADDRESS, process_pkt, websock.run)
print("Station started")
timaccop.run()

