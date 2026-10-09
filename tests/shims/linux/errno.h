/* SPDX-License-Identifier: GPL-2.0-only */
#include <errno.h>
/* glibc's errno.h itself includes linux/errno.h; avoid that shim recursion. */
#ifndef EINVAL
#define EINVAL 22
#endif
