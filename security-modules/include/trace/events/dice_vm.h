/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Trace events for the DICE VM driver.
 *
 * Usage (ftrace):
 *   echo 1 > /sys/kernel/debug/tracing/events/dice_vm/enable
 *   cat /sys/kernel/debug/tracing/trace
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM dice_vm

#if !defined(_TRACE_DICE_VM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_DICE_VM_H

#include <linux/tracepoint.h>
#include <linux/types.h>

/*
 * Fired at the end of dice_vm_read().
 * Captures how many bytes userspace requested and what was returned.
 */
TRACE_EVENT(dice_vm_read,
	TP_PROTO(const char *vm_name, size_t len, ssize_t ret),
	TP_ARGS(vm_name, len, ret),
	TP_STRUCT__entry(
		__string(vm_name,	vm_name)
		__field(size_t,		len)
		__field(ssize_t,	ret)
	),
	TP_fast_assign(
		__assign_str(vm_name, vm_name);
		__entry->len	= len;
		__entry->ret	= ret;
	),
	TP_printk("vm_name=%s len=%zu ret=%zd",
		__get_str(vm_name), __entry->len, __entry->ret)
);

/*
 * Fired at the end of dice_vm_mmap().
 * Captures the requested mapping size, the actual carveout size, and the
 * result — letting you see size mismatches and remap_pfn_range failures.
 */
TRACE_EVENT(dice_vm_mmap,
	TP_PROTO(const char *vm_name, size_t requested, u64 carveout, int ret),
	TP_ARGS(vm_name, requested, carveout, ret),
	TP_STRUCT__entry(
		__string(vm_name,	vm_name)
		__field(size_t,		requested)
		__field(u64,		carveout)
		__field(int,		ret)
	),
	TP_fast_assign(
		__assign_str(vm_name, vm_name);
		__entry->requested	= requested;
		__entry->carveout	= carveout;
		__entry->ret		= ret;
	),
	TP_printk("vm_name=%s requested=0x%zx carveout=0x%llx ret=%d",
		__get_str(vm_name), __entry->requested,
		__entry->carveout, __entry->ret)
);

#endif /* _TRACE_DICE_VM_H */

/* Must be outside the header guard. */
#include <trace/define_trace.h>
