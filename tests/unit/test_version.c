/* SPDX-License-Identifier: MIT */

#include "ig_test.h"
#include "core/ig_version.h"

#include <stdio.h>

IG_TEST(version_returns_version_macro)
{
	IG_ASSERT_STR_EQ(IG_VERSION, ig_version());
}

IG_TEST(version_is_built_from_components)
{
	char expected[64];

	snprintf(expected, sizeof(expected), "%d.%d.%d%s", IG_VERSION_MAJOR,
		IG_VERSION_MINOR, IG_VERSION_PATCH, IG_VERSION_SUFFIX);
	IG_ASSERT_STR_EQ(expected, ig_version());
}
