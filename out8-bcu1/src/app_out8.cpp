/*
 *  app_out8.cpp - The application for the 8 channel output acting as a Jung 2118
 *
 *  Copyright (c) 2014 Martin Gl�  ck <martin@mangari.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include "app_out8.h"
#include "com_objs.h"
#include <sblib/timeout.h>
#include <sblib/eib/com_objects.h>
#include <sblib/eib/user_memory.h>
#include <sblib/timer.h>

#define PWM_TIMEOUT 50
#define PWM_PERIOD     857
#define PWM_DUTY_33    (588)

// state of the application
static unsigned int  invertOutputs;
static unsigned int  outputs;
static unsigned int  oldOutputs;
static unsigned int  objectStates;
static unsigned char specialState[4];
static unsigned int  blockedStates;

static Timeout channel_timeout[8];
static Timeout pwm_timeout;

// internal functions
static void          _switchObjects(void);
static void          _switchChannels(void);
static void          _sendFeedbackObjects(void);
static unsigned int  _handle_logic_function(int objno, unsigned int value);
static unsigned int  _handle_timed_functions(int objno, unsigned int value);

static const unsigned int  _delayBases[] =
{     1 * MS2TICKS(130)
,     2 * MS2TICKS(130)
,     4 * MS2TICKS(130)
,     8 * MS2TICKS(130)
,    16 * MS2TICKS(130)
,    32 * MS2TICKS(130)
,    64 * MS2TICKS(130)
,   128 * MS2TICKS(130)
,   256 * MS2TICKS(130)
,   512 * MS2TICKS(130)
,  1024 * MS2TICKS(130)
,  2048 * MS2TICKS(130)
,  4096 * MS2TICKS(130)
,  8192 * MS2TICKS(130)
, 16384 * MS2TICKS(130)
, 32768 * MS2TICKS(130)
};

static unsigned int _handle_logic_function(int objno, unsigned int value)
{
	// TODO check function
    unsigned int specialFunc;    // special function number (0: no sf)
    unsigned int specialFuncTyp; // special function type
    unsigned int logicFuncTyp;   // type of logic function ( 1: or, 2: and)
    unsigned int logicState;     // state of logic function
             int sfOut;          // output belonging to sf
    unsigned int sfMask;         // special function bit mask (1 of 4)
    unsigned int saved_out;      // value of the outputs before the logic functions
    unsigned int savedObjState;  // value of the objectStates before the change;

    savedObjState = objectStates;
    if (objno >= 8)
    {
    	specialState[objno - 8] = value;
        if (value & 0x01) // for 2 bit values save the lower bit
        {
            objectStates |=  (1 << objno);
        } else
        {
            objectStates &= ~(1 << objno);
        }
        /* if a special function is addressed (and changed in most cases),
         * then the "real" object belonging to that sf. has to be evaluated again
         * taking into account the changed logic and blocking states.
         */
        /* determine the output belonging to that sf */
        objno -= 8;
        sfOut  = userEeprom[APP_SPECIAL_CONNECT + (objno >> 1)] >> ((objno & 1) * 4) & 0x0F;
        /* get associated object no. and state of that object*/
        if (sfOut)
        {
            if (sfOut > 8) return 0;
            objno =  sfOut - 1;
            // get the current value of the object from the internal state
            // this will reflect the state after the timeout handling!
            value = (objectStates >> objno) & 0x01;
        }
        else return 0;
    }

    /** logic function */
    /* check if we have a special function for this object */
    logicFuncTyp = 0;
    saved_out    = outputs;
    outputs      = objectStates;
    for (specialFunc = 0; specialFunc < 4; specialFunc++)
    {
        sfMask = 1 << specialFunc;
        sfOut  = userEeprom[APP_SPECIAL_FUNC_OBJ_1_2 + (specialFunc >> 1)] >> ((specialFunc & 1) * 4) & 0x0F;
        if (sfOut == (objno + 1))
        {
            /* we have a special function, see which type it is */
            specialFuncTyp = userEeprom[APP_SPECIAL_FUNC_MODE] >> (specialFunc * 2) & 0x03;
            /* get the logic state for the special function object */
            logicState = ((objectStates >> specialFunc) >> 8) & 0x01;
            switch (specialFuncTyp)
            {
            case 0 : // logic function (OR/AND/AND with recirculation
                logicFuncTyp = userEeprom[APP_SPECIAL_LOGIC_MODE] >> (specialFunc * 2) & 0x03;
                switch (logicFuncTyp)
                {
                case 1 : // or
                    value |= logicState;
                    break;
                case 2 : // and
                    value &= logicState;
                    break;
                case 3 : // and with recirculation
                    value &= logicState;
                    if ( (savedObjState & (1 << (specialFunc + 8))) && !logicState)
                        objectUpdate(objno, value);
                    break;
                }
                outputs = (outputs & ~(1 << objno)) | (value & 0x01) << objno;
                break; // logicFuncTyp

            case 1: /* blocking function */
                if (((objectStates >> 8) ^ userEeprom[APP_SPECIAL_POLARITY]) & sfMask)
                {   /* start blocking */
                    if (blockedStates & sfMask)
                    {
                        return 0; // we are already blocked, nothing to do
                    }
                    blockedStates |= sfMask;
                    value = (userEeprom[APP_SPECIAL_FUNCTION1 + (specialFunc>>1)])>>((specialFunc&1)*4)&0x03;
                    switch (value)
                    {
                    case 1 : // disable the output
                        outputs &= ~(1 << objno);
                        break;
                    case 2 : // enable the output
                        outputs |= (1 << objno);
                    case 0 : // no action at the beginning of the blocking
                    default:
                    	break;
                    }
                    /*
                    objectValues.outputs[objno] = (outputs >> objno) & 0x1 ;
                    objectWrite(objno, (unsigned int) objectValues.outputs[objno]);
                    _switchObjects();
                    return 0;
                    */
                } else {
                    /* end blocking */
                    if (blockedStates & sfMask )
                    {   // we have to unblock
                        blockedStates &= ~sfMask;
                        /* action at end of blocking, 0: nothing, 1: off, 2: on */
                        value = (userEeprom[APP_SPECIAL_FUNCTION1 + (specialFunc>>1)])>>((specialFunc&1)*4+2)&0x03;
                        switch (value)
                        {
                        case 1 : // disable the output
                            outputs &= ~(1 << objno);
                            break;
                        case 2 : // enable the output
                            outputs |= (1 << objno);
                        case 0 : // no action at the end of the blocking
                        default:
                        	break;
                        }
                    }
                }

            case 2: // forced value
            	if (specialState [specialFunc] & 0b10)
            	{
            		/* the value of the special object has a higher priority than the the channel value */
            		value   = specialState [specialFunc] & 0b01;
                    outputs = (outputs & ~(1 << objno)) | (value & 0x01) << objno;
            	}
            	break;
            }
        }
    }
    return (outputs != saved_out ? 1 : 0);
}

