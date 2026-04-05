#ifndef CONFIGURATION_H
#define CONFIGURATION_H


#define F_WAKEUP_TIMER 36075  // Hz
#define F_IRC 24000000ull  // Hz
#define CLOCK_DIVISOR 3
#define F_CPU (F_IRC / CLOCK_DIVISOR)  // Hz
#define F_SYS_TICK 20  // Hz


/**
 * STC8G1K08 [SOP8]
 *
 *                     +----------+
 *  rotary encoder A --+ 1      8 +-- TM1637 I2C CLK
 *               VCC --+ 2      7 +-- TM1637 I2C Data
 *  rotary encoder B --+ 3      6 +-- MOSFET            [HIGH - conductive, LOW - blocking]
 *               GND --+ 4      5 +-- push button       [pull-up, LOW when pushed]
 *                     +----------+
 */

#define LED_PORT_NUMBER 1
#define LED_PIN_NUMBER 2

#define NEO_PIXEL_PORT_NUMBER 5
#define NEO_PIXEL_PIN_NUMBER 4



#endif // CONFIGURATION_H
