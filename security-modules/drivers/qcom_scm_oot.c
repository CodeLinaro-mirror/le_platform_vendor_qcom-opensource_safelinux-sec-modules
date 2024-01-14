// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010,2015,2019,2021 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include "qcom_scm_oot.h"
#include <linux/module.h>
#include <linux/of_platform.h>

static const char* scm_dev_name = "scm";
static struct device *scm_dev = NULL;

/* Calls needed to support qtee_shmbridge */

int qcom_scm_create_shm_bridge(u64 pfn_and_ns_perm_flags,
                        u64 ipfn_and_s_perm_flags, u64 size_and_flags,
                        u64 ns_vmids, u64 *handle, struct device *dev)
{
	int ret;
        struct qcom_scm_desc desc = {
                .svc = QCOM_SCM_SVC_MP,
                .cmd = QCOM_SCM_MEMP_SHM_BRDIGE_CREATE,
                .owner = ARM_SMCCC_OWNER_SIP
        };
        struct qcom_scm_res res;

        desc.args[0] = pfn_and_ns_perm_flags;
        desc.args[1] = ipfn_and_s_perm_flags;
        desc.args[2] = size_and_flags;
        desc.args[3] = ns_vmids;

        desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL, QCOM_SCM_VAL,
                                        QCOM_SCM_VAL, QCOM_SCM_VAL);

        ret = qcom_scm_call(scm_dev, &desc, &res);

        if (handle)
                *handle = res.result[1];

        return ret ? : res.result[0];
}
EXPORT_SYMBOL(qcom_scm_create_shm_bridge);

/* Calls needed to support tz_log */

static int qcom_scm_get_feat_version(struct device *dev, u64 feat_id, u64 *version)
{
        int ret;
        struct qcom_scm_desc desc = {
                .svc = QCOM_SCM_SVC_INFO,
                .cmd = QCOM_SCM_INFO_GET_FEAT_VERSION_CMD,
                .owner = ARM_SMCCC_OWNER_SIP,
                .args[0] = feat_id,
                .arginfo = QCOM_SCM_ARGS(1),
        };
        struct qcom_scm_res res;

        ret = qcom_scm_call(scm_dev, &desc, &res);
        if (version)
                *version = res.result[0];
        return ret;
}

int qcom_scm_get_tz_log_feat_id(u64 *version, struct device *dev)
{
        return qcom_scm_get_feat_version(scm_dev, QCOM_SCM_FEAT_LOG_ID, version);
}
EXPORT_SYMBOL(qcom_scm_get_tz_log_feat_id);

int qcom_scm_get_tz_feat_id_version(u64 feat_id, u64 *version, struct device *dev)
{
        return qcom_scm_get_feat_version(scm_dev, feat_id, version);
}
EXPORT_SYMBOL(qcom_scm_get_tz_feat_id_version);

int qcom_scm_register_qsee_log_buf(phys_addr_t buf, size_t len, struct device *dev)
{
        int ret;
        struct qcom_scm_desc desc = {
                .svc = QCOM_SCM_SVC_QSEELOG,
                .cmd = QCOM_SCM_QSEELOG_REGISTER,
                .owner = ARM_SMCCC_OWNER_TRUSTED_OS,
                .args[0] = buf,
                .args[1] = len,
                .arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW),
        };
        struct qcom_scm_res res;

        ret = qcom_scm_call(scm_dev, &desc, &res);

        return ret ? : res.result[0];
}
EXPORT_SYMBOL(qcom_scm_register_qsee_log_buf);

int qcom_scm_query_encrypted_log_feature(u64 *enabled, struct device *dev)
{
        int ret;
        struct qcom_scm_desc desc = {
                .svc = QCOM_SCM_SVC_QSEELOG,
                .cmd = QCOM_SCM_QUERY_ENCR_LOG_FEAT_ID,
                .owner = ARM_SMCCC_OWNER_TRUSTED_OS
        };
        struct qcom_scm_res res;

        ret = qcom_scm_call(scm_dev, &desc, &res);
        if (enabled)
                *enabled = res.result[0];

        return ret;
}
EXPORT_SYMBOL(qcom_scm_query_encrypted_log_feature);

int qcom_scm_request_encrypted_log(phys_addr_t buf,
                                   size_t len,
                                   uint32_t log_id,
                                   bool is_full_tz_logs_supported,
                                   bool is_full_tz_logs_enabled, struct device *dev)
{
        int ret;
        struct qcom_scm_desc desc = {
                .svc = QCOM_SCM_SVC_QSEELOG,
                .cmd = QCOM_SCM_REQUEST_ENCR_LOG_ID,
                .owner = ARM_SMCCC_OWNER_TRUSTED_OS,
                .args[0] = buf,
                .args[1] = len,
                .args[2] = log_id
        };
        struct qcom_scm_res res;

        if (is_full_tz_logs_supported) {
                if (is_full_tz_logs_enabled) {
                        /* requesting full logs */
                        desc.args[3] = 1;
                } else {
                        /* requesting incremental logs */
                        desc.args[3] = 0;
                }
                desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW);
        } else {
                desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW);
        }
        ret = qcom_scm_call(scm_dev, &desc, &res);

        return ret ? : res.result[0];
}
EXPORT_SYMBOL(qcom_scm_request_encrypted_log);

/* Calls needed to support smcinvoke */

int qcom_scm_invoke_smc_legacy(phys_addr_t in_buf, size_t in_buf_size,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data, struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_INVOKE_LEGACY,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = in_buf,
		.args[1] = in_buf_size,
		.args[2] = out_buf,
		.args[3] = out_buf_size,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
			QCOM_SCM_RW, QCOM_SCM_VAL),
	};

	struct qcom_scm_res res;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	if (result)
		*result = res.result[1];

	if (response_type)
		*response_type = res.result[0];

	if (data)
		*data = res.result[2];

	return ret;
}
EXPORT_SYMBOL(qcom_scm_invoke_smc_legacy);

int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,
		phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
		u64 *response_type, unsigned int *data, struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_INVOKE,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = in_buf,
		.args[1] = in_buf_size,
		.args[2] = out_buf,
		.args[3] = out_buf_size,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
					QCOM_SCM_RW, QCOM_SCM_VAL),
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	if (result)
		*result = res.result[1];

	if (response_type)
		*response_type = res.result[0];

	if (data)
		*data = res.result[2];

	return ret;
}
EXPORT_SYMBOL(qcom_scm_invoke_smc);

int qcom_scm_invoke_callback_response(phys_addr_t out_buf,
	size_t out_buf_size, int32_t *result, u64 *response_type,
	unsigned int *data, struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_CB_RSP,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = out_buf,
		.args[1] = out_buf_size,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	if (result)
		*result = res.result[1];

	if (response_type)
		*response_type = res.result[0];

	if (data)
		*data = res.result[2];

	return ret;
}

EXPORT_SYMBOL(qcom_scm_invoke_callback_response);

static int qcom_scm_oot_init(void)
{
	struct platform_device *pdev;
	struct device_node* np;

	np = of_find_node_by_name(NULL, scm_dev_name);
	if (!np)
	{
		pr_err("Failed to find name of scm device\n");
		return -ENODEV;
	}
	pdev = of_find_device_by_node(np);
	if (!pdev)
	{
		pr_err("Failed to find scm platform device\n");
		return -ENODEV;
	}
	scm_dev = &pdev->dev;

	return 0;
}

static void qcom_scm_oot_exit(void)
{
	scm_dev = NULL;
}

module_init(qcom_scm_oot_init);
module_exit(qcom_scm_oot_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QCOM OOT SCM driver");
