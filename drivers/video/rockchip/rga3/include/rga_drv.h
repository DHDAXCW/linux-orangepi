/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author: Huang Lee <Putin.li@rock-chips.com>
 */

#ifndef __LINUX_RGA_DRV_H_
#define __LINUX_RGA_DRV_H_

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-cache.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/wakelock.h>

#include <asm/cacheflush.h>

#include "rga.h"
#include "rga_debugger.h"

#define RGA_CORE_REG_OFFSET 0x10000

/* sample interval: 100ms */
#define RGA_LOAD_INTERVAL 100000

#if ((defined(CONFIG_RK_IOMMU) || defined(CONFIG_ROCKCHIP_IOMMU)) \
	&& defined(CONFIG_ION_ROCKCHIP))
#define CONFIG_RGA_IOMMU
#endif

/* Driver information */
#define DRIVER_DESC		"RGA multicore Device Driver"
#define DRIVER_NAME		"rga_multicore"
#define DRIVER_VERSION		"1.1.1"
#define RGA3_VERSION		"2.000"

/* time limit */
#define RGA_ASYNC_TIMEOUT_DELAY		HZ
#define RGA_RESET_TIMEOUT			1000

#define RGA_MAX_SCHEDULER	3
#define RGA_MAX_BUS_CLK		10

#define RGA_BUFFER_POOL_MAX_SIZE	64

#ifndef MIN
#define MIN(X, Y)		 ((X) < (Y) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y)		 ((X) > (Y) ? (X) : (Y))
#endif

#ifndef ABS
#define ABS(X)			 (((X) < 0) ? (-(X)) : (X))
#endif

#ifndef CLIP
#define CLIP(x, a, b)	 (((x) < (a)) \
	? (a) : (((x) > (b)) ? (b) : (x)))
#endif

enum {
	RGA3_SCHEDULER_CORE0		= 1 << 0,
	RGA3_SCHEDULER_CORE1		= 1 << 1,
	RGA2_SCHEDULER_CORE0		= 1 << 2,
	RGA_CORE_MASK			 = 0x7,
	RGA_NONE_CORE			 = 0x0,
};

struct rga_fence_context {
	unsigned int context;
	unsigned int seqno;
	spinlock_t spinlock;
};

struct rga_fence_waiter {
	/* Base sync driver waiter structure */
	struct dma_fence_cb waiter;

	struct rga_job *job;
};

struct rga_dma_buffer_t {
	/* DMABUF information */
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;

	dma_addr_t iova;
	unsigned long size;
	void *vaddr;
	enum dma_data_direction dir;

	/* It indicates whether the buffer is cached */
	bool cached;

	struct list_head link;
	struct kref refcount;

	/*
	 * use dma_buf directly,
	 * do not call dma_buf_put, such as mpi
	 */
	bool use_dma_buf;
};

struct rga_dma_buffer_pool {
	int __user *fd;
	int size;
};

struct rga_dma_session {
	struct mutex lock;
	/* cached dma buffer list */
	struct list_head cached_list;

	/* the count of buffer in the cached_list */
	int buffer_count;
};

/*
 * yqw add:
 * In order to use the virtual address to refresh the cache,
 * it may be merged into sgt later.
 */
struct rga2_mmu_other_t {
	uint32_t *MMU_src0_base;
	uint32_t *MMU_src1_base;
	uint32_t *MMU_dst_base;
	uint32_t MMU_src0_count;
	uint32_t MMU_src1_count;
	uint32_t MMU_dst_count;

	uint32_t MMU_len;
	bool MMU_map;
};

struct rga_job {
	struct list_head head;
	struct rga_req rga_command_base;
	uint32_t cmd_reg[32 * 8];
	uint32_t csc_reg[12];

	struct rga_dma_buffer_t *rga_dma_buffer_src0;
	struct rga_dma_buffer_t *rga_dma_buffer_src1;
	struct rga_dma_buffer_t *rga_dma_buffer_dst;
	/* used by rga2 */
	struct rga_dma_buffer_t *rga_dma_buffer_els;

	struct dma_buf *dma_buf_src0;
	struct dma_buf *dma_buf_src1;
	struct dma_buf *dma_buf_dst;
	struct dma_buf *dma_buf_els;

	struct rga2_mmu_other_t vir_page_table;

	struct dma_fence *out_fence;
	struct dma_fence *in_fence;
	spinlock_t fence_lock;
	ktime_t timestamp;
	unsigned int flags;
	int job_id;
	int priority;
	int core;
	int ret;
};

struct rga_scheduler_t;

struct rga_backend_ops {
	int (*get_version)(struct rga_scheduler_t *scheduler);
	int (*set_reg)(struct rga_job *job, struct rga_scheduler_t *scheduler);
	int (*init_reg)(struct rga_job *job);
	void (*soft_reset)(struct rga_scheduler_t *scheduler);
};

struct rga_scheduler_t {
	struct device *dev;
	void __iomem *rga_base;

	struct clk *clks[RGA_MAX_BUS_CLK];
	int num_clks;

	struct rga_job *running_job;
	struct list_head todo_list;
	spinlock_t irq_lock;
	wait_queue_head_t job_done_wq;
	const struct rga_backend_ops *ops;
	const struct rga_hw_data *data;
	int job_count;
	int irq;
	char version[16];
	int core;
	unsigned int core_offset;
};

struct rga_drvdata_t {
	struct miscdevice miscdev;

	struct rga_fence_context *fence_ctx;

	struct mutex mutex; // mutex

	/* used by rga2's mmu lock */
	struct mutex lock;

	struct rga_scheduler_t *rga_scheduler[RGA_MAX_SCHEDULER];
	int num_of_scheduler;

	struct delayed_work power_off_work;
	struct wake_lock wake_lock;

	struct rga_dma_session *dma_session;

#ifdef CONFIG_ROCKCHIP_RGA_DEBUGGER
	struct rga_debugger *debugger;
#endif
};

struct rga_irqs_data_t {
	const char *name;
	irqreturn_t (*irq_hdl)(int irq, void *ctx);
	irqreturn_t (*irq_thread)(int irq, void *ctx);
};

struct rga_match_data_t {
	const char * const *clks;
	int num_clks;
	const struct rga_irqs_data_t *irqs;
	int num_irqs;
};

static inline int rga_read(int offset, struct rga_scheduler_t *rga_scheduler)
{
	return readl(rga_scheduler->rga_base + offset);
}

static inline void rga_write(int value, int offset, struct rga_scheduler_t *rga_scheduler)
{
	writel(value, rga_scheduler->rga_base + offset);
}

#endif /* __LINUX_RGA_FENCE_H_ */