static unsigned int _handle_timed_functions(int objno, unsigned int value)
{
	unsigned int needToSwitch = 0;
    unsigned int mask             = 1 << objno;
    // Set some variables to make next commands better readable_handle_timed_functions
    unsigned int timerCfg       = userEeprom[APP_DELAY_ACTIVE] & mask;
    unsigned int timerOffFactor = userEeprom[APP_DELAY_FACTOR_OFF + objno];
    unsigned int timerOnFactor  = userEeprom[APP_DELAY_FACTOR_ON  + objno];

    // Get configured delay base
    unsigned int delayBaseIdx = userEeprom[APP_DELAY_BASE + ((objno + 1) >> 1)];
    unsigned int delayBase;

    if ((objno & 0x01) == 0x00)
        delayBaseIdx >>= 4;
    delayBaseIdx      &= 0x0F;
    delayBase          = _delayBases[delayBaseIdx];

    if (!timerCfg) // this is the on/off delay mode
    {
        // Check if a delay is configured for falling edge
        if ( (objectStates & mask) && !value && timerOffFactor)
        {
        	channel_timeout[objno].start(delayBase * timerOffFactor);
        }
        // Check if a delay is configured for raising edge
        if (!(objectStates & mask) && value && timerOnFactor)
        {
        	channel_timeout[objno].start(delayBase * timerOnFactor);
        }
    } else // this is the timed function mode
    {
        if (!timerOnFactor) // no delay for  on -> switch on the output
        {
            // Check for a timer function without delay factor for raising edge
            if ( !(objectStates & mask) && value)
            {
            	objectStates |= mask;
                needToSwitch++;
            	channel_timeout[objno].start (delayBase * timerOffFactor);
            }
        }
        else
        {
            // Check for a timer function with delay factor for on
            if ( !(objectStates & mask) && value)
            {
            	channel_timeout[objno].start (delayBase * timerOnFactor);
            }

            // Check for delay factor for off
            if ((objectStates & mask) && value)
            {
            	// once the output is ON start the OFF delay
            	channel_timeout[objno].start(delayBase * timerOffFactor);
            }
        }
        // check how to handle off telegram while in timer modus
        if ((objectStates & mask) && !value)
        {
            // only switch off if on APP_DELAY_ACTION the value is equal zero
            if (! (userEeprom[APP_DELAY_ACTION] & mask))
            {
                if (channel_timeout[objno].started ())
                {
                	channel_timeout[objno].stop ();
                	objectStates &= ~mask;
                    needToSwitch++;
                }
            }
        }
    }
    return needToSwitch;
}

void objectUpdated(int objno)
{
    unsigned int value;
    unsigned int changed = 0;

    // get value of object (0=off, 1=on)
    value = objectRead(objno);

    // check if we have a delayed action for this object, only Outputs
    if(objno < COMOBJ_SPECIAL1)
    {
    	changed += _handle_timed_functions(objno, value);
		if(channel_timeout[objno].stopped())
		{
			if (value == 0x01)
			{
				objectStates |=   1 << objno;
			}
			else
			{
				objectStates &= ~(1 << objno);
			}
			changed += 1;
		}
    }

    // handle the logic functions for this channel
    changed += _handle_logic_function (objno, value);

    if (changed)
        _switchObjects();
}

