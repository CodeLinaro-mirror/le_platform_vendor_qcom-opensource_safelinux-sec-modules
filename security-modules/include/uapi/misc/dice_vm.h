/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

/**
 * Qualcomm DICE VM UAPI
 * Provides metadata for DICE VM handover to userspace.
 */

#ifndef _UAPI_MISC_DICE_VM_H
#define _UAPI_MISC_DICE_VM_H

#include <linux/types.h>

#define MAX_NAME 32
#define DRIVER_NAME "dice-vm"

/**
 * struct dice_vm_info - Metadata for DICE VM handover
 * @max_handover_size: Max size of the reserved memory region (bytes)
 * @fw_name: Firmware image name for VM loader
 * @pas_id: PIL image ID for VM loader
 */
typedef struct dice_vm_info {
	unsigned long max_handover_size;
	char fw_name[MAX_NAME];
	int pas_id;
} dice_vm_info_t;

#endif /* _UAPI_MISC_DICE_VM_H */
