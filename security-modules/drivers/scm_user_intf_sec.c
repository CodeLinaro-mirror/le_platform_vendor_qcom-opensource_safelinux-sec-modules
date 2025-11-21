// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/arm-smccc.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/firmware/qcom/qcom_tzmem.h>
#include <linux/dma-buf.h>
#include <linux/version.h>
#include <uapi/misc/scm_user_intf.h>

#if (LINUX_VERSION_CODE != KERNEL_VERSION(5, 14, 0))
#include <qcom_scm.h>
#endif

#define ARG_TO_ARRAY(type, ...) ((type[]) {__VA_ARGS__})

#define DMA_FD_ARGS(...) ARG_TO_ARRAY(__u32, __VA_ARGS__)

#define SVC_CMD_GRP(svc, num_of_cmds, ...)                      \
	{                                                       \
		.svc_id = (__u32)svc,                           \
		.num_cmd = (__u32)num_of_cmds,                  \
		.cmd_ids = ARG_TO_ARRAY(__u32, __VA_ARGS__),        \
	}

#define MAX_NOF_MEM_OBJS 15

#if (LINUX_VERSION_CODE != KERNEL_VERSION(5, 14, 0))
struct scm_user_res {
	u64 result[MAX_QCOM_SCM_RETS];
};
#endif

struct svc_cmd_dma_list {
	__u32 svc_id;
	__u32 cmd_id;
	__u32 num_dma_fds;
	__u32 *dma_fds;
};

struct scm_dev_data {
	struct platform_device *scm_pdev;
	struct miscdevice scm_miscdev;
	struct device *dev;
};

typedef struct {
	u64 fd;
	u64 offset;
} mem_obj_info_t;

typedef struct {
	mem_obj_info_t mem_obj_info[MAX_NOF_MEM_OBJS];
	u32 nof_mem_objs;
} mem_obj_info_list_t;

typedef struct {
	u64 p_addr;
	size_t p_addr_len;
	void *vaddr;
	struct sg_table *sgt;
	struct dma_buf_attachment *attach;
	struct dma_buf *dmabuf;
	mem_obj_info_list_t mem_obj_info_list;
} dma_buf_info_t;

typedef struct {
	u64 p_addr;
	u64 len;
} mem_obj_buffer_info_t;

static struct scm_dev_data *__scm_dev = NULL;
static const char* scm_dev_name = "qcom_scm";
static u64 g_tzmem_handle = 0;

/*
 * SVC/ Cmd ID pair that needs DMA buf fd to PA translation
 */
static const struct svc_cmd_dma_list dma_fd_tbl[] = {

	{QCOM_SCM_SVC_SMC_INVOKE, QCOM_SCM_SMCINVOKE, 1, DMA_FD_ARGS(0)},
	{QCOM_SCM_SVC_SMC_INVOKE, QCOM_SCM_SMCINVOKE_CB_RESP, 1, DMA_FD_ARGS(0)},

};

static int scm_user_intf_dmabuf_map_pa(int fd, dma_buf_info_t *dma_buf_info);
static int scm_user_intf_dmabuf_map(int fd, dma_buf_info_t *dma_buf_info);
static void scm_user_intf_dmabuf_unmap(dma_buf_info_t *dma_buf_info);

static int get_dmafd_args(unsigned int svc_id, unsigned int cmd_id, unsigned int *dma_fds)
{
	u32 len = sizeof(dma_fd_tbl) / sizeof(dma_fd_tbl[0]);
	int i;

	struct svc_cmd_dma_list dma_args;
	for(i = 0; i < len; i++) {
		dma_args = dma_fd_tbl[i];
		if (dma_args.svc_id == svc_id && dma_args.cmd_id == cmd_id) {
			memcpy(dma_fds, dma_args.dma_fds, sizeof(u32) * dma_args.num_dma_fds);

			return dma_args.num_dma_fds;
		}
	}

	return -EINVAL;
}

