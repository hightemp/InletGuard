/* SPDX-License-Identifier: MIT */
/* Sanity checks for the unit test framework itself. */

#include "ig_test.h"

IG_TEST(framework_true_assertion_passes)
{
	IG_ASSERT_TRUE(1);
	IG_ASSERT_FALSE(0);
}

IG_TEST(framework_int_and_string_equality)
{
	IG_ASSERT_INT_EQ(42, 40 + 2);
	IG_ASSERT_STR_EQ("inletguard", "inletguard");
}
