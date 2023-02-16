#include <stdlib.h>
#include <unistd.h>
#include "RFQueue.h"
#include <stdio.h>
#include "ti_drivers_config.h"
#include <ti/drivers/GPIO.h>

#include "millis.h"
#include "uart.h"
#include "radio.h"

// UART VARIABLES from uart.c
extern char input[];
extern size_t bytesReadCount;
// END UART VARIABLES from uart.c

// RADIO VARIABLES from radio.c
#define  max_rf_rx_buffer  10
extern uint8_t got_rx;
extern uint8_t worked_got_rx;
extern uint8_t packetDataPointer[max_rf_rx_buffer+1][1000];
// END RADIO VARIABLES from radio.c


// Variables, definitions and function to handle data coming in via RF
#pragma pack(1)
typedef struct
{
    uint8_t start_byte;
    uint8_t len_all;
    uint8_t hdr1;
    uint8_t hdr2;
    uint8_t src_add_mode;
    uint8_t src_add[8];
    uint8_t dst_add_mode;
    uint8_t dst_add[8];
    uint32_t timestamp;
    uint16_t timestamp2;
    uint8_t src_pan_id[2];
    uint8_t dst_pan_id[2];
    uint8_t link_quality;
    uint8_t correlation;
    uint8_t rssi;
    uint8_t dsn;
    uint8_t key_source[8];
    uint8_t security_level;
    uint8_t key_id_mode;
    uint8_t key_index;
    uint8_t length;
}mac_data_ind;
#pragma pack()

void uartMacWrite(uint8_t *in_buffer, int len_in)
{
    uint8_t the_out_data[512];

    mac_data_ind ind;
    memset(&ind, 0x00,sizeof(mac_data_ind));
    ind.start_byte = 0xFE;

    uint16_t frame_ctrl = (in_buffer[2]<<8)|in_buffer[1];
    uint8_t frame_type = frame_ctrl & 0x0007;

    if(frame_type == 2){// ACK message

    }else if(frame_type == 1){
        ind.hdr1 = 0x42;
        ind.hdr2 = 0x85;
        ind.dsn = in_buffer[3];

        ind.dst_pan_id[0] = in_buffer[4];
        ind.dst_pan_id[1] = in_buffer[5];
        int posi_data = 0;

        if((frame_ctrl&0x0C00) == 0x0C00){//Long
            ind.dst_add_mode = 3;
            ind.dst_add[0] = in_buffer[6];
            ind.dst_add[1] = in_buffer[7];
            ind.dst_add[2] = in_buffer[8];
            ind.dst_add[3] = in_buffer[9];
            ind.dst_add[4] = in_buffer[10];
            ind.dst_add[5] = in_buffer[11];
            ind.dst_add[6] = in_buffer[12];
            ind.dst_add[7] = in_buffer[13];
            posi_data += 4;
        }else{// short
            ind.dst_add_mode = 2;
            ind.dst_add[0] = in_buffer[6];
            ind.dst_add[1] = in_buffer[7];
            ind.src_pan_id[0] = in_buffer[posi_data + 8];
            ind.src_pan_id[1] = in_buffer[posi_data + 9];
        }

        if((frame_ctrl&0xC000) == 0xC000){// Long
            ind.src_add_mode = 3;
            ind.src_add[0] = in_buffer[posi_data + 10];
            ind.src_add[1] = in_buffer[posi_data + 11];
            ind.src_add[2] = in_buffer[posi_data + 12];
            ind.src_add[3] = in_buffer[posi_data + 13];
            ind.src_add[4] = in_buffer[posi_data + 14];
            ind.src_add[5] = in_buffer[posi_data + 15];
            ind.src_add[6] = in_buffer[posi_data + 16];
            ind.src_add[7] = in_buffer[posi_data + 17];
            posi_data += 6;
        }else{// short
            ind.src_add_mode = 2;
            ind.src_add[0] = in_buffer[posi_data + 10];
            ind.src_add[1] = in_buffer[posi_data + 11];
        }

        ind.length = in_buffer[0] - posi_data - 13;
        ind.len_all = sizeof(mac_data_ind) + ind.length - 4;
        memcpy(&the_out_data, (uint8_t * )&ind, sizeof(mac_data_ind));
        memcpy(&the_out_data[sizeof(mac_data_ind)], &in_buffer[posi_data + 12], ind.length);
        uint8_t crc = 0x00;
        for(int i = 1;i< ind.len_all+4;i++)
        {
            crc ^= the_out_data[i];
        }
        the_out_data[ind.len_all + 4] = crc;

        //int j = sprintf(the_msg, "RX crc: %lu  %02X : ",getMillis(), the_out_data[ind.len_all + 4]);
        //for (int i = 0; i < (ind.len_all + 5); i++)
        //{
        //    j += sprintf(the_msg + j, "%02X", the_out_data[i]);
        //}
        //puts(the_msg);
        uartWrite(the_out_data, ind.len_all + 5);
    }

}
// END Variables, definitions and function to handle data coming in via RF



