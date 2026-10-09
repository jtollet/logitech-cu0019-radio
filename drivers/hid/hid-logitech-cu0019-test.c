// SPDX-License-Identifier: GPL-2.0-only

#include <kunit/test.h>
#include <linux/module.h>

#include "hid-logitech-cu0019.h"

static void cu0019_allowlist_test(struct kunit *test)
{
	unsigned int address;
	bool expected;

	for (address = 0; address <= 0xffff; address++) {
		expected = address == 0x0508 || address == 0x0438 ||
			   address == 0x0740 || address == 0x04d4 ||
			   address == 0x0458;
		KUNIT_EXPECT_EQ(test, cu0019_address_allowed(address),
				expected);
	}
}

static void cu0019_read_request_test(struct kunit *test)
{
	static const u8 expected[] = { 5, 0xc0, 0x58, 0x04, 0, 0, 0, 0 };
	u8 buffer[CU0019_REPORT_SIZE];
	u8 before[CU0019_REPORT_SIZE];
	unsigned int address;
	int ret;

	KUNIT_ASSERT_EQ(test, cu0019_build_read_request(0x0458, buffer), 0);
	KUNIT_EXPECT_MEMEQ(test, buffer, expected, sizeof(expected));
	for (address = 0; address <= 0xffff; address++) {
		memset(buffer, 0xa5, sizeof(buffer));
		memcpy(before, buffer, sizeof(before));
		ret = cu0019_build_read_request(address, buffer);
		if (cu0019_address_allowed(address)) {
			KUNIT_ASSERT_EQ(test, ret, 0);
			KUNIT_EXPECT_EQ(test, buffer[0], (u8)5);
			KUNIT_EXPECT_EQ(test, buffer[1], (u8)0xc0);
			KUNIT_EXPECT_EQ(test, get_unaligned_le16(buffer + 2),
					(u16)address);
			KUNIT_EXPECT_EQ(test, get_unaligned_le32(buffer + 4),
					0U);
		} else {
			KUNIT_EXPECT_EQ(test, ret, -EINVAL);
			KUNIT_EXPECT_MEMEQ(test, buffer, before,
					   sizeof(before));
		}
	}
}

static void cu0019_reply_length_test(struct kunit *test)
{
	u8 reply[] = { 0x12, 0x34, 0x56, 0x78, 3, 2, 0x21, 0x84 };
	int length;

	for (length = -1; length <= 9; length++)
		KUNIT_EXPECT_EQ(test, cu0019_response_valid(reply, length),
				length == CU0019_REPORT_SIZE);
}

static void cu0019_reply_signature_test(struct kunit *test)
{
	static const u8 signature[] = { 3, 2, 0x21, 0x84 };
	u8 reply[] = { 0x12, 0x34, 0x56, 0x78, 3, 2, 0x21, 0x84 };
	unsigned int i;
	unsigned int value;

	for (i = 0; i < sizeof(signature); i++) {
		for (value = 0; value <= 0xff; value++) {
			reply[4 + i] = value;
			KUNIT_EXPECT_EQ(test,
					cu0019_response_valid(reply,
							      sizeof(reply)),
					value == signature[i]);
		}
		reply[4 + i] = signature[i];
	}
}

static void cu0019_dma_test(struct kunit *test)
{
	u8 dma[] = { 0x68, 0x94, 6, 1 };
	unsigned int value;

	for (value = 0; value <= 0xffff; value++) {
		put_unaligned_le16(value, dma);
		KUNIT_EXPECT_EQ(test, cu0019_dma_valid(dma),
				value == 0x9468 || value == 0x94c8);
	}
	put_unaligned_le16(0x9468, dma);
	for (value = 0; value <= 0xffff; value++) {
		put_unaligned_le16(value, dma + 2);
		KUNIT_EXPECT_EQ(test, cu0019_dma_valid(dma), value == 0x0106);
	}
}

static void cu0019_rf_config_test(struct kunit *test)
{
	static const u8 expected[] = { 0x37, 0x72, 0x77, 0xec };
	u8 rf[] = { 0x37, 0x72, 0x77, 0xec };
	unsigned int i;
	unsigned int value;

	for (i = 0; i < sizeof(expected); i++) {
		for (value = 0; value <= 0xff; value++) {
			rf[i] = value;
			KUNIT_EXPECT_EQ(test, cu0019_rf_config_valid(rf),
					value == expected[i]);
		}
		rf[i] = expected[i];
	}
}

static void cu0019_pll_test(struct kunit *test)
{
	u8 pll[] = { 0, 0, 0, 0 };
	unsigned int value;

	for (value = 0; value <= 0xffff; value++) {
		put_unaligned_le16(value, pll + 2);
		KUNIT_EXPECT_EQ(test, cu0019_pll_valid(pll),
				value >= 2400 && value <= 2484);
	}
}

static struct kunit_case cu0019_test_cases[] = {
	KUNIT_CASE(cu0019_allowlist_test),
	KUNIT_CASE(cu0019_read_request_test),
	KUNIT_CASE(cu0019_reply_length_test),
	KUNIT_CASE(cu0019_reply_signature_test),
	KUNIT_CASE(cu0019_dma_test),
	KUNIT_CASE(cu0019_rf_config_test),
	KUNIT_CASE(cu0019_pll_test),
	{}
};

static struct kunit_suite cu0019_test_suite = {
	.name = "hid-logitech-cu0019-protocol",
	.test_cases = cu0019_test_cases,
};

kunit_test_suite(cu0019_test_suite);

MODULE_DESCRIPTION("KUnit tests for C-U0019 register read guards");
MODULE_LICENSE("GPL");