static int translate_fd_to_pa(unsigned  int *dmabuf_fd,
			      unsigned int no_dma_fds,
			      long long unsigned int *scm_args)
{
	int i;

	for (i = 0; i < no_dma_fds; i++) {
		dma_buf_info_t dma_buf_info = {0};

		if (scm_user_intf_dmabuf_map_pa(scm_args[dmabuf_fd[i]], &dma_buf_info) < 0)
			return -EINVAL;

		scm_args[dmabuf_fd[i]] = dma_buf_info.p_addr;
		scm_user_intf_dmabuf_unmap(&dma_buf_info);
	}

	return 0;
}

static int qcom_scm_open(struct inode *inode, struct file *filp)
{
	struct scm_dev_data *dev_data = container_of(filp->private_data,
						     struct scm_dev_data,
						     scm_miscdev);

	filp->private_data = dev_data;
	return 0;
}

static bool is_dma_fd(int fd, struct dma_buf **dma_buf)
{
	*dma_buf = dma_buf_get(fd);
	return IS_ERR_OR_NULL(*dma_buf) ? false : true;
}

/**
 * @brief Get physical address of a dma-buf.
 */
static int scm_user_intf_dmabuf_map_pa(int fd, dma_buf_info_t *dma_buf_info)
{
	int ret = 0;
	struct dma_buf_attachment *new_attach = NULL;
	struct sg_table *new_sgt = NULL;
	struct dma_buf *new_dma_buf = NULL;

	if (!is_dma_fd(fd, &new_dma_buf)) {
		ret = -ENOMEM;
		dev_err(__scm_dev->dev, "dma buf get failed, ret: %d\n", ret);
		goto err;
	}
	dma_buf_info->dmabuf = new_dma_buf;

	new_attach = dma_buf_attach(new_dma_buf,  __scm_dev->dev);
	if (IS_ERR(new_attach)) {
		ret = -ENOMEM;
		dev_err(__scm_dev->dev, "dma buf attach failed, ret: %d\n", ret);
		goto err;
	}
	dma_buf_info->attach = new_attach;

	new_sgt = dma_buf_map_attachment(new_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(new_sgt)) {
		dev_err(__scm_dev->dev, "mapping dma buffers failed, ret: %ld\n", PTR_ERR(new_sgt));
		ret = -ENOMEM;
		goto err;
	}
	dma_buf_info->sgt = new_sgt;

	/* contiguous only => nents=1 */
	if (new_sgt->nents != 1) {
		ret = -EINVAL;
		dev_err(__scm_dev->dev, "sg enries are not contigous, ret: %d\n", ret);
		goto err;
	}
	dma_buf_info->p_addr = sg_dma_address(new_sgt->sgl);
	if (!dma_buf_info->p_addr) {
		ret = -EINVAL;
		dev_err(__scm_dev->dev, "invalid physical address, ret: %d\n", ret);
		goto err;
	}
	dma_buf_info->p_addr_len = new_sgt->sgl->length;

	return ret;

err:
	scm_user_intf_dmabuf_unmap(dma_buf_info);
	return ret;
}

/**
 * @brief Map a dma-buf into a physical address and a virtual address.
 */
static int scm_user_intf_dmabuf_map(int fd, dma_buf_info_t *dma_buf_info)
{
	int ret = 0;
	struct iosys_map new_dma_buf_map = {0};
	void *new_va = NULL;

	ret = scm_user_intf_dmabuf_map_pa(fd, dma_buf_info);
	if (ret) {
		dev_err(__scm_dev->dev, "Failed to get pa from dmabuf fd=0x%x\n", fd);
		goto err;
	}

	dma_buf_begin_cpu_access(dma_buf_info->dmabuf, DMA_BIDIRECTIONAL);

	ret = dma_buf_vmap(dma_buf_info->dmabuf, &new_dma_buf_map);
	new_va = ret ? NULL : new_dma_buf_map.vaddr;
	if (!new_va) {
		dev_err(__scm_dev->dev, "dma_buf_vmap failed\n");
		ret = -ENOMEM;
		//Flush the buffer (i.e. clean and invalidate)
		dma_buf_end_cpu_access(dma_buf_info->dmabuf, DMA_BIDIRECTIONAL);
		dma_buf_begin_cpu_access(dma_buf_info->dmabuf, DMA_BIDIRECTIONAL);
		goto err;
	}
	dma_buf_info->vaddr = new_va;

	return ret;

err:
	scm_user_intf_dmabuf_unmap(dma_buf_info);
	return ret;
}

