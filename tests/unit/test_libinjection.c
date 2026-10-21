/* SPDX-License-Identifier: MIT */
/* The vendored libinjection is linked and is the expected release. */

#include "ig_test.h"
#include "libinjection.h"

IG_TEST(libinjection_is_linked_and_is_v4)
{
	IG_ASSERT_STR_EQ("4.0.0", libinjection_version());
}