// Variables, definitions and function to handle data coming in via UART
#pragma pack(1)
typedef struct
{
    uint8_t dst_add_mode; // 2 short, 3 long
    uint8_t dst_add[8]; // destination address
    uint8_t dst_panid[2]; // destination PAN id
    uint8_t src_add_mode; // 2 short, 3 long
    uint8_t handle; // no idea, is 12 ^^
    uint8_t tx_option; // stuff like, ACK requesting, not supported here
    uint8_t logical_channel; // always 0
    uint8_t power; // always 127
    uint8_t kew_source[8]; // All 0
    uint8_t security_level; // always 0
    uint8_t key_id_mode; // always 0
    uint8_t key_index; // always 0
    uint8_t msdu_length; // Length of data
}mac_tx_s;
#pragma pack()

uint8_t MAC_extended_add[8];
uint8_t MAC_short_add[2];
uint8_t PANID[2];
uint8_t MAC_ch = 11;;
uint8_t nextDsn = 0;
// At this point we got a checked cmd where the first byte is the len and the rest the data
void handleCmd(uint8_t *cmdb,int len)
{
    GPIO_write(7,1);
    uint16_t theMacCmd = cmdb[0]<<8 | cmdb[1];
    switch(theMacCmd){
    case 0x2203:// MAC_START_REQ panid and channels
    {
        PANID[0] = cmdb[6];
        PANID[1] = cmdb[7];
        MAC_ch = cmdb[8];
        initRadio(MAC_ch);
    }
    break;
    case 0x2209:// This sets the next DSN value.
    {
        switch(cmdb[2])
        {
        case 0x4C:// ZMAC_DSN
            nextDsn = cmdb[3];
            break;
        case 0xE2:// ZMAC_EXTENDED_ADDRESS
            MAC_extended_add[0] = cmdb[3];
            MAC_extended_add[1] = cmdb[4];
            MAC_extended_add[2] = cmdb[5];
            MAC_extended_add[3] = cmdb[6];
            MAC_extended_add[4] = cmdb[7];
            MAC_extended_add[5] = cmdb[8];
            MAC_extended_add[6] = cmdb[9];
            MAC_extended_add[7] = cmdb[10];
            break;
        case 0x53:// ZMAC_SHORT_ADDRESS
            MAC_short_add[0] = cmdb[3];
            MAC_short_add[1] = cmdb[4];
            break;
        }
    }
    break;
    case 0x2205:// This contains a Radio packet
    {
            mac_tx_s mac_tx;
            memcpy((uint8_t *)&mac_tx,&cmdb[2],sizeof(mac_tx_s));

            // Here we need to pack all the data together into a full Zigbee MSG

            uint8_t outRf[256];
            outRf[0] = mac_tx.tx_option | 0x41;// extra config, like request ack, no idea how this really works!
            outRf[1] = (mac_tx.src_add_mode?0xC0:0x80)|(mac_tx.dst_add_mode?0x0C:0x08);
            outRf[2] = nextDsn;

            outRf[3] = mac_tx.dst_panid[0];
            outRf[4] = mac_tx.dst_panid[1];
            int posi_data = 0;

            if(mac_tx.dst_add_mode == 3)// Long
            {
                outRf[5] = mac_tx.dst_add[7];
                outRf[6] = mac_tx.dst_add[6];
                outRf[7] = mac_tx.dst_add[5];
                outRf[8] = mac_tx.dst_add[4];
                outRf[9] = mac_tx.dst_add[3];
                outRf[10] = mac_tx.dst_add[2];
                outRf[11] = mac_tx.dst_add[1];
                outRf[12] = mac_tx.dst_add[0];
                posi_data += 6;
            }else// Short
            {
                outRf[5] = mac_tx.dst_add[0];
                outRf[6] = mac_tx.dst_add[1];
            }

            if(mac_tx.src_add_mode == 3)// Long
            {
                outRf[posi_data + 7] = MAC_extended_add[0];
                outRf[posi_data + 8] = MAC_extended_add[1];
                outRf[posi_data + 9] = MAC_extended_add[2];
                outRf[posi_data + 10] = MAC_extended_add[3];
                outRf[posi_data + 11] = MAC_extended_add[4];
                outRf[posi_data + 12] = MAC_extended_add[5];
                outRf[posi_data + 13] = MAC_extended_add[6];
                outRf[posi_data + 14] = MAC_extended_add[7];
                posi_data += 6;
            }else// Short
            {
                outRf[posi_data + 7] = MAC_short_add[0];
                outRf[posi_data + 8] = MAC_short_add[1];
            }
            memcpy(&outRf[posi_data + 9], (uint8_t *)&cmdb[sizeof(mac_tx_s)+2], mac_tx.msdu_length);
            radioSendData(outRf,  mac_tx.msdu_length + posi_data + 9);
    }
    break;
    }
    GPIO_write(7,0);
}

