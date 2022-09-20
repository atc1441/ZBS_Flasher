#define __packed
#include "proto.h"
#include "settings.h"
#include "asmUtil.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "drawing.h"
#include "printf.h"
#include "eeprom.h"
#include "screen.h"
#include "radio.h"
#include "sleep.h"
#include "timer.h"
#include "comms.h"
#include "board.h"
#include "chars.h"
#include "wdt.h"
#include "ccm.h"
#include "adc.h"
#include "cpu.h"

//#define PICTURE_FRAME_FLIP_EVERY_N_CHECKINS		24		//undefine to disable picture frame mode
#define HARDWARE_UNPAIR // undefine to disable hardware unpair

static const uint64_t __code VERSIONMARKER mVersionRom = 0x0000011200000000ull;

static uint64_t __xdata mVersion;
static uint8_t __xdata mRxBuf[COMMS_MAX_PACKET_SZ];
static struct Settings __xdata mSettings;
static uint32_t __idata mTimerWaitStart;
static uint8_t __xdata mNumImgSlots;
static struct CommsInfo __xdata mCi;
uint8_t __xdata mSelfMac[8];
uint16_t __xdata battery_voltage = 0;
int8_t __xdata mCurTemperature;

struct EepromContentsInfo
{
	uint32_t latestCompleteImgAddr, latestInprogressImgAddr, latestCompleteImgSize;
	uint64_t latestCompleteImgVer, latestInprogressImgVer;
	uint8_t numValidImages, latestImgIdx;
};

const char __xdata *fwVerString(void)
{
	static char __xdata fwVer[32];

	if (!fwVer[0])
	{
		spr(fwVer, "FW v%u.%u.%*u.%*u",
			*(((uint8_t __xdata *)&mVersion) + 5),
			*(((uint8_t __xdata *)&mVersion) + 4),
			(uintptr_near_t)(((uint8_t __xdata *)&mVersion) + 2),
			(uintptr_near_t)(((uint8_t __xdata *)&mVersion) + 0));
	}

	return fwVer;
}

const char __xdata *voltString(void)
{
	static char __xdata voltStr[32];

	if (!voltStr[0])
	{
		spr(voltStr, "%u.%uV",
			(uint16_t)mathPrvDiv32x16(battery_voltage, 1000), mathPrvDiv16x8(mathPrvMod32x16(battery_voltage, 1000), 100));
	}

	return voltStr;
}

void getVolt(void)
{
	if (battery_voltage == 0)
		battery_voltage = adcSampleBattery();
}

const char __xdata *macSmallString(void)
{
	static char __xdata macStr[32];

	if (!macStr[0])
	{
		spr(macStr, "%02X%02X%02X%02X%02X%02X%02X%02X",
			mSelfMac[7],
			mSelfMac[6],
			mSelfMac[5],
			mSelfMac[4],
			mSelfMac[3],
			mSelfMac[2],
			mSelfMac[1],
			mSelfMac[0]);
	}

	return macStr;
}

const char __xdata *macString(void)
{
	static char __xdata macStr[28];

	if (!macStr[0])
		spr(macStr, "%M", (uintptr_near_t)mSelfMac);

	return macStr;
}

static void prvEepromIndex(struct EepromContentsInfo __xdata *eci)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)mScreenRow; // use screen buffer
	uint8_t slotId;

	xMemSet(eci, 0, sizeof(*eci));

	for (slotId = 0; slotId < mNumImgSlots; slotId++)
	{

		uint32_t PDATA addr = mathPrvMul32x8(EEPROM_IMG_EACH, slotId) + EEPROM_IMG_START;
		static const uint32_t __xdata markerInProgress = EEPROM_IMG_INPROGRESS;
		static const uint32_t __xdata markerValid = EEPROM_IMG_VALID;

		uint32_t __xdata *addrP;
		uint64_t __xdata *verP;
		uint32_t __xdata *szP = 0;
		__bit isImage = false;

		eepromRead(addr, eih, sizeof(struct EepromImageHeader));
		// pr("DATA slot %u (@0x%08lx): type 0x%*08lx ver 0x%*016llx\n",
		//	slotId, addr, (uintptr_near_t)&eih->validMarker, (uintptr_near_t)&eih->version);

		if (xMemEqual4(&eih->validMarker, &markerInProgress))
		{

			verP = &eci->latestInprogressImgVer;
			addrP = &eci->latestInprogressImgAddr;
		}
		else if (xMemEqual4(&eih->validMarker, &markerValid))
		{

			eci->numValidImages++;
			isImage = true;
			verP = &eci->latestCompleteImgVer;
			addrP = &eci->latestCompleteImgAddr;
			szP = &eci->latestCompleteImgSize;
		}
		else
			continue;

		if (!u64_isLt(&eih->version, verP))
		{
			u64_copy(verP, &eih->version);
			*addrP = addr;
			if (szP)
				xMemCopy(szP, &eih->size, sizeof(eih->size));
			if (isImage)
				eci->latestImgIdx = slotId;
		}
	}
}

