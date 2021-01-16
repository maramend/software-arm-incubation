/*
 *  app_main.cpp - The application's main.
 *
 *  Copyright (c) 2014 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include "app_out8.h"
#include "com_objs.h"
#include <sblib/eib.h>
#include <sblib/eib/sblib_default_objects.h>
#include <sblib/analog_pin.h>
#ifdef BUSFAIL
#   include <sblib/math.h>
#endif

#ifndef BI_STABLE
#   include "outputs.h"
#else
#   include "outputsBiStable.h"
#endif

#ifdef BUSFAIL
#    include "bus_voltage.h"
#    include "app_nov_settings.h"
#endif

#ifdef DEBUG
#    include <sblib/serial.h>
#endif


#ifdef BUSFAIL
    typedef struct ApplicationData {
        unsigned char relaisstate;         // current relays state
    } ApplicationData;

    NonVolatileSetting AppNovSetting(0xEA00, 0x100);  // flash-storage for application relevant parameters
    ApplicationData AppData;
#endif



ObjectValues& objectValues = *(ObjectValues*) (userRamData + UR_COM_OBJ_VALUE0);

/*
 * simple IO test
 */
void ioTest()
{

#ifdef IO_TEST

#   ifdef BI_STABLE
        const int relaySwitchms = ON_DELAY;  // see inc/outputsBiStable.h for more info
        // check, maybe we need to wait 4s, cause PIO_SDA is pulled-up to high on ARM-reset causing the relay coil to drain the bus.
        for (unsigned int i = 0; i < sizeof(outputPins)/sizeof(outputPins[0]); i++)
        {
            if (outputPins[i] == PIO_SDA)
            {
                delay(4000);
                break;
            }
        }
#   else
        const int relaySwitchms = PWM_TIMEOUT;  // see inc/outputs.h for more info
#   endif

    // setup LED's
    const int togglePausems = 250;
#   ifdef HAND_ACTUATION
        for (unsigned int i = 0; i < sizeof(handPins)/sizeof(handPins[0]); i++)
        {
            digitalWrite(handPins[i], 0);
            pinMode(handPins[i], OUTPUT);
        }
#   else
        const int disablePins[9] = {PIN_LT1, PIN_LT2, PIN_LT3, PIN_LT4, PIN_LT5, PIN_LT6, PIN_LT7, PIN_LT8, PIN_LT9};
        for (unsigned int i = 0; i < sizeof(disablePins)/sizeof(disablePins[0]); i++)
        {
            pinMode(disablePins[i], INPUT);
        }
#   endif /* HAND_ACTUATION */

    // all relay-pins off
    for (unsigned int i = 0; i < sizeof(outputPins)/sizeof(outputPins[0]); i++)
    {
        digitalWrite(outputPins[i], 0);
        pinMode(outputPins[i], OUTPUT);
    }

    // initialize relays for ioTest
    relays.begin(0x00, 0x00, NO_OF_CHANNELS);
    relays.updateOutputs();
    delay(relaySwitchms);
    relays.checkPWM();
    delay(togglePausems);

    // toggle every channel & hand actuation LED with a little delay
    for (unsigned int i = 0; i < relays.channelCount(); i++)
    {
        relays.updateChannel(i, 1);     // relay high/set
        relays.updateOutput(i);         // also sets hand actuation led
        delay(relaySwitchms);
        relays.checkPWM();

        delay(togglePausems);

        relays.updateChannel(i, 0);     // relay low/reset
        relays.updateOutput(i);         // also sets hand actuation led
        delay(relaySwitchms);
        relays.checkPWM();
        delay(togglePausems);
    }
#endif /* IO_TEST */
}


#ifdef DEBUG_SERIAL
void initSerial()
{
    serial.setRxPin(PIO2_7);
    serial.setTxPin(PIO2_8);
    serial.begin(9600);
    serial.println("out8 serial debug started");
}
#endif

/*
 * Initialize the application.
 */
