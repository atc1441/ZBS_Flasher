#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>

typedef uint16_t uintptr_near_t;

#pragma callee_saves clockingAndIntsInit
void clockingAndIntsInit(void);

#pragma callee_saves powerPortsDownForSleep
void powerPortsDownForSleep(void);

#pragma callee_saves boardInit
void boardInit(void);

#endif
