#include <stdbool.h>
//#include "datamatrix.h"
#include "barcode.h"
#include "asmUtil.h"
#include "drawing.h"
#include "printf.h"
#include "screen.h"
#include "eeprom.h"
#include "chars.h"
#include "board.h"
#include "adc.h"
#include "cpu.h"

#define COMPRESSION_BITPACKED_3x5_to_7 0x62700357 // 3 pixels of 5 possible colors in 7 bits
#define COMPRESSION_BITPACKED_5x3_to_8 0x62700538 // 5 pixels of 3 possible colors in 8 bits
#define COMPRESSION_BITPACKED_3x6_to_8 0x62700368 // 3 pixels of 6 possible colors in 8 bits

struct BitmapFileHeader
{
	uint8_t sig[2];
	uint32_t fileSz;
	uint8_t rfu[4];
	uint32_t dataOfst;
	uint32_t headerSz; // 40
	int32_t width;
	int32_t height;
	uint16_t colorplanes; // must be one
	uint16_t bpp;
	uint32_t compression;
	uint32_t dataLen; // may be 0
	uint32_t pixelsPerMeterX;
	uint32_t pixelsPerMeterY;
	uint32_t numColors; // if zero, assume 2^bpp
	uint32_t numImportantColors;
};

struct BitmapClutEntry
{
	uint8_t b, g, r, x;
};

struct BitmapDrawInfo
{
	// dimensions
	uint16_t w, h, effectiveW, effectiveH, stride /* 0 -> 1, 5 - >7, 255 -> 256 */;
	uint8_t numColorsM1;

	// data start
	uint32_t dataAddr;

	// compression state
	uint8_t packetPixelDivVal;
	uint8_t packetNumPixels;
	uint8_t packetBitSz;
	uint8_t packetBitMask; // derived from the above

	// flags
	uint8_t bpp : 4;
	uint8_t bottomUp : 1;
};

static uint8_t __xdata mClutMap[256];
static struct BitmapDrawInfo __xdata mDrawInfo;

#pragma callee_saves drawPrvParseHeader
static uint32_t drawPrvParseHeader(uint32_t addr) // return clut addr or zero on error
{
	struct BitmapFileHeader __xdata bmph;
	uint16_t __xdata packetsPerRow;
	uint32_t compression;

	addr += sizeof(struct EepromImageHeader);
	eepromRead(addr, &bmph, sizeof(bmph));

	if (bmph.sig[0] != 'B' || bmph.sig[1] != 'M')
		goto fail;

	if (bmph.colorplanes != 1)
		goto fail;

	if (u32minusU16(&bmph.headerSz, 40)) // < 40
		goto fail;

	if (bmph.bpp > 8)
		goto fail;
	mDrawInfo.bpp = bmph.bpp;

	if (!u32minusU16(&bmph.headerSz, 257)) // >= 257
		goto fail;

	if (u32Nonzero(&bmph.numColors))
		mDrawInfo.numColorsM1 = (uint8_t)bmph.numColors - (uint8_t)1;
	else
		mDrawInfo.numColorsM1 = (uint8_t)((uint8_t)1 << (uint8_t)mDrawInfo.bpp) - (uint8_t)1;

	if (!u32Nonzero(&bmph.height))
		goto fail;

	if (u32minusU16(&bmph.width, 1) || !u32minusU16(&bmph.width, 0xffff))
		goto fail;
	mDrawInfo.w = bmph.width;

	if (i32Negative(&bmph.height))
	{
		if (u32plusU16(&bmph.height, 0xffff)) // carries if val too negative
			goto fail;
		mDrawInfo.h = -bmph.height;
		mDrawInfo.bottomUp = false;
	}
	else
	{
		if (!u32minusU16(&bmph.headerSz, 0xffff)) // no carry if val too big
			goto fail;
		mDrawInfo.h = bmph.height;
		mDrawInfo.bottomUp = true;
	}

	compression = bmph.compression;
	if (compression == COMPRESSION_BITPACKED_3x5_to_7 || compression == COMPRESSION_BITPACKED_5x3_to_8 || compression == COMPRESSION_BITPACKED_3x6_to_8)
	{

		mDrawInfo.packetNumPixels = (uint8_t)(compression >> 8) & 0x0f;
		mDrawInfo.packetBitSz = (uint8_t)compression & 0x0f;
		mDrawInfo.packetPixelDivVal = ((uint8_t)compression) >> 4;
	}
	else if (bmph.compression)
	{

		pr("unknown compression 0x%*08lx\n", (uint16_t)&bmph.compression);
		goto fail;
	}
	else
	{ // uncompressed

		mDrawInfo.packetPixelDivVal = 0;
		mDrawInfo.packetNumPixels = 1;
		mDrawInfo.packetBitSz = mDrawInfo.bpp;
	}
	packetsPerRow = mathPrvDiv16x8(mDrawInfo.w + mDrawInfo.packetNumPixels - 1, mDrawInfo.packetNumPixels);
	mDrawInfo.stride = mathPrvDiv32x8(mathPrvMul16x8(packetsPerRow, mDrawInfo.packetBitSz) + 31, 32) * 4UL;
	mDrawInfo.packetBitMask = (uint8_t)(((uint8_t)1) << (uint8_t)mDrawInfo.packetBitSz) - (uint8_t)1;

	// calc effective size
	mDrawInfo.effectiveH = (mDrawInfo.h > SCREEN_HEIGHT) ? SCREEN_HEIGHT : mDrawInfo.h;
	mDrawInfo.effectiveW = (mDrawInfo.w > SCREEN_WIDTH) ? SCREEN_WIDTH : mDrawInfo.w;

	// calc addrs
	mDrawInfo.dataAddr = addr + bmph.dataOfst;
	return addr + bmph.dataOfst - sizeof(struct BitmapClutEntry) * (1 + mDrawInfo.numColorsM1);

// seriously, fuck SDCC
fail:
	return 0;
}

