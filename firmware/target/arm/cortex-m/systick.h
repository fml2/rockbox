/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * This file was automatically generated by headergen, DO NOT EDIT it.
 * headergen version: 3.0.0
 * cortex_m7 version: 1.0
 * cortex_m7 authors: Aidan MacDonald
 *
 * Copyright (C) 2015 by the authors
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
#ifndef __ARM_CORTEX_M_SYSTICK_H__
#define __ARM_CORTEX_M_SYSTICK_H__

#include "macro.h"

#define REG_SYSTICK_CSR                 cm_reg(SYSTICK_CSR)
#define CMA_SYSTICK_CSR                 (0xe000e000 + 0x10)
#define CMT_SYSTICK_CSR                 CMIO_32_RW
#define CMN_SYSTICK_CSR                 SYSTICK_CSR
#define BP_SYSTICK_CSR_COUNTFLAG        16
#define BM_SYSTICK_CSR_COUNTFLAG        0x10000
#define BF_SYSTICK_CSR_COUNTFLAG(v)     (((v) & 0x1) << 16)
#define BFM_SYSTICK_CSR_COUNTFLAG(v)    BM_SYSTICK_CSR_COUNTFLAG
#define BF_SYSTICK_CSR_COUNTFLAG_V(e)   BF_SYSTICK_CSR_COUNTFLAG(BV_SYSTICK_CSR_COUNTFLAG__##e)
#define BFM_SYSTICK_CSR_COUNTFLAG_V(v)  BM_SYSTICK_CSR_COUNTFLAG
#define BP_SYSTICK_CSR_CLKSOURCE        2
#define BM_SYSTICK_CSR_CLKSOURCE        0x4
#define BV_SYSTICK_CSR_CLKSOURCE__EXT   0x0
#define BV_SYSTICK_CSR_CLKSOURCE__CPU   0x1
#define BF_SYSTICK_CSR_CLKSOURCE(v)     (((v) & 0x1) << 2)
#define BFM_SYSTICK_CSR_CLKSOURCE(v)    BM_SYSTICK_CSR_CLKSOURCE
#define BF_SYSTICK_CSR_CLKSOURCE_V(e)   BF_SYSTICK_CSR_CLKSOURCE(BV_SYSTICK_CSR_CLKSOURCE__##e)
#define BFM_SYSTICK_CSR_CLKSOURCE_V(v)  BM_SYSTICK_CSR_CLKSOURCE
#define BP_SYSTICK_CSR_TICKINT          1
#define BM_SYSTICK_CSR_TICKINT          0x2
#define BF_SYSTICK_CSR_TICKINT(v)       (((v) & 0x1) << 1)
#define BFM_SYSTICK_CSR_TICKINT(v)      BM_SYSTICK_CSR_TICKINT
#define BF_SYSTICK_CSR_TICKINT_V(e)     BF_SYSTICK_CSR_TICKINT(BV_SYSTICK_CSR_TICKINT__##e)
#define BFM_SYSTICK_CSR_TICKINT_V(v)    BM_SYSTICK_CSR_TICKINT
#define BP_SYSTICK_CSR_ENABLE           0
#define BM_SYSTICK_CSR_ENABLE           0x1
#define BF_SYSTICK_CSR_ENABLE(v)        (((v) & 0x1) << 0)
#define BFM_SYSTICK_CSR_ENABLE(v)       BM_SYSTICK_CSR_ENABLE
#define BF_SYSTICK_CSR_ENABLE_V(e)      BF_SYSTICK_CSR_ENABLE(BV_SYSTICK_CSR_ENABLE__##e)
#define BFM_SYSTICK_CSR_ENABLE_V(v)     BM_SYSTICK_CSR_ENABLE

