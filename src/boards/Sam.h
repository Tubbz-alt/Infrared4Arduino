/*
Copyright (C) 2020 Bengt Martensson.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/.
 */

#pragma once

#include "InfraredTypes.h"

#define CURRENT_CLASS Sam

#define HAS_HARDWARE_PWM 1

//  Default PWM pin
#define PWM_PIN 3

#ifdef ISR
#undef ISR
#endif
#define ISR(f)  void interruptServiceRoutine()

class Sam : public Board {
public:

    Sam() {
    };

private:
    pin_t pwmPin;
    static constexpr bool invert = false;
    uint16_t maxValue;
    uint16_t onLength;
    bool timerTCC0;
    bool timerTCC1;
    bool timerTCC2;
    Tcc* TCCx;

    void TIMER_CONFIG_NORMAL();

    void TIMER_ENABLE_INTR();

    void TIMER_DISABLE_INTR();

    void TIMER_CONFIG_HZ(frequency_t hz __attribute__ ((unused)), dutycycle_t dutyCycle __attribute__ ((unused))) {
        TIMER_CONFIG_HZ(PWM_PIN, hz, dutyCycle);
    };

    void TIMER_CONFIG_HZ(pin_t pin, frequency_t hz, dutycycle_t dutyCycle);

    void TIMER_ENABLE_PWM() {
        setValue(onLength);
    };

    void TIMER_DISABLE_PWM() {
        setValue(0U);
    }

    static const unsigned int TIMER_PRESCALER_DIV = 64U;

    void setTimerFrequency(frequency_t hz);

    void setValue(uint16_t value);
};