#pragma callee_saves drawPrvLoadAndMapClut
static void drawPrvLoadAndMapClut(uint32_t clutAddr)
{
	struct BitmapClutEntry __xdata clut;
	uint8_t __xdata i;

	// convert clut to our understanding of color
	i = 0;
	do
	{
		uint8_t __xdata entry;

		eepromRead(clutAddr, &clut, sizeof(clut));
		clutAddr += sizeof(struct BitmapClutEntry);

		if (SCREEN_EXTRA_COLOR_INDEX >= 0 && clut.r == 0xff && (clut.g == 0xff || clut.g == 0) && clut.b == 0) // yellow/red
			entry = SCREEN_EXTRA_COLOR_INDEX;
		else
		{

			uint16_t intensity = 0;

			intensity += mathPrvMul8x8(0x37, clut.r);
			intensity += mathPrvMul8x8(0xB7, clut.g);
			intensity += mathPrvMul8x8(0x12, clut.b);
			// adds up to 0xff00 -> fix it
			intensity += (uint8_t)(intensity >> 8);

			entry = mathPrvMul16x8(intensity, SCREEN_NUM_GREYS) >> 16;
			entry += SCREEN_FIRST_GREY_IDX;
		}
		// pr("mapped clut %u (%d %d %d) -> %d\n", i, clut.r, clut.g, clut.b, entry);
		mClutMap[i] = entry;
	} while (i++ != mDrawInfo.numColorsM1);

	// replicate clut down if not a full 256-entry clut
	if (mDrawInfo.bpp != 8)
	{
		uint8_t num = (uint8_t)((uint8_t)1 << (uint8_t)mDrawInfo.bpp);

		// we can use the fact that our memcpy always copies forward
		xMemCopyShort(mClutMap + num, mClutMap, (uint8_t)256 - (uint8_t)num);
	}
}

__bit offline = 0;

void set_offline(__bit state)
{
	offline = state;
}