#define REG_SYSTICK_RVR             cm_reg(SYSTICK_RVR)
#define CMA_SYSTICK_RVR             (0xe000e000 + 0x14)
#define CMT_SYSTICK_RVR             CMIO_32_RW
#define CMN_SYSTICK_RVR             SYSTICK_RVR
#define BP_SYSTICK_RVR_VALUE        0
#define BM_SYSTICK_RVR_VALUE        0xffffff
#define BF_SYSTICK_RVR_VALUE(v)     (((v) & 0xffffff) << 0)
#define BFM_SYSTICK_RVR_VALUE(v)    BM_SYSTICK_RVR_VALUE
#define BF_SYSTICK_RVR_VALUE_V(e)   BF_SYSTICK_RVR_VALUE(BV_SYSTICK_RVR_VALUE__##e)
#define BFM_SYSTICK_RVR_VALUE_V(v)  BM_SYSTICK_RVR_VALUE

#define REG_SYSTICK_CVR             cm_reg(SYSTICK_CVR)
#define CMA_SYSTICK_CVR             (0xe000e000 + 0x18)
#define CMT_SYSTICK_CVR             CMIO_32_RW
#define CMN_SYSTICK_CVR             SYSTICK_CVR
#define BP_SYSTICK_CVR_VALUE        0
#define BM_SYSTICK_CVR_VALUE        0xffffff
#define BF_SYSTICK_CVR_VALUE(v)     (((v) & 0xffffff) << 0)
#define BFM_SYSTICK_CVR_VALUE(v)    BM_SYSTICK_CVR_VALUE
#define BF_SYSTICK_CVR_VALUE_V(e)   BF_SYSTICK_CVR_VALUE(BV_SYSTICK_CVR_VALUE__##e)
#define BFM_SYSTICK_CVR_VALUE_V(v)  BM_SYSTICK_CVR_VALUE

#define REG_SYSTICK_CALIB               cm_reg(SYSTICK_CALIB)
#define CMA_SYSTICK_CALIB               (0xe000e000 + 0x1c)
#define CMT_SYSTICK_CALIB               CMIO_32_RW
#define CMN_SYSTICK_CALIB               SYSTICK_CALIB
#define BP_SYSTICK_CALIB_TENMS          0
#define BM_SYSTICK_CALIB_TENMS          0xffffff
#define BF_SYSTICK_CALIB_TENMS(v)       (((v) & 0xffffff) << 0)
#define BFM_SYSTICK_CALIB_TENMS(v)      BM_SYSTICK_CALIB_TENMS
#define BF_SYSTICK_CALIB_TENMS_V(e)     BF_SYSTICK_CALIB_TENMS(BV_SYSTICK_CALIB_TENMS__##e)
#define BFM_SYSTICK_CALIB_TENMS_V(v)    BM_SYSTICK_CALIB_TENMS
#define BP_SYSTICK_CALIB_NOREF          31
#define BM_SYSTICK_CALIB_NOREF          0x80000000
#define BF_SYSTICK_CALIB_NOREF(v)       (((v) & 0x1) << 31)
#define BFM_SYSTICK_CALIB_NOREF(v)      BM_SYSTICK_CALIB_NOREF
#define BF_SYSTICK_CALIB_NOREF_V(e)     BF_SYSTICK_CALIB_NOREF(BV_SYSTICK_CALIB_NOREF__##e)
#define BFM_SYSTICK_CALIB_NOREF_V(v)    BM_SYSTICK_CALIB_NOREF
#define BP_SYSTICK_CALIB_SKEW           30
#define BM_SYSTICK_CALIB_SKEW           0x40000000
#define BF_SYSTICK_CALIB_SKEW(v)        (((v) & 0x1) << 30)
#define BFM_SYSTICK_CALIB_SKEW(v)       BM_SYSTICK_CALIB_SKEW
#define BF_SYSTICK_CALIB_SKEW_V(e)      BF_SYSTICK_CALIB_SKEW(BV_SYSTICK_CALIB_SKEW__##e)
#define BFM_SYSTICK_CALIB_SKEW_V(v)     BM_SYSTICK_CALIB_SKEW

#endif /* __ARM_CORTEX_M_SYSTICK_H__*/
