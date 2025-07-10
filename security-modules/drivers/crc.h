/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file crc.h
 *
 * @brief CRC common data structure
 *
 */

#ifndef CRC_H
#define CRC_H

#include "safetylibs_types.h"

#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF(...)
#endif

#define SUCCESS             0 //@todo: changed enum to macro, was getting conflicts with ipcc lib
typedef enum {
    INVALID_PARAMETER = 0x111,
    CRC_ERROR = 0x222,
    SEQ_ERROR = 0x444,
    BOTH_CRC_SEQ_ERROR = 0x666
} error_type;

#endif
