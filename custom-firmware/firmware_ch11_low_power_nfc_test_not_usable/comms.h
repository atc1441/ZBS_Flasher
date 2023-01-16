#ifndef _COMMS_H_
#define _COMMS_H_

#include <stdint.h>
#include "ccm.h"

struct CommsInfo {
	const uint8_t __xdata *myMac;
	const uint8_t __xdata *masterMac;
	const void __xdata *encrKey;
	uint32_t __xdata *nextIV;
};

#define COMMS_MAX_RADIO_WAIT_MSEC		200

#define COMMS_IV_SIZE					(4)		//zeroes except these 4 counter bytes

#define COMMS_RX_ERR_NO_PACKETS			(-1)
#define COMMS_RX_ERR_INVALID_PACKET		(-2)
#define COMMS_RX_ERR_MIC_FAIL			(-3)

#define COMMS_MAX_PACKET_SZ				(127 /* max phy len */ - 21 /* max mac frame with panID compression */ - 2 /* FCS len */ - AES_CCM_MIC_SIZE - COMMS_IV_SIZE)

bool commsTx(struct CommsInfo __xdata *info, bool bcast, const void __xdata *packet, uint8_t len);
int8_t commsRx(struct CommsInfo __xdata *info, void __xdata *data, uint8_t __xdata *fromMac);	//returns length or COMMS_RX_ERR_*

#pragma callee_saves commsGetLastPacketLQI
uint8_t commsGetLastPacketLQI(void);

#pragma callee_saves commsGetLastPacketRSSI
int8_t commsGetLastPacketRSSI(void);


#endif
