#include "8051_helpers.h"
#include "pinout.h"
#include "prescaler.h"
#include "static_assert.h"
#include "stc8g.h"
#include "unused.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>


#define LED_PORT_NUMBER 1
#define LED_PIN_NUMBER 2
#define LED_PIN MAKE_PIN_NAME(LED_PORT_NUMBER, LED_PIN_NUMBER)

#define NEO_PIXEL_PORT_NUMBER 5
#define NEO_PIXEL_PIN_NUMBER 5
#define NEO_PIXEL_PIN MAKE_PIN_NAME(NEO_PIXEL_PORT_NUMBER, NEO_PIXEL_PIN_NUMBER)


#define F_WAKEUP_TIMER 36075  // Hz
#define F_IRC 24000000ull  // Hz
#define CLOCK_DIVISOR 1
#define F_CPU (F_IRC / CLOCK_DIVISOR)  // Hz
#define F_SYS_TICK 20  // Hz

// Prescaler for different time domains.
// 20 Hz -> 2 Hz
#define PRE_SCALER_ONE_INIT (10 - 1)

static uint8_t preScalerOne = PRE_SCALER_ONE_INIT;


#define NEO_PIXEL_DATA_BYTES_PER_PIXEL 4
#define NEO_PIXEL_DATA_OFFSET_RED 1
#define NEO_PIXEL_DATA_OFFSET_GREEN 0
#define NEO_PIXEL_DATA_OFFSET_BLUE 2
#define NEO_PIXEL_DATA_OFFSET_WHITE 3

static uint8_t neoPixelData[4 * /*bytes per pixel*/4];



void show(uint8_t const * data, uint8_t const length) __reentrant __naked
{

#if defined(DSDCC_MODEL_HUGE) || defined(DSDCC_MODEL_MEDIUM) || defined(__SDCC_ds390)
    // I only counted clock cycles of __gptrget for --model-small.
    COMPILE_TIME_ASSERT(false);
#endif

    UNUSED(data);
    UNUSED(length);

    // WS2812B
    // 0:  high 0.40 us, low 0.85 us
    // 1:  high 0.80 us, low 0.45 us
    //
    // @24 MHz:
    // 0:  high  9.6 clk, low 20.4 clk
    // 1:  high 19.2 clk, low 10.8 clk

    // b - memory type
    // [dpl, dph] - *data
    // r0 -
    // r1 -
    // r2 -
    // r3 -
    // r4 - remainingBytes
    // r5 - counter
    // r6 - remainingBits
    // r7 - datum

    __asm__ (
    "	push	_bp\n"
    "	mov	_bp,sp\n"
    "	mov	a,_bp\n"        // readout "length" from stack [--stack-auto or reentrant].
    "	add	a,#0xfd\n"      // stack frame address - 3 [_bp, return address from lcall]
    "	mov	r0,a\n"
    "	mov	a,@r0\n"
    "	mov	r4,a\n"         // r4 = byteLength = "remainingBytes"
    "   inc r4\n"             // Increment r4 so that djnz can decrement and then compare to 0 the right amount of times.
    "001$:\n"
    "	djnz r4, 002$\n"    // if (0 != --remainingBytes)       2 [2/3]
    "	ljmp	003$\n"     // else                             3 [3]
    "002$:\n"
    "	lcall	__gptrget\n"
    "	mov	r7,a\n"         // r7 = datum = data[byteIndex]
    "	inc dptr\n"         // ++byteIndex                      1 [1]
    "	mov	r6,#0x09\n"     // remainingBits = 8 + 1            2 [1]
    "004$:\n"
    "	djnz r6, 005$\n"    // if (0 != --remainingBits)        2 [2/3]
    "	ljmp	006$\n"     //                                  3 [3]
    "005$:\n"
    "	mov	a,r7\n"         //                                  1 [1]
    "	rlc	a\n"            // datum & 0x80 in carry            1 [1]
    "	mov	r7,a\n"         // datm << 1                        1 [1]

    // The following is the inner loop and must be timed precisely for each bit to be correct for the WS2812.
    // Please note, that as long as the inter-bit times are smaller than the reset time, the will be cascaded correctly.
    // So not timing requirements in between byte - apart from being less than 50 us apart.
                            //                                                              (1)     (0)
    "	setb	_P5_5\n"    //                                  2 [1]                       1       1
    "	jc	007$\n"         //                                  2 [1/3]                     4       2
    "	ljmp	008$\n"     //                                  3 [3]                               5

    // bit 1 transmission
    "007$:\n"
    "   mov r5,#5\n"        // 5 = 4 [jump] + 1 [not jump]      2 [1]                       5
    "013$:\n"
    "   djnz r5,013$\n"     // Decrement register and jump if not Zero     2 [2/3]          19
    "	clr	_P5_5\n"        //                                  2 [1]                       _1
    "   mov r5,#2\n"        // 1 = 1 [jump] + 1 [not jump]      2 [1]                       _2
    "014$:\n"
    "   djnz r5,014$\n"     // Decrement register and jump if not Zero     2 [2/3]          _7
    "	nop	\n"             //                                                              _8
    "	ljmp	009$\n"     //                                  3 [3]                       _11

    // bit 0 transmission
    "008$:\n"
    "	nop	\n"             //                                                                      6
    "	nop	\n"             //                                                                      7
    "	nop	\n"             //                                                                      8
    "	nop	\n"             //                                                                      9
    "	nop	\n"             //                                                                      10
    "	clr	_P5_5\n"        //                                  2 [1]                               _1
    "   mov r5,#6\n"        // 6 = 5 [jump] + 1 [not jump]      2 [1]                               _2
    "015$:\n"
    "   djnz r5,015$\n"     // Decrement register and jump if not Zero     2 [2/3]                  _19
    "	NOP	\n"             //                                                                      _20
    "009$:\n"

    // Here the single bit is over, so no precise timing required anymore.
    "	ljmp	004$\n"     //                                  3 [3]
    "006$:\n"
    "	ljmp	001$\n"     //                                  3 [3]

    "003$:\n"
    "	mov	sp,_bp\n"
    "	pop	_bp\n"
    "	ret\n"
    );
}


