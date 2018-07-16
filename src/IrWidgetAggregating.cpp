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

// Wait for a signal on pin ICP1 and store the captured time values in the array captureData
void IrWidgetAggregating::capture() {
#ifdef ARDUINO
    uint8_t tccr0b = TCCR0B;

    period = ((F_CPU) / (20000UL)) >> CAPTURE_PRESCALER_BITS; // the time of one period in CPU clocks (presently = 100)
    //uint16_t aggThreshold = (period * 10UL) / 8UL; // 65 us = (1/20kHz * 130%) might be a good starting point
    uint16_t aggThreshold = period * 2;

    setupTriggerAndTimers();

    uint16_t *pCapDat = captureData; // pointer to current item in captureData[]

    // disabling IRQs for a long time will disconnect the USB connection of the ATmega32U4, therefore we
    // defer the sbi() instruction until we got the starting edge and only stop the Timer0 in the meanwhile
    uint8_t sreg = SREG;

    timeouted = ! waitForFirstEdge();
    if (!timeouted) {
        TCCR0B &= ~(_BV(CS02) | _BV(CS01) | _BV(CS00)); // stop timer0 (disables timer IRQs)
        uint16_t val = CAT2(ICR, CAP_TIM);
        CAT3(OCR, CAP_TIM, CAP_TIM_OC) = val; // timeout based on previous trigger time

        noInterrupts(); // disable IRQs after the first edge

        // clear input capture and output compare flag bit
        CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));
        uint16_t prevVal = val;

        // process all following edges
        uint8_t calShiftM1 = 1;
        uint8_t calCount = 1 << (calShiftM1 + 1);
        ovlBitsDataType ovlCnt = 0;
        uint8_t aggCount = 0;
        uint32_t pulseTime = 0; // was: aggVal;
        while (pCapDat <= &captureData[bufferSize - sampleSize]) { // sampleSize (= 2) values are stored in each loop
            // wait for edge or overflow (output compare match)
            uint8_t tifr = waitForTimer(); // cache the result of reading TIFR1 (masked with ICF1 and OCF1A)
            uint16_t val = CAT2(ICR, CAP_TIM);
            CAT3(OCR, CAP_TIM, CAP_TIM_OC) = val; // timeout based on previous trigger time

            if (overflowBit(tifr)) {
                if (ovlCnt >= endingTimeout) {
                    store(pCapDat, pulseTime, (uint32_t) endingTimeout << 16);
                    break; // Normal exit point
                    // maximum value reached, treat this as timeout and abort capture
                }
                ovlCnt++;
                // clear input capture and output compare flag bit
                CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));
            } else {

                // clear input capture and output compare flag bit
                CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM)) | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC));

                uint32_t diffVal = (val - prevVal) | ((uint32_t) ovlCnt << 16);
                ovlCnt = 0;
                prevVal = val;

                if (diffVal < aggThreshold) {
                    pulseTime += diffVal;

                    if (calCount) {
                        aggCount++; // only used to calculate the period
                        // do a calibration on every aggCount which is a power of two because then dividing by calShiftM1
                        // (shiftcount - 1) can simply be performed by shifting right
                        if (aggCount == calCount) {
                            aggThreshold = pulseTime >> calShiftM1;
                            calShiftM1++;
                            calCount = calCount << 1; // this will automatically terminate calibrating when calCount is 128 because then (128 << 1) & 0xff = 0
                        }
                    }
                } else {
                    store(pCapDat, pulseTime, diffVal);
                    pulseTime = 0;
                    calCount = 0; // avoid further period calculation and calibration
                }
            }
        }
    }

    TCCR0B = tccr0b; // re-enable Timer0
    SREG = sreg; // enable IRQs

    captureCount = pCapDat - captureData;
    period = aggThreshold / 2;

    uint32_t mediumPeriod = timerValueToNanoSeconds(period);
    frequency = (frequency_t) (1000000000L / mediumPeriod);
#endif // ARDUINO
}

bool IrWidgetAggregating::waitForFirstEdge() {
#ifdef ARDUINO
    uint32_t timeForBeginTimeout = millis() + beginningTimeout;
    while (!(CAT2(TIFR, CAP_TIM) & (_BV(CAT2(ICF, CAP_TIM))))) {
        if (millis() >= timeForBeginTimeout)
            return false;
    }
#endif
    return true;
}

void IrWidgetAggregating::setupTriggerAndTimers() {
#ifdef ARDUINO
    uint8_t icesn_val = _BV(CAT2(ICES, CAP_TIM));
    uint8_t tccrnb = CAT3(TCCR, CAP_TIM, B);
    if (invertingSensor)
        tccrnb &= ~icesn_val; // trigger on falling edge
    else
        tccrnb |= icesn_val; // trigger on rising edge

    CAT3(TCCR, CAP_TIM, B) = tccrnb;

    OCR1A = CAT2(TCNT, CAP_TIM) - 1;
    CAT2(TIFR, CAP_TIM) = _BV(CAT2(ICF, CAP_TIM))
            | _BV(CAT3(OCF, CAP_TIM, CAP_TIM_OC)) | _BV(CAT2(TOV, CAP_TIM)); // clear all timer flags
#endif
}

void IrWidgetAggregating::store(uint16_t* &pCapDat, uint32_t pulseTime, uint32_t gapTime) {
    //if (pulseTime > 0UL) {
        // TODO check is to value is small enough to be stored
        *pCapDat = packTimeVal(pulseTime); // store the pulse length
        pCapDat++;
        *pCapDat = packTimeVal(gapTime);
        pCapDat++;
    //}
}
