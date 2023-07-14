/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "rollback.h"
#include "rollback_private.h"
#include "string.h"
#include "system.h"
#include "test_util.h"

static const uint32_t VALID_ROLLBACK_COOKIE = 0x0b112233;
static const uint32_t UNINITIALIZED_ROLLBACK_COOKIE = 0xffffffff;

static const uint8_t FAKE_ENTROPY[] = { 0xff, 0xff, 0xff, 0xff };

/*
 * Generated by concatenating 32-bytes (256-bits) of zeros with the 4 bytes
 * of FAKE_ENTROPY and computing SHA256 sum:
 *
 * echo -n -e '\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'\
 * '\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'\
 * '\xFF\xFF\xFF\xFF'  | sha256sum
 *
 * 890ed82cf09f22243bdc4252e4d79c8a9810c1391f455dce37a7b732eb0a0e4f
 */
#define EXPECTED_SECRET                                                     \
	0x89, 0x0e, 0xd8, 0x2c, 0xf0, 0x9f, 0x22, 0x24, 0x3b, 0xdc, 0x42,   \
		0x52, 0xe4, 0xd7, 0x9c, 0x8a, 0x98, 0x10, 0xc1, 0x39, 0x1f, \
		0x45, 0x5d, 0xce, 0x37, 0xa7, 0xb7, 0x32, 0xeb, 0x0a, 0x0e, \
		0x4f
__maybe_unused static const uint8_t _EXPECTED_SECRET[] = { EXPECTED_SECRET };
BUILD_ASSERT(sizeof(_EXPECTED_SECRET) == CONFIG_ROLLBACK_SECRET_SIZE);

/*
 * Generated by concatenating 32-bytes (256-bits) of EXPECTED_SECRET with the 4
 * bytes of FAKE_ENTROPY and computing SHA256 sum:
 *
 * echo -n -e '\x89\x0e\xd8\x2c\xf0\x9f\x22\x24\x3b\xdc\x42\x52\xe4\xd7\x9c'\
 * '\x8a\x98\x10\xc1\x39\x1f\x45\x5d\xce\x37\xa7\xb7\x32\xeb\x0a\x0e\x4f\xFF'\
 * '\FF\xFF' | sha256sum
 *
 * b5d2c08b1f9109ac5c67de15486f0ac267ef9501bd9f646f4ea80085cb08284c
 */
#define EXPECTED_SECRET2                                                    \
	0xb5, 0xd2, 0xc0, 0x8b, 0x1f, 0x91, 0x09, 0xac, 0x5c, 0x67, 0xde,   \
		0x15, 0x48, 0x6f, 0x0a, 0xc2, 0x67, 0xef, 0x95, 0x01, 0xbd, \
		0x9f, 0x64, 0x6f, 0x4e, 0xa8, 0x00, 0x85, 0xcb, 0x08, 0x28, \
		0x4c
__maybe_unused static const uint8_t _EXPECTED_SECRET2[] = { EXPECTED_SECRET2 };
BUILD_ASSERT(sizeof(_EXPECTED_SECRET2) == CONFIG_ROLLBACK_SECRET_SIZE);

#define EXPECTED_UNINITIALIZED_ROLLBACK_SECRET                              \
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   \
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
		0xff,
__maybe_unused static const uint8_t _EXPECTED_UNINITIALIZED_ROLLBACK_SECRET[] = {
	EXPECTED_UNINITIALIZED_ROLLBACK_SECRET
};
BUILD_ASSERT(sizeof(_EXPECTED_UNINITIALIZED_ROLLBACK_SECRET) ==
	     CONFIG_ROLLBACK_SECRET_SIZE);

test_static void print_rollback(const struct rollback_data *rb_data)
{
	int i;

	ccprintf("rollback secret: 0x");
	for (i = 0; i < sizeof(rb_data->secret); i++)
		ccprintf("%02x", rb_data->secret[i]);
	ccprintf("\n");

	ccprintf("rollback id: %d\n", rb_data->id);
	ccprintf("rollback cookie: %0x\n", rb_data->cookie);
	ccprintf("rollback_min_version: %d\n", rb_data->rollback_min_version);
}