void setup()
{
    // first set pin mode for Info & Run LED
    pinMode(PIN_INFO, OUTPUT); // this also sets pin to high/true
    pinMode(PIN_RUN, OUTPUT); // this also sets pin to high/true
    // then set values
    digitalWrite(PIN_INFO, 0);
#ifdef DEBUG
    digitalWrite(PIN_RUN, 1);
#endif

    // Configure the output pins
    for (unsigned int channel = 0; channel < sizeof(outputPins)/sizeof(outputPins[0]); channel++)
    {
        pinMode(outputPins[channel], OUTPUT);
        digitalWrite(outputPins[channel], 0);
    }

    bcu.begin(MANUFACTURER, DEVICETYPE, APPVERSION);

#ifdef BUSFAIL
    if (!AppNovSetting.RecallAppData((unsigned char*)&AppData, sizeof(ApplicationData))) // load custom app settings
    {
#   ifdef DEBUG
        digitalWrite(PIN_INFO, 1);
        delay(1000);
        digitalWrite(PIN_INFO, 0);
#   endif
    }
    // enable bus voltage monitoring
    vBus.enableBusVRefMonitoring(VBUS_AD_PIN, VBUS_AD_CHANNEL, VBUS_THRESHOLD_FAILED, VBUS_THRESHOLD_RETURN);
#endif

#ifdef DEBUG_SERIAL
    initSerial();
#endif

    ioTest();

#ifndef BI_STABLE
    //pinMode(PIN_IO11, OUTPUT);
    //digitalWrite(PIN_IO11, 1);
#endif

#ifndef BI_STABLE
#   ifdef ZERO_DETECT
       pinInterruptMode(PIO_SDA, INTERRUPT_EDGE_FALLING | INTERRUPT_ENABLED);
       enableInterrupt(EINT0_IRQn);
       pinEnableInterrupt(PIO_SDA);
#   endif
#endif

#ifdef BUSFAIL
    initApplication(AppData.relaisstate);
#else
    initApplication();
#endif
}

/*
 * The main processing loop.
 */
void loop()
{
    int objno;
    // Handle updated communication objects
    while ((objno = nextUpdatedObject()) >= 0)
    {
        objectUpdated(objno);
    }
    // check if any of the timeouts for an output has expire and react on them
    checkTimeouts();
#ifdef BUSFAIL
    // check the bus voltage, should be done before waitForInterrupt()
    vBus.checkPeriodic();
#endif
    // Sleep up to 1 millisecond if there is nothing to do
    if (bus.idle())
    {
        waitForInterrupt();
#ifdef DEBUG_SERIAL
       serial.print("mV:");
       serial.println(vBus.valuemV());
#endif
    }
}

void loop_noapp()
{
    waitForInterrupt();
};



void ResetDefaultApplicationData()
{
#ifdef BUSFAIL
    AppData.relaisstate = 0x00;
#endif
}
/*
 * will be called by the bus_voltage.h ISR which handles the ADC interrupt for the bus voltage.
 */
void BusVoltageFail()
{
#ifdef BUSFAIL
    pinMode(PIN_INFO, OUTPUT); // even in non DEBUG flash Info LED to display app data storing
    digitalWrite(PIN_INFO, 1);

    AppData.relaisstate = getRelaysState();
    // write application settings to flash
    if (AppNovSetting.StoreApplData((unsigned char*)&AppData, sizeof(ApplicationData)))
        digitalWrite(PIN_INFO, 0);
    else
        digitalWrite(PIN_INFO, 1);

    stopApplication();

#ifdef DEBUG
    digitalWrite(PIN_RUN, 0); // switch RUN-LED off, to save some power
#endif
#endif
}

/*
 * will be called by the bus_voltage.h ISR which handles the ADC interrupt for the bus voltage.
 */
void BusVoltageReturn()
{
#ifdef BUSFAIL
#ifdef DEBUG
    digitalWrite(PIN_RUN, 1); // switch RUN-LED ON
#endif
    //restore app settings
    if (!AppNovSetting.RecallAppData((unsigned char*)&AppData, sizeof(AppData))) // load custom app settings
    {
        // load default values
        ResetDefaultApplicationData();
#ifdef DEBUG
        digitalWrite(PIN_INFO, 1);
        delay(1000);
        digitalWrite(PIN_INFO, 0);
#endif
    }

#ifdef BUSFAIL
    initApplication(AppData.relaisstate);
#else
    initApplication();
#endif
#endif
}

int convertADmV(int valueAD)
 {
#ifdef BUSFAIL
    // good approximation between 17 & 30V for the 4TE-ARM controller
    if (valueAD > 2158)
        return 30000;
    else if (valueAD < 1546)
        return 0;
    else
        return 0.0198*sq(valueAD) - 52.104*valueAD + 50375;

    /*
     *  4TE ARM-Controller coefficients found with following measurements:
     *  ---------------------
     *  | Bus mV  ADC-Value |
     *  ---------------------
     *  | 30284   2158      |
     *  | 30006   2150      |
     *  | 29421   2132      |
     *  | 27397   2073      |
     *  | 26270   2035      |
     *  | 25210   1996      |
     *  | 24094   1953      |
     *  | 22924   1903      |
     *  | 21081   1811      |
     *  | 20003   1751      |
     *  | 18954   1683      |
     *  | 17987   1619      |
     *  | 17007   1546      |
     *  ---------------------
     *
    */
#endif
}