/**
 * @brief Release all dma-buf resource taken in the mapping process.
 */
static void scm_user_intf_dmabuf_unmap(dma_buf_info_t *dma_buf_info)
{
	if (dma_buf_info->vaddr) {
		struct iosys_map dmabufmap = IOSYS_MAP_INIT_VADDR(dma_buf_info->vaddr);
		dma_buf_vunmap(dma_buf_info->dmabuf, &dmabufmap);
		dma_buf_end_cpu_access(dma_buf_info->dmabuf, DMA_BIDIRECTIONAL);
		dma_buf_info->vaddr = NULL;
	}
	if (dma_buf_info->sgt) {
		dma_buf_unmap_attachment(dma_buf_info->attach, dma_buf_info->sgt, DMA_BIDIRECTIONAL);
		dma_buf_info->sgt = NULL;
	}
	if (dma_buf_info->attach) {
		dma_buf_detach(dma_buf_info->dmabuf, dma_buf_info->attach);
		dma_buf_info->attach = NULL;
	}
	if (dma_buf_info->dmabuf) {
		dma_buf_put(dma_buf_info->dmabuf);
		dma_buf_info->dmabuf = NULL;
	}
}

/**
 * @brief Update smcinvoke memory objects physical address and length
 */
static int scm_smcinvoke_memobj_update_paddr(struct scm_hand_shake *scm_data, dma_buf_info_t *dma_buf_info)
{
	mem_obj_info_list_t *mem_obj_info_list = &dma_buf_info->mem_obj_info_list;
	mem_obj_buffer_info_t *mem_obj_buffer_info = NULL;
	u64 in_buf_size = scm_data->args_buffer[1];
	dma_buf_info_t mem_obj_dma_buf_info = {0};
	mem_obj_info_t *mem_obj_info = NULL;
	u32 nof_mem_objs = 0;
	int ret = 0;

	void __user *ip = (void __user *)scm_data->args_buffer[3];
	if (copy_from_user(mem_obj_info_list, ip, sizeof(*mem_obj_info_list))) {
		return -EFAULT;
	}

	nof_mem_objs = mem_obj_info_list->nof_mem_objs;

	if (nof_mem_objs > MAX_NOF_MEM_OBJS) {
		dev_err(__scm_dev->dev, "nof_mem_obj=%u is above maximum=%d\n", nof_mem_objs, MAX_NOF_MEM_OBJS);
		return -EINVAL;
	}

	for (int i_mem_obj = 0; i_mem_obj < nof_mem_objs; ++i_mem_obj) {
		mem_obj_info = &mem_obj_info_list->mem_obj_info[i_mem_obj];
		if (mem_obj_info->fd != 0) {
			/* offset overflow check */
			if (__UINT64_MAX__ - in_buf_size < mem_obj_info->offset ||
					__UINT64_MAX__ - in_buf_size - mem_obj_info->offset < sizeof(mem_obj_buffer_info_t) ||
					in_buf_size + mem_obj_info->offset + sizeof(mem_obj_buffer_info_t) > dma_buf_info->p_addr_len) {
				dev_err(__scm_dev->dev, "mem_obj fd=0x%llx integer overflow\n", mem_obj_info->fd);
				return -EINVAL;
			}

			ret = scm_user_intf_dmabuf_map_pa(mem_obj_info->fd, &mem_obj_dma_buf_info);
			if (ret) {
				dev_err(__scm_dev->dev, "scm_user_intf_dmabuf_map_pa failed for mem_obj fd=0x%llx\n", mem_obj_info->fd);
				return ret;
			}

			/* Update mem_obj physical address and length inside the TZ buffer */
			mem_obj_buffer_info = dma_buf_info->vaddr + in_buf_size + mem_obj_info->offset;
			mem_obj_buffer_info->p_addr = mem_obj_dma_buf_info.p_addr;
			mem_obj_buffer_info->len = mem_obj_dma_buf_info.p_addr_len;

			scm_user_intf_dmabuf_unmap(&mem_obj_dma_buf_info);
		}
	}

	return ret;
}

