#include "8051_helpers.h"
#include "pinout.h"
#include "prescaler.h"
#include "specifics.h"
#include "static_assert.h"
#include "stc8g.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>


#define LED_PORT_NUMBER 1
#define LED_PIN_NUMBER 2
#define LED_PIN MAKE_PIN_NAME(LED_PORT_NUMBER, LED_PIN_NUMBER)


#define F_IRC 24000000ull  // Hz
#define CLOCK_DIVISOR 1
#define F_CPU (F_IRC / CLOCK_DIVISOR)  // Hz
#define F_SYS_TICK 1000ull  // Hz

volatile Timestamp milliseconds_;

// The name of this function does not really matter [apart from good coding practices], what
// is important is that it is declared the ISR handler via "__interrupt (x)" for the ISR-vector.
void TM0_Isr(void) __interrupt (TF0_VECTOR)
{
    /* action to be taken when timer 0 overflows */
    // Toggle inside interrupt routine.
    // LED_PIN ^= 1;


    COMPILE_TIME_ASSERT((1000ull / F_SYS_TICK) * F_SYS_TICK == 1000ull); // Prevent numeric precision loss in accumulation of ms.
    milliseconds_ += 1000 / F_SYS_TICK;

    COMPILE_TIME_ASSERT(1 == (1000 / F_SYS_TICK));
}


// TM1637 driver
// static uint8_t tm1637DisplayControl; // remember On/Off and brightness as I can't read it back.


// Prescaler for different time domains.
// 1000 Hz -> 20 Hz
#define PRE_SCALER_ONE_INIT (255 - 1)

static uint8_t preScalerOne = PRE_SCALER_ONE_INIT;


// main()

void main()
{
    SFRX_ON();

    // Precise timing is more important than less current for this application.
    // CLKSEL = 0b00; // 0b00 - internal high-precision IRC, 0b11 - internal 32KHz low speed IRC
    COMPILE_TIME_ASSERT(0 < CLOCK_DIVISOR);
    CLKDIV = CLOCK_DIVISOR;
    // HIRCCR = 1 << 7; // ENHIRC[7]
    // IRC32KCR = 0 << 7; // ENIRC32K[7]
    SFRX_OFF();

    P1M0 = (0x00 /* DIO_MODE_HIGH_Z_INPUT_M0 */) |
           (DIO_MODE_PUSH_PULL_OUTPUT_M0 << LED_PIN_NUMBER);
    P1M1 = (0xff /* DIO_MODE_HIGH_Z_INPUT_M1 */) &
           ((DIO_MODE_PUSH_PULL_OUTPUT_M1 << LED_PIN_NUMBER) | ~(1 << LED_PIN_NUMBER));

    // TMOD = (0 * T0_GATE) | (0 * T0_CT) | (0 * T0_M1) | (0 * T0_M0); // un-gated [0], timer [0], 16-bit auto-reload [00]
    AUXR |= (1 << 7); // AUXR.T0x12[7] = 0 -> 0: The clock source of T0 is SYSclk/12, 1: The clock source of T0 is SYSclk/1.
    #define TIMER0_COUNT (65536ull - (F_CPU /*/ 12*/ / F_SYS_TICK))
    // TR0 = 0; // Disable Timer0 so that the newly configured TH0 and TL0 will be used from the first cycle.
    TL0 = TIMER0_COUNT % 256;
    TH0 = TIMER0_COUNT / 256;
    TR0 = 1; // Timer0 run control bit
    ET0 = 1; // Enable Timer0 interrupt.

    interrupts(); // enable interrupts

    LED_PIN = 0;

    while (true)
    {
        // // Test outside of interrupt routine.
        // LED_PIN ^= 1;

        if (updatePrescaler(&preScalerOne, PRE_SCALER_ONE_INIT))
        {
            // Somewhat slower.
            LED_PIN ^= 1;
        }
        else
        {
            // intentionally empty
        }

        PCON |= (1 << 0);  // PCON.IDL[0] = 1 - Enter idle mode
    }
}
