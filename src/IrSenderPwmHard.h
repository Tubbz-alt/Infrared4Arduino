/*
Copyright (C) 2015 Bengt Martensson.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see http://www.gnu.org/licenses/.
*/

#pragma once

#include <Arduino.h>
#include "IrSenderPwm.h"
#include "boards/boarddefs.h" // For HAS_HARDWARE_PWM

#ifndef HAS_HARDWARE_PWM
#error Current board does not support hardware PWM and thus not the class IrSendPwm. Consider using IrSenderSoftPwm or IrSenderSpin instead.
#endif

/**
 * Sending function using timer PWM. Due to the nature of the timers, this is a Highlander,
 * ("There can only be one"), so the class is a singleton class, with private constructor,
 * a factory method that enforces the "highlander property".
 */
/*final*/
class IrSenderPwmHard : public IrSenderPwm {
public:
    IrSenderPwmHard(pin_t outputPin = SEND_PIN);
    virtual ~IrSenderPwmHard();

private:
    static IrSenderPwmHard *instance;
    void enable(frequency_t frequency);
    //void sendSpace(microseconds_t time);
    void sendMark(microseconds_t time);

    static void barfForInvalidPin(pin_t outputPin) { if (outputPin != SEND_PIN) {/*error("Silly pin")*/};};

public:
    /**
     * Returns a pointer to the instance, or NULL if not initialized.
     * If argument true, in the latter case creates a new instance and returns it.
     */
    static IrSenderPwmHard *getInstance(bool create = false, pin_t ouputPin = SEND_PIN);

    /**
     *  Creates a new instance (if not existing) and returns it.
     *  Returns NULL if an instance already exists.
     */
    static IrSenderPwmHard *newInstance(pin_t ouputPin = SEND_PIN);

    static void deleteInstance() {
        delete instance;
        instance = NULL;
    }
};
