/* SPDX-License-Identifier: GPL-2.0-only */
/* Host-test accessors only; the kernel uses its own implementation. */
#ifndef CU0019_HOST_UNALIGNED_H
#define CU0019_HOST_UNALIGNED_H
#include <linux/types.h>
static inline u16 get_unaligned_le16(const void *pointer)
{
	const u8 *bytes = pointer;
	return bytes[0] | ((u16)bytes[1] << 8);
}
static inline void put_unaligned_le16(u16 value, void *pointer)
{
	u8 *bytes = pointer;
	bytes[0] = value & 0xff;
	bytes[1] = value >> 8;
}
#endif
