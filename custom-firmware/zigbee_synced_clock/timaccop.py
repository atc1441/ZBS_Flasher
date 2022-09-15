import serial

ZMAC_ACK_WAIT_DURATION	=	0x40
ZMAC_ASSOCIATION_PERMIT	=	0x41
ZMAC_AUTO_REQUEST	=	0x42
ZMAC_BATT_LIFE_EXT	=	0x43
ZMAC_BATT_LEFT_EXT_PERIODS	=	0x44
ZMAC_BEACON_MSDU	=	0x45
ZMAC_BEACON_MSDU_LENGTH	=	0x46
ZMAC_BEACON_ORDER	=	0x47
ZMAC_BEACON_TX_TIME	=	0x48
ZMAC_BSN	=	0x49
ZMAC_COORD_EXTENDED_ADDRESS	=	0x4A
ZMAC_COORD_SHORT_ADDRESS	=	0x4B
ZMAC_DSN	=	0x4C
ZMAC_GTS_PERMIT	=	0x4D
ZMAC_MAX_CSMA_BACKOFFS	=	0x4E
ZMAC_MIN_BE	=	0x4F
ZMAC_PANID	=	0x50
ZMAC_PROMISCUOUS_MODE	=	0x51
ZMAC_RX_ON_IDLE	=	0x52
ZMAC_SHORT_ADDRESS	=	0x53
ZMAC_SUPERFRAME_ORDER	=	0x54
ZMAC_TRANSACTION_PERSISTENCE_TIME	=	0x55
ZMAC_ASSOCIATED_PAN_COORD	=	0x56
ZMAC_MAX_BE	=	0x57
ZMAC_FRAME_TOTAL_WAIT_TIME	=	0x58
ZMAC__MAC_FRAME_RETRIES	=	0x59
ZMAC_RESPONSE_WAIT_TIME	=	0x5A
ZMAC_SYNC_SYMBOL_OFFSET	=	0x5B
ZMAC_TIMESTAMP_SUPPORTED	=	0x5C
ZMAC_SECURITY_ENABLED	=	0x5D
ZMAC_PHY_TRANSMIT_POWER	=	0xE0
ZMAC_LOGICAL_CHANNEL	=	0xE1
ZMAC_EXTENDED_ADDRESS	=	0xE2
ZMAC_ALT_BE	=	0xE3

MAC_INIT =      [ 0x00, 0x22, 0x02 ]
MAC_RESET_REQ = [ 0x01, 0x22, 0x01, 0x01]
MAC_SET_REQ =   [ 0x11, 0x22, 0x09 ] # + Attribute, AttributeValue 
MAC_GET_REQ =   [ 0x01, 0x22, 0x08 ] # + Attribute
MAC_START_REQ = [ 0x23, 0x22, 0x03 ]
UTIL_CALLBACK_SUB_CMD = [ 0x03, 0x27, 0x06, 0xff, 0xff, 1 ]

pkt_callback = None
ser = None

def fcs(data):
    cs = 0
    for ch in data:
        cs ^= ch
        #print("cs:", cs)
    return cs

def frame(data):
    cs = fcs(data)
    out = [0xfe]
    out += data
    #cs = fcs(out)
    out += [cs]
    return bytes(out)

def await_res():
    b = ser.read(1)
    if not len(b):
        return None
    pre = b[0]
    if pre != 0xfe:
        print("incorrect preamble:", pre)
        return
    head = ser.read(3)
    dlen = head[0]
    data = ser.read(dlen) #2bytes cmd + data
    frm = head + data
    ccs = fcs(frm)
    cs = ser.read(1)[0]
    #print("R:", (bytes([pre])+frm+bytes([cs])).hex())
    if cs != ccs:
        print("incorrect fcs, expected", ccs, "got", cs)
    return frm

def send_sreq(data):
    fr = frame(data)
    #print("W:", fr.hex())
    ser.write(fr)
    return await_res()

def send_areq(data):
    fr = frame(data)
    #print("W:", fr)
    ser.write(fr)

def mac_set_req(attribute, value):
    if type(value) is not list:
        value = [value]
    return send_sreq(MAC_SET_REQ + [attribute] + value + [0]*(0x10 - len(value)))

def mac_get_req(attribute):
    res = send_sreq(MAC_GET_REQ + [attribute])
    #print(res[4:].hex())
    return res

def mac_start_req(panid, channel):
    start_time = [0]*4
    pan_id = panid
    logical_channel = channel
    channel_page = 0
    beacon_order = 15
    super_frame_order = 0
    pan_coordinator = 1
    battery_life_ext = 0
    coord_realignment = 0
    realign_key_source = [0]*8
    realign_security_level = 0
    realign_key_id_mode = 0
    realing_key_index = 0
    beacon_key_source = [0]*8
    beacon_security_level = 0
    beacon_key_id_mode = 0
    beacon_key_index = 0
    
    start_params = start_time + pan_id + [
        logical_channel,
        channel_page,
        beacon_order,
        super_frame_order,
        pan_coordinator,
        battery_life_ext,
        coord_realignment,
        ] + realign_key_source + [
        realign_security_level,
        realign_key_id_mode,
        realing_key_index,
        ] + beacon_key_source + [
        beacon_security_level,
        beacon_key_id_mode,
        beacon_key_index,
    ]
    
    return send_sreq(MAC_START_REQ + start_params)

