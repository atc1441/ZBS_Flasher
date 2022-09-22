#include <stdbool.h>+
#include "asmUtil.h"
#include "drawing.h"
#include "printf.h"
#include "screen.h"
#include "chars.h"
#include "board.h"
#include "adc.h"
#include "soc.h"

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


#pragma callee_saves myStrlen
static uint16_t myStrlen(const char *str)
{
	const char *__xdata strP = str;

	while (charsPrvDerefAndIncGenericPtr(&strP))
		;

	return strP - str;
}

uint8_t position = 5;
void drawFullscreenMsg(const char *str, const char *str1)
{
	volatile uint16_t PDATA textRow, textRowEnd; // without volatile, compiler ignores "__pdata"
	struct CharDrawingParams __xdata cdp;
	uint8_t __xdata rowIdx;
	uint8_t iteration;
	uint16_t i, r;

	// getVolt();

	pr("MESSAGE '%s'  '%s'\n", str, str1);

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
		__bit inBarcode = false;
		rowIdx = 0;

		cdp.magnify = UI_MSG_MAGNIFY1;
		cdp.str = str;
		cdp.x = mathPrvI16Asr1(SCREEN_WIDTH - mathPrvMul8x8(CHAR_WIDTH * cdp.magnify, myStrlen(cdp.str)));

		cdp.foreColor = UI_MSG_FORE_COLOR_1;
		cdp.backColor = UI_MSG_BACK_COLOR;

		textRow = position;

		position += 24;
		if (position >= 265)
		{
			position = 5;
		}

		textRowEnd = textRow + 24;

		for (r = 0; r < SCREEN_HEIGHT; r++)
		{

			// clear the row
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				mScreenRow[i] = SCREEN_BYTE_FILL;

			if (r >= textRowEnd)
			{
				switch (rowIdx)
				{
				case 0:
					rowIdx = 1;
					textRow = textRowEnd + 1;
					cdp.magnify = UI_MSG_MAGNIFY2;
					cdp.foreColor = UI_MSG_FORE_COLOR_2;
					cdp.str = str1;
					cdp.x = 0;
					textRowEnd = textRow + CHAR_HEIGHT * cdp.magnify;
					break;
				case 1:
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
