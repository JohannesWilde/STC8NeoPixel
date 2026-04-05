#include "ws2812.h"

#include "configuration.h"
#include "static_assert.h"
#include "stc8g.h"
#include "unused.h"


#define MAKE_ASM_PIN_NAME_(port, pin) "_P" #port "_" #pin
#define MAKE_ASM_PIN_NAME(port, pin) MAKE_ASM_PIN_NAME_(port, pin)

#define NEO_PIXEL_ASM_PIN_STRING MAKE_ASM_PIN_NAME(NEO_PIXEL_PORT_NUMBER, NEO_PIXEL_PIN_NUMBER)


void show(uint8_t const * data, uint8_t const length, uint8_t const brightness) __reentrant __naked
{
    UNUSED(data);
    UNUSED(length);
    UNUSED(brightness);

    // WS2812B
    // 1:  high 0.80 us, low 0.45 us
    // 0:  high 0.40 us, low 0.85 us

    // b - memory type
    // [dpl, dph] - *data
    // r0 - temporary/datum
    // r1 -
    // r2 -
    // r3 -
    // r4 - remainingBytes
    // r5 - counter
    // r6 - remainingBits
    // r7 - brightness

    __asm__ (
    "; Define register names [this is not an assignment of values but a definition of symbols].\n"
    "    ar7 = 0x07\n"
    "    ar6 = 0x06\n"
    "    ar5 = 0x05\n"
    "    ar4 = 0x04\n"
    "    ar3 = 0x03\n"
    "    ar2 = 0x02\n"
    "    ar1 = 0x01\n"
    "    ar0 = 0x00\n"

    "    push _bp\n"
    "    mov    _bp,sp\n"

    "    push _PSW\n" // indirectly modified with carry-flag
    "    push acc\n"
    // "    push b\n" // unmodified in this function
    "    push dpl\n"
    "    push dph\n"
    "    push ar0\n"
    "    push ar4\n"
    "    push ar5\n"
    "    push ar6\n"
    "    push ar7\n"

    "    orl _PSW,#0x18\n"     // select register bank 0 explicitely

    "    mov    a,_bp\n"        // readout "length" from stack [--stack-auto or reentrant].
    "    add    a,#0xfd\n"      // stack frame address - 3 [_bp, return address from lcall]
    "    mov    r0,a\n"
    "    mov    a,@r0\n"
    "    mov    r4,a\n"         // r4 = byteLength = "remainingBytes"
    "    inc r4\n"             // Increment r4 so that djnz can decrement and then compare to 0 the right amount of times.

    "    mov    a,_bp\n"        // readout "brightness" from stack [--stack-auto or reentrant].
    "    add    a,#0xfc\n"      // stack frame address - 4 [_bp, return address from lcall, "length"]
    "    mov    r0,a\n"
    "    mov    a,@r0\n"
    "    mov    r7,a\n"         // r7 = "brightness"

    "001$:\n"
    "    djnz r4, 002$\n"    // if (0 != --remainingBytes)       2 [2/3]
    "    ljmp    003$\n"     // else                             3 [3]
    "002$:\n"
    "    lcall    __gptrget\n"
    "    mov    r0,a\n"         // r0 = datum = data[byteIndex]
    "    inc dptr\n"         // ++byteIndex                      1 [1]
    "    mov    r6,#0x09\n"     // remainingBits = 8 + 1            2 [1]

    // Apply "brightness" [r7]  to "datum" [r0 and still in a].
    "    push b\n"           // backup memory type to stack      2 [1]
    "    mov b, r7\n"        // "brightness" to b                2 [1]
    "    mul ab\n"           // b - high byte, a - low byte      1 [2]
    "    clr c\n"            //                                  1 [1]
    "    add a, r0\n"        // [datum + 1] * [brightness + 1] = [datum * brightness + datum + brightness] + 1       1 [1]
    "    add a, r7\n"        //                                  1 [1]
    "    mov a, b\n"         //                                  2 [1]
    "    addc a,#0\n"        //                                  2 [1]
    "    mov    r0,a\n"      // save scaled value to r0          1 [1]
    "    pop b\n"            // restore memory type from stack   2 [1]

    "004$:\n"
    "    djnz r6, 005$\n"    // if (0 != --remainingBits)        2 [2/3]
    "    ljmp    001$\n"     //                                  3 [3]
    "005$:\n"
    "    mov    a,r0\n"         //                                  1 [1]
    "    rlc    a\n"            // datum & 0x80 in carry            1 [1]
    "    mov    r0,a\n"         // datm <<= 1                       1 [1]

    // The following is the inner loop and must be timed precisely for each bit to be correct for the WS2812.
    // Please note, that as long as the inter-bit times are smaller than the reset time, it will be cascaded correctly.
    // So not timing requirements in between bits - apart from being less than 50 us apart.

#if (24000000 == F_CPU)
    // @24 MHz:
    // 1:  high 19.2 clk, low 10.8 clk
    // 0:  high  9.6 clk, low 20.4 clk

    "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                                  2 [1]                       1       1
    "    jc    007$\n"       //                                  2 [1/3]                     4       2
    "    ljmp    008$\n"     //                                  3 [3]                               5

    // bit "1" transmission
    "007$:\n"
    "    mov r5,#5\n"        // 5 = 4 [jump] + 1 [not jump]      2 [1]                       5
    "013$:\n"
    "    djnz r5,013$\n"     // Decrement register and jump if not Zero     2 [2/3]          19
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                       _1
    // "   mov r5,#2\n"      // 1 = 1 [jump] + 1 [not jump]      2 [1]                       _2
    // "014$:\n"
    // "    djnz r5,014$\n"  // Decrement register and jump if not Zero     2 [2/3]          _7
    // "    nop    \n"       //                                                              _8
    "    ljmp    004$\n"     //                                  3 [3]                       _11

    // bit "0" transmission
    "008$:\n"
    "    nop    \n"          //                                                                      6
    "    nop    \n"          //                                                                      7
    "    nop    \n"          //                                                                      8
    "    nop    \n"          //                                                                      9
    "    nop    \n"          //                                                                      10
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                               _1
    "    mov r5,#3\n"        // 6 - 3 = 5 - 3 [jump] + 1 [not jump]      2 [1]                       _2
    "015$:\n"
    "    djnz r5,015$\n"     // Decrement register and jump if not Zero     2 [2/3]                  _19
    "    nop    \n"          //                                                                      _20
#elif (16000000 == F_CPU)
    // @16 MHz
    // 1:  high 12.8 clk, low  7.2 clk
    // 0:  high  6.4 clk, low 13.6 clk

    "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                                  2 [1]                       1       1
    "    jc    007$\n"       //                                  2 [1/3]                     4       2
    "    ljmp    008$\n"     //                                  3 [3]                               5

    // bit "1" transmission
    "007$:\n"
    "    mov r5,#3\n"        // 3 = 2 [jump] + 1 [not jump]      2 [1]                       5
    "013$:\n"
    "    djnz r5,013$\n"     // Decrement register and jump if not Zero     2 [2/3]          13
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                       _1
    // "    nop    \n"       //                                                              _2
    // "    nop    \n"       //                                                              _3
    // "    nop    \n"       //                                                              _4
    "    ljmp    004$\n"     //                                  3 [3]                       _7

    // bit "0" transmission
    "008$:\n"
    "    nop    \n"          //                                                                      6
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                               _1
    "    mov r5,#1\n"        // 4 - 3 = 3 - 3 [jump] + 1 [not jump]      2 [1]                       _2
    "015$:\n"
    "    djnz r5,015$\n"     // Decrement register and jump if not Zero     2 [2/3]                  _13
    "    nop    \n"          //                                                                      _14
#elif (8000000 == F_CPU)
    // @8 MHz
    // 1:  high  6.4 clk, low  3.6 clk
    // 0:  high  3.2 clk, low  6.8 clk

    "    jc    007$\n"       //                                  2 [1/3]
    "    ljmp    008$\n"     //                                  3 [3]

    // bit "1" transmission
    "007$:\n"
    "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                                  2 [1]                       1
    "    nop    \n"          //                                                              2
    "    nop    \n"          //                                                              3
    "    nop    \n"          //                                                              4
    "    nop    \n"          //                                                              5
    "    nop    \n"          //                                                              6
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                       _1
    "    ljmp    004$\n"     //                                  3 [3]                       _4

    // bit "0" transmission
    "008$:\n"
    "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                                  2 [1]                               1
    "    nop    \n"          //                                                                      2
    "    nop    \n"          //                                                                      3
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                               _1
    // "    nop    \n"       //                                                                      _2
    // "    nop    \n"       //                                                                      _3
    // "    nop    \n"       //                                                                      _4
    // "    nop    \n"       //                                                                      _5
    // "    nop    \n"       //                                                                      _6
    // "    nop    \n"       //                                                                      _7
#elif (4000000 == F_CPU)
    // @4 MHz
    // 1:  high  3.2 clk, low  1.8 clk
    // 0:  high  1.6 clk, low  3.4 clk

    "    jc    007$\n"       //                                  2 [1/3]
    "    ljmp    008$\n"     //                                  3 [3]

    // bit "1" transmission
    "007$:\n"
    "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                                  2 [1]                       1
    "    nop    \n"          //                                                              2
    "    nop    \n"          //                                                              3
    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                       _1
    "    ljmp    004$\n"     //                                  3 [3]                       _2 + 2

    // bit "0" transmission
    "008$:\n"
    "    setb    " NEO_PIXEL_ASM_PIN_STRING "\n"    //                                  2 [1]                               1

    // Even though as per the datasheet 2 clk = 500 ns should be in spec [400 ns +- 150 ns] my NeoPixels
    // seemed to always interpret this still as a "1"-bit.
    // Commenting out this line and thus reducing the HIGH-duration to 250 ns is just in spec - and did
    // work. But I would not trust it to always work reliably.
    // "    nop    \n"       //                                                                      2

    "    clr    " NEO_PIXEL_ASM_PIN_STRING "\n"     //                                  2 [1]                               _1
    // "    nop    \n"       //                                                                      _2
    // "    nop    \n"       //                                                                      _3
#else
    COMPILE_TIME_ASSERT(false); // frequency not supported
#endif

    // Here the single bit is over, so no precise timing required anymore.
    "    ljmp    004$\n"     //                                  3 [3]

    "003$:\n"

    "    pop ar7\n"
    "    pop ar6\n"
    "    pop ar5\n"
    "    pop ar4\n"
    "    pop ar0\n"
    "    pop dph\n"
    "    pop dpl\n"
    // "    push b\n" // always restored in this function
    "    pop acc\n"
    "    pop _PSW\n" // indirectly modified with carry-flag

    "    mov    sp,_bp\n"
    "    pop    _bp\n"
    "    ret\n"
    );
}
