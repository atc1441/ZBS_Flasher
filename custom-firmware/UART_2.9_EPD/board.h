#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>

#include "spi.h"
#include "uart.h"

//colors for ui messages
#define UI_MSG_MAGNIFY1			1
#define UI_MSG_MAGNIFY2			1
#define UI_MSG_MAGNIFY3			1
#define UI_MSG_BACK_COLOR		4
#define UI_MSG_FORE_COLOR_1		0
#define UI_MSG_FORE_COLOR_2		5
#define UI_MSG_FORE_COLOR_3		5
#define UI_BARCODE_VERTICAL

#define eepromByte				spiByte
#define eepromPrvSelect()		do { __asm__("nop\nnop\nnop\n"); P1_1 = 0; __asm__("nop\nnop\nnop\n"); } while(0)
#define eepromPrvDeselect()		do { __asm__("nop\nnop\nnop\n"); P1_1 = 1; __asm__("nop\nnop\nnop\n"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()
#define dbgUartOff()
#define dbgUartByte				uartTx

//eeprom map
#define EEPROM_SETTINGS_AREA_START		(0x01000UL)
#define EEPROM_SETTINGS_AREA_LEN		(0x03000UL)
#define EEPROM_UPDATA_AREA_START		(0x04000UL)
#define EEPROM_UPDATE_AREA_LEN			(0x10000UL)
#define EEPROM_IMG_START				(0x14000UL)
#define EEPROM_IMG_EACH					(0x04000UL)
//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
#define EEPROM_PROGRESS_BYTES			(128)

//radio cfg
#define RADIO_FIRST_CHANNEL				(11)		//2.4-GHz channels start at 11
#define RADIO_NUM_CHANNELS				(1)

//hw types
#define HW_TYPE_NORMAL					HW_TYPE_29_INCH_ZBS_026
#define HW_TYPE_CYCLING					HW_TYPE_29_INCH_ZBS_026_FRAME_MODE


#pragma callee_saves powerPortsDownForSleep
void powerPortsDownForSleep(void);

#pragma callee_saves boardInit
void boardInit(void);

//late, after eeprom
#pragma callee_saves boardInit
__bit boardGetOwnMac(uint8_t __xdata *mac);


//some sanity checks
#include "eeprom.h"


#if !EEPROM_SETTINGS_AREA_START
	#error "settings cannot be at address 0"
#endif

#if (EEPROM_SETTINGS_AREA_LEN % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "settings area must be an integer number of eeprom blocks"
#endif

#if (EEPROM_SETTINGS_AREA_START % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "settings must begin at an integer number of eeprom blocks"
#endif

#if (EEPROM_IMG_EACH % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "each image must be an integer number of eeprom blocks"
#endif

#if (EEPROM_IMG_START % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "images must begin at an integer number of eeprom blocks"
#endif

#if (EEPROM_UPDATE_AREA_LEN % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "update must be an integer number of eeprom blocks"
#endif

#if (EEPROM_UPDATA_AREA_START % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "images must begin at an integer number of eeprom blocks"
#endif



#endif