// main()

void main()
{
    LED_PIN = 0;
    NEO_PIXEL_PIN = 0;

    COMPILE_TIME_ASSERT(1 == LED_PORT_NUMBER);
    P1M0 = (0x00 /* DIO_MODE_HIGH_Z_INPUT_M0 */) |
           (DIO_MODE_PUSH_PULL_OUTPUT_M0 << LED_PIN_NUMBER);
    P1M1 = (0xff /* DIO_MODE_HIGH_Z_INPUT_M1 */) &
           ((DIO_MODE_PUSH_PULL_OUTPUT_M1 << LED_PIN_NUMBER) | ~(1 << LED_PIN_NUMBER));

    COMPILE_TIME_ASSERT(5 == NEO_PIXEL_PORT_NUMBER);
    P5M0 = (0x00 /* DIO_MODE_HIGH_Z_INPUT_M0 */) |
           (DIO_MODE_PUSH_PULL_OUTPUT_M0 << NEO_PIXEL_PIN_NUMBER);
    P5M1 = (0xff /* DIO_MODE_HIGH_Z_INPUT_M1 */) &
           ((DIO_MODE_PUSH_PULL_OUTPUT_M1 << NEO_PIXEL_PIN_NUMBER) | ~(1 << NEO_PIXEL_PIN_NUMBER));


    COMPILE_TIME_ASSERT(0 < CLOCK_DIVISOR);
    #if 1 < CLOCK_DIVISOR
        SFRX_ON();
        CLKDIV = CLOCK_DIVISOR;
        while (!(HIRCCR & 0x01));
        SFRX_OFF();
    #endif

    #define WAKEUP_TIMER_COUNT ((F_WAKEUP_TIMER / F_SYS_TICK / 16) - 1)
    COMPILE_TIME_ASSERT((1ull << 15) > WAKEUP_TIMER_COUNT);
    WKTCL = WAKEUP_TIMER_COUNT % 256;
    WKTCH = ((/*enabled*/ 1) << 7) | (0x7f & (WAKEUP_TIMER_COUNT / 256));

    interrupts(); // enable interrupts

    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED]    = 0xff;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0x00;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0x00;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0x00;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED]    = 0x00;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0xff;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0x00;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0x00;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED]    = 0x00;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0x00;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0xff;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED]    = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0xff;
    show(neoPixelData, /*bytes*/ 4 * 4);

    while (true)
    {
        if (updatePrescaler(&preScalerOne, PRE_SCALER_ONE_INIT))
        {
            // Somewhat slower.
            // LED_PIN ^= 1;

            __asm__ (
                "; Toggle the LED at P1.2.\n"
                "CPL _P1_2"
            );

        }
        else
        {
            // intentionally empty
        }

        PCON |= 0x02;  // PCON.PD = 1 - Enter power-down mode
    }
}