// similar to prvEepromIndex
static void uiPrvDrawNthValidImage(uint8_t n)
{
	static const uint32_t __xdata markerValid = EEPROM_IMG_VALID;
	uint8_t slotId;

	for (slotId = 0; slotId < mNumImgSlots; slotId++)
	{

		struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)mScreenRow; // use screen buffer
		uint32_t PDATA addr = mathPrvMul32x8(EEPROM_IMG_EACH, slotId) + EEPROM_IMG_START;

		eepromRead(addr, eih, sizeof(struct EepromImageHeader));

		if (xMemEqual4(&eih->validMarker, &markerValid))
		{

			if (!n--)
			{

				mSettings.lastShownImgSlotIdx = slotId;
				// drawImageAtAddress(addr);
				return;
			}
		}
	}
}

static __bit showVersionAndVerifyMatch(void)
{
	static const __code uint64_t verMask = ~VERSION_SIGNIFICANT_MASK;
	uint8_t i;

	u64_copyFromCode(&mVersion, &mVersionRom);
	pr("Booting FW ver 0x%*016llx\n", (uintptr_near_t)&mVersion);

	for (i = 0; i < 8; i++)
	{

		if (((const uint8_t __code *)&mVersionRom)[i] & ((const uint8_t __code *)&verMask)[i])
		{
			pr("ver num @ red zone\n");
			return false;
		}
	}

	pr(" -> %ls\n", (uintptr_near_t)fwVerString());
	return true;
}

static void prvFillTagState(struct TagState __xdata *state)
{
#if defined(PICTURE_FRAME_FLIP_EVERY_N_CHECKINS) && defined(HW_TYPE_CYCLING)
	state->hwType = HW_TYPE_CYCLING;
#elif !defined(PICTURE_FRAME_FLIP_EVERY_N_CHECKINS) && defined(HW_TYPE_NORMAL)
	state->hwType = HW_TYPE_NORMAL;
#else
#error "ths hardware does not support this mode"
#endif

	u64_copy(&state->swVer, &mVersion);
	state->batteryMv = adcSampleBattery();
}

