/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 *
 * Author: Huang Lee <Putin.li@rock-chips.com>
 */

#ifndef __LINUX_RKRGA_JOB_H_
#define __LINUX_RKRGA_JOB_H_

#include <linux/spinlock.h>
#include <linux/dma-fence.h>

#include "rga_drv.h"

#define RGA_JOB_DONE (1 << 0)
#define RGA_JOB_ASYNC (1 << 1)
#define RGA_JOB_SYNC (1 << 2)

void rga_job_done(struct rga_scheduler_t *rga_scheduler, int ret);
int rga_commit(struct rga_req *rga_command_base, int flags);

int rga_kernel_commit(struct rga_req *rga_command_base,
	struct rga_mpi_job_t *mpi_job, int flags);

#endif /* __LINUX_RKRGA_JOB_H_ */