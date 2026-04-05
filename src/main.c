#include "8051_helpers.h"
#include "configuration.h"
#include "pinout.h"
#include "prescaler.h"
#include "static_assert.h"
#include "stc8g.h"
#include "ws2812.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>


#define LED_PIN MAKE_PIN_NAME(LED_PORT_NUMBER, LED_PIN_NUMBER)
#define NEO_PIXEL_PIN MAKE_PIN_NAME(NEO_PIXEL_PORT_NUMBER, NEO_PIXEL_PIN_NUMBER)


// Prescaler for different time domains.
// 20 Hz -> 2 Hz
#define PRE_SCALER_ONE_INIT (10 - 1)

static uint8_t preScalerOne = PRE_SCALER_ONE_INIT;


#define NEO_PIXEL_DATA_BYTES_PER_PIXEL 4
#define NEO_PIXEL_DATA_OFFSET_RED 1
#define NEO_PIXEL_DATA_OFFSET_GREEN 0
#define NEO_PIXEL_DATA_OFFSET_BLUE 2
#define NEO_PIXEL_DATA_OFFSET_WHITE 3

static uint8_t neoPixelData[9 * NEO_PIXEL_DATA_BYTES_PER_PIXEL];




#define BRIGHTNESS_DELTA_STEP 1

static uint8_t colorBrightness;
static uint8_t colorDelta = BRIGHTNESS_DELTA_STEP;


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
    neoPixelData[1 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0xff;
    neoPixelData[2 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0xff;
    neoPixelData[3 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0xff;
    neoPixelData[4 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED]    = 0xff;
    neoPixelData[5 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_GREEN]  = 0xff;
    neoPixelData[6 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_BLUE]   = 0xff;
    neoPixelData[7 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_WHITE]  = 0xff;
    neoPixelData[8 * NEO_PIXEL_DATA_BYTES_PER_PIXEL + NEO_PIXEL_DATA_OFFSET_RED]    = 0xff;

    while (true)
    {
        if (BRIGHTNESS_DELTA_STEP > colorBrightness)
        {
            colorDelta = BRIGHTNESS_DELTA_STEP;
        }
        else if ((255 - BRIGHTNESS_DELTA_STEP) < colorBrightness)
        {
            colorDelta = (uint8_t)(-BRIGHTNESS_DELTA_STEP);
        }
        else
        {
            // intentionally empty
        }

        colorBrightness = colorBrightness + colorDelta;

        show(neoPixelData, /*bytes*/ 9 * NEO_PIXEL_DATA_BYTES_PER_PIXEL, /*brightness*/ colorBrightness);

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
        SFRX_ON();
        while (!(HIRCCR & 0x01));
        SFRX_OFF();
    }
}