#define MAC_CMD_LEN 300
uint8_t cmdBuff[MAC_CMD_LEN];
int curCmdPos = 0;
int cmdState = 0;
int cmdExpLen = 0;
uint8_t cmdCrc = 0;
uint32_t lastCmdTime = 0;
void handleUartByte(uint8_t byteIn)
{
    if(getMillis() - lastCmdTime >= 10)
    {
        cmdState = 0;
    }
    lastCmdTime = getMillis();
    switch(cmdState){
    case 0:
        if(byteIn == 0xfe)
            cmdState++;
        break;
    case 1:
        cmdExpLen = byteIn + 2;
        curCmdPos = 0;
        cmdCrc = byteIn;
        cmdState++;
        break;
    case 2:
        cmdBuff[curCmdPos++] = byteIn;
        cmdCrc ^= byteIn;
        if(curCmdPos >= cmdExpLen)
            cmdState++;
        break;
    case 3:
        if(byteIn == cmdCrc)
            handleCmd(cmdBuff, cmdExpLen);
        cmdState = 0;
        break;
    }
}
// END Variables, definitions and function to handle data coming in via UART

void *mainThread(void *arg0)
{
    GPIO_setConfig(7, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_write(7,1);

    millisInit();
    initUart();

    while(1)
    {
        // Handle RF RX Data
        if(got_rx != worked_got_rx)
        {
            GPIO_write(7,1);
            uartMacWrite(packetDataPointer[worked_got_rx], packetDataPointer[worked_got_rx][0]);
            worked_got_rx++;
            if(worked_got_rx>=max_rf_rx_buffer)
                worked_got_rx = 0;
            GPIO_write(7,0);
        }
        // Handle UART RX Data
        if(bytesReadCount){
            for(int i=0;i<bytesReadCount;i++)
            {
                handleUartByte(input[i]);
            }
            bytesReadCount = 0;
            uartEnableInt();
        }
    }
}