static uint32_t uiNotPaired(void)
{
	char __code signalIcon[] = {CHAR_SIGNAL_PT1, CHAR_SIGNAL_PT2, 0};
	char __code noSignalIcon[] = {CHAR_NO_SIGNAL_PT1, CHAR_NO_SIGNAL_PT2, 0};

	struct
	{
		uint8_t pktType;
		struct TagInfo ti;
	} __xdata packet = {
		0,
	};
	uint_fast8_t i, ch;

	packet.pktType = PKT_ASSOC_REQ;
	packet.ti.protoVer = PROTO_VER_CURRENT;
	prvFillTagState(&packet.ti.state);
	packet.ti.screenPixWidth = SCREEN_WIDTH;
	packet.ti.screenPixHeight = SCREEN_HEIGHT;
	packet.ti.screenMmWidth = SCREEN_WIDTH_MM;
	packet.ti.screenMmHeight = SCREEN_HEIGHT_MM;
	packet.ti.compressionsSupported = PROTO_COMPR_TYPE_BITPACK;
	packet.ti.maxWaitMsec = COMMS_MAX_RADIO_WAIT_MSEC;
	packet.ti.screenType = SCREEN_TYPE;

	if (mSettings.helperInit != 1)
	{
		struct EepromContentsInfo __xdata eci;
		prvEepromIndex(&eci);
		pr("Associate is displayed\n");
		mSettings.helperInit = 1;
		settingsWrite(&mSettings);
		if (eci.numValidImages)
		{
			set_offline(1);
			drawImageAtAddress(EEPROM_IMG_START + (mathPrvMul32x8(EEPROM_IMG_EACH >> 8, eci.latestImgIdx) << 8));
		}
		else
		{
			eepromDeepPowerDown();
			drawFullscreenMsg((const __xdata char *)"ASSOCIATE READY");
		}
		wdtDeviceReset();
	}
	else
	{
		pr("Associate is not displayed\n");
	}
	timerDelay(TIMER_TICKS_PER_SECOND / 2);

	// RX on
	radioRxEnable(true, true);
	for (ch = 0; ch < RADIO_NUM_CHANNELS; ch++)
	{

		pr("try ch %u\n", RADIO_FIRST_CHANNEL + ch);
		radioSetChannel(RADIO_FIRST_CHANNEL + ch);

		for (i = 0; i < 2; i++)
		{ // try 2 times
			commsTx(&mCi, true, &packet, sizeof(packet));

			mTimerWaitStart = timerGet();
			while (timerGet() - mTimerWaitStart < TIMER_TICKS_PER_SECOND / 5)
			{ // wait 200 ms before retransmitting

				int8_t ret;

				ret = commsRx(&mCi, mRxBuf, mSettings.masterMac);

				if (ret == COMMS_RX_ERR_NO_PACKETS)
					continue;

				pr("RX pkt: 0x%02x + %d\n", mRxBuf[0], ret);

				if (ret == COMMS_RX_ERR_MIC_FAIL)
					pr("RX: invalid MIC\n");
				else if (ret <= 0)
					pr("WTF else ret = %x\n", (int16_t)(int8_t)ret); // nothing
				else if (ret < sizeof(uint8_t) + sizeof(struct AssocInfo))
					pr("RX: %d < %d\n", ret, sizeof(uint8_t) + sizeof(struct AssocInfo));
				else if (mRxBuf[0] != PKT_ASSOC_RESP)
					pr("RX: pkt 0x%02x @ pair\n", mRxBuf[0]);
				// else if (commsGetLastPacketRSSI() < -60)
				//   pr("RX: too weak to associate: %d\n", commsGetLastPacketRSSI());
				else
				{
					struct AssocInfo __xdata *ai = (struct AssocInfo __xdata *)(mRxBuf + 1);

					mSettings.checkinDelay = ai->checkinDelay;
					mSettings.retryDelay = ai->retryDelay;
					mSettings.failedCheckinsTillBlank = ai->failedCheckinsTillBlank;
					mSettings.failedCheckinsTillDissoc = ai->failedCheckinsTillDissoc;
					mSettings.channel = ch;
					mSettings.numFailedCheckins = 0;
					mSettings.nextIV = 0;
					xMemCopyShort(mSettings.encrKey, ai->newKey, sizeof(mSettings.encrKey));
					mSettings.isPaired = 1;
					mSettings.helperInit = 0;

					// power the radio down
					radioRxEnable(false, true);

					pr("Associated to master %m\n", (uintptr_near_t)&mSettings.masterMac);

					// pr("Erz IMG\n");
					// eepromErase(EEPROM_IMG_START, mathPrvMul32x8(EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ, mNumImgSlots));

					pr("Erz UPD\n");
					eepromErase(EEPROM_UPDATA_AREA_START, EEPROM_UPDATE_AREA_LEN / EEPROM_ERZ_SECTOR_SZ);

					struct EepromContentsInfo __xdata eci;
					prvEepromIndex(&eci);
					pr("Displaying the normal image again if available\n");
					settingsWrite(&mSettings);
					if (eci.numValidImages)
					{
						set_offline(0);
						drawImageAtAddress(EEPROM_IMG_START + (mathPrvMul32x8(EEPROM_IMG_EACH >> 8, eci.latestImgIdx) << 8));
					}
					else
					{
						drawFullscreenMsg(signalIcon);
					}

					return 1000; // wake up in a second to check in
				}
			}
		}
	}

	// power the radio down
	radioRxEnable(false, true);

	// drawFullscreenMsg(noSignalIcon);

	return 900000;
}

static struct PendingInfo __xdata *prvSendCheckin(void)
{
	struct
	{
		uint8_t pktTyp;
		struct CheckinInfo cii;
	} __xdata packet = {
		.pktTyp = PKT_CHECKIN,
	};
	uint8_t __xdata fromMac[8];

	prvFillTagState(&packet.cii.state);

	packet.cii.lastPacketLQI = mSettings.lastRxedLQI;
	packet.cii.lastPacketRSSI = mSettings.lastRxedRSSI;
	packet.cii.temperature = mCurTemperature + CHECKIN_TEMP_OFFSET;

	if (!commsTx(&mCi, false, &packet, sizeof(packet)))
	{
		pr("Fail to TX checkin\n");
		return 0;
	}