/**
 * @brief Clear smcinvoke memory objects physical address before returning to userspace
 */
static void scm_smcinvoke_memobj_clear_paddr(struct scm_hand_shake *scm_data, dma_buf_info_t *dma_buf_info)
{
	mem_obj_info_list_t *mem_obj_info_list = &dma_buf_info->mem_obj_info_list;
	mem_obj_info_t *mem_obj_info = NULL;
	mem_obj_buffer_info_t *mem_obj_buffer_info = NULL;
	u64 in_buf_size = scm_data->args_buffer[1];
	u32 nof_mem_objs = mem_obj_info_list->nof_mem_objs;

	for (int i_mem_obj = 0; i_mem_obj < nof_mem_objs; ++i_mem_obj) {
		mem_obj_info = &mem_obj_info_list->mem_obj_info[i_mem_obj];
		if (mem_obj_info->fd != 0) {
			/* Clear mem_obj physical address and length inside the TZ buffer */
			mem_obj_buffer_info = dma_buf_info->vaddr + in_buf_size + mem_obj_info->offset;
			mem_obj_buffer_info->p_addr = 0;
			mem_obj_buffer_info->len = 0;
		}
	}
}

static long scm_smcinvoke_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *ip = (void __user *)arg;
	u32 dma_fd_args[MAX_QCOM_SCM_ARGS];
	dma_buf_info_t dma_buf_info = {0};
	struct scm_hand_shake scm_data;
#if (LINUX_VERSION_CODE == KERNEL_VERSION(5, 14, 0))
	struct qcom_scm_desc desc;
	struct qcom_scm_res res;
#else
	struct scm_user_res res;
#endif
	int no_dma_fds = 0;
	int i, no_of_args;
	int ret = 0;

	if (copy_from_user(&scm_data, ip, sizeof(struct scm_hand_shake))) {
		return -EFAULT;
	}

	/* Validate number of args */
	no_of_args = scm_data.arginfo & 0x0F;
	if (no_of_args > MAX_QCOM_SCM_ARGS)
		return -EINVAL;

	/* Save dmabuf fd before it's translated into pa */
	u64 dmabuf_fd = scm_data.args_buffer[0];

	/* Translate the fd in scm_data.args_buffer[0] to pa */
	no_dma_fds = get_dmafd_args(scm_data.svc, scm_data.cmd, dma_fd_args);
	if (no_dma_fds > 0) {
		ret = translate_fd_to_pa(dma_fd_args, no_dma_fds, scm_data.args_buffer);
		if (ret)
			return ret;
	}

	u64 in_buf = scm_data.args_buffer[0];
	u64 in_buf_size = scm_data.args_buffer[1];
	u64 out_buf = scm_data.args_buffer[0] + scm_data.args_buffer[1];
	u64 out_buf_size = scm_data.args_buffer[2];

