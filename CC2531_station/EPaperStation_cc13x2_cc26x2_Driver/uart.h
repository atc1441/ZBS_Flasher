#pragma once
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

void initUart();
void uartEnableInt();
void uartWrite(uint8_t *buffer, size_t len);