#pragma callee_saves drawPrvDecodeImageOnce
static void drawPrvDecodeImageOnce(void)
{
	uint8_t __xdata rowBuf[SCREEN_WIDTH];
	uint16_t __xdata er, c;

	if (mDrawInfo.bottomUp)
		er = mDrawInfo.effectiveH - 1;
	else
		er = 0;

	while (1)
	{ // we account differently for loop gets compiled worse
		uint8_t __xdata inIdx = 0, bitpoolInUsed = 0, bitpoolIn = 0;
		uint16_t __xdata nBytesOut = 0;

#if SCREEN_TX_BPP == 4
		uint8_t __xdata txPrev = 0;
		__bit emit = false;
#else
		uint8_t __xdata bitpoolOutUsedUsed = 0;
		uint16_t __xdata bitpoolOut = 0;
#endif

		// get a row
		if (offline && (er >= SCREEN_HEIGHT - 5))
		{
			xMemSet(rowBuf, 0x00, SCREEN_WIDTH);
		}
		else
		{
			eepromRead(mathPrvMul16x16(er, mDrawInfo.stride) + mDrawInfo.dataAddr, rowBuf, mDrawInfo.stride);
		}
		// convert to our format
		c = mDrawInfo.effectiveW;
		do
		{
			uint8_t packet, packetIdx, packetMembers = mDrawInfo.packetNumPixels;

			if (bitpoolInUsed >= mDrawInfo.packetBitSz)
			{
				bitpoolInUsed -= mDrawInfo.packetBitSz;
				packet = bitpoolIn >> bitpoolInUsed;
			}
			else
			{
				uint8_t __xdata packetBitSz = mDrawInfo.packetBitSz;
				uint8_t __xdata t = rowBuf[inIdx++];

				packet = (bitpoolIn << (packetBitSz - bitpoolInUsed)) | (t >> (8 - (packetBitSz - bitpoolInUsed)));
				bitpoolInUsed += 8 - packetBitSz;

				bitpoolIn = t;
			}
			packet &= mDrawInfo.packetBitMask;

			// val is now a packet - unpack it
			if (packetMembers > c)
				packetMembers = c;

			for (packetIdx = 0; packetIdx < packetMembers; packetIdx++)
			{

				uint8_t __xdata val;

				// extract
				if (mDrawInfo.packetPixelDivVal)
				{

					val = packet % mDrawInfo.packetPixelDivVal;
					packet /= mDrawInfo.packetPixelDivVal;
				}
				else
					val = packet;

				// map
				val = mClutMap[val];

// get bits out
#if SCREEN_TX_BPP == 4

				if (emit)
				{
					emit = false;
					screenByteTx(txPrev | val);
					nBytesOut++;
					txPrev = 0;
				}
				else
				{
					emit = true;
					txPrev = val << 4;
				}

#else
				bitpoolOut <<= SCREEN_TX_BPP;
				bitpoolOut |= val;
				bitpoolOutUsedUsed += SCREEN_TX_BPP;
				if (bitpoolOutUsedUsed >= 8)
				{
					screenByteTx(bitpoolOut >> (bitpoolOutUsedUsed -= 8));
					bitpoolOut &= (1 << bitpoolOutUsedUsed) - 1;
					nBytesOut++;
				}
#endif
			}
			c -= packetMembers;
		} while (c);

#if SCREEN_TX_BPP == 4

		if (emit)
		{

			screenByteTx(txPrev);
			nBytesOut++;
		}

#else

		if (bitpoolOutUsedUsed)
		{

			screenByteTx(bitpoolOut);
			nBytesOut++;
		}

#endif

		// if we did not produce enough bytes, do so
		nBytesOut = ((long)SCREEN_WIDTH * SCREEN_TX_BPP + 7) / 8 - nBytesOut;
		while (nBytesOut--)
			screenByteTx(SCREEN_BYTE_FILL);

		// update row
		if (mDrawInfo.bottomUp)
		{
			if (er)
				er--;
			else
				break;
		}
		else
		{
			er++;
			if (er == mDrawInfo.effectiveH)
				break;
		}
	}

	// fill the rest of the screen
	for (er = mDrawInfo.effectiveH - SCREEN_HEIGHT; er; er--)
	{

		for (c = ((long)SCREEN_WIDTH * SCREEN_TX_BPP + 7) / 8; c; c--)
		{

			screenByteTx(SCREEN_BYTE_FILL);
		}
	}
}

void drawImageAtAddress(uint32_t addr)
{
	uint32_t __xdata clutAddr;
	uint8_t __xdata iter;

	clutAddr = drawPrvParseHeader(addr);
	if (!clutAddr)
		return;
	drawPrvLoadAndMapClut(clutAddr);

	screenTxStart(false);
	for (iter = 0; iter < SCREEN_DATA_PASSES; iter++)
	{

		drawPrvDecodeImageOnce();
		screenEndPass();
	}

	screenTxEnd();
	screenShutdown();
}

#pragma callee_saves myStrlen
static uint16_t myStrlen(const char *str)
{
	const char *__xdata strP = str;

	while (charsPrvDerefAndIncGenericPtr(&strP))
		;

	return strP - str;
}

