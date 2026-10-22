/* SPDX-License-Identifier: MIT */
/* ig_detect_sqli(): SQL injection detection over libinjection (FR-2.1). */

#include "ig_test.h"
#include "core/ig_detect.h"

#include <string.h>

/* Fingerprints of the vendored libinjection 4.0.0; they change only together
 * with a deliberate update of third_party/libinjection. */
static const struct {
	const char *payload;
	const char *fingerprint;
} attacks[] = {
	{"' OR 1=1--", "s&1c"},
	{"1' OR '1'='1", "s&sos"},
	{"1 UNION SELECT username, password FROM users", "1UEnk"},
	{"' UNION SELECT NULL--", "sUEvc"},
	{"1; DROP TABLE users", "1;Tnn"},
	{"'; DROP TABLE users; --", "s;Tnn"},
	{"admin'--", "sc"},
	{"1 AND SLEEP(5)", "1&f(1"},
	{"1' AND 1=1#", "s&1c"},
	{"-1 OR 1=1", "1&1"},
};

static const char *const benign[] = {
	"hello world",
	"O'Reilly",
	"John Smith",
	"123",
	"",
	"Привет, мир",
	"please select a product from the list",
	"Rock 'n' roll",
	"user@example.com",
	"2026-10-22",
	"I'm here",
	"1 or 2",
	"drop the table and union with friends",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

IG_TEST(sqli_detects_classic_payloads_with_fingerprint)
{
	size_t i;
	ig_result_t res;

	for (i = 0; i < COUNT(attacks); i++) {
		const char *p = attacks[i].payload;
		ig_verdict_t v = ig_detect_sqli(p, strlen(p), &res);

		if (v != IG_ATTACK || res.verdict != IG_ATTACK ||
			strcmp(res.fingerprint, attacks[i].fingerprint) != 0) {
			ig_test_fail(__FILE__, __LINE__,
				"\"%s\": verdict %d/%d, fingerprint \"%s\", expected \"%s\"", p, (int)v,
				(int)res.verdict, res.fingerprint, attacks[i].fingerprint);
			return;
		}
	}
}

IG_TEST(sqli_benign_strings_are_clean_with_empty_fingerprint)
{
	size_t i;
	ig_result_t res;

	for (i = 0; i < COUNT(benign); i++) {
		ig_verdict_t v = ig_detect_sqli(benign[i], strlen(benign[i]), &res);

		if (v != IG_CLEAN || res.verdict != IG_CLEAN || res.fingerprint[0] != '\0') {
			ig_test_fail(__FILE__, __LINE__, "\"%s\": verdict %d, fingerprint \"%s\"", benign[i],
				(int)v, res.fingerprint);
			return;
		}
	}
}

IG_TEST(sqli_embedded_nul_does_not_hide_an_attack)
{
	static const char p[] = "abc\0' OR 1=1--";
	ig_result_t res;

	IG_ASSERT_INT_EQ(IG_ATTACK, ig_detect_sqli(p, sizeof(p) - 1, &res));
	IG_ASSERT_STR_EQ("s&1c", res.fingerprint);
}

IG_TEST(sqli_fingerprint_is_cleared_when_result_is_reused)
{
	ig_result_t res;

	memset(&res, 'X', sizeof(res));
	IG_ASSERT_INT_EQ(IG_ATTACK, ig_detect_sqli("' OR 1=1--", 10, &res));
	IG_ASSERT_INT_EQ(IG_CLEAN, ig_detect_sqli("hello", 5, &res));
	IG_ASSERT_STR_EQ("", res.fingerprint);

	memset(&res, 'X', sizeof(res));
	IG_ASSERT_INT_EQ(IG_CLEAN, ig_detect_sqli("hello", 5, &res));
	IG_ASSERT_STR_EQ("", res.fingerprint);
}

IG_TEST(sqli_fingerprint_fits_the_buffer)
{
	ig_result_t res;

	IG_ASSERT_INT_EQ(8, IG_FINGERPRINT_SIZE);
	IG_ASSERT_INT_EQ(IG_FINGERPRINT_SIZE, sizeof(res.fingerprint));
}

IG_TEST(sqli_null_arguments)
{
	ig_result_t res;

	IG_ASSERT_INT_EQ(IG_ERROR, ig_detect_sqli("' OR 1=1--", 10, NULL));

	memset(&res, 'X', sizeof(res));
	IG_ASSERT_INT_EQ(IG_ERROR, ig_detect_sqli(NULL, 5, &res));
	IG_ASSERT_INT_EQ(IG_ERROR, res.verdict);
	IG_ASSERT_STR_EQ("", res.fingerprint);

	memset(&res, 'X', sizeof(res));
	IG_ASSERT_INT_EQ(IG_CLEAN, ig_detect_sqli(NULL, 0, &res));
	IG_ASSERT_INT_EQ(IG_CLEAN, res.verdict);
	IG_ASSERT_STR_EQ("", res.fingerprint);
}
