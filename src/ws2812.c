#include "ws2812.h"

#include "configuration.h"
#include "static_assert.h"
#include "stc8g.h"
#include "unused.h"


#define MAKE_ASM_PIN_NAME_(port, pin) "_P" #port "_" #pin
#define MAKE_ASM_PIN_NAME(port, pin) MAKE_ASM_PIN_NAME_(port, pin)

#define NEO_PIXEL_ASM_PIN_STRING MAKE_ASM_PIN_NAME(NEO_PIXEL_PORT_NUMBER, NEO_PIXEL_PIN_NUMBER)


void show(uint8_t const * data, uint8_t const length, uint8_t const brightness) __reentrant
{
    for (uint8_t byteIndex = length; 0 < byteIndex; --byteIndex)
    {
        // Scale [0, 255] linearly to [0, 65535].
        uint8_t datum = ((((uint16_t)(*data) + 1) * (brightness + 1)) - 1) >> 8;
        ++data;

        for (uint8_t bitIndex = 8; 0 < bitIndex; --bitIndex)
        {
            uint8_t const bit = 0x80 & datum;
            datum <<= 1;

#if (24000000 == F_CPU)
            // @24 MHz:
            // 1:  high 19.2 clk, low 10.8 clk
            // 0:  high  9.6 clk, low 20.4 clk

            if (bit)
            {
                __asm__ (
                // bit "1" transmission
                "    push ar0\n"
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    mov r0,#6\n"        // 6 = 5 [jump] + 1 [not jump]                 2 [1]             2
                "001$:\n"
                "    djnz r0,001$\n"     // Decrement register and jump if not Zero     2 [2/3]          19
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    mov r0,#3\n"      // 3 = 2 [jump] + 1 [not jump]                   2 [1]             _2
                "002$:\n"
                "    djnz r0,002$\n"  // Decrement register and jump if not Zero        2 [2/3]           _10
                // "    nop    \n"       //                                                                  _11
                "    pop ar0\n"
                );
            }
            else
            {
                __asm__ (
                // bit "0" transmission
                "    push ar0\n"
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    mov r0,#3\n"        // 3 = 2 [jump] + 1 [not jump]                 2 [1]             2
                "003$:\n"
                "    djnz r0,003$\n"     // Decrement register and jump if not Zero     2 [2/3]          10
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    mov r0,#6\n"      // 6 = 5 [jump] + 1 [not jump]                   2 [1]             _2
                "004$:\n"
                "    djnz r0,004$\n"  // Decrement register and jump if not Zero        2 [2/3]           _19
                // "    nop    \n"       //                                                                  _20
                "    pop ar0\n"
                );
            }
#elif (16000000 == F_CPU)
            // @16 MHz
            // 1:  high 12.8 clk, low  7.2 clk
            // 0:  high  6.4 clk, low 13.6 clk

            if (bit)
            {
                __asm__ (
                // bit "1" transmission
                "    push ar0\n"
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    mov r0,#4\n"        // 4 = 3 [jump] + 1 [not jump]                 2 [1]             2
                "001$:\n"
                "    djnz r0,001$\n"     // Decrement register and jump if not Zero     2 [2/3]          13
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    mov r0,#2\n"      // 2 = 1 [jump] + 1 [not jump]                   2 [1]             _2
                "002$:\n"
                "    djnz r0,002$\n"  // Decrement register and jump if not Zero        2 [2/3]           _7
                "    pop ar0\n"
                );
            }
            else
            {
                __asm__ (
                // bit "0" transmission
                "    push ar0\n"
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    nop    \n"       //                                                                  2
                "    nop    \n"       //                                                                  3
                "    nop    \n"       //                                                                  4
                "    nop    \n"       //                                                                  5
                "    nop    \n"       //                                                                  6
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    mov r0,#4\n"      // 4 = 3 [jump] + 1 [not jump]                   2 [1]             _2
                "004$:\n"
                "    djnz r0,004$\n"  // Decrement register and jump if not Zero        2 [2/3]           _13
                // "    nop    \n"       //                                                                  _14
                "    pop ar0\n"
                );
            }
#elif (8000000 == F_CPU)
            // @8 MHz
            // 1:  high  6.4 clk, low  3.6 clk
            // 0:  high  3.2 clk, low  6.8 clk

            if (bit)
            {
                __asm__ (
                // bit "1" transmission
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    nop    \n"       //                                                                  2
                "    nop    \n"       //                                                                  3
                "    nop    \n"       //                                                                  4
                "    nop    \n"       //                                                                  5
                "    nop    \n"       //                                                                  6
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    nop    \n"       //                                                                  _2
                "    nop    \n"       //                                                                  _3
                // "    nop    \n"       //                                                                  _4
                );
            }
            else
            {
                __asm__ (
                // bit "0" transmission
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    nop    \n"       //                                                                  2
                "    nop    \n"       //                                                                  3
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    nop    \n"       //                                                                  2
                "    nop    \n"       //                                                                  3
                "    nop    \n"       //                                                                  4
                "    nop    \n"       //                                                                  5
                "    nop    \n"       //                                                                  6
                "    nop    \n"       //                                                                  7
                );
            }
#elif (4000000 == F_CPU)
            // @4 MHz
            // 1:  high  3.2 clk, low  1.8 clk
            // 0:  high  1.6 clk, low  3.4 clk

            if (bit)
            {
                __asm__ (
                // bit "1" transmission
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1
                "    nop    \n"       //                                                                  2
                "    nop    \n"       //                                                                  3
                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    nop    \n"       //                                                                  _2
                );
            }
            else
            {
                __asm__ (
                // bit "0" transmission
                // HIGH
                "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                      2 [1]             1

                // Even though as per the datasheet 2 clk = 500 ns should be in spec [400 ns +- 150 ns] my NeoPixels
                // seemed to always interpret this still as a "1"-bit.
                // Commenting out this line and thus reducing the HIGH-duration to 250 ns is just in spec - and did
                // work. But I would not trust it to always work reliably.
                // "    nop    \n"       //                                                                  2

                // LOW
                "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                      2 [1]             _1
                "    nop    \n"       //                                                                  2
                "    nop    \n"       //                                                                  3
                );
            }
#else
    COMPILE_TIME_ASSERT(false); // frequency not supported
#endif
        }
    }
}
