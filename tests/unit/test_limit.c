/* SPDX-License-Identifier: MIT */
/* Length limit of an inspected value and the truncated flag (FR-1.4). */

#include "ig_test.h"
#include "core/ig_detector.h"

#include <string.h>

IG_TEST(limit_len_below_at_and_above)
{
	int t = -1;

	IG_ASSERT_INT_EQ(5, ig_limit_len(5, 8, &t));
	IG_ASSERT_INT_EQ(0, t);
	IG_ASSERT_INT_EQ(8, ig_limit_len(8, 8, &t));
	IG_ASSERT_INT_EQ(0, t);
	IG_ASSERT_INT_EQ(8, ig_limit_len(9, 8, &t));
	IG_ASSERT_INT_EQ(1, t);
	IG_ASSERT_INT_EQ(0, ig_limit_len(0, 8, &t));
	IG_ASSERT_INT_EQ(0, t);
}

IG_TEST(limit_len_zero_means_no_limit)
{
	int t = -1;

	IG_ASSERT_INT_EQ(100000, ig_limit_len(100000, 0, &t));
	IG_ASSERT_INT_EQ(0, t);
	IG_ASSERT_INT_EQ(8, ig_limit_len(9, 8, NULL));
}

IG_TEST(detector_run_finds_attack_in_prefix)
{
	const ig_detector_t *sqli = ig_detector_find("sqli", 4);
	const char *s = "' OR 1=1-- padding padding padding";
	ig_result_t direct;
	ig_result_t res;

	IG_ASSERT_INT_EQ(IG_ATTACK, ig_detector_run(sqli, s, strlen(s), 10, &res));
	IG_ASSERT_INT_EQ(1, res.truncated);
	ig_detect_sqli(s, 10, &direct);
	IG_ASSERT_STR_EQ(direct.fingerprint, res.fingerprint);
}

IG_TEST(detector_run_misses_attack_past_the_limit)
{
	const ig_detector_t *xss = ig_detector_find("xss", 3);
	const char *s = "hello world <script>alert(1)</script>";
	ig_result_t res;

	IG_ASSERT_INT_EQ(IG_CLEAN, ig_detector_run(xss, s, strlen(s), 11, &res));
	IG_ASSERT_INT_EQ(1, res.truncated);
	IG_ASSERT_INT_EQ(IG_ATTACK, ig_detector_run(xss, s, strlen(s), 0, &res));
	IG_ASSERT_INT_EQ(0, res.truncated);
}

IG_TEST(detector_run_short_value_is_not_truncated)
{
	const ig_detector_t *xss = ig_detector_find("xss", 3);
	ig_result_t res;

	res.truncated = 1;
	IG_ASSERT_INT_EQ(IG_ATTACK, ig_detector_run(xss, "<script>x</script>", 18, 8192, &res));
	IG_ASSERT_INT_EQ(0, res.truncated);
	IG_ASSERT_STR_EQ("html", res.fingerprint);
}

IG_TEST(detector_run_null_arguments)
{
	ig_result_t res;

	res.verdict = IG_CLEAN;
	IG_ASSERT_INT_EQ(IG_ERROR, ig_detector_run(NULL, "x", 1, 0, &res));
	IG_ASSERT_INT_EQ(IG_ERROR, res.verdict);
	IG_ASSERT_INT_EQ(0, res.truncated);
	IG_ASSERT_INT_EQ(IG_ERROR, ig_detector_run(ig_detector_at(0), "x", 1, 0, NULL));
}

IG_TEST(direct_detectors_reset_truncated)
{
	ig_result_t res;

	res.truncated = 1;
	ig_detect_sqli("abc", 3, &res);
	IG_ASSERT_INT_EQ(0, res.truncated);
	res.truncated = 1;
	ig_detect_xss("abc", 3, &res);
	IG_ASSERT_INT_EQ(0, res.truncated);
	res.truncated = 1;
	ig_detect_xss(NULL, 0, &res);
	IG_ASSERT_INT_EQ(0, res.truncated);
}
