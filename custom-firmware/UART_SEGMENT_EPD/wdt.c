
#include "zbs243.h"
#include "wdt.h"

void wdtOff(void)
{
	uint8_t cfgPageBck;
	
	cfgPageBck = CFGPAGE;
	CFGPAGE = 4;
	WDTENA = 0;
	WDTCONF &=~ 0x80;
	CFGPAGE = cfgPageBck;
}

void wdtDeviceReset(void)
{
	CFGPAGE = 4;
	WDTCONF = 0x80;
	WDTENA = 1;
	WDTRSTVALH = 0xff;
	WDTRSTVALM = 0xff;
	WDTRSTVALL = 0xff;
	while(1);
}