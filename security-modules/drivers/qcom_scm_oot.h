/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015, 2018-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QCOM_SCM_OOT_H__
#define __QCOM_SCM_OOT_H__

/* qtee_shmbridge */
#define QCOM_SCM_MEMP_SHM_BRDIGE_CREATE         0x1e

/* tz_log */
#define QCOM_SCM_INFO_GET_FEAT_VERSION_CMD      0x03
#define QCOM_SCM_SVC_QSEELOG                    0x01
#define QCOM_SCM_QSEELOG_REGISTER               0x06
#define QCOM_SCM_QUERY_ENCR_LOG_FEAT_ID         0x0b
#define QCOM_SCM_REQUEST_ENCR_LOG_ID            0x0c

/* smcinvoke */
#define QCOM_SCM_SVC_SMCINVOKE			0x06
#define QCOM_SCM_SMCINVOKE_INVOKE_LEGACY	0x00
#define QCOM_SCM_SMCINVOKE_INVOKE		0x02
#define QCOM_SCM_SMCINVOKE_CB_RSP		0x01

/* Feature IDs for QCOM_SCM_INFO_GET_FEAT_VERSION */
#define QCOM_SCM_FEAT_LOG_ID                    0x0a

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
                           (((a) & 0x3) << 4) | \
                           (((b) & 0x3) << 6) | \
                           (((c) & 0x3) << 8) | \
                           (((d) & 0x3) << 10) | \
                           (((e) & 0x3) << 12) | \
                           (((f) & 0x3) << 14) | \
                           (((g) & 0x3) << 16) | \
                           (((h) & 0x3) << 18) | \
                           (((i) & 0x3) << 20) | \
                           (((j) & 0x3) << 22) | \
                           ((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

enum qcom_scm_arg_types {
        QCOM_SCM_VAL,
        QCOM_SCM_RO,
        QCOM_SCM_RW,
        QCOM_SCM_BUFVAL,
};

extern int qcom_scm_create_shm_bridge(u64 pfn_and_ns_perm_flags,
 			u64 ipfn_and_s_perm_flags, u64 size_and_flags,
 			u64 ns_vmids, u64 *handle, struct device *dev);

extern int qcom_scm_get_tz_log_feat_id(u64 *version, struct device *dev);

extern int qcom_scm_get_tz_feat_id_version(u64 feat_id, u64 *version, struct device *dev);

extern int qcom_scm_register_qsee_log_buf(phys_addr_t buf, size_t len, struct device *dev);

extern int qcom_scm_request_encrypted_log(phys_addr_t buf,
                                   size_t len,
                                   uint32_t log_id,
                                   bool is_full_tz_logs_supported,
                                   bool is_full_tz_logs_enabled, struct device *dev);


extern int qcom_scm_query_encrypted_log_feature(u64 *enabled, struct device *dev);

extern int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data, struct device *dev);

extern int qcom_scm_invoke_smc_legacy(phys_addr_t in_buf, size_t in_buf_size,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data, struct device *dev);

extern int qcom_scm_invoke_callback_response(phys_addr_t out_buf,
		size_t out_buf_size, int32_t *result, u64 *response_type,
		unsigned int *data, struct device *dev);

#endif /*__QCOM_SCM_OOT_H__*/