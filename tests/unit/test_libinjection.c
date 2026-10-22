/* SPDX-License-Identifier: MIT */
/* The vendored libinjection is linked and is the expected release. */

#include "ig_test.h"
#include "libinjection.h"

IG_TEST(libinjection_is_linked_and_is_v4)
{
	IG_ASSERT_STR_EQ("4.0.0", libinjection_version());
}

/* ig_li_verdict(): libinjection results mapped to InletGuard verdicts. An
 * unknown value is an error, never "clean" (fail_mode decides, NFR-3). */

#include "core/ig_libinjection.h"

IG_TEST(libinjection_false_is_clean)
{
	IG_ASSERT_INT_EQ(IG_CLEAN, ig_li_verdict(LIBINJECTION_RESULT_FALSE));
}

IG_TEST(libinjection_true_is_attack)
{
	IG_ASSERT_INT_EQ(IG_ATTACK, ig_li_verdict(LIBINJECTION_RESULT_TRUE));
}

IG_TEST(libinjection_error_is_error)
{
	IG_ASSERT_INT_EQ(IG_ERROR, ig_li_verdict(LIBINJECTION_RESULT_ERROR));
}

IG_TEST(libinjection_unknown_result_is_error)
{
	IG_ASSERT_INT_EQ(IG_ERROR, ig_li_verdict((injection_result_t)2));
	IG_ASSERT_INT_EQ(IG_ERROR, ig_li_verdict((injection_result_t)-7));
}
