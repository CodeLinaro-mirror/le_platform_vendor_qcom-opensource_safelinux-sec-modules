/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

/*
 * Qualcomm DICE VM Driver
 *
 * The dice_vm driver allows userspace to populate a physical memory
 * carveout with a DICE handover structure.
 * The memory carveout is meant to be passed to a GVM running DICE,
 * expecting to receive DICE handover from a PVM.
 *
 * The driver exposes DICE VM handover metadata to userspace via a miscdevice.
 * Userspace can read a small struct (dice_vm_info_t) with the reserved
 * memory and VM firmware information, and can mmap the reserved region
 * and write its content.
 *
 * Device Tree bindings (example):
 *   dice_vm: dice-vm {
 *       compatible     = "qcom,dice-vm";
 *       memory-region  = <&gvm_dice>;		// reserved-memory phandle
 *       qcom,vm        = <&auto_vm>;		// phandle to qcom,auto_vm@0 node
 *       qcom,vm-loader = <&auto_vm_0>; 	// phandle to gh-secure-vm-loader@0 node
 *   };
 *
 *   auto_vm: qcom,auto_vm@0 {
 *       vm_name = "autoghgvm";
 *   };
 *
 *   auto_vm_0: gh-secure-vm-loader@0 {
 *       qcom,pas-id        = <2>;
 *       qcom,firmware-name = "autoghgvm";
 *   }
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <errno.h>
#include <uapi/misc/dice_vm.h>

/** Context data for an instance of the driver */
struct dice_vm_drvdata {
	char name[MAX_NAME];
	struct reserved_mem *rmem;
	const char *fw_name;
	int pas_id;
	const char *vm_name;
	struct miscdevice misc;
};

/** Retrieve the deivce_vm_drvdata from the filp */
static inline struct dice_vm_drvdata *to_dice_vm_drvdata(struct file *filp)
{
	/** The private_data points to miscdevice, return its container  */
	return container_of(filp->private_data, struct dice_vm_drvdata, misc);
}

/**
 * Return DICE VM info to userspace, in a user provided buffer.
 * Copies a dice_vm_info_t to userspace with:
 *  - max_handover_size: reserved-memory max size
 *  - pas_id: PIL image ID
 *  - fw_name: firmware name of VM loader image
 */
static ssize_t dice_vm_read(struct file *filp, char __user *ptr, size_t len,
			      loff_t *off)
{
	dice_vm_info_t vm_info;
	struct dice_vm_drvdata *drv = to_dice_vm_drvdata(filp);

	if (!drv || !drv->rmem || !ptr)
		return -EINVAL;

	vm_info.max_handover_size = (u64)drv->rmem->size;
	vm_info.pas_id = drv->pas_id;
	strscpy(vm_info.fw_name, drv->fw_name ? drv->fw_name : "", sizeof(vm_info.fw_name));

	return simple_read_from_buffer(ptr, len, off, &vm_info, sizeof(vm_info));
}

/** Intentionally unsupported. The device is read-only; write() is a no-op. */
static ssize_t dice_vm_write(struct file *filp, const char __user *ptr,
			       size_t len, loff_t *off)
{
	return -EOPNOTSUPP;
}

/*
 * Creates a mapping of the reserved memory region in user address space.
 */
static int dice_vm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct dice_vm_drvdata *drvdata = to_dice_vm_drvdata(filp);
	unsigned long pfn_off = 0;
	size_t map_size;

	if (!drvdata || !drvdata->rmem || !vma)
		return -EINVAL;

	/** Validate mapping size vs reserved size */
	map_size = vma->vm_end - vma->vm_start;
	if (map_size > drvdata->rmem->size)
		return -EINVAL;

	/* Ensure userspace cannot acquire VM_WRITE + VM_SHARED later. */
	if (vma->vm_flags & VM_WRITE)
		vm_flags_clear(vma, VM_MAYSHARE);
	else if (vma->vm_flags & VM_SHARED)
		vm_flags_clear(vma, VM_MAYWRITE);

	/* Create write-combine mapping so all clients observe a write. */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Don't copy to child VMAs and don't include in core dumps. */
	vm_flags_set(vma, VM_DONTCOPY | VM_DONTDUMP);

	/** Offset safety: map from reserved_mem base */
	pfn_off = (drvdata->rmem->base >> PAGE_SHIFT);

	return remap_pfn_range(vma, vma->vm_start, pfn_off, map_size, vma->vm_page_prot);
}

static const struct file_operations dice_vm_fops = {
	.owner = THIS_MODULE,
	.read = dice_vm_read,
	.write = dice_vm_write,
	.mmap = dice_vm_mmap,
	.llseek = no_llseek,
};

