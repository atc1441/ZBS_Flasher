#ifndef _SCREEN_SEGMENTED_H_
#define _SCREEN_SEGMENTED_H_

#include <stdint.h>

// data is 13/30 bytes (12 significant), assumes spi & gpios are set up
__bit screenDraw(const uint8_t __xdata *data, __bit inverted, __bit custom_lut);

void display_end();

uint8_t is_drawing();

#endif
