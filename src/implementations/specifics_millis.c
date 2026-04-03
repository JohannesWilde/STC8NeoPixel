#include "../specifics.h"

#include "../8051_helpers.h"


extern volatile Timestamp milliseconds_;

Timestamp millis()
{
    Timestamp copy = 0;
    noInterrupts();
    copy = milliseconds_;
    interrupts();
    return copy;
}
