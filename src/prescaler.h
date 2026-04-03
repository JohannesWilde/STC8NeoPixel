#ifndef PRESCALER_H
#define PRESCALER_H

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief updatePrescaler Update prescaler value.
 * @return true on underrun, false otherwise.
 *
 * The priodicity of the prescaler is (initialValue + 1).
 *
 * Typical use case:
 *
 *     #define PRESCALER_INIT (10 - 1)
 *     uint8_t prescaler = PRESCALER_INIT; // or 0
 *     while (true)
 *     {
 *         if (updatePrescaler(&prescaler, PRESCALER_INIT))
 *         {
 *              // do stuff
 *         }
 *         sleep();
 *     }
 *
 */
inline bool updatePrescaler(uint8_t * value, uint8_t const initialValue)
{
    bool const valueUnderrun = (0 == (*value));
    if (valueUnderrun)
    {
        *value = initialValue;
    }
    else
    {
        --(*value);
    }
    return valueUnderrun;
}


#endif // PRESCALER_H