MAC_TXOPTION_ACK = 0x01
MAC_TXOPTION_GTS = 0x02
MAC_TXOPTION_INDIRECT = 0x04
MAC_TXOPTION_NO_RETRANS = 0x10
MAC_TXOPTION_NO_CNF = 0x20
MAC_TXOPTION_ALT_BE = 0x40
MAC_TXOPTION_PWR_CHAN = 0x80

def mac_data_req(dst_add, dst_panid, handle, dsn, data, options = 1):
    if len(dst_add) == 2:
        dst_add_mode = 2
    else:
        dst_add_mode = 3
    dst_add = list(dst_add) + [0]*(8-len(dst_add))
    dst_panid = list(dst_panid)
    data = list(data)
    src_add_mode = 3
    tx_option = 0 + options# + MAC_TXOPTION_PWR_CHAN
    logical_channel = 0
    power = 127
    key_source = [0]*8
    security_level = 0
    key_id_mode = 0
    key_index = 0
    msdu_length = len(data)
    msdu = data

    params = [ dst_add_mode ] + dst_add + dst_panid + [ src_add_mode, handle, tx_option, 
            logical_channel, power ] + key_source + [ security_level, key_id_mode, key_index, msdu_length ]

    mac_set_req(ZMAC_DSN, dsn)
    send_sreq(bytes([ len(params) + len(data), 0x22, 0x05 ] + params + msdu))


def parse_mac_data_ind(data):
    pkt = { "type": "parse_mac_data_ind" }
    pkt["src_add_mode"] = data[3]
    src_add = list(data[4:12])
    src_add.reverse()
    if pkt["src_add_mode"] == 2:
        pkt["src_add"] = src_add[:2]
    else:
        pkt["src_add"] = src_add[:8]
    pkt["dst_add_mode"] = data[12]
    if pkt["dst_add_mode"] == 2:
        pkt["dst_add"] = data[13:15]
    else:
        pkt["dst_add"] = data[13:21]
    pkt["timestamp"] = data[21:25]
    pkt["timestamp2"] = data[25:27]
    pkt["src_pan_id"] = data[27:29]
    pkt["dst_pan_id"] = data[29:31]
    pkt["link_quality"] = data[31]
    pkt["correlation"] = data[32]
    pkt["rssi"] = data[33]
    pkt["dsn"] = data[34]
    pkt["key_source"] = data[35:43]
    pkt["security_level"] = data[43]
    pkt["key_id_mode"] = data[44]
    pkt["key_index"] = data[45]
    pkt["length"] = data[46]
    pkt["data"] = data[47:]
    return pkt

def parse_mac_data_cnf(data):
    pkt = { "type": "mac_data_cnf" }
    pkt["status"] = data[3]
    pkt["handle"] = data[4]
    pkt["timestamp"] = data[5:9]
    pkt["timestamp2"] = data[9:11]
    return pkt

def parse_areq(data):
    dlen = data[0]
    cmd0 = data[1]
    cmd1 = data[2]
    pkt = None
    if cmd0 == 0x42 and cmd1 == 0x85:
        pkt = parse_mac_data_ind(data)
        if pkt_callback:
            pkt_callback(pkt)
    if cmd0 == 0x42 and cmd1 == 0x84:
        pkt = parse_mac_data_cnf(data)
    if pkt:
        #print("Got cmd\n", pkt)
        if pkt["type"] == "mac_data_cnf":
            #sys.exit(0)
            pass
    else:
        #print("Unknown cmd\n", data)
        pass

def init(port, pan_id, channel, ext_add, pk):
    global pkt_callback
    pkt_callback = pk

    global ser
    ser = serial.Serial(port, timeout=1)
    ser.flushInput()

    ext_add.reverse()
    short_add = ext_add[-2:]

    send_sreq(MAC_INIT)
    send_sreq(MAC_RESET_REQ)
    
    # https://e2e.ti.com/support/wireless-connectivity/zigbee-thread-group/zigbee-and-thread/f/zigbee-thread-forum/426380/ti-mac-cop-cc2531-asynchronous-responses
    send_sreq(UTIL_CALLBACK_SUB_CMD)
    
    mac_set_req(ZMAC_EXTENDED_ADDRESS, ext_add)
    mac_set_req(ZMAC_SHORT_ADDRESS, short_add)
    mac_get_req(ZMAC_EXTENDED_ADDRESS)
    mac_get_req(ZMAC_SHORT_ADDRESS)
    
    mac_set_req(ZMAC_RX_ON_IDLE, 1)
    mac_get_req(ZMAC_RX_ON_IDLE)
    mac_set_req(ZMAC_PROMISCUOUS_MODE, 1)
    mac_get_req(ZMAC_PROMISCUOUS_MODE)
    mac_get_req(ZMAC_LOGICAL_CHANNEL)
    mac_get_req(ZMAC_SECURITY_ENABLED)
    
    mac_start_req(pan_id, channel)
    
def run():    
    while True:
        data = await_res()
        if data:
            parse_areq(data)
    
