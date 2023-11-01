/*
 *  Copyright (c) 2023 Thomas Dallmair <dev@thomas-dallmair.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include "smoke_detector_group_objects.h"

SmokeDetectorGroupObjects::SmokeDetectorGroupObjects(ComObjects *comObjects)
    : comObjects(comObjects)
{
}

/**
 * Send any group object onto the bus with the value currently stored.
 *
 * @param groupObject The group object to send
 */
void SmokeDetectorGroupObjects::send(GroupObject groupObject) const
{
    // The communication objects already have the correct value, but there is no method to
    // just mark them for sending. So, just write the same value again.
    comObjects->objectWrite(groupObject, comObjects->objectRead(groupObject));
}

/**
 * Set any group object to the respective value. Does not send the change onto the bus.
 *
 * @param groupObject The group object to update the stored value of
 * @param value       New value of the group object
 */
void SmokeDetectorGroupObjects::setValue(GroupObject groupObject, uint32_t value) const
{
    comObjects->objectSetValue(groupObject, value);
}

/**
 * Set any group object to the respective value, and if the value differs, send the change onto the bus.
 *
 * @param groupObject The group object to update the stored value of
 * @param value       New value of the group object
 */
void SmokeDetectorGroupObjects::writeIfChanged(GroupObject groupObject, uint32_t value) const
{
    auto oldValue = comObjects->objectRead(groupObject);
    if (oldValue != value)
    {
        comObjects->objectWrite(groupObject, value);
    }
}
