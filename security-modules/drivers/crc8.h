/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file crc8.h
 *
 * @brief CRC8 API and data structure
 *
 */

#ifndef CRC8_H
#define CRC8_H

#include "crc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * crc8_generate() -  Calculate CRC value
 * @param[in] buf:    Input data buffer pointer
 * @param[in] len:    length of the data buffer
 * @param[out] crc:   OUTPUT parameter that will contain the value of generated CRC result
 *
 * Used to generate a CRC value for specific data and length.
 *
 * @return:
 * @retval SUCCESS:   generate CRC successfully
 * @retval INVALID_PARAMETER:  parameter invalid
 */
error_type crc8_generate(const char *buf, sl_size_t len, sl_u8_t *crc);

/**
 * crc8_verify() -  Verify CRC value
 * @param[in] buf:    Input data buffer pointer
 * @param[in] len:    Length of the data buffer
 * @param[in] crc:    The CRC that to be verified
 *
 * Used to verify the CRC value for specific data and length.
 *
 * @return:
 * @retval SUCCESS:   verify CRC successfully
 * @retval INVALID_PARAMETER:  parameter invalid
 * @retval CRC_ERROR:  Recalculated CRC mismatch with received CRC
 */
error_type crc8_verify(const char *buf, sl_size_t len, sl_u8_t crc);


#ifdef __cplusplus
}
#endif

#endif