#if (LINUX_VERSION_CODE != KERNEL_VERSION(5, 14, 0))
	if (scm_data.cmd == QCOM_SCM_SMCINVOKE) {
		scm_data.ret = qcom_scm_invoke_smc(in_buf, in_buf_size, out_buf, out_buf_size, (int32_t *)&res.result[1], &res.result[0], (unsigned int *)&res.result[2]);
	} else if (scm_data.cmd == QCOM_SCM_SMCINVOKE_CB_RESP) {
		ret = scm_user_intf_dmabuf_map(dmabuf_fd, &dma_buf_info);
		if (ret) {
			dev_err(__scm_dev->dev, "scm_smcinvoke_ioctl: scm_user_intf_dmabuf_map failed for fd=0x%llx, ret = %d\n", dmabuf_fd, ret);
			return ret;
		}

		ret = scm_smcinvoke_memobj_update_paddr(&scm_data, &dma_buf_info);
		if (ret) {
			dev_err(__scm_dev->dev, "scm_smcinvoke_memobj_update_paddr failed for fd=0x%llx, rv = %d\n", dmabuf_fd, ret);
			scm_smcinvoke_memobj_clear_paddr(&scm_data, &dma_buf_info);
			scm_user_intf_dmabuf_unmap(&dma_buf_info);
			return ret;
		}

		scm_data.ret = qcom_scm_invoke_callback_response(out_buf, out_buf_size, (int32_t *)&res.result[1], &res.result[0], (unsigned int *)&res.result[2]);
		scm_user_intf_dmabuf_unmap(&dma_buf_info);
	} else {
		return -EFAULT;
	}
#else
	desc.svc = scm_data.svc;
	desc.cmd = scm_data.cmd;
	desc.owner = ARM_SMCCC_OWNER_TRUSTED_OS;
	desc.arginfo = scm_data.arginfo;

	if (scm_data.cmd == QCOM_SCM_SMCINVOKE) {
		desc.args[0] = in_buf;
		desc.args[1] = in_buf_size;
		desc.args[2] = out_buf;
		desc.args[3] = out_buf_size;
	} else if (scm_data.cmd == QCOM_SCM_SMCINVOKE_CB_RESP) {
		desc.args[0] = out_buf;
		desc.args[1] = out_buf_size;

		ret = scm_user_intf_dmabuf_map(dmabuf_fd, &dma_buf_info);
		if (ret) {
			dev_err(__scm_dev->dev, "scm_smcinvoke_ioctl: scm_user_intf_dmabuf_map failed for fd=0x%llx, ret = %d\n", dmabuf_fd, ret);
			return ret;
		}

		ret = scm_smcinvoke_memobj_update_paddr(&scm_data, &dma_buf_info);
		if (ret) {
			dev_err(__scm_dev->dev, "scm_smcinvoke_memobj_update_paddr failed for fd=0x%llx, rv = %d\n", dmabuf_fd, ret);
			scm_smcinvoke_memobj_clear_paddr(&scm_data, &dma_buf_info);
			scm_user_intf_dmabuf_unmap(&dma_buf_info);
			return ret;
		}
		scm_user_intf_dmabuf_unmap(&dma_buf_info);
	} else {
		return -EFAULT;
	}

	scm_data.ret = qcom_scm_call(__scm_dev->dev, &desc, &res);
#endif

	for (i = 0; i < MAX_QCOM_SCM_RETS; i++) {
		scm_data.qcom_scm_res[i] = res.result[i];
	}

	if (copy_to_user(ip, &scm_data, sizeof(struct scm_hand_shake))) {
		return -EFAULT;
	}

	if (scm_data.ret) {
		dev_err(__scm_dev->dev, "scm smcinvoke ioctl failed - ret : %d\n", scm_data.ret);
		return scm_data.ret;
	}

	return ret;
}

static long scm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (cmd != SCM_HAND_SHAKE_IOCTL)
		return -EFAULT;

	return scm_smcinvoke_ioctl(file, cmd, arg);
}

static const struct file_operations qcom_scm_fops = {
	.open = qcom_scm_open,
	.unlocked_ioctl = scm_ioctl,
};

/**
 * @brief Get drvdata of the scm driver.
 */
static int qcom_scm_get_scm_drvdata(void **scm_drvdata)
{
	struct platform_device *pdev = NULL;
	struct device_node* np = NULL;

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

	*scm_drvdata = platform_get_drvdata(pdev);

	return 0;
}

