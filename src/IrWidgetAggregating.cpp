// Copyright (c) 2012 Michael Dreher  <michael(at)5dot1.de>
// this code may be distributed under the terms of the General Public License V2 (GPL V2)

// This is a slight reorganization of the original code, by Bengt Martensson.

#include "IrWidgetAggregating.h"

IrWidgetAggregating::IrWidgetAggregating(size_t captureLength,
        bool pullup,
        int16_t markExcess,
        milliseconds_t beginningTimeout,
        milliseconds_t endingTimeout)
: IrWidget(captureLength, pullup, markExcess, beginningTimeout, endingTimeout) {
}

IrWidgetAggregating *IrWidgetAggregating::instance = NULL;

IrWidgetAggregating *IrWidgetAggregating::newIrWidgetAggregating(size_t captureLength,
            bool pullup,
            int16_t markExcess,
            milliseconds_t beginningTimeout,
            milliseconds_t endingTimeout) {
    if (instance != NULL)
        return NULL;
    instance = new IrWidgetAggregating(captureLength, pullup, markExcess,
            beginningTimeout, endingTimeout);
    return instance;
}

void IrWidgetAggregating::deleteInstance() {
    delete instance;
    instance = NULL;
}

// Wait for a signal on pin ICP1 and store the captured time values in the array 'captureData'
void IrWidgetAggregating::capture() {
    //TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // stop timer0 (disables timer IRQs)

    //uint16_t aggThreshold = (period * 10UL) / 8UL; // 65 us = (1/20kHz * 130%) might be a good starting point
    CAT3(TCCR, CAP_TIM, B) = computeTccrnb();
    OCR1A = CAT2(TCNT, CAP_TIM) - 1;
    // clear all timer flags
    CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC)) | _BV(CAT2(TOV, CAP_TIM));

    // disabling IRQs for a long time will disconnect the USB connection of the ATmega32U4, therefore we
    // defer the sbi() instruction until we got the starting edge and only stop the Timer0 in the meanwhile
    saveTimerIrq();
    debugPinClear();
    captureCount = 0U;

    /////////////////////////////////////////
    // wait for first edge
    timeouted = !waitForFirstEdge();
    if (timeouted) {
        endCapture();
        return;
    }

    TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // stop timer0 (disables timer IRQs)
    debugPinToggle();
    uint16_t val = CAT2(ICR, CAP_TIM);
    CAT3(OCR, CAP_TIM, CAP_TIM_OC) = val; // timeout based on previous trigger time

    noInterrupts(); // disable IRQs after the first edge

    // clear input capture and output compare flag bit
    CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));
    uint16_t prevVal = val;
    aggThreshold = period * 2U;
    uint32_t aggVal = 0;
    unsigned int ovlCnt = 0;
    uint8_t calShiftM1 = 1;
    uint8_t calCount = 1 << (calShiftM1 + 1);
    uint8_t aggCount = 0;

    /////////////////////////////////////////
    // wait for all following edges
    while (captureCount <= bufferSize - numberSavesPerPeriod) {
        debugPinToggle();
        // wait for edge or overflow (output compare match)
        uint8_t tifr = waitForEdgeOrOverflow(); // cache the result of reading TIFR1 (masked with ICF1 and OCF1A)
        val = CAT2(ICR, CAP_TIM);
        CAT3(OCR, CAP_TIM, CAP_TIM_OC) = val; // timeout based on previous trigger time

        if (isOverflow(tifr)) {
            // check for overflow bit
            if (ovlCnt >= endingTimeout) {
                // TODO: handle this check together with the check for the pulse length (if packTimeValNormal can handle the value)
                if (aggVal > 0)
                    storePulse(aggVal, (uint32_t) endingTimeout << 16);
                break; // maximum value reached, treat this as timeout and abort capture
            }
            ovlCnt++;
            // clear input capture and output compare flag bit
            CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));
            continue;
        }

        // clear input capture and output compare flag bit
        CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));

        uint32_t diffVal = ((val - prevVal) & 0xffff) | ((uint32_t) ovlCnt << 16UL);
        ovlCnt = 0U;
        prevVal = val;

        if (diffVal < aggThreshold) {
            aggVal += diffVal;

            // calculate the carrier frequency only within the first burst (often a preamble)
            if (calCount > 0U) {
                aggCount++; // only used to calculate the period
                // do a calibration on every aggCount which is a power of two because then dividing by calShiftM1
                // (shiftcount - 1) can simply be performed by shifting right
                if (aggCount == calCount) {
                    aggThreshold = aggVal >> calShiftM1;
                    calShiftM1++;
                    calCount = calCount << 1U; // this will automatically terminate calibrating when calCount is 128 because then (128 << 1) & 0xff = 0
                }
            }
        } else {
            storePulse(aggVal, diffVal);
            aggVal = 0;
            calCount = 0; // avoid further period calculation and calibration
        }
    }
    endCapture();
}

void IrWidgetAggregating::endCapture() {
    debugPinClear();
    restoreTimerIrq();
    if (aggThreshold == period * 2U) {
        frequency = 0U;
    } else {
        uint32_t mediumPeriod = timerValueToNanoSeconds(aggThreshold / 2U);
        frequency = (frequency_t) (1000000000UL / mediumPeriod);
    }
}

bool IrWidgetAggregating::waitForFirstEdge() {
    uint32_t timeForBeginTimeout = millis() + beginningTimeout;
    while (!(CAT2(TIFR, CAP_TIM) & (_BV(CAT2(ICF, CAP_TIM))))) {
        if (millis() >= timeForBeginTimeout)
            return false;
    }
    return true;
}

// wait for edge or overflow (output compare match)
uint8_t IrWidgetAggregating::waitForEdgeOrOverflow() {
    uint8_t tifr; // cache the result of reading TIFR1 (masked with ICF1 and OCF1A)
    do {
        // Get the input capture and output compare flags.
        tifr = CAT2(TIFR, CAP_TIM) & (_BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC)));
    } while (!tifr);

    debugPinToggle();

    return tifr;
}

void IrWidgetAggregating::saveTimerIrq() {
    tccr0b = TCCR0B;
    sreg = SREG;
}

void IrWidgetAggregating::restoreTimerIrq() {
    TCCR0B = tccr0b; // re-enable Timer0
    SREG = sreg; // enable IRQs
}

uint8_t IrWidgetAggregating::computeTccrnb() {
    uint8_t icesn_val = _BV(CAT2(ICES, CAP_TIM));
    uint8_t tccrnb = CAT3(TCCR, CAP_TIM, B);
    if (invertingSensor)
        tccrnb &= ~icesn_val; // trigger on falling edge
    else
        tccrnb |= icesn_val; // trigger on rising edge
    return tccrnb;
}

void IrWidgetAggregating::storePulse(uint32_t onTime, uint32_t offTime) {
    // TODO check is to value is small enough to be stored
    captureData[captureCount] = packTimeVal(onTime);
    captureCount++;
    captureData[captureCount] = packTimeVal(offTime);
    captureCount++;
}

bool IrWidgetAggregating::isOverflow(uint8_t tifr) {
    return tifr & _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));
}
