#ifndef _WDT_H_
#define _WDT_H_

#include <stdint.h>

#pragma callee_saves wdtOff
void wdtOff(void);

#pragma callee_saves wdtDeviceReset
void wdtDeviceReset(void);


#endif