static int dice_vm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct device_node *mem_node;
	struct device_node *vmloader_node;
	struct device_node *vm_node;
	struct reserved_mem *rmem;
	u32 pas_id;
	const char* fw_name;
	const char* vm_name;
	struct dice_vm_drvdata *drvdata;

	mem_node = of_parse_phandle((&pdev->dev)->of_node, "memory-region", 0);
	if (!mem_node) {
		dev_err(dev, "missing memory-region phandle\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(mem_node);
	if (!rmem) {
		dev_err(dev, "failed to lookup reserved memory\n");
		return -EINVAL;
	}

	if (!rmem->size || (rmem->size > ULONG_MAX)) {
		dev_err(dev, "invalid memory region size\n");
		return -EINVAL;
	}

	if (!PAGE_ALIGNED(rmem->base) || !PAGE_ALIGNED(rmem->size)) {
		dev_err(dev, "memory region must be page-aligned\n");
		return -EINVAL;
	}

	vm_node = of_parse_phandle((&pdev->dev)->of_node, "qcom,vm", 0);
	if (!vm_node) {
		dev_err(dev, "missing qcom,vm phandle\n");
		return -EINVAL;
	}

	ret = of_property_read_string(vm_node, "vm_name", &vm_name);
  	if (ret) {
		dev_err(dev, "DT error getting \"vm_name\": %d\n", ret);
  		return -EINVAL;
  	}

	dev_dbg(dev, "vm-name=%s\n", vm_name);

	vmloader_node = of_parse_phandle((&pdev->dev)->of_node, "qcom,vm-loader", 0);
	if (!vmloader_node) {
		dev_err(dev, "missing qcom,vm-loader phandle\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(vmloader_node, "qcom,pas-id", &pas_id);
  	if (ret) {
  		dev_err(dev, "DT error getting \"qcom,pas-id\": %d\n", ret);
  		return -EINVAL;
  	}

	dev_dbg(dev, "pas-id=%d\n", pas_id);

	ret = of_property_read_string(vmloader_node, "qcom,firmware-name", &fw_name);
  	if (ret) {
  		dev_err(dev, "DT error getting \"qcom,firmware-name\": %d\n", ret);
  		return -EINVAL;
  	}

	dev_dbg(dev, "firmware-name=%s\n", fw_name);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	*drvdata = (struct dice_vm_drvdata) {
		.rmem = rmem,
		.vm_name = vm_name,
		.fw_name = fw_name,
		.pas_id = (int)pas_id,
		.misc = (struct miscdevice){
			.parent	= dev,
			.name	= drvdata->name,
			.minor	= MISC_DYNAMIC_MINOR,
			.fops	= &dice_vm_fops,
			.mode	= 0600,
		},
	};

	/** Validate vm_name length to avoid truncation surprises */
	if (strnlen(vm_name, sizeof(drvdata->name)) >= (sizeof(drvdata->name) - (sizeof(DRIVER_NAME) + 1))) {
		dev_warn(dev, "vm_name too long, truncating for device name\n");
	}

	/** Compose the device name from a prefix and the VM name. */
	snprintf(drvdata->name, sizeof(drvdata->name), "%s_%s", DRIVER_NAME, vm_name);

	ret = misc_register(&drvdata->misc);
	if (ret) {
		dev_err(dev, "failed to register misc device '%s': %d\n",
			drvdata->name, ret);
		return ret;
	}

	platform_set_drvdata(pdev, drvdata);

	dev_info_once(dev, "DICE VM device '%s' registered\n", drvdata->name);
	return 0;
}

static int dice_vm_remove(struct platform_device *pdev)
{
	struct dice_vm_drvdata *drvdata = platform_get_drvdata(pdev);

	misc_deregister(&drvdata->misc);

	return 0;
}

static const struct of_device_id dice_vm_of_match[] = {
	{ .compatible = "qcom,dice-vm" },
	{}
};
MODULE_DEVICE_TABLE(of, dice_vm_of_match);

static struct platform_driver dice_vm_driver = {
	.probe = dice_vm_probe,
	.remove = dice_vm_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = dice_vm_of_match,
	},
};

int __init dice_vm_driver_init(void)
{
	return platform_driver_register(&dice_vm_driver);
}

void __exit dice_vm_driver_exit(void)
{
	platform_driver_unregister(&dice_vm_driver);
}

module_init(dice_vm_driver_init);
module_exit(dice_vm_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QCOM DICE VM driver");
MODULE_AUTHOR("Qualcomm Automotive Team");
