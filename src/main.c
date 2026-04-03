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


void show() __naked
{


    // WS2812B
    // 0:  high 0.40 us, low 0.85 us
    // 1:  high 0.80 us, low 0.45 us
    //
    // @24 MHz:
    // 0:  high  9.6 clk, low 20.4 clk
    // 1:  high 19.2 clk, low 10.8 clk



    __asm__ (
    "; Backup register values.\n"
    "   push _PSW\n"                   // Program Status Word register (PSW)
    "   mov _PSW, #0x00\n"             // reset carry flags and select register bank 0 [A]
    "   push ar7\n"                    // Backup register 7 of bank 0 on stack.
    "   push ar6\n"                    // Backup register 6 of bank 0 on stack.
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
    "   pop ar6\n"                     // Restore register 6.
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


    show();

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
