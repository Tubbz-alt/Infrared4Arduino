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

#include <Arduino.h>
#include "boards/boarddefs.h" // for HAS_HARDWARE_PWM

#ifdef HAS_HARDWARE_PWM

#include "IrSenderPwm.h"
#ifdef HAS_HARDWARE_PWM
#include "IrSenderPwmHard.h"
#else
#include "IrSenderPwmSoft.h"
#endif

IrSenderPwm *IrSenderPwm::instance = NULL;

IrSenderPwm::IrSenderPwm(pin_t outputPin) : IrSender(outputPin) {
}

IrSenderPwm *IrSenderPwm::newInstance(pin_t outputPin) {
    if (instance != NULL)
        return NULL;
    instance =
#ifdef HAS_HARDWARE_PWM
            IrSenderPwmHard::newInstance(outputPin);
#else
            new IrSenderPwmSoftDelay(outputPin);
#endif
    return instance;
}

IrSenderPwm *IrSenderPwm::getInstance(bool create, pin_t outputPin) {
    if (instance == NULL && create)
        instance = newInstance(outputPin);
    return instance;
}

#endif // HAS_HARDWARE_PWM
