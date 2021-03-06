// Copyright (C) Simon A. F. Lund <simon.lund@samsung.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef __INTERNAL_XNVME_BE_LINUX_NIL_H
#define __INTERNAL_XNVME_BE_LINUX_NIL_H

#define XNVME_BE_LINUX_NIL_CTX_DEPTH_MAX 23

struct xnvme_async_ctx_nil {
	uint32_t depth;		///< IO depth
	uint32_t outstanding;	///< Outstanding IO on the context/ring/queue

	struct xnvme_req *reqs[XNVME_BE_LINUX_NIL_CTX_DEPTH_MAX];
};
XNVME_STATIC_ASSERT(
	sizeof(struct xnvme_async_ctx_nil) == XNVME_BE_ACTX_NBYTES,
	"Incorrect size"
)

#endif /* __INTERNAL_XNVME_BE_LINUX_NIL */