test_static int check_equal(const struct rollback_data *actual,
			    const struct rollback_data *expected)
{
	int rv = memcmp(actual->secret, expected->secret,
			sizeof(actual->secret));
	TEST_EQ(rv, 0, "%d");
	TEST_EQ(actual->rollback_min_version, expected->rollback_min_version,
		"%d");
	TEST_EQ(actual->id, expected->id, "%d");
	TEST_EQ(actual->cookie, expected->cookie, "%d");
	return EC_SUCCESS;
}

test_static int test_add_entropy(void)
{
	int rv;
	struct rollback_data rb_data;

	const struct rollback_data expected_empty = {
		.id = 0,
		.rollback_min_version = 0,
		.secret = { 0 },
		.cookie = VALID_ROLLBACK_COOKIE
	};

	const struct rollback_data expected_uninitialized = {
		.id = -1,
		.rollback_min_version = -1,
		.secret = { EXPECTED_UNINITIALIZED_ROLLBACK_SECRET },
		.cookie = UNINITIALIZED_ROLLBACK_COOKIE
	};

	const struct rollback_data expected_secret = {
		.id = 1,
		.rollback_min_version = 0,
		.secret = { EXPECTED_SECRET },
		.cookie = VALID_ROLLBACK_COOKIE
	};

	const struct rollback_data expected_secret2 = {
		.id = 2,
		.rollback_min_version = 0,
		.secret = { EXPECTED_SECRET2 },
		.cookie = VALID_ROLLBACK_COOKIE
	};

	if (system_get_image_copy() != EC_IMAGE_RO) {
		ccprintf("This test is only works when running RO\n");
		return EC_ERROR_UNKNOWN;
	}

	/*
	 * After flashing both rollback regions will be uninitialized (all
	 * 0xFF). During the boot process, we expect region 0 to be initialized
	 * by the call to rollback_get_minimum_version().
	 */
	rv = read_rollback(0, &rb_data);
	TEST_EQ(rv, EC_SUCCESS, "%d");
	TEST_EQ(check_equal(&rb_data, &expected_empty), EC_SUCCESS, "%d");

	/* Immediately after boot region 1 should not yet be initialized. */
	rv = read_rollback(1, &rb_data);
	TEST_EQ(rv, EC_SUCCESS, "%d");
	TEST_EQ(check_equal(&rb_data, &expected_uninitialized), EC_SUCCESS,
		"%d");

	/*
	 * Add entropy. The result should end up being written to the unused
	 * region (region 1).
	 */
	if (IS_ENABLED(SECTION_IS_RO)) {
		rv = rollback_add_entropy(FAKE_ENTROPY, sizeof(FAKE_ENTROPY));
		TEST_EQ(rv, EC_SUCCESS, "%d");
	}

	/* Validate that region 1 has been updated correctly. */
	rv = read_rollback(1, &rb_data);
	TEST_EQ(rv, EC_SUCCESS, "%d");
	TEST_EQ(check_equal(&rb_data, &expected_secret), EC_SUCCESS, "%d");

	/* Validate that region 0 has not changed. */
	rv = read_rollback(0, &rb_data);
	TEST_EQ(rv, EC_SUCCESS, "%d");
	TEST_EQ(check_equal(&rb_data, &expected_empty), EC_SUCCESS, "%d");

	/*
	 * Add more entropy. The result should now end up being written to
	 * region 0.
	 */
	if (IS_ENABLED(SECTION_IS_RO)) {
		rv = rollback_add_entropy(FAKE_ENTROPY, sizeof(FAKE_ENTROPY));
		TEST_EQ(rv, EC_SUCCESS, "%d");
	}

	/* Check region 0. */
	rv = read_rollback(0, &rb_data);
	TEST_EQ(rv, EC_SUCCESS, "%d");
	TEST_EQ(check_equal(&rb_data, &expected_secret2), EC_SUCCESS, "%d");

	/* Check region 1 has not changed. */
	rv = read_rollback(1, &rb_data);
	TEST_EQ(rv, EC_SUCCESS, "%d");
	TEST_EQ(check_equal(&rb_data, &expected_secret), EC_SUCCESS, "%d");

	return rv;
}

void run_test(int argc, const char **argv)
{
	ccprintf("Running rollback_entropy test\n");
	RUN_TEST(test_add_entropy);
	test_print_result();
}
