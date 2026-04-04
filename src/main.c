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

    // jb      _B_7,codeptr$        ; >0x80 code       ; 3
    // sjmp    .                                       ; 2


    // 4.1.5.1 Global Registers used for Parameter Passing
    // The compiler always uses the global registers DPL, DPH, B and ACC to pass the first (non-bit, non-struct) parameter
    // to a function, and also to pass the return value of function; according to the following scheme: one byte return value in
    // DPL, two byte value in DPL (LSB) and DPH (MSB). three byte values (generic pointers) in DPH, DPL and B, and four
    // byte values in DPH, DPL, B and ACC. Generic pointers contain type of accessed memory in B: 0x00 – xdata/far, 0x40 –
    // idata/near – , 0x60 – pdata, 0x80 – code.
    //
    //              B       _gptrget cycles
    // xdata/far    0x00    9
    // idata/near   0x40    11
    // pdata        0x60    18                          [paged data - i.e. for 8-bit access to xdata-space of up to 0xff bytes]
    // code         0x80    11
    //
    // The lcall takes an additional 3 cycles each, plus setting up parameters [only once] and copying read value [1] this takes too long.
    //
    // https://github.com/swegener/sdcc/blob/32d54288f93a1337c369e7d293bfa58cd0b0bc5f/device/lib/_gptrget.c
    //
    // void
    // _gptrget (char *gptr) __naked
    // {
    // /* This is the new version with pointers up to 16 bits.
    //    B cannot be trashed */
    //
    //     gptr; /* hush the compiler */
    //
    //     __asm
    //     ;
    //     ;   depending on the pointer type acc. to SDCCsymt.h
    //     ;
    //         jb      _B_7,codeptr$        ; >0x80 code       ; 3 [1/3]
    //         jnb     _B_6,xdataptr$       ; <0x40 far        ; 3 [1/3]
    //
    //         mov     dph,r0 ; save r0 independent of regbank ; 2 [1]
    //         mov     r0,dpl ; use only low order address     ; 2 [1]
    //
    //         jb      _B_5,pdataptr$       ; >0x60 pdata      ; 3 [1/3]
    //     ;
    //     ;   Pointer to data space
    //     ;
    //         mov     a,@r0                                   ; 1 [1]
    //  dataptrrestore$:
    //         mov     r0,dph ; restore r0                     ; 2 [1]
    //         mov     dph,#0 ; restore dph                    ; 3 [1]
    //         ret                                             ; 1 [3]
    //     ;
    //     ;   pointer to xternal stack or pdata
    //     ;
    //  pdataptr$:
    //         movx    a,@r0                                   ; 1 [3]
    //         sjmp    dataptrrestore$                         ; 2 [3]
    //     ;
    //     ;   pointer to code area, max 16 bits
    //     ;
    //  codeptr$:
    //         clr     a                                       ; 1 [1]
    //         movc    a,@a+dptr                               ; 1 [4]
    //         ret                                             ; 1 [3]
    //     ;
    //     ;   pointer to xternal data, max 16 bits
    //     ;
    //  xdataptr$:
    //         movx    a,@dptr                                 ; 1 [2]
    //         ret                                             ; 1 [3]
    //                                                         ;===
    //                                                         ;28 bytes
    //      __endasm;
    // }

    // The effective commands without any jumps and register restoring are:
    //
    // xdata [0x00]:
    //      movx    a,@dptr                                 ; 1 [2]
    //
    // idata [0x40]:
    //      mov     r0,dpl ; use only low order address     ; 2 [1]
    //      mov     a,@r0                                   ; 1 [1]
    //
    // pdata [0x60]:
    //      mov     r0,dpl ; use only low order address     ; 2 [1]
    //      movx    a,@r0                                   ; 1 [3]
    //
    // code [0x80]:
    //      clr     a                                       ; 1 [1]
    //      movc    a,@a+dptr                               ; 1 [4]



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
    // r5 -
    // r6 - remainingBits
    // r7 - datum

    // __asm__ (
    // "	push	_bp\n"
    // "	mov	_bp,sp\n"
    // "	mov	a,_bp\n"        // readout "length" from stack [--stack-auto or reentrant].
    // "	add	a,#0xfd\n"      // stack frame address - 3 [_bp, return address from lcall]
    // "	mov	r0,a\n"
    // "	mov	a,@r0\n"
    // "	mov	r4,a\n"         // r4 = byteLength = "remainingBytes"
    // "   inc r4\n"             // Increment r4 so that djnz can decrement and then compare to 0 the right amount of times.

    // // The following is the inner loop and must be timed precisely for the bits to be correct for the WS2812.
    // // Or do I have to be precise only on a single-bit basis and just ensure that between bits is less than T_res = 50 us?

    // "001$:\n"
    // "	djnz r4, 002$\n"    // if (0 != --remainingBytes)       2 [2/3]
    // "	ljmp	003$\n"     // else                             3 [3]
    // "002$:\n"
    // "	lcall	__gptrget\n"
    // "	mov	r7,a\n"         // r7 = datum = data[byteIndex]
    // "	inc dptr\n"         // ++byteIndex                      1 [1]
    // "	mov	r6,#0x09\n"     // remainingBits = 8 + 1            2 [1]
    // "004$:\n"
    // "	djnz r6, 005$\n"    // if (0 != --remainingBits)        2 [2/3]
    // "	ljmp	006$\n"     //                                  3 [3]
    // "005$:\n"
    // "	mov	a,r7\n"         //                                  1 [1]
    // "	rlc	a\n"            //                                  1 [1]
    // "	setb	_P5_5\n"    //                                  2 [1]
    // "	jc	007$\n"         //                                  2 [1/3]
    // "	ljmp	008$\n"     //                                  3 [3]
    // "007$:\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	ljmp	009$\n"     //                                  3 [3]
    // "008$:\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "009$:\n"
    // ";	assignBit\n"
    // "	clr	_P5_5\n"        //                                  2 [1]
    // "	jc	010$\n"         //                                  2 [1/3]
    // "	ljmp	011$\n"     //                                  3 [3]
    // "010$:\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	ljmp 012$\n"        //                                  3 [3]
    // "011$:\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "	NOP	\n"
    // "012$:\n"
    // "	mov	r7,a\n"         //                                  1 [1]
    // "	ljmp	004$\n"     //                                  3 [3]
    // "006$:\n"
    // "	ljmp	001$\n"     //                                  3 [3]

    // // Here the loop is over, so no precise timing required anymore.
    // "003$:\n"
    // "	mov	sp,_bp\n"
    // "	pop	_bp\n"
    // "	ret\n"
    // );



    __asm__ (
        "; Backup register values.\n"
        "   push _PSW\n"                   // Program Status Word register (PSW)
        "   mov _PSW, #0x00\n"             // reset carry flags and select register bank 0 [A]
        "   push ar7\n"                    // Backup register 7 of bank 0 on stack.
    );

    __asm__ (
        "; Transfer 32 bit -> 1 NeoPixel.\n"
        "   mov r7,#32\n"
        "; Start loop.\n"
        "001$:\n"
        "; Begin bit transmission by going HIGH.\n"
        "   setb _P5_5\n"                  // [1]
        "   nop\n"                         // [1]
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "; Second part of bit transmission by going LOW.\n"
        "   clr _P5_5\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "   nop\n"
        "; Next byte?\n"
        "   djnz r7,001$\n"                // Decrement register and jump if not Zero [2/3]
    );

    __asm__ (
        // "   sjmp 001$\n"
        // "   setb _P5_5\n" // for testing only
        "; Restore register values.\n"
        "   pop ar7\n"                     // Restore register 7.
        "   pop _PSW\n"                    // Restore PSW.
        "   ret"
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
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0xaa;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0xaa;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0xaa;
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
    for (int i = 0; 4 > i; ++i)
    {
        show(neoPixelData, /*bytes*/ 2);
    }

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
