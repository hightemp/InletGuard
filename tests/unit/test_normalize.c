/* SPDX-License-Identifier: MIT */
/* Normalizer: repeated URL decoding, %uXXXX, NUL removal (FR-1.3). */

#include "ig_test.h"
#include "core/ig_normalize.h"

#include <string.h>

/* decode1 <in> <flags> <expected>: one round into a separate buffer */
static int decode1(const char *in, unsigned int flags, const char *want)
{
	char out[128];
	size_t n = ig_url_decode(in, strlen(in), out, flags);

	if (n != strlen(want) || memcmp(out, want, n) != 0) {
		ig_test_fail(__FILE__, __LINE__, "decode \"%s\": got \"%.*s\" (%zu), expected \"%s\"", in,
			(int)(n < sizeof(out) ? n : 0), out, n, want);
		return 0;
	}
	return 1;
}

/* norm <in> <len> <rounds> <expected> <stats>: full normalization */
static int norm(
	const char *in, size_t len, unsigned int rounds, const char *want, ig_norm_stats_t *st)
{
	char out[128];
	size_t n = ig_normalize(in, len, rounds, IG_DECODE_PLUS, out, st);

	if (n != strlen(want) || memcmp(out, want, n) != 0) {
		ig_test_fail(__FILE__, __LINE__, "normalize \"%s\": got \"%.*s\" (%zu), expected \"%s\"",
			in, (int)(n < sizeof(out) ? n : 0), out, n, want);
		return 0;
	}
	return 1;
}

IG_TEST(url_decode_hex_in_both_cases)
{
	if (!decode1("%27%20OR%201%3d1", 0, "' OR 1=1")) {
		return;
	}
	if (!decode1("%3C%3c%3E%3e", 0, "<<>>")) {
		return;
	}
	decode1("plain", 0, "plain");
}

IG_TEST(url_decode_keeps_invalid_and_truncated_sequences)
{
	if (!decode1("%zz%g1%1g", 0, "%zz%g1%1g")) {
		return;
	}
	if (!decode1("100%", 0, "100%")) {
		return;
	}
	if (!decode1("a%4", 0, "a%4")) {
		return;
	}
	decode1("%%41", 0, "%A");
}

IG_TEST(url_decode_percent_u)
{
	if (!decode1("%u0041", 0, "A")) {
		return;
	}
	if (!decode1("%u0410%U0411", 0, "\xd0\x90\xd0\x91")) { /* "АБ" */
		return;
	}
	if (!decode1("%u20AC", 0, "\xe2\x82\xac")) { /* "€" */
		return;
	}
	if (!decode1("%u0027%u003c", 0, "'<")) {
		return;
	}
	/* surrogates, invalid hex and truncated forms are kept */
	if (!decode1("%uD800%udfff", 0, "%uD800%udfff")) {
		return;
	}
	if (!decode1("%u12g4", 0, "%u12g4")) {
		return;
	}
	decode1("%u123", 0, "%u123");
}

IG_TEST(url_decode_plus_only_with_flag)
{
	if (!decode1("1+OR+1", IG_DECODE_PLUS, "1 OR 1")) {
		return;
	}
	decode1("1+OR+1", 0, "1+OR+1");
}

IG_TEST(url_decode_in_place_and_empty)
{
	char buf[] = "%27%u0041+x";
	size_t n = ig_url_decode(buf, strlen(buf), buf, IG_DECODE_PLUS);

	IG_ASSERT_INT_EQ(4, n);
	IG_ASSERT_TRUE(memcmp(buf, "'A x", 4) == 0);
	IG_ASSERT_INT_EQ(0, ig_url_decode("", 0, buf, 0));
}

IG_TEST(normalize_decodes_double_encoding)
{
	ig_norm_stats_t st;

	if (!norm("%2527%2520OR%25201%253D1", 24, 2, "' OR 1=1", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(2, st.rounds);
	IG_ASSERT_FALSE(st.limit_hit);
}

IG_TEST(normalize_reports_the_round_limit)
{
	ig_norm_stats_t st;

	/* triple encoding with two rounds stops at %27 */
	if (!norm("%252527", 7, 2, "%27", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(2, st.rounds);
	IG_ASSERT_TRUE(st.limit_hit);
}

IG_TEST(normalize_stops_when_stable)
{
	ig_norm_stats_t st;

	if (!norm("%27x", 4, 5, "'x", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(1, st.rounds);
	IG_ASSERT_FALSE(st.limit_hit);

	if (!norm("plain text", 10, 5, "plain text", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(0, st.rounds);
}

IG_TEST(normalize_removes_nul_bytes)
{
	ig_norm_stats_t st;

	if (!norm("a\0b%00c", 7, 2, "abc", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(2, st.nul_removed);

	/* the NUL hides a sequence until it is removed: %<NUL>41 -> %41 -> A */
	if (!norm("%%0041", 6, 2, "A", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(1, st.nul_removed);
}

IG_TEST(normalize_with_zero_rounds_only_removes_nul)
{
	ig_norm_stats_t st;

	if (!norm("%27\0x", 5, 0, "%27x", &st)) {
		return;
	}
	IG_ASSERT_INT_EQ(0, st.rounds);
	IG_ASSERT_INT_EQ(1, st.nul_removed);
	IG_ASSERT_TRUE(st.limit_hit);
}

IG_TEST(normalize_in_place_and_without_stats)
{
	char buf[] = "%2527x";
	size_t n = ig_normalize(buf, 6, 2, 0, buf, NULL);

	IG_ASSERT_INT_EQ(2, n);
	IG_ASSERT_TRUE(memcmp(buf, "'x", 2) == 0);
}

IG_TEST(normalize_null_arguments)
{
	char out[8];
	ig_norm_stats_t st;

	IG_ASSERT_TRUE(ig_normalize(NULL, 3, 2, 0, out, &st) == IG_NORM_ERROR);
	IG_ASSERT_TRUE(ig_normalize("abc", 3, 2, 0, NULL, &st) == IG_NORM_ERROR);
	IG_ASSERT_INT_EQ(0, ig_normalize(NULL, 0, 2, 0, out, &st));
	IG_ASSERT_INT_EQ(0, st.rounds);
	IG_ASSERT_TRUE(ig_url_decode(NULL, 3, out, 0) == IG_NORM_ERROR);
	IG_ASSERT_TRUE(ig_url_decode("abc", 3, NULL, 0) == IG_NORM_ERROR);
}
