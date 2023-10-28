/*
 *    _____ ________    __________  __  _______    ____  __  ___
 *   / ___// ____/ /   / ____/ __ )/ / / / ___/   / __ \/  |/  /
 *   \__ \/ __/ / /   / /_  / __  / / / /\__ \   / /_/ / /|_/ /
 *  ___/ / /___/ /___/ __/ / /_/ / /_/ /___/ /  / _, _/ /  / /
 * /____/_____/_____/_/   /_____/\____//____/  /_/ |_/_/  /_/
 *
 *  Original written for LPC922:
 *  Copyright (c) 2015-2017 Stefan Haller
 *  Copyright (c) 2013 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  Modified for LPC1115 ARM processor:
 *  Copyright (c) 2017 Oliver Stefan <o.stefan252@googlemail.com>
 *  Copyright (c) 2020 Stefan Haller
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef rm_app_h
#define rm_app_h

#include <sblib/eibBCU1.h>

extern BCU1 bcu;


/**
 * Den Zustand der Alarme bearbeiten.
 */
extern void process_alarm_stats();

/**
 * Com-Objekte bearbeiten deren Wert gesendet werden soll.
 */
extern void process_objs();

/**
 * Alle Applikations-Parameter zurücksetzen.
 */
extern void initApplication();

/**
 * Empfangene Kommunikationsobjekte verarbeiten
 */
void objectUpdated(int objno);

/**
 * Setup and start periodic timer @ref timer32_0
 * @param milliseconds Timer interval in milliseconds
 */
void setupPeriodicTimer(uint32_t milliseconds);

#endif /*rm_app_h*/
