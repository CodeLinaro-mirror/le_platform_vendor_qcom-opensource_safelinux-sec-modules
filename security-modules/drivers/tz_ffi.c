/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

/*
 * TZ FFI driver notifies TrustZone of all non-safety memories, thus
 * TrustZone will mark them as RW regions.
 */

#include <linux/arm-smccc.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/firmware/qcom/qcom_tzmem.h>
#include <errno.h>
#include "crc.h"
#include "crc8.h"

/* Max wait for TZ availability ~200ms */
#define QCOM_TZ_FFI_MAX_RETRIES (200u)
#define TZ_FFI_NUM_RANGES (32u)
#define TZ_FFI_VERSION (1u)

/* 
 * define UINT32_MAX manually since I have conflicts between
 * stdint.h and linux/types.h
 */
#define UINT32_MAX 4294967295U

#define ROUND_TO_MIN_DMA_ALLOC_SIZE(size) (((size + 4095) / 4096) * 4096)

/* Memory entry */
typedef struct  __attribute__ ((packed))
{
	uint64_t mem_addr; /* Starting address of memory */
	uint64_t mem_size; /* Size in bytes of memory region passed */
} mem_entry_t;

/* Structure for regions asked by HLOS as RW */
typedef struct  __attribute__ ((packed))
{
	uint32_t version;		   /* version  */
	uint32_t mem_list_count;	/* Number of ranges */
	mem_entry_t mem_list[TZ_FFI_NUM_RANGES];	/* List of ranges */
} mem_info_t;

static void init_mem_info(mem_info_t *mem_info)
{
	(void)memset((void*)mem_info, 0, sizeof(*mem_info));

	mem_info->version = TZ_FFI_VERSION;
	mem_info->mem_list_count = 0;
}

static int get_mem_info_actual_size(struct device *dev, mem_info_t *mem_info, size_t *actual_size)
{
	uint32_t entry_index = mem_info->mem_list_count;
	if (entry_index > TZ_FFI_NUM_RANGES) {
		dev_err(dev, "Too many ranges, index %u surpass tz_ffi_num_ranges %u\n",
				entry_index, TZ_FFI_NUM_RANGES);
		return -EINVAL;
	}

	uint32_t unused_entries = TZ_FFI_NUM_RANGES - entry_index;
	if (unused_entries > UINT32_MAX / sizeof(mem_entry_t)) {
		dev_err(dev, "Size of mem_entry_t is too big %lu\n", sizeof(mem_entry_t));
		return -EINVAL;
	}

	uint32_t reduction = unused_entries * sizeof(mem_entry_t);
	if (reduction > sizeof(*mem_info)) {
		dev_err(dev, "Too many unused ranges %u, multiplication would overflow\n",
				unused_entries);
		return -EINVAL;
	}

	/* This is the size with the real amount of regions to be sent to TZ */
	uint32_t actual_mem_info_size = sizeof(*mem_info) - reduction;

	*actual_size = actual_mem_info_size;

	return 0;
}