	mTimerWaitStart = timerGet();
	while (timerGet() - mTimerWaitStart < (uint32_t)((uint64_t)TIMER_TICKS_PER_SECOND * COMMS_MAX_RADIO_WAIT_MSEC / 1000))
	{

		int8_t ret = commsRx(&mCi, mRxBuf, fromMac);

		if (ret == COMMS_RX_ERR_NO_PACKETS)
			continue;

		pr("RX pkt: 0x%02x + %d\n", mRxBuf[0], ret);

		if (ret == COMMS_RX_ERR_MIC_FAIL)
		{

			pr("RX: invalid MIC\n");
			return 0;
		}

		if (ret < sizeof(uint8_t) + sizeof(struct PendingInfo))
		{

			pr("RX: %d < %d\n", ret, sizeof(uint8_t) + sizeof(struct PendingInfo));
			return 0;
		}

		if (mRxBuf[0] != PKT_CHECKOUT)
		{
			pr("RX: pkt 0x%02x @ checkin\n", mRxBuf[0]);
			return 0;
		}

		return (struct PendingInfo __xdata *)(mRxBuf + 1);
	}

	return 0;
}

static void uiPrvDrawImageAtSlotIndex(uint8_t idx)
{
	mSettings.lastShownImgSlotIdx = idx;
	drawImageAtAddress(EEPROM_IMG_START + (mathPrvMul32x8(EEPROM_IMG_EACH >> 8, idx) << 8));
}

static void prvApplyUpdateIfNeeded(void)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)mScreenRow;

	eepromRead(EEPROM_UPDATA_AREA_START, eih, sizeof(struct EepromImageHeader));

	if (eih->validMarker == EEPROM_IMG_INPROGRESS)
		pr("update: not fully downloaded\n");
	else if (eih->validMarker == 0xffffffff)
		pr("update: nonexistent\n");
	else if (eih->validMarker != EEPROM_IMG_VALID)
		pr("update: marker invalid: 0x%*08lx\n", (uintptr_near_t)&eih->validMarker);
	else
	{

		uint64_t __xdata tmp64 = VERSION_SIGNIFICANT_MASK;
		u64_and(&tmp64, &eih->version);

		if (!u64_isLt((const uint64_t __xdata *)&mVersion, &tmp64))
		{

			pr("update is not new enough: 0x%*016llx, us is: 0x%*016llx. Erasing\n", (uintptr_near_t)&eih->version, (uintptr_near_t)&mVersion);

			eepromErase(EEPROM_UPDATA_AREA_START, EEPROM_UPDATE_AREA_LEN / EEPROM_ERZ_SECTOR_SZ);
		}
		else
		{
			mSettings.isPaired = 0;
			settingsWrite(&mSettings);
			pr("applying the update 0x%*016llx -> 0x%*016llx\n", (uintptr_near_t)&mVersion, (uintptr_near_t)&eih->version);
			eepromReadStart(EEPROM_UPDATA_AREA_START + sizeof(struct EepromImageHeader));
			selfUpdate();
		}
	}
}

