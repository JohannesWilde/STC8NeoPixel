#include "8051_helpers.h"
#include "pinout.h"
#include "prescaler.h"
#include "static_assert.h"
#include "stc8g.h"

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



void show(uint8_t const * data, uint8_t const length) /*__naked*/
{

#if defined(DSDCC_MODEL_HUGE) || defined(DSDCC_MODEL_MEDIUM) || defined(__SDCC_ds390)
    // I only counted clock cycles of __gptrget for --model-small.
    COMPILE_TIME_ASSERT(false);
#endif

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

    // The effective commands without an jumps and register restoring are:
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



    // for (uint8_t index = 0; length > index; ++index)
    // {
    //     uint8_t datum = data[index];
    //     for (uint8_t bitIndex = 8; 0 < bitIndex; --bitIndex)
    //     {
    //         // MSb sent first.
    //         bool const bitValue = datum & 0x80;

    //         NEO_PIXEL_PIN = 1;
    //         if (bitValue)
    //         {
    //             NOP(); // 19.2
    //         }
    //         else
    //         {
    //             NOP(); // 9.6
    //         }
    //         NEO_PIXEL_PIN = 0;
    //         if (bitValue)
    //         {
    //             NOP(); // 10.8
    //         }
    //         else
    //         {
    //             NOP(); // 20.4
    //         }

    //         datum <<= 1;
    //     }
    // }



    // _show:
    //     ar7 = 0x07
    //     ar6 = 0x06
    //     ar5 = 0x05
    //     ar4 = 0x04
    //     ar3 = 0x03
    //     ar2 = 0x02
    //     ar1 = 0x01
    //     ar0 = 0x00
    //     push	_bp
    //     mov	_bp,sp
    //     push	dpl
    //     push	dph
    //     push	b
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:143: for (uint8_t index = 0; length > index; ++index)
    //     mov	r4,#0x00
    // 00113$:
    //     mov	a,_bp
    //     add	a,#0xfd
    //     mov	r0,a
    //     clr	c
    //     mov	a,r4
    //     subb	a,@r0
    //     jc	00155$
    //     ljmp	00108$
    // 00155$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:145: uint8_t datum = data[index];
    //     mov	r0,_bp
    //     inc	r0
    //     mov	a,r4
    //     add	a, @r0
    //     mov	r2,a
    //     mov	a,#0x00
    //     inc	r0
    //     addc	a, @r0
    //     mov	r3,a
    //     inc	r0
    //     mov	ar7,@r0
    //     mov	dpl,r2
    //     mov	dph,r3
    //     mov	b,r7
    //     lcall	__gptrget
    //     mov	r7,a
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:146: for (uint8_t bitIndex = 8; 0 < bitIndex; --bitIndex)
    //     mov	r6,#0x08
    // 00110$:
    //     clr	c
    //     mov	a,#0x00
    //     subb	a,r6
    //     jc	00156$
    //     ljmp	00114$
    // 00156$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:149: bool const bitValue = datum & 0x80;
    //     mov	a,r7
    //     rlc	a
    //     mov	b0,c
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:151: NEO_PIXEL_PIN = 1;
    // ;	assignBit
    //     setb	_P5_5
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:152: if (bitValue)
    //     jb	b0,00157$
    //     ljmp	00102$
    // 00157$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:154: NOP(); // 19.2
    //     NOP
    //     ljmp	00103$
    // 00102$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:158: NOP(); // 9.6
    //     NOP
    // 00103$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:160: NEO_PIXEL_PIN = 0;
    // ;	assignBit
    //     clr	_P5_5
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:161: if (bitValue)
    //     jb	b0,00158$
    //     ljmp	00105$
    // 00158$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:163: NOP(); // 10.8
    //     NOP
    //     ljmp	00106$
    // 00105$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:167: NOP(); // 20.4
    //     NOP
    // 00106$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:170: datum <<= 1;
    //     mov	a,r7
    //     add	a,acc
    //     mov	r7,a
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:146: for (uint8_t bitIndex = 8; 0 < bitIndex; --bitIndex)
    //     dec	r6
    //     ljmp	00110$
    // 00114$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:143: for (uint8_t index = 0; length > index; ++index)
    //     inc	r4
    //     ljmp	00113$
    // 00108$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:319: );
    // ;	Backup register values.
    // ;	Transfer 32 bit -> 1 NeoPixel.
    //     mov	r7,#64
    // ;	Start loop.
    // 001$:
    // ;	Begin bit transmission by going HIGH.
    //     setb	_P5_5
    //     mov	r6,#5
    // 002$:
    //     djnz	r6,002$
    // ;	Second part of bit transmission by going LOW.
    //     clr	_P5_5
    //     mov	r6,#1
    // 003$:
    //     djnz	r6,003$
    //     nop
    //     nop
    // ;	Next byte?
    //     djnz	r7,001$
    //     nop
    // ;	Restore register values.
    //     ret
    // 00115$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:320: }
    //     mov	sp,_bp
    //     pop	_bp
    //     ret




    // _show:
    //     ar7 = 0x07           // register bank 0, registr initialization
    //     ar6 = 0x06
    //     ar5 = 0x05
    //     ar4 = 0x04
    //     ar3 = 0x03
    //     ar2 = 0x02
    //     ar1 = 0x01
    //     ar0 = 0x00
    //     push	_bp                 // remember old stack base pointer
    //     mov	_bp,sp              // set current stack base pointer
    //     mov	r5, dpl             // uint8_t const data from [dpl, dph] to registers [r5, r6]
    //     mov	r6, dph
    //     mov	r7, b               // memory type copied to r7
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:47: for (uint8_t index = 0; length > index; ++index)

    // STC8G-en
    // 7.2.2 Program Status Word register (PSW)
    // The stack of STC8G series of microcontrollers grows upward, which
    // means that when a datum is pushed into the stack, the content of SP will increase.

    // https://www.8052mcu.com/51push
    // PUSH "pushes" the value of the specified iram addr onto the stack. PUSH first increments the value of the Stack Pointer
    // by 1, then takes the value stored in iram addr and stores it in Internal RAM at the location pointed to by the
    // incremented Stack Pointer.

    // https://www.8052mcu.com/51pop
    // POP "pops" the last value placed on the stack into the iram addr specified. In other words, POP will load iram addr with
    // the value of the Internal RAM address pointed to by the current Stack Pointer. The stack pointer is then decremented by 1.

    // https://www.8052mcu.com/51lcall.php
    // LCALL calls a program subroutine. LCALL increments the program counter by 3 (to point to the instruction following LCALL) and pushes that value onto the stack (low byte first, high byte second). The Program Counter is then set to the 16-bit value which follows the LCALL opcode, causing program execution to continue at that address.

    // https://www.8052mcu.com/51ret.php
    // RET is used to return from a subroutine previously called by LCALL or ACALL. Program execution continues at the address that is calculated by popping the topmost 2 bytes off the stack. The most-significant-byte is popped off the stack first, followed by the least-significant-byte.

    //     mov	a,_bp               // stack base pointer to accumulator
    //     add	a,#0xfd             // 0xfd = (int8_t)-3 ; second argument uint8_t const length is passed via stack [--stack-auto]; subtract 3 because [2 * return address, 1 * bp_].
    //     mov	r0,a                // Store address of length in r0.
    //     clr	c                   // Clear carry flag.
    //     mov	a,#0x00             // acc = 0
    //     subb	a,@r0               // acc = acc - @r0 [@r0 = length]
    //     jc	00143$              // if (a < length) -> 00143$   [inside for-loop]
    //     ljmp	00108$              // else: for-loop end
    // 00143$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:49: uint8_t datum = data[index];
    //     mov	dpl,r5              // Setup address and memory type for __gptrget function call parameters.
    //     mov	dph,r6
    //     mov	b,r7
    //     lcall	__gptrget
    //     mov	r7,a                // Copy read byte to r7.
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:50: for (uint8_t bitIndex = 8; 0 < bitIndex; --index)
    // 00110$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:53: bool const bitValue = datum & 0x80;
    //     mov	a,r7                // acc = r7
    //     rlc	a                   // rotate left through carry -> bit 0x80 shifted into carry
    //     mov	b0,c                // Move carry flag to b0 [BIT_BANK[0]]
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:55: NEO_PIXEL_PIN = 1;
    // ;	assignBit
    //     setb	_P5_5               // output pin HIGH
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:56: if (bitValue)
    //     jb	b0,00144$           // bit set -> if ()
    //     ljmp	00102$              // bit not set -> else ()
    // 00144$:                      // if ()
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:58: NOP(); // 19.2
    //     NOP
    //     ljmp	00103$
    // 00102$:                      // else ()
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:62: NOP(); // 9.6
    //     NOP
    // 00103$:                      // end of if-else
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:64: NEO_PIXEL_PIN = 0;
    // ;	assignBit
    //     clr	_P5_5               // output pin LOW
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:65: if (bitValue)
    //     jb	b0,00145$           // bit set -> if ()
    //     ljmp	00105$              // bit not set -> else ()
    // 00145$:                      // if ()
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:67: NOP(); // 10.8
    //     NOP
    //     ljmp	00106$
    // 00105$:                      // else()
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:71: NOP(); // 20.4
    //     NOP
    // 00106$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:74: datum <<= 1;
    //     mov	a,r7                // acc = datum
    //     add	a,acc               // acc = acc + acc = 2 * acc = acc << 1     - without carry
    //     mov	r7,a                // r7 = acc
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:50: for (uint8_t bitIndex = 8; 0 < bitIndex; --index)
    //     ljmp	00110$
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:47: for (uint8_t index = 0; length > index; ++index)
    // 00108$:
    // ;	C:\Users\User\Documents\STC\STC8NeoPixel\src\main.c:123: }
    //     pop	_bp
    //     ret





    // WS2812B
    // 0:  high 0.40 us, low 0.85 us
    // 1:  high 0.80 us, low 0.45 us
    //
    // @24 MHz:
    // 0:  high  9.6 clk, low 20.4 clk
    // 1:  high 19.2 clk, low 10.8 clk



    __asm__ (
    "; Backup register values.\n"
    // "   push _bp\n"                    // Stack Frame Pointer pushed to stack.
    // "   mov _bp,sp\n"                     // Stack Frame Pointer updated for this function.
    "; Transfer 32 bit -> 1 NeoPixel.\n"
    "   mov r7,#64\n"                  // [1]
    "; Start loop.\n"
    "001$:\n"
    "; Begin bit transmission by going HIGH.\n"
    "   setb _P5_5\n"                  // [1]

    // mov [1] + 5 * jump [3] + 1 * not-jump [2] -> [18]
    "   mov r6,#5\n"                  // [1]
    "002$:\n"
    "   djnz r6,002$\n"                // Decrement register and jump if not Zero [2/3]

    "; Second part of bit transmission by going LOW.\n"
    "   clr _P5_5\n"                    // [1]

    // mov [1] + 1 * jump [3] + 1 * not-jump [2] -> [6]
    "   mov r6,#1\n"                   // [1]
    "003$:\n"
    "   djnz r6,003$\n"                // Decrement register and jump if not Zero [2/3]
    "   nop\n"                         // [1]
    "   nop\n"                         // [1]

    "; Next byte?\n"
    "   djnz r7,001$\n"                // Decrement register and jump if not Zero [2/3]
    "   nop\n"                         // [1]

    "; Restore register values.\n"
    // "   pop _bp\n"                     // Restore Stack Frame Pointer.
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

    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED] = 0xff;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN] = 0x00;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE] = 0x00;
    neoPixelData[0 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE] = 0x00;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED] = 0x00;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN] = 0xff;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE] = 0x00;
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE] = 0x00;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED] = 0x00;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN] = 0x00;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE] = 0xff;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE] = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED] = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN] = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE] = 0x00;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE] = 0xff;
    show(neoPixelData, /*bytes*/ 4);

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
