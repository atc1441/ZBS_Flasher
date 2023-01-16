#include "settings.h"
#include "asmUtil.h"
#include "eeprom.h"
#include "printf.h"
#include "board.h"
#include "cpu.h"


#define EEPROM_NUM_SETTINGS_PAGES	(EEPROM_SETTINGS_AREA_LEN / EEPROM_ERZ_SECTOR_SZ)
#define SETTINGS_MAGIC				(0x31415930)



static __xdata uint32_t mCurSettingsAddr;



static void settingsPrvDoWriteAtLocation(uint32_t addr, struct Settings __xdata *settings)
{
	settings->hdr.structSize = sizeof(struct Settings);
	u64_inc(&settings->hdr.revision);
	mCurSettingsAddr = addr;
	eepromWrite(addr, settings, sizeof(struct Settings));
}

//this is impossible to call before calling read. thus mCurSettingsAddr will be set
void settingsPrvDoWrite(struct Settings __xdata *settings)
{
	uint32_t PDATA addr;
	
	//first we try to fit in the current page, after current (latest) settings
	if (mCurSettingsAddr) {
		
		struct SettingsHeader __xdata sh;
	
		eepromRead(mCurSettingsAddr, &sh, sizeof(struct SettingsHeader));
		addr = mCurSettingsAddr + sh.structSize;
		
		//is there space?
		if ((addr % EEPROM_ERZ_SECTOR_SZ) != 0 && (addr % EEPROM_ERZ_SECTOR_SZ) + sizeof(struct Settings) <= EEPROM_ERZ_SECTOR_SZ) {
			
			uint8_t i;
			
			//is it erased
			for (i = 0; i < (uint8_t)sizeof(struct Settings); i++) {
				
				uint8_t __xdata byte;
				
				eepromRead(addr + i, &byte, 1);
				if (byte != 0xff)
					break;
			}
			
			if (i == (uint8_t)sizeof(struct Settings)) {
				
				settingsPrvDoWriteAtLocation(addr, settings);
				return;
			}
		}
	
		//we need to erase - use next page (or 0th page if no current page at all)
		
		addr = (mCurSettingsAddr + EEPROM_ERZ_SECTOR_SZ) / EEPROM_ERZ_SECTOR_SZ * EEPROM_ERZ_SECTOR_SZ;
		if (addr == EEPROM_SETTINGS_AREA_START + EEPROM_SETTINGS_AREA_LEN)
			addr = EEPROM_SETTINGS_AREA_START;
	}
	else
		addr = EEPROM_SETTINGS_AREA_START;
	
	eepromErase(addr, 1);
	settingsPrvDoWriteAtLocation(addr, settings);
}

void settingsRead(struct Settings __xdata *settings)
{
	uint64_t __xdata bestRevision = 0;
	struct SettingsHeader __xdata sh;
	uint32_t PDATA bestAddr = 0;
	uint16_t PDATA ofst;
	__bit doWrite = true;
	uint8_t page;
	
	for (page = 0; page < EEPROM_NUM_SETTINGS_PAGES; page++) {
		
		for (ofst = 0; ofst < EEPROM_ERZ_SECTOR_SZ - sizeof(struct SettingsHeader); ofst += sh.structSize) {
			
			uint32_t addr = mathPrvMul16x8(EEPROM_ERZ_SECTOR_SZ, page) + EEPROM_SETTINGS_AREA_START + ofst;
			
			eepromRead(addr, &sh, sizeof(struct SettingsHeader));
			
			//sanity checks. struct is only allowed to grow in size...
			if (sh.magic != SETTINGS_MAGIC || ofst + sh.structSize > EEPROM_ERZ_SECTOR_SZ || sh.structSize > sizeof(struct Settings) || !sh.structSize)
				break;
			
			if (u64_isLt(&bestRevision, &sh.revision)) {
				
				u64_copy(&bestRevision, &sh.revision);
				bestAddr = addr;
			}
		}
	}
	
	if (bestAddr) {
		eepromRead(bestAddr, &sh, sizeof(struct SettingsHeader));	//to get size
		eepromRead(bestAddr, settings, sh.structSize);
		mCurSettingsAddr = bestAddr;
	}
	else {
		settings->hdr.structVersion = SETTINGS_VER_NONE;
		settings->hdr.revision = 1;
		mCurSettingsAddr = 0;
	}
	
	//migrate
	switch (settings->hdr.structVersion) {
		
		//current version here - mark as such
		case SETTINGS_CURRENT:
			doWrite = false;
			break;
		
		case SETTINGS_VER_NONE:	//migrate to v1
			xMemSet(settings, 0, sizeof(struct Settings));
			settings->hdr.magic = SETTINGS_MAGIC;
			settings->prevDlProgress = 0xffff;
			//fallthrough
		
		case SETTINGS_VER_1:	//migrate to v2
			settings->checkinsToFlipCtr = 0;
			settings->lastShownImgSlotIdx = 0xffff;
			//fallthrough
			
		
		//new migrations here in order from lower vers to higher vers
		
			settings->hdr.structVersion = SETTINGS_CURRENT;
			settings->hdr.structSize = sizeof(struct Settings);
			break;
	}
	
	if (doWrite)
		settingsPrvDoWrite(settings);
}

void settingsWrite(struct Settings __xdata *settings)
{
	struct Settings __xdata s;
	
	settingsRead(&s);
	if (!xMemEqual(&s, settings, sizeof(struct Settings)))
		settingsPrvDoWrite(settings);
}