// prevStateP should be 0xffff for first invocation
static void prvProgressBar(uint16_t done, uint16_t outOf, uint16_t __xdata *prevStateP)
{

#if defined(SCREEN_PARTIAL_W2B) && defined(SCREEN_PARTIAL_B2W) && defined(SCREEN_PARTIAL_KEEP)

	uint16_t now = mathPrvDiv32x16(mathPrvMul16x16(done, SCREEN_WIDTH * SCREEN_TX_BPP / 8) + (outOf / 2), outOf); // in bytes
	__bit blacken, redraw = false;
	uint16_t i, j, min, max;
	uint8_t iteration;

	if (*prevStateP == 0xffff)
	{
		redraw = true;
	}
	if (now < *prevStateP)
	{
		min = now;
		max = *prevStateP;
		blacken = false;
	}
	else if (now == *prevStateP)
	{

		return;
	}
	else
	{
		blacken = true;
		min = *prevStateP;
		max = now;
	}
	*prevStateP = now;

	if (!screenTxStart(true))
		return;

	for (iteration = 0; iteration < SCREEN_DATA_PASSES; iteration++)
	{

		if (redraw)
		{
			for (i = 0; i < now; i++)
				screenByteTx(SCREEN_PARTIAL_W2B);
			for (; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(SCREEN_PARTIAL_B2W);
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(SCREEN_PARTIAL_B2W);
		}
		else
		{
			for (i = 0; i < min; i++)
				screenByteTx(SCREEN_PARTIAL_KEEP);
			for (; i < max; i++)
				screenByteTx(blacken ? SCREEN_PARTIAL_W2B : SCREEN_PARTIAL_B2W);
			for (; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(SCREEN_PARTIAL_KEEP);
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(SCREEN_PARTIAL_KEEP);
		}

		// fill rest
		for (j = 0; j < SCREEN_HEIGHT - 2; j++)
		{
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(SCREEN_PARTIAL_KEEP);
		}

		screenEndPass();
	}
	screenTxEnd();

#elif defined(SCREEN_PARTIAL_W2B) || defined(SCREEN_PARTIAL_B2W) || defined(SCREEN_PARTIAL_KEEP)
#error "some but not all partial defines found - this is an error"
#endif
}

static uint32_t prvDriveDownload(struct EepromImageHeader __xdata *eih, uint32_t addr, __bit isOS)
{
	struct
	{
		uint8_t pktTyp;
		struct ChunkReqInfo cri;
	} __xdata packet = {
		.pktTyp = PKT_CHUNK_REQ,
		.cri = {
			.osUpdatePlz = isOS,
		},
	};
	uint16_t nPieces = mathPrvDiv32x8(eih->size + EEPROM_PIECE_SZ - 1, EEPROM_PIECE_SZ);
	struct ChunkInfo __xdata *chunk = (struct ChunkInfo *)(mRxBuf + 1);
	uint8_t __xdata *data = (uint8_t *)(chunk + 1);
	__bit progressMade = false;
	uint16_t curPiece;

	// sanity check
	if (nPieces > sizeof(eih->piecesMissing) * 8)
	{
		pr("DL too large: %*lu\n", (uintptr_near_t)&eih->size);
		return mSettings.checkinDelay;
	}

	// prepare the packet
	u64_copy(&packet.cri.versionRequested, &eih->version);

	// find where we are in downloading
	for (curPiece = 0; curPiece < nPieces && !((eih->piecesMissing[curPiece / 8] >> (curPiece % 8)) & 1); curPiece++)
		;

	pr("Requesting piece %u/%u of %s\n", curPiece, nPieces, isOS ? "UPDATE" : "IMAGE");

	// download
	for (; curPiece < nPieces; curPiece++)
	{

		uint8_t now, nRetries;
		int8_t ret;

		// any piece that is not last will be of standard size
		if (curPiece != nPieces - 1)
			now = EEPROM_PIECE_SZ;
		else
			now = eih->size - mathPrvMul16x8(nPieces - 1, EEPROM_PIECE_SZ);

		packet.cri.offset = mathPrvMul16x8(curPiece, EEPROM_PIECE_SZ);
		packet.cri.len = now;

		if (!(((uint8_t)curPiece) & 0x7f))
			prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);

		for (nRetries = 0; nRetries < 5; nRetries++)
		{

			commsTx(&mCi, false, &packet, sizeof(packet));

			mTimerWaitStart = timerGet();
			while (1)
			{

				if (timerGet() - mTimerWaitStart > (uint32_t)((uint64_t)TIMER_TICKS_PER_SECOND * COMMS_MAX_RADIO_WAIT_MSEC / 1000))
				{
					pr("RX timeout in download\n");
					break;
				}

				ret = commsRx(&mCi, mRxBuf, mSettings.masterMac);

				if (ret == COMMS_RX_ERR_NO_PACKETS)
					continue; // let it time out
				else if (ret == COMMS_RX_ERR_INVALID_PACKET)
					continue; // let it time out
				else if (ret == COMMS_RX_ERR_MIC_FAIL)
				{
					pr("RX: invalid MIC\n");

					// mic errors are unlikely unless someone is deliberately messing with us - check in later
					goto checkin_again;
				}
				else if ((uint8_t)ret < (uint8_t)(sizeof(uint8_t) + sizeof(struct ChunkInfo)))
				{
					pr("RX: %d < %d\n", ret, sizeof(uint8_t) + sizeof(struct AssocInfo));

					// server glitch? check in later
					return mSettings.checkinDelay;
				}
				else if (mRxBuf[0] != PKT_CHUNK_RESP)
				{
					pr("RX: pkt 0x%02x @ DL\n", mRxBuf[0]);

					// weird packet? worth retrying soner
					break;
				}

				// get payload len
				ret -= sizeof(uint8_t) + sizeof(struct ChunkInfo);

				if (chunk->osUpdatePlz != isOS)
				{
					pr("RX: wrong data type @ DL: %d\n", chunk->osUpdatePlz);
					continue; // could be an accidental RX of older packet - ignore
				}
				else if (chunk->offset != packet.cri.offset)
				{
					pr("RX: wrong ofst @ DL 0x%08lx != 0x%*08lx\n", chunk->offset, (uintptr_near_t)&packet.cri.offset);
					continue; // could be an accidental RX of older packet - ignore
				}
				else if (!ret)
				{
					pr("RX: DL not avail\n");

					// just check in later
					goto checkin_again;
				}
				else if ((uint8_t)ret != (uint8_t)packet.cri.len)
				{

					pr("RX: Got %ub, reqd %u\n", ret, packet.cri.len);

					// server glitch? check in later
					goto checkin_again;
				}

				// write data
				eepromWrite(addr + mathPrvMul16x8(curPiece, EEPROM_PIECE_SZ) + sizeof(struct EepromImageHeader), data, ret);

				// write marker
				eih->piecesMissing[curPiece / 8] &= ~(1 << (curPiece % 8));
				eepromWrite(addr + offsetof(struct EepromImageHeader, piecesMissing[curPiece / 8]), &eih->piecesMissing[curPiece / 8], 1);

				progressMade = true;
				nRetries = 100; // so we break the loop
				break;
			}
		}
		if (nRetries == 5)
		{
			pr("retried too much\n");
			if (progressMade)
				goto retry_later;
			else
				goto checkin_again;
		}
	}

downloadDone:
	pr("Done at piece %u/%u\n", curPiece, nPieces);
	prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);

	// if we are here, we succeeeded in finishing the download
	eih->validMarker = EEPROM_IMG_VALID;
	eepromWrite(addr + offsetof(struct EepromImageHeader, validMarker), &eih->validMarker, sizeof(eih->validMarker));

	pr("DL completed\n");
	mSettings.prevDlProgress = 0xffff;

	// power the radio down
	radioRxEnable(false, true);

	// act on it
	if (isOS)
		prvApplyUpdateIfNeeded();
	else if (eih->size >= DRAWING_MIN_BITMAP_SIZE)
	{
		mSettings.lastShownImgSlotIdx = 0xffff;
		mSettings.checkinsToFlipCtr = 0;
		mSettings.lastRxedLQI = commsGetLastPacketLQI();
		mSettings.lastRxedRSSI = commsGetLastPacketRSSI();

		settingsWrite(&mSettings);

		drawImageAtAddress(addr);
		eepromInit();
		settingsRead(&mSettings);
		pr("image drawn\n");
	}

checkin_again:
	if (curPiece != nPieces)
		prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);
	return mSettings.checkinDelay;