static int qcom_scm_intf_probe(struct platform_device *pdev)
{
	uint32_t ns_vm_perms[] = {QCOM_SCM_PERM_RW};
	uint32_t ns_vmids[] = {QCOM_SCM_VMID_HLOS};
	struct scm_dev_data *dev_data = NULL;
	struct device_node *mem_node = NULL;
	struct reserved_mem *rmem = NULL;
	struct device *dev = &pdev->dev;
	static void *scm_drvdata = NULL;
	uint32_t ns_vmid_num = 1;
	int ret = 0;

	if (!qcom_scm_is_available())
		return dev_err_probe(dev, -EPROBE_DEFER, "qcom_scm is not up!\n");

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(dev, "dma_set_mask_and_coherent failed %d\n", ret);
		return ret;
	}

	dev_data = devm_kzalloc(dev, sizeof(struct scm_dev_data), GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	dev_data->scm_pdev = pdev;
	dev_data->dev = &pdev->dev;

	dev_data->scm_miscdev.minor = MISC_DYNAMIC_MINOR;
	dev_data->scm_miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						    "scmnode_sec");
	dev_data->scm_miscdev.fops = &qcom_scm_fops;
	ret = misc_register(&dev_data->scm_miscdev);
	if (ret) {
		dev_err(dev, "failed to register misc device. err %d\n", ret);
		return ret;
	}

	__scm_dev = dev_data;
	__scm_dev->dev = dev_data->dev;

	/* Set the drvdata to be the same as the scm drvdata, which is expected by the
	 * ADCI mechanism.
	 */
	ret = qcom_scm_get_scm_drvdata(&scm_drvdata);
	if (ret) {
		dev_err(dev, "failed to get scm drvdata. err %d\n", ret);
		misc_deregister(&dev_data->scm_miscdev);
		return ret;
	}
	platform_set_drvdata(pdev, scm_drvdata);


#ifndef CONFIG_SAFELINUX_KERNEL
	/*
	 * Register the tzcom heap in tzmem.
	 * In case memory-region is not found it means that the registration
	 * is done somewhere else.
	 */
	mem_node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (mem_node) {
		rmem = of_reserved_mem_lookup(mem_node);
		of_node_put(mem_node);
		if (!rmem) {
			dev_err(dev, "%s: Failed to get memory-region node\n", __func__);
			misc_deregister(&dev_data->scm_miscdev);
			return -EINVAL;
		}

		ret = qcom_tzmem_register(rmem->base,
				   rmem->size,
				   ns_vmids,
				   ns_vm_perms,
				   ns_vmid_num,
				   QCOM_SCM_PERM_RW,
				   &g_tzmem_handle);
		if (ret) {
			dev_err(dev, "%s: Failed to tzmem register. err %d\n", __func__, ret);
			misc_deregister(&dev_data->scm_miscdev);
			return ret;
		}
	}
#endif

	return 0;
}

static int qcom_scm_intf_remove(struct platform_device *pdev)
{
	struct scm_dev_data *dev_data = __scm_dev;

#ifndef CONFIG_SAFELINUX_KERNEL
	if (g_tzmem_handle) {
		qcom_tzmem_deregister(g_tzmem_handle);
	}
#endif

	misc_deregister(&dev_data->scm_miscdev);

	return 0;
}

static const struct of_device_id qcom_scm_intf_dt_match[] = {
	{.compatible = "qcom,scm-user-intf-sec",
	 },
	{}
};

MODULE_DEVICE_TABLE(of, qcom_scm_intf_dt_match);

static struct platform_driver qcom_scm_intf_sec_driver = {
	.driver = {
		   .name = "qcom_scm_intf_sec",
		   .of_match_table = qcom_scm_intf_dt_match,
		   .suppress_bind_attrs = true,
		   },
	.probe = qcom_scm_intf_probe,
	.remove = qcom_scm_intf_remove,
};

module_platform_driver(qcom_scm_intf_sec_driver);
MODULE_IMPORT_NS(DMA_BUF);
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. user interface Security SCM driver");
MODULE_LICENSE("GPL v2");