void checkTimeouts(void)
{
    unsigned int needToSwitch = 0;
    unsigned int objno;

#ifdef HAND
    // manual Operation is enabled
    // check one button every app_loop() passing through
    if (handActuationCounter <= OBJ_OUT7  &&  checkHandActuation(handActuationCounter))
    {
        needToSwitch=1;
    }
    handActuationCounter++;     //count to 255 for debounce buttons, ca. 30ms
#endif

    // check if we can enable PWM
    if(pwm_timeout.expired())
    {
    	timer16_0.match(MAT2, PWM_DUTY_33);
        //digitalWrite(PIO1_4, 0);
    }

    for (objno = 0; objno < COMOBJ_SPECIAL1; ++objno) {
        if(channel_timeout[objno].expired ())
        {
            unsigned int obj_value;
            objectStates            ^= 1 << objno;
            obj_value                = (objectStates >> objno) & 0x1;
            _handle_timed_functions(objno, obj_value);
            _handle_logic_function(objno, obj_value);
            needToSwitch++;
        }
    }

    if(needToSwitch)  _switchObjects();
}

static void _sendFeedbackObjects(void)
{
    unsigned int changed = outputs ^ oldOutputs;

    if(changed)
    {   // at least one output has changed, requires sending a feedback objects
        unsigned int value = outputs ^ userEeprom[APP_REPORT_BACK_INVERT];
        unsigned int i;
        unsigned int mask = 0x01;
        for (i = 0; i < 8; i++)
        {
            if(changed & mask)
            {   // this output pin has changed. set the object value
            	objectValues.feedback[i] = (value & mask) ? 1 : 0;
                // and request the transmission of the feedback object
                objectWrite(COMOBJ_FEEDBACK1 + i, (unsigned int) objectValues.feedback[i]);
            }
            mask <<= 1;
        }
   }
}

static void _switchObjects(void)
{
    _sendFeedbackObjects();
    _switchChannels();
}

static void _switchChannels(void)
{
	int i;
	int mask = 0x01;
	int value = outputs ^ invertOutputs;

    if((outputs ^ oldOutputs) & outputs)
    { // at least one port will be switched from 0 to 1 -> need to disable the PWM
        timer16_0.match(MAT2, 0);// disable the PWM
    	pwm_timeout.start(PWM_TIMEOUT);
        //digitalWrite(PIO1_4, 1);
    }
    for(i = 0; i < NO_OF_CHANNELS; i++, mask <<= 1)
    {
    	digitalWrite(outputPins[i], value & mask);
    }
    oldOutputs = outputs;
}

void initApplication(void)
{
    unsigned int i;
    unsigned int initialChannelAction;

    pinMode(PIO3_2, OUTPUT_MATCH);  // configure digital pin PIO3_2(PWM) to match MAT2 of timer16 #0
    //pinMode(PIO1_4, OUTPUT);

    timer16_0.begin();

    timer16_0.prescaler((SystemCoreClock / 10000000) - 1);
    timer16_0.matchMode(MAT2, SET);  // set the output of PIO3_2 to 1 when the timer matches MAT1
    timer16_0.match(MAT2, 0);        // match MAT1 when the timer reaches this value
    timer16_0.pwmEnable(MAT2);       // enable PWM for match channel MAT1

    // Reset the timer when the timer matches MAT3
    timer16_0.matchMode(MAT3, RESET);
    timer16_0.match(MAT3, PWM_PERIOD);     // match MAT3 ato create 14lHz
    timer16_0.start();
    pwm_timeout.start(PWM_TIMEOUT); // start the timer to switch back to a PWM operation
    //digitalWrite(PIO1_4, 1);

    outputs              = userEeprom[APP_PIN_STATE_MEMORY]; // load from eeprom
    invertOutputs        = userEeprom[APP_CLOSER_MODE];
    initialChannelAction = userEeprom[APP_RESTORE_AFTER_PL_HI] << 8
                                   | userEeprom[APP_RESTORE_AFTER_PL_LO];
    for (i=0; i < 8; i++) {
        unsigned int temp = (initialChannelAction >> (i * 2)) & 0x03;
        if      (temp == 0x01)
        {   // open contact
            outputs &= ~(1 << i);
        }
        else if (temp == 0x02)
        {   // close contact
            outputs |=  (1 << i);
        }
        // Send status of every object to bus on power on
    	objectValues.outputs[i]  = (outputs >> i) & 0x1;
    	objectValues.feedback[i] = objectValues.outputs[i];
        // and request the transmission of the feedback object
    	// TODO check INVERTED REPORTING
        objectWrite(COMOBJ_FEEDBACK1 + i, (unsigned int) objectValues.outputs[i]);
    }
    objectStates         = outputs;
    oldOutputs           = ~outputs;                 // force a "toggle" on each output
    _switchChannels();
}