retry_later:
	prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);
	return mSettings.retryDelay;
}

static void prvWriteNewHeader(struct EepromImageHeader __xdata *eih, uint32_t addr, uint8_t eeNumSecs, uint64_t __xdata *verP, uint32_t size) __reentrant
{
	// zero it
	xMemSet(eih, 0, sizeof(struct EepromImageHeader));

	// mark all pieces missing
	xMemSet(eih->piecesMissing, 0xff, sizeof(eih->piecesMissing));

	eepromErase(addr, eeNumSecs);

	u64_copy(&eih->version, verP);
	eih->validMarker = EEPROM_IMG_INPROGRESS;
	eih->size = size;

	eepromWrite(addr, eih, sizeof(struct EepromImageHeader));
}

static uint32_t prvDriveUpdateDownload(uint64_t __xdata *verP, uint32_t size)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)mScreenRow; // use screen buffer

	// see what's there already
	eepromRead(EEPROM_UPDATA_AREA_START, eih, sizeof(struct EepromImageHeader));
	if (!u64_isEq(&eih->version, verP))
		prvWriteNewHeader(eih, EEPROM_UPDATA_AREA_START, EEPROM_UPDATE_AREA_LEN / EEPROM_ERZ_SECTOR_SZ, verP, size);

	return prvDriveDownload(eih, EEPROM_UPDATA_AREA_START, true);
}

static uint32_t prvDriveImageDownload(const struct EepromContentsInfo __xdata *eci, uint64_t __xdata *verP, uint32_t size)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata *)mScreenRow; // use screen buffer
	uint32_t PDATA addr;

	// sort out where next image should live
	if (eci->latestInprogressImgAddr)
		addr = eci->latestInprogressImgAddr;
	else if (!eci->latestCompleteImgAddr)
		addr = EEPROM_IMG_START;
	else
	{
		addr = eci->latestCompleteImgAddr + EEPROM_IMG_EACH;

		if (addr >= EEPROM_IMG_START + (mathPrvMul16x8(EEPROM_IMG_EACH >> 8, mNumImgSlots) << 8))
			addr = EEPROM_IMG_START;
	}

	// see what's there already
	eepromRead(addr, eih, sizeof(struct EepromImageHeader));
	if (!u64_isEq(&eih->version, verP))
		prvWriteNewHeader(eih, addr, EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ, verP, size);

	return prvDriveDownload(eih, addr, false);
}

static __bit uiPrvIsShowingTooLongWithNoCheckinsMessage(void)
{
	return mSettings.failedCheckinsTillBlank && mSettings.numFailedCheckins >= mSettings.failedCheckinsTillBlank;
}

