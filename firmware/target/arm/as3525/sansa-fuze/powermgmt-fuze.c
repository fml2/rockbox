/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright © 2008 Rafaël Carré
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "config.h"

/* The battery manufacturer's website shows discharge curves down to 3.0V,
   so 'disksafe' and 'shutoff' levels of 3.4V and 3.3V should be safe.
 */
unsigned short battery_level_disksafe = 3400;

unsigned short battery_level_shutoff = 3300;

/* voltages (millivolt) of 0%, 10%, ... 100% when charging disabled */
unsigned short percent_to_volt_discharge[11] =
{
    3300, 3676, 3715, 3749, 3783, 3812, 3850, 3918, 3982, 4065, 4148

};

#if CONFIG_CHARGING
/* voltages (millivolt) of 0%, 10%, ... 100% when charging enabled */
unsigned short percent_to_volt_charge[11] =
{
    3364, 3816, 3879, 3903, 3934, 3983, 4051, 4119, 4188, 4188, 4188
};
#endif /* CONFIG_CHARGING */

