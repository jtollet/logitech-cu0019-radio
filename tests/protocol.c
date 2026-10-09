/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "hid-logitech-cu0019.h"

int main(void)
{
	u8 buffer[8], before[8];
	u8 reply[] = { 0x12, 0x34, 0x56, 0x78, 3, 2, 0x21, 0x84 };
	u8 signature[] = { 3, 2, 0x21, 0x84 };
	u8 dma[] = { 0x68, 0x94, 6, 1 };
	u8 rf[] = { 0x37, 0x72, 0x77, 0xec };
	u8 expected_rf[] = { 0x37, 0x72, 0x77, 0xec };
	u8 pll[4] = { 0 };
	unsigned int address, count = 0, i, value;
	int ret, length;
	bool expected;

	for (address = 0; address <= 0xffff; address++) {
		expected = address == 0x0508 || address == 0x0438 ||
			   address == 0x0740 || address == 0x04d4 ||
			   address == 0x0458;
		assert(cu0019_address_allowed(address) == expected);
		memset(buffer, 0xa5, sizeof(buffer));
		memcpy(before, buffer, sizeof(before));
		ret = cu0019_build_read_request(address, buffer);
		if (expected) {
			count++;
			assert(ret == 0);
			assert(buffer[0] == 5 && buffer[1] == 0xc0);
			assert(get_unaligned_le16(buffer + 2) == address);
			assert(!buffer[4] && !buffer[5] && !buffer[6] && !buffer[7]);
		} else {
			assert(ret == -EINVAL);
			assert(memcmp(buffer, before, sizeof(before)) == 0);
		}
	}
	assert(count == 5);
	for (length = -1; length <= 9; length++)
		assert(cu0019_response_valid(reply, length) == (length == 8));
	for (i = 0; i < sizeof(signature); i++) {
		for (value = 0; value <= 0xff; value++) {
			reply[4 + i] = value;
			assert(cu0019_response_valid(reply, 8) ==
			       (value == signature[i]));
		}
		reply[4 + i] = signature[i];
	}
	for (value = 0; value <= 0xffff; value++) {
		put_unaligned_le16(value, dma);
		assert(cu0019_dma_valid(dma) ==
		       (value == 0x9468 || value == 0x94c8));
	}
	put_unaligned_le16(0x9468, dma);
	for (value = 0; value <= 0xffff; value++) {
		put_unaligned_le16(value, dma + 2);
		assert(cu0019_dma_valid(dma) == (value == 0x0106));
	}
	for (i = 0; i < sizeof(rf); i++) {
		for (value = 0; value <= 0xff; value++) {
			rf[i] = value;
			assert(cu0019_rf_config_valid(rf) == (value == expected_rf[i]));
		}
		rf[i] = expected_rf[i];
	}
	for (value = 0; value <= 0xffff; value++) {
		put_unaligned_le16(value, pll + 2);
		assert(cu0019_pll_valid(pll) == (value >= 2400 && value <= 2484));
	}
	puts("PASS: exact kernel protocol helpers; exhaustive allowlist, request, reply and register guards");
	return 0;
}