#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
static void uiPrvPictureFrameFlip(struct EepromContentsInfo __xdata *eci)
{
	// if we're showing the "too long since checkin" message, do nothing
	if (uiPrvIsShowingTooLongWithNoCheckinsMessage())
		return;

	if (++mSettings.checkinsToFlipCtr >= PICTURE_FRAME_FLIP_EVERY_N_CHECKINS)
	{

		uint16_t prev;
		uint8_t n;

		mSettings.checkinsToFlipCtr = 0;

		if (!eci->numValidImages)
			return;
		else if (eci->numValidImages == 1)
			n = 0;
		else
		{
			n = mathPrvMod32x16(rndGen32(), eci->numValidImages - 1);

			if (n >= mSettings.lastShownImgSlotIdx)
				n++;
		}
		prev = mSettings.lastShownImgSlotIdx;
		uiPrvDrawNthValidImage(n);
		pr("Flip %u->%u\n", prev, mSettings.lastShownImgSlotIdx);
	}
}
#endif

static uint32_t uiPaired(void)
{
	__bit updateOs, updateImg, checkInSucces = false, wasShowingError;
	uint64_t __xdata tmp64 = VERSION_SIGNIFICANT_MASK;
	struct EepromContentsInfo __xdata eci;
	struct PendingInfo __xdata *pi;
	uint8_t i;

	// do this before we get started with the radio
	prvEepromIndex(&eci);

#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
	uiPrvPictureFrameFlip(&eci);
#endif

	// power the radio up
	radioRxEnable(true, true);

	// try five times
	for (i = 0; i < 4; i++)
	{
		pi = prvSendCheckin();
		if (pi)
		{
			checkInSucces = true;
			break;
		}
	}

	if (!checkInSucces)
	{ // fail

		mSettings.numFailedCheckins++;
		if (!mSettings.numFailedCheckins) // do not allow overflow
			mSettings.numFailedCheckins--;
		pr("checkin #%u fails\n", mSettings.numFailedCheckins);

		// power the radio down
		radioRxEnable(false, true);

		if (mSettings.failedCheckinsTillDissoc && mSettings.numFailedCheckins == mSettings.failedCheckinsTillDissoc)
		{
			pr("Disassoc as %u = %u\n", mSettings.numFailedCheckins, mSettings.failedCheckinsTillDissoc);
			mSettings.isPaired = 0;

			return 1000; // wake up in a second to try to pair
		}

		if (mSettings.failedCheckinsTillBlank && mSettings.numFailedCheckins == mSettings.failedCheckinsTillBlank)
		{
			mSettings.lastShownImgSlotIdx = 0xffff;
			pr("Blank as %u = %u\n", mSettings.numFailedCheckins, mSettings.failedCheckinsTillBlank);
			// drawFullscreenMsg((const __xdata char*)"NO SIGNAL");
		}

		// try again in due time
		return mSettings.checkinDelay;
	}

	// if we got here, we succeeded with the check-in, but store if we were showing the error ...
	wasShowingError = uiPrvIsShowingTooLongWithNoCheckinsMessage();
	mSettings.numFailedCheckins = 0;

	// if screen was blanked, redraw it
	if (wasShowingError)
	{

#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
		uiPrvPictureFrameFlip(&eci);
#else
		// uiPrvDrawImageAtSlotIndex(eci.latestImgIdx);
#endif
	}

	// if there is an update, we want it. we know our version number is properly masked as we checked it at boot
	u64_and(&tmp64, &pi->osUpdateVer);
	updateOs = u64_isLt((uint64_t __xdata *)&mVersion, &tmp64);
	updateImg = u64_isLt(&eci.latestCompleteImgVer, &pi->imgUpdateVer);

	pr("Base: OS  ver 0x%*016llx, us 0x%*016llx (upd: %u)\n", (uintptr_near_t)&pi->osUpdateVer, (uintptr_near_t)&mVersion, updateOs);
	pr("Base: IMG ver 0x%*016llx, us 0x%*016llx (upd: %u)\n", (uintptr_near_t)&pi->imgUpdateVer, (uintptr_near_t)&eci.latestCompleteImgVer, updateImg);

	if (updateOs)
		return prvDriveUpdateDownload(&pi->osUpdateVer, pi->osUpdateSize);

	if (updateImg)
		return prvDriveImageDownload(&eci, &pi->imgUpdateVer, pi->imgUpdateSize);

	// nothing? guess we'll check again later
	return mSettings.checkinDelay;
}

