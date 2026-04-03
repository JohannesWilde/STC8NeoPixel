#ifndef PINOUT_H
#define PINOUT_H


#define MAKE_PIN_NAME_(port, pin) P##port##_##pin
#define MAKE_PIN_NAME(port, pin) MAKE_PIN_NAME_(port, pin)


#endif // CONFIGURATION_H
