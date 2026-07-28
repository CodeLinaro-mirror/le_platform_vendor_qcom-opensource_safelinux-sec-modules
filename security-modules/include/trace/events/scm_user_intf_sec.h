/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Trace events for the SCM user interface security driver.
 *
 * Usage (ftrace):
 *   echo 1 > /sys/kernel/debug/tracing/events/scm_user_intf_sec/enable
 *   cat /sys/kernel/debug/tracing/trace
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM scm_user_intf_sec

#if !defined(_TRACE_SCM_USER_INTF_SEC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCM_USER_INTF_SEC_H

#include <linux/tracepoint.h>
#include <linux/types.h>

/*
 * Fired at the end of scm_smcinvoke_ioctl().
 * Captures the SCM svc/cmd, the dma-buf fd and buffer sizes involved, the
 * TZ-side return code, and the ioctl's own return value.
 */
TRACE_EVENT(scm_smcinvoke_ioctl,
	TP_PROTO(u32 svc, u32 cmd, u64 dmabuf_fd, u64 in_buf_size,
		 u64 out_buf_size, u32 scm_ret, int ret),
	TP_ARGS(svc, cmd, dmabuf_fd, in_buf_size, out_buf_size, scm_ret, ret),
	TP_STRUCT__entry(
		__field(u32,	svc)
		__field(u32,	cmd)
		__field(u64,	dmabuf_fd)
		__field(u64,	in_buf_size)
		__field(u64,	out_buf_size)
		__field(u32,	scm_ret)
		__field(int,	ret)
	),
	TP_fast_assign(
		__entry->svc		= svc;
		__entry->cmd		= cmd;
		__entry->dmabuf_fd	= dmabuf_fd;
		__entry->in_buf_size	= in_buf_size;
		__entry->out_buf_size	= out_buf_size;
		__entry->scm_ret	= scm_ret;
		__entry->ret		= ret;
	),
	TP_printk("svc=0x%x cmd=0x%x dmabuf_fd=0x%llx in_buf_size=0x%llx out_buf_size=0x%llx scm_ret=%u ret=%d",
		__entry->svc, __entry->cmd, __entry->dmabuf_fd,
		__entry->in_buf_size, __entry->out_buf_size,
		__entry->scm_ret, __entry->ret)
);

/*
 * Fired at the end of scm_user_intf_dmabuf_map_pa().
 * Captures the dma-buf fd, the resolved physical address and length, and
 * the result — letting you see fd-to-PA translation failures across all
 * three call sites (translate_fd_to_pa, scm_user_intf_dmabuf_map, and the
 * per-mem-obj loop in scm_smcinvoke_memobj_update_paddr).
 */
TRACE_EVENT(scm_dmabuf_map_pa,
	TP_PROTO(int fd, u64 p_addr, size_t p_addr_len, int ret),
	TP_ARGS(fd, p_addr, p_addr_len, ret),
	TP_STRUCT__entry(
		__field(int,		fd)
		__field(u64,		p_addr)
		__field(size_t,		p_addr_len)
		__field(int,		ret)
	),
	TP_fast_assign(
		__entry->fd		= fd;
		__entry->p_addr		= p_addr;
		__entry->p_addr_len	= p_addr_len;
		__entry->ret		= ret;
	),
	TP_printk("fd=0x%x p_addr=0x%llx p_addr_len=0x%zx ret=%d",
		__entry->fd, __entry->p_addr, __entry->p_addr_len, __entry->ret)
);

#endif /* _TRACE_SCM_USER_INTF_SEC_H */

/* Must be outside the header guard. */
#include <trace/define_trace.h>