void main(void)
{
	uint32_t __xdata sleepDuration;
	uint32_t eeSize;
	uint16_t nSlots;

	clockingAndIntsInit();
	timerInit();
	boardInit();

	pr("booted at 0x%04x\n", (uintptr_near_t)&main);

	if (!eepromInit())
	{
		pr("failed to init eeprom\n");
		drawFullscreenMsg((const __xdata char *)"eeprom failed");
		while (1)
			;
	}

	boardInitStage2();

	irqsOn();

	if (!showVersionAndVerifyMatch())
	{
		drawFullscreenMsg((const __xdata char *)"Verify Error");
		while (1)
			;
	}

	if (!boardGetOwnMac(mSelfMac))
	{
		pr("failed to get MAC. Aborting\n");
		drawFullscreenMsg((const __xdata char *)"Failed MAC");
		while (1)
			;
	}
	pr("MAC %ls\n", (uintptr_near_t)macString());

	mCurTemperature = adcSampleTemperature();
	pr("temp: %d\n", mCurTemperature);

	// for zbs, this must follow temp reading
	radioInit();

// sort out how many images EEPROM stores
#ifdef EEPROM_IMG_LEN

	mNumImgSlots = EEPROM_IMG_LEN / EEPROM_IMG_EACH;

#else
	eeSize = eepromGetSize();
	nSlots = mathPrvDiv32x16(eeSize - EEPROM_IMG_START, EEPROM_IMG_EACH >> 8) >> 8;
	if (eeSize < EEPROM_IMG_START || !nSlots)
	{

		pr("eeprom is too small\n");
		drawFullscreenMsg((const __xdata char *)"eeprom too small");
		while (1)
			;
	}
	else if (nSlots >> 8)
	{

		pr("eeprom is too big, some will be unused\n");
		mNumImgSlots = 255;
	}
	else
		mNumImgSlots = nSlots;
#endif
	pr("eeprom has %u image slots\n", mNumImgSlots);

	if (0)
	{

		drawFullscreenMsg((const __xdata char *)"ASSOCIATE READY");
		// screenTest();
		eepromDeepPowerDown();
		screenShutdown();
		powerPortsDownForSleep();
		sleepForMsec(0);
		wdtDeviceReset();
	}
	else if (0)
	{

		struct EepromContentsInfo __xdata eci;
		prvEepromIndex(&eci);
		uiPrvDrawImageAtSlotIndex(eci.latestImgIdx);
	}
	else
	{

		settingsRead(&mSettings);

#ifdef HARDWARE_UNPAIR
		// check if P1.0 is driven low externally; if so, remove pairing info
		P1DIR |= (1 << 0);	// P1.0 = input;
		P1PULL |= (1 << 0); // P1.0 = pullup;
		timerDelay(TIMER_TICKS_PER_SECOND / 1000);
		if (!(P1 & 0x01))
		{
			pr("Now deleting pairing info...\n");
			mSettings.helperInit = 0;
			mSettings.isPaired = 0;
			settingsWrite(&mSettings);
			drawFullscreenMsg((const __xdata char *)"HW UNPAIR!");
		}
		P1PULL &= ~(1 << 0);
#endif

		radioRxFilterCfg(mSelfMac, 0x10000, PROTO_PAN_ID);

		mCi.myMac = mSelfMac;
		mCi.masterMac = mSettings.masterMac;
		mCi.encrKey = mSettings.encrKey;
		mCi.nextIV = &mSettings.nextIV;

		// init the "random" number generation unit
		rndSeed(mSelfMac[0] ^ (uint8_t)timerGetLowBits(), mSelfMac[1] ^ (uint8_t)mSettings.hdr.revision);

		if (mSettings.isPaired)
		{

			radioSetChannel(RADIO_FIRST_CHANNEL + mSettings.channel);
			radioSetTxPower(10);

			prvApplyUpdateIfNeeded();

			sleepDuration = uiPaired();
		}
		else
		{

			static const uint32_t __xdata presharedKey[] = PROTO_PRESHARED_KEY;

			radioSetTxPower(10); // rather low

			xMemCopyShort(mSettings.encrKey, presharedKey, sizeof(mSettings.encrKey));

			sleepDuration = uiNotPaired();
		}

		mSettings.lastRxedLQI = commsGetLastPacketLQI();
		mSettings.lastRxedRSSI = commsGetLastPacketRSSI();

		// mSettings.isPaired = 0;
		settingsWrite(&mSettings);

		// vary sleep a little to avoid repeated collisions. Only down because 8-bit math is hard...
		sleepDuration = mathPrvMul32x8(sleepDuration, (rndGen8() & 31) + 224) >> 8;
	out:
		if (sleepDuration == 0)
			sleepDuration = 900000;
		pr("sleep: %lu\n", sleepDuration);
		eepromDeepPowerDown();
		screenShutdown();

		powerPortsDownForSleep();

		sleepForMsec(sleepDuration);

		// we may wake up here, but we prefer a reset so we cause one
		wdtDeviceReset();
	}
	while (1)
		;
}
