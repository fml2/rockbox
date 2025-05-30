/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2007 by Dave Chapman
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
#ifndef SYSTEM_TARGET_H
#define SYSTEM_TARGET_H

#include "system-arm.h"
#include "mmu-arm.h"
#include "cpucache-arm.h"

#define CPUFREQ_SLEEP      32768
#define CPUFREQ_MAX     (1843200 * 4 * 26 / 1) /* 191692800 Hz */
#define CPUFREQ_DEFAULT (CPUFREQ_MAX/4)        /*  47923200 Hz */
#define CPUFREQ_NORMAL  (CPUFREQ_MAX/4)

#define STORAGE_WANTS_ALIGN

#define inl(a)    (*(volatile unsigned long *) (a))
#define outl(a,b) (*(volatile unsigned long *) (b) = (a))
#define inb(a)    (*(volatile unsigned char *) (a))
#define outb(a,b) (*(volatile unsigned char *) (b) = (a))
#define inw(a)    (*(volatile unsigned short*) (a))
#define outw(a,b) (*(volatile unsigned short*) (b) = (a))

static inline void udelay(unsigned usecs)
{
    unsigned stop = USEC_TIMER + usecs;
    while (TIME_BEFORE(USEC_TIMER, stop));
}

#endif /* SYSTEM_TARGET_H */