void drawFullscreenMsg(const char *str)
{
	volatile uint16_t PDATA textRow, textRowEnd; // without volatile, compiler ignores "__pdata"
	struct CharDrawingParams __xdata cdp;
	uint8_t __xdata rowIdx;
	uint8_t iteration;
	uint16_t i, r;

	getVolt();

#if defined(DATAMATRIX)

	uint8_t __xdata macBar[48];

	i = adcSampleBattery();
	spr(macBar + 0, "MAC: %02X:%02X:%02X:%02X:", mSelfMac[7], mSelfMac[6], mSelfMac[5], mSelfMac[4]);
	spr(macBar + 17, "%02X:%02X:%02X:%02X\nbatt: ", mSelfMac[3], mSelfMac[2], mSelfMac[1], mSelfMac[0]);
	spr(macBar + 35, "%u.%uV", (uint16_t)mathPrvDiv32x16(i, 1000), mathPrvDiv16x8(mathPrvMod32x16(i, 1000), 100));

#elif defined(BARCODE)

	uint8_t __xdata macBar[17];

	spr(macBar + 0, "%02X%02X%02X%02X", mSelfMac[7], mSelfMac[6], mSelfMac[5], mSelfMac[4]);
	spr(macBar + 8, "%02X%02X%02X%02X", mSelfMac[3], mSelfMac[2], mSelfMac[1], mSelfMac[0]);

#endif

	pr("MESSAGE '%s'\n", str);

#if 1

#ifdef DATAMATRIX
	if (!datamatrixInit())
		pr("DME: init\n");
	for (iteration = 0; iteration < (uint8_t)xStrLen(macBar); iteration++)
	{

		if (!datamatrixAppendByte(macBar[iteration] + 1))
			pr("DME: add\n");
	}
	if (!datamatrixFinish())
		pr("DME: fin\n");
#endif

	screenTxStart(false);

	for (iteration = 0; iteration < SCREEN_DATA_PASSES; iteration++)
	{

#if defined(BARCODE)
#if defined(UI_BARCODE_VERTICAL)
		struct BarcodeInfo __xdata bci = {
			.str = macBar,
		};
#elif defined(UI_BARCODE_HORIZ)
		uint8_t PDATA nBarcodeRows = 20;
#endif
#elif defined(DATAMATRIX)

#define DATAMATRIX_EFFECTIVE_W (DATAMATRIX_DRAW_WIDTH + 4)
#define DATAMATRIX_MAGNIFY_C (SCREEN_WIDTH / DATAMATRIX_EFFECTIVE_W)
#define DATAMATRIX_MAGNIFY ((DATAMATRIX_MAGNIFY_C > 3) ? 3 : DATAMATRIX_MAGNIFY_C)
#define DATAMATRIX_X ((SCREEN_WIDTH - DATAMATRIX_DRAW_WIDTH * DATAMATRIX_MAGNIFY) / 2)
#define DATAMATRIX_H (DATAMATRIX_DRAW_HEIGHT * DATAMATRIX_MAGNIFY)
#define DATAMATRIX_V_SPACE 2

		uint8_t barcodeRowIdx = 0;

#endif
		__bit inBarcode = false;
		rowIdx = 0;

		cdp.magnify = UI_MSG_MAGNIFY1;
		cdp.str = str;
		cdp.x = mathPrvI16Asr1(SCREEN_WIDTH - mathPrvMul8x8(CHAR_WIDTH * cdp.magnify, myStrlen(cdp.str)));

		cdp.foreColor = UI_MSG_FORE_COLOR_1;
		cdp.backColor = UI_MSG_BACK_COLOR;

#ifdef UI_MSG_MAIN_TEXT_V_CENTERED
		textRow = SCREEN_HEIGHT - (uint8_t)((uint8_t)CHAR_HEIGHT * (uint8_t)cdp.magnify);
		textRow >>= 1;
#else
		textRow = 5;
#endif

		textRowEnd = textRow + (uint8_t)((uint8_t)CHAR_HEIGHT * (uint8_t)cdp.magnify);

		for (r = 0; r < SCREEN_HEIGHT; r++)
		{

#if defined(BARCODE) && defined(UI_BARCODE_HORIZ)
			struct BarcodeInfo __xdata bci = {
				.str = macBar,
			};
#endif

			// clear the row
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				mScreenRow[i] = SCREEN_BYTE_FILL;

			if (inBarcode)
			{

#if defined(DATAMATRIX)

				if (++barcodeRowIdx >= DATAMATRIX_V_SPACE)
				{

					uint16_t pos = 0;
					uint8_t j = 0, val = 0;

					for (i = 0; i < SCREEN_WIDTH; i++)
					{

						__bit set = false;

						if (i >= DATAMATRIX_X && i - DATAMATRIX_X < DATAMATRIX_DRAW_WIDTH * DATAMATRIX_MAGNIFY)
						{

							set = datamatrixRead(mathPrvDiv16x8(barcodeRowIdx - DATAMATRIX_V_SPACE, DATAMATRIX_MAGNIFY), mathPrvDiv16x8(i - DATAMATRIX_X, DATAMATRIX_MAGNIFY));
						}

						val <<= SCREEN_TX_BPP;
						if (set)
							val += UI_MSG_FORE_COLOR_1;
						else
							val += UI_MSG_BACK_COLOR;

						j += SCREEN_TX_BPP;
						if (j == 8)
						{
							mScreenRow[pos++] = val;
							j = 0;
						}
					}
				}

#elif !defined(BARCODE)

				// nothing

#elif defined(UI_BARCODE_VERTICAL)
				uint8_t color = barcodeNextBar(&bci) ? ((UI_MSG_FORE_COLOR_1) | (UI_MSG_FORE_COLOR_1 << SCREEN_TX_BPP) | (UI_MSG_FORE_COLOR_1 << (2 * SCREEN_TX_BPP)) | (UI_MSG_FORE_COLOR_1 << (8 * SCREEN_TX_BPP))) : SCREEN_BYTE_FILL;

				for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8 / 8; i++)
				{

					mScreenRow[i] = color;
					mScreenRow[SCREEN_WIDTH * SCREEN_TX_BPP / 8 - 1 - i] = color;
				}

#elif defined(UI_BARCODE_HORIZ)

				uint8_t skipCols = (SCREEN_WIDTH - barcodeWidth(sizeof(macBar) - 1)) / 2;

				if (nBarcodeRows)
				{
					nBarcodeRows--;

					for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
					{

						uint8_t j, val;

						for (j = 0, val = 0; j < 8; j += SCREEN_TX_BPP)
						{
							val <<= SCREEN_TX_BPP;

							if (skipCols)
							{
								val += UI_MSG_BACK_COLOR;
								skipCols--;
							}
							else
								val += barcodeNextBar(&bci) ? UI_MSG_FORE_COLOR_1 : UI_MSG_BACK_COLOR;
						}
						mScreenRow[i] = val;
					}
				}

#endif
			}

			if (r >= textRowEnd)
			{

				switch (rowIdx)
				{
				case 0:
					rowIdx = 1;
					textRow = textRowEnd + 3;
					cdp.magnify = UI_MSG_MAGNIFY2;
					cdp.foreColor = UI_MSG_FORE_COLOR_2;
					cdp.str = macSmallString();
					cdp.x = 0;
					textRowEnd = textRow + CHAR_HEIGHT * cdp.magnify;
					break;

				case 1:
					rowIdx = 2;
					textRow = SCREEN_HEIGHT - CHAR_HEIGHT - CHAR_HEIGHT;
					cdp.magnify = UI_MSG_MAGNIFY3;
					cdp.foreColor = UI_MSG_FORE_COLOR_3;
					cdp.str = voltString();
					cdp.x = 1;
					inBarcode = true;
#if defined(DATAMATRIX)
					textRowEnd = textRow + DATAMATRIX_H;
#elif defined(BARCODE)
					textRowEnd = textRow + CHAR_HEIGHT;
#else
					inBarcode = false;
#endif
					break;
				case 2:
					rowIdx = 3;
					textRow = SCREEN_HEIGHT - CHAR_HEIGHT;
					cdp.magnify = UI_MSG_MAGNIFY3;
					cdp.foreColor = UI_MSG_FORE_COLOR_3;
					cdp.str = fwVerString();
					cdp.x = 1;
					inBarcode = true;
#if defined(DATAMATRIX)
					textRowEnd = textRow + DATAMATRIX_H;
#elif defined(BARCODE)
					textRowEnd = textRow + CHAR_HEIGHT;
#else
					inBarcode = false;
#endif
					break;

				case 3:
					cdp.str = "";
					break;
				}
			}
			else if (r > textRow)
			{
				inBarcode = false;
				cdp.imgRow = r - textRow;
				charsDrawString(&cdp);
			}

			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(mScreenRow[i]);
		}

		screenEndPass();
	}

	screenTxEnd();
#endif
}
