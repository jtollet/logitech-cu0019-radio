/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef HID_LOGITECH_CU0019_H
#define HID_LOGITECH_CU0019_H

#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/unaligned.h>

#define CU0019_VENDOR_ID 0x046d
#define CU0019_PRODUCT_ID 0xc542
#define CU0019_BCD_DEVICE 0x0302
#define CU0019_REPORT_ID 5
#define CU0019_REPORT_SIZE 8
#define CU0019_READ_OPCODE 0xc0

#define CU0019_REG_DMA 0x0508
#define CU0019_REG_RF_CONFIG 0x0438
#define CU0019_REG_TICK 0x0740
#define CU0019_REG_PLL 0x04d4
#define CU0019_REG_RSSI 0x0458

static inline bool cu0019_address_allowed(u16 address)
{
	return address == CU0019_REG_DMA || address == CU0019_REG_RF_CONFIG ||
	       address == CU0019_REG_TICK || address == CU0019_REG_PLL ||
	       address == CU0019_REG_RSSI;
}

/* SET_REPORT submits a firmware read request, not a register write. */
static inline int cu0019_build_read_request(u16 address, u8 *buffer)
{
	if (!cu0019_address_allowed(address))
		return -EINVAL;

	memset(buffer, 0, CU0019_REPORT_SIZE);
	buffer[0] = CU0019_REPORT_ID;
	buffer[1] = CU0019_READ_OPCODE;
	put_unaligned_le16(address, buffer + 2);
	return 0;
}

/* Replies contain four data bytes followed by this firmware signature. */
static inline bool cu0019_response_valid(const u8 *buffer, int length)
{
	return length == CU0019_REPORT_SIZE && buffer[4] == 0x03 &&
	       buffer[5] == 0x02 && buffer[6] == 0x21 && buffer[7] == 0x84;
}

static inline bool cu0019_dma_valid(const u8 *buffer)
{
	u16 address = get_unaligned_le16(buffer);

	return (address == 0x9468 || address == 0x94c8) &&
	       get_unaligned_le16(buffer + 2) == 0x0106;
}

static inline bool cu0019_rf_config_valid(const u8 *buffer)
{
	return buffer[0] == 0x37 && buffer[1] == 0x72 && buffer[2] == 0x77 &&
	       buffer[3] == 0xec;
}

static inline bool cu0019_pll_valid(const u8 *buffer)
{
	u16 frequency = get_unaligned_le16(buffer + 2);

	return frequency >= 2400 && frequency <= 2484;
}

#endif /* HID_LOGITECH_CU0019_H */
