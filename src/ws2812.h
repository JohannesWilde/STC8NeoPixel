#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>


void show(uint8_t const * data, uint8_t const length, uint8_t const brightness) __reentrant __naked;

#endif // WS2812_H
