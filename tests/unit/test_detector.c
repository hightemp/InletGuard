/* SPDX-License-Identifier: MIT */
/* The detector interface and the core detector registry (FR-2.4). */

#include "ig_test.h"
#include "core/ig_detector.h"

#include <string.h>

IG_TEST(detector_registry_holds_sqli_and_xss)
{
	IG_ASSERT_INT_EQ(2, ig_detector_count());
	IG_ASSERT_STR_EQ("sqli", ig_detector_at(0)->name);
	IG_ASSERT_INT_EQ(IG_DETECT_SQLI, ig_detector_at(0)->mask);
	IG_ASSERT_STR_EQ("xss", ig_detector_at(1)->name);
	IG_ASSERT_INT_EQ(IG_DETECT_XSS, ig_detector_at(1)->mask);
}

IG_TEST(detector_bits_are_stable)
{
	/* Public API values (InletGuard\SQLI, InletGuard\XSS): never renumber. */
	IG_ASSERT_INT_EQ(1, IG_DETECT_SQLI);
	IG_ASSERT_INT_EQ(2, IG_DETECT_XSS);
}

IG_TEST(detector_table_invariants)
{
	size_t i;
	size_t j;
	unsigned int seen = 0;

	for (i = 0; i < ig_detector_count(); i++) {
		const ig_detector_t *d = ig_detector_at(i);

		IG_ASSERT_TRUE(d != NULL);
		IG_ASSERT_TRUE(d->name != NULL && d->name[0] != '\0');
		IG_ASSERT_TRUE(d->detect != NULL);
		/* exactly one bit, not used by another detector */
		IG_ASSERT_TRUE(d->mask != 0 && (d->mask & (d->mask - 1)) == 0);
		IG_ASSERT_TRUE((seen & d->mask) == 0);
		seen |= d->mask;
		for (j = 0; j < i; j++) {
			IG_ASSERT_TRUE(strcmp(ig_detector_at(j)->name, d->name) != 0);
		}
	}
}

IG_TEST(detector_at_out_of_range_is_null)
{
	IG_ASSERT_TRUE(ig_detector_at(ig_detector_count()) == NULL);
	IG_ASSERT_TRUE(ig_detector_at((size_t)-1) == NULL);
}

IG_TEST(detector_find_by_name)
{
	IG_ASSERT_TRUE(ig_detector_find("sqli", 4) == ig_detector_at(0));
	IG_ASSERT_TRUE(ig_detector_find("xss", 3) == ig_detector_at(1));
	/* the length is explicit: a longer buffer with the name as prefix */
	IG_ASSERT_TRUE(ig_detector_find("xss,sqli", 3) == ig_detector_at(1));

	IG_ASSERT_TRUE(ig_detector_find("lfi", 3) == NULL);
	IG_ASSERT_TRUE(ig_detector_find("SQLI", 4) == NULL);
	IG_ASSERT_TRUE(ig_detector_find("sql", 3) == NULL);
	IG_ASSERT_TRUE(ig_detector_find("sqlix", 5) == NULL);
	IG_ASSERT_TRUE(ig_detector_find("", 0) == NULL);
	IG_ASSERT_TRUE(ig_detector_find(NULL, 4) == NULL);
}

IG_TEST(detector_find_by_mask)
{
	IG_ASSERT_TRUE(ig_detector_by_mask(IG_DETECT_SQLI) == ig_detector_at(0));
	IG_ASSERT_TRUE(ig_detector_by_mask(IG_DETECT_XSS) == ig_detector_at(1));

	IG_ASSERT_TRUE(ig_detector_by_mask(0) == NULL);
	IG_ASSERT_TRUE(ig_detector_by_mask(IG_DETECT_SQLI | IG_DETECT_XSS) == NULL);
	IG_ASSERT_TRUE(ig_detector_by_mask(1u << 30) == NULL);
}

IG_TEST(detector_detect_is_the_detector_function)
{
	ig_result_t direct;
	ig_result_t via;

	IG_ASSERT_INT_EQ(IG_ATTACK, ig_detector_find("sqli", 4)->detect("' OR 1=1--", 10, &via));
	ig_detect_sqli("' OR 1=1--", 10, &direct);
	IG_ASSERT_STR_EQ(direct.fingerprint, via.fingerprint);

	IG_ASSERT_INT_EQ(
		IG_ATTACK, ig_detector_find("xss", 3)->detect("<script>alert(1)</script>", 25, &via));
	IG_ASSERT_STR_EQ("html", via.fingerprint);
	IG_ASSERT_INT_EQ(IG_CLEAN, ig_detector_find("xss", 3)->detect("' OR 1=1--", 10, &via));
}