static int get_tz_ffi_mem_info(struct device *dev, mem_info_t *mem_info)
{
	uint32_t entry_index = mem_info->mem_list_count;
	struct device_node *memory_region_np = NULL;
	struct device_node *firmware_np = NULL;
	struct device_node *qcom_scm_np = NULL;
	struct reserved_mem *rmem = NULL;
	mem_entry_t *tzcom_entry = NULL;

	if (entry_index == TZ_FFI_NUM_RANGES) {
		dev_err(dev, "Too many ranges, index %u equals tz_ffi_num_ranges %u\n",
				entry_index, TZ_FFI_NUM_RANGES);
		return -EINVAL;
	}

	firmware_np = of_find_node_by_name(NULL, "firmware");
	if (!firmware_np) {
		dev_err(dev, "Failed to find firmware node\n");
		return -ENODEV;
	}

	qcom_scm_np = of_get_child_by_name(firmware_np, "qcom_scm");
	if (!qcom_scm_np) {
		dev_err(dev, "Failed to find qcom_scm node\n");
		of_node_put(firmware_np);
		return -ENODEV;
	}

	memory_region_np = of_parse_phandle(qcom_scm_np, "memory-region", 0);
	if (!memory_region_np) {
		dev_err(dev, "No memory-region specified for qcom_scm under firmware node\n");
		of_node_put(qcom_scm_np);
		of_node_put(firmware_np);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(memory_region_np);
	of_node_put(memory_region_np);
	of_node_put(qcom_scm_np);
	of_node_put(firmware_np);
	if (!rmem) {
		dev_err(dev, "Failed to lookup reserved memory-region for qcom_scm under firmware node\n");
		return -ENODEV;
	}

	tzcom_entry = &mem_info->mem_list[entry_index];
	tzcom_entry->mem_addr = rmem->base;
	tzcom_entry->mem_size = rmem->size;
	++mem_info->mem_list_count;

	dev_info(dev, "tz-ffi addr 0x%llx size 0x%llx\n", rmem->base, rmem->size);

	return 0;
}

/* Get memory information for the TZCom heap from DeviceTree */
static int get_tzcom_mem_info(struct device *dev, mem_info_t *mem_info)
{
	uint32_t entry_index = mem_info->mem_list_count;
	struct device_node *qcom_dma_heaps_np = NULL;
	struct device_node *memory_region_np = NULL;
	struct device_node *qcom_tzcom_np = NULL;
	struct reserved_mem *rmem = NULL;
	mem_entry_t *tzcom_entry = NULL;

	if (entry_index == TZ_FFI_NUM_RANGES) {
		dev_err(dev, "Too many ranges, index %u equals tz_ffi_num_ranges %u\n",
				entry_index, TZ_FFI_NUM_RANGES);
		return -EINVAL;
	}

	qcom_dma_heaps_np = of_find_compatible_node(NULL, NULL, "qcom,dma-heaps");
	if (!qcom_dma_heaps_np) {
		dev_err(dev, "Failed to find qcom,tzcom node\n");
		return -ENODEV;
	}

	qcom_tzcom_np = of_get_child_by_name(qcom_dma_heaps_np, "qcom,tzcom");
	if (!qcom_tzcom_np) {
		dev_err(dev, "Failed to find qcom,tzcom node\n");
		of_node_put(qcom_dma_heaps_np);
		return -ENODEV;
	}

	memory_region_np = of_parse_phandle(qcom_tzcom_np, "memory-region", 0);
	if (!memory_region_np) {
		dev_err(dev, "No memory-region specified for qcom,tzcom under qcom,dma-heaps node\n");
		of_node_put(qcom_tzcom_np);
		of_node_put(qcom_dma_heaps_np);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(memory_region_np);
	of_node_put(memory_region_np);
	of_node_put(qcom_tzcom_np);
	of_node_put(qcom_dma_heaps_np);
	if (!rmem) {
		dev_err(dev, "Failed to lookup reserved memory-region for qcom,tzcom under qcom,dma-heaps node\n");
		return -ENODEV;
	}

	tzcom_entry = &mem_info->mem_list[entry_index];
	tzcom_entry->mem_addr = rmem->base;
	tzcom_entry->mem_size = rmem->size;
	++mem_info->mem_list_count;

	dev_info(dev, "tzcom addr 0x%llx size 0x%llx\n", rmem->base, rmem->size);

	return 0;
}

/* Get memory information for the dump heap from DeviceTree */
static int get_dump_mem_info(struct device *dev, mem_info_t *mem_info)
{
	uint32_t entry_index = mem_info->mem_list_count;
	struct device_node *mem_dump_np = NULL;
	struct device_node *memory_region_np = NULL;
	struct reserved_mem *rmem = NULL;
	mem_entry_t *mem_dump_entry = NULL;

	if (entry_index == TZ_FFI_NUM_RANGES) {
		dev_err(dev, "Too many ranges, index %u equals tz_ffi_num_ranges %u\n",
				entry_index, TZ_FFI_NUM_RANGES);
		return -EINVAL;
	}

	mem_dump_np = of_find_compatible_node(NULL, NULL, "qcom,mem-dump");
	if (!mem_dump_np) {
		dev_err(dev, "Failed to find qcom,mem-dump node\n");
		return -ENODEV;
	}

	memory_region_np = of_parse_phandle(mem_dump_np, "memory-region", 0);
	if (!memory_region_np) {
		dev_err(dev, "No memory-region specified under qcom,mem-dump node\n");
		of_node_put(mem_dump_np);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(memory_region_np);
	of_node_put(memory_region_np);
	of_node_put(mem_dump_np);
	if (!rmem) {
		dev_err(dev, "Failed to lookup reserved memory-region under qcom,mem-dump node\n");
		return -ENODEV;
	}

	mem_dump_entry = &mem_info->mem_list[entry_index];
	mem_dump_entry->mem_addr = rmem->base;
	mem_dump_entry->mem_size = rmem->size;
	++mem_info->mem_list_count;

	dev_info(dev, "mem-dump addr 0x%llx size 0x%llx\n", rmem->base, rmem->size);

	return 0;
}

/* Get memory information for the GVM */
static int get_gvm_mem_info(struct device *dev, mem_info_t *mem_info)
{
	uint32_t entry_index = mem_info->mem_list_count;
	struct device_node *mem_np = NULL;
	mem_entry_t *tzcom_entry = NULL;
	struct device_node *np = NULL;
	struct resource res = {0};
	int count = 0;
	int ret = 0;
	int i = 0;

	for_each_compatible_node(np, NULL, "qcom,gh-secure-vm-loader") {
		if (!np) {
			dev_err(dev, "Node gh-secure-vm-loader not found\n");
			return -ENODEV;
		}

		count = of_count_phandle_with_args(np, "memory-region", NULL);
		if (count < 0) {
			dev_err(dev, "Failed to count memory regions\n");
			return -ENODEV;
		}

		if (entry_index + count >= TZ_FFI_NUM_RANGES) {
			dev_err(dev, "Too many ranges, adding %d ranges surpass tz_ffi_num_ranges %u\n",
				   count, TZ_FFI_NUM_RANGES);
			return -EINVAL;
		}

		for (i = 0; i < count; ++i) {
			mem_np = of_parse_phandle(np, "memory-region", i);
			if (mem_np) {
				ret = of_address_to_resource(mem_np, 0, &res);
				if (ret) {
					dev_err(dev, "Failed to get resource\n");
					of_node_put(mem_np);
					return ret;
				}

				tzcom_entry = &mem_info->mem_list[entry_index];
				tzcom_entry->mem_addr = res.start;
				tzcom_entry->mem_size = resource_size(&res);
				++mem_info->mem_list_count;
				++entry_index;

				dev_info(dev, "GVM memory-region %d: base = 0x%llx, size = 0x%llx\n", i, res.start, resource_size(&res));
				of_node_put(mem_np);
			}
		}
	}

	return 0;
}

/* Get memory info of all tz ffi related memories */
static int get_mem_list(struct device *dev, mem_info_t *mem_info)
{
	int ret = 0;

	ret = get_tz_ffi_mem_info(dev, mem_info);
	if (ret) {
		return ret;
	}

	ret = get_tzcom_mem_info(dev, mem_info);
	if (ret) {
		return ret;
	}

	ret = get_dump_mem_info(dev, mem_info);
	if (ret) {
		return ret;
	}

	ret = get_gvm_mem_info(dev, mem_info);
	if (ret) {
		return ret;
	}

	return ret;
}

/* Compute CRC over the buffer we send to TrustZone */
static int crc_compute(struct device *dev, mem_info_t *mem_info, size_t size, uint8_t *crc8)
{
	error_type crc_ret = crc8_generate((const char *)mem_info, size, crc8);
	if (crc_ret != SUCCESS) {
		dev_err(dev, "Failed to compute CRC, ret=%d\n", crc_ret);
	}
	dev_info(dev, "Compute CRC, ret=%d, crc8=0x%x\n", crc_ret, *crc8);

	return crc_ret;
}

/*
 * Send non-safety memories to TrustZone.
 * Send the buffer along with its CRC.
 */
static int smc_tz_safety_enable_ffi(struct device *dev, phys_addr_t mem_info_paddr, size_t size, uint8_t crc8)
{
	int ret = 0;

	ret = qcom_scm_tz_safety_enable_ffi(mem_info_paddr, size, crc8);
	if (ret) {
		dev_err(dev, "Failed to enable TZ FFI, ret=%d\n", ret);
	}

	return ret;
}

static int tz_ffi_probe(struct platform_device *pdev)
{
	size_t size = ROUND_TO_MIN_DMA_ALLOC_SIZE(sizeof(mem_info_t));
	struct device *dev = &pdev->dev;
	dma_addr_t mem_info_paddr = 0;
	mem_info_t *mem_info = NULL;
	uint32_t ns_vm_perms[] = {};
	uint64_t tzmem_handle = 0;
	uint32_t ns_vmids[] = {};
	uint32_t ns_vmid_num = 0;
	size_t actual_size = 0;
	uint8_t crc8 = 0;
	int ret = 0;

	dev_info(dev, "tz_ffi_probe enter\n");

	/* Make the dma_alloc_coherent calls allocate from the
	 * memory-region specified in the Device Tree */
	ret = of_reserved_mem_device_init(dev);
	if (ret) {
		dev_err(dev, "reserved-mem init failed: %d\n", ret);
		return -ENXIO;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(dev, "dma_set_mask_and_coherent failed %d\n", ret);
		of_reserved_mem_device_release(dev);
		return -ENXIO;
	}

	mem_info = dma_alloc_coherent(dev, size, &mem_info_paddr, GFP_KERNEL);
	if (mem_info == NULL) {
		dev_err(dev, "Failed to allocate memory\n");
		of_reserved_mem_device_release(dev);
		return -ENOMEM;
	}

	ret = qcom_tzmem_register(mem_info_paddr,
					size, ns_vmids,
					ns_vm_perms, ns_vmid_num,
					QCOM_SCM_PERM_RW, &tzmem_handle);
	if (ret) {
		dev_err(dev, "Failed to create tzffi bridge, ret %d\n", ret);
		dma_free_coherent(dev, size, mem_info, mem_info_paddr);
		of_reserved_mem_device_release(dev);
		return -EINVAL;
	}

	init_mem_info(mem_info);

	do {
		ret = get_mem_list(dev, mem_info);
		if (ret) {
			dev_err(dev, "Failed to get tz-ffi memory list\n");
			break;
		}

		ret = get_mem_info_actual_size(dev, mem_info, &actual_size);
		if (ret) {
			dev_err(dev, "Failed to get actual size\n");
			break;
		}

		ret = crc_compute(dev, mem_info, actual_size, &crc8);
		if (ret) {
			dev_err(dev, "Failed to compute crc\n");
			break;
		}

		ret = smc_tz_safety_enable_ffi(dev, mem_info_paddr, actual_size, crc8);
		if (ret) {
			dev_err(dev, "Failed to enable ffi\n");
			break;
		}
	} while (0);

	qcom_tzmem_deregister(tzmem_handle);
	dma_free_coherent(dev, size, mem_info, mem_info_paddr);
	of_reserved_mem_device_release(dev);

	dev_info(dev, "tz_ffi_probe exit\n");
	return ret;
}

static int tz_ffi_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	dev_info(dev, "qcom_tz_ffi_exit\n");

	return 0;
}

static const struct of_device_id tz_ffi_of_match[] = {
	{ .compatible = "qcom,tzffi" },
	{}
};
MODULE_DEVICE_TABLE(of, tz_ffi_of_match);

static struct platform_driver tz_ffi_driver = {
	.probe = tz_ffi_probe,
	.remove = tz_ffi_remove,
	.driver = {
		.name = "tz_ffi",
		.of_match_table = tz_ffi_of_match,
	},
};

int __init tz_ffi_driver_init(void)
{
	return platform_driver_register(&tz_ffi_driver);
}

void __exit tz_ffi_driver_exit(void)
{
	platform_driver_unregister(&tz_ffi_driver);
}

module_init(tz_ffi_driver_init);
module_exit(tz_ffi_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QCOM TZ FFI driver");
