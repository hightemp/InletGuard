/* SPDX-License-Identifier: MIT */
/* ig_detect_xss(): libinjection_xss plus the dangerous URL scheme check (FR-2.2). */

#include "ig_test.h"
#include "core/ig_detect.h"

#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* Injection into HTML: reported by libinjection. */
static const char *const html_attacks[] = {
	"<script>alert(1)</script>",
	"<img src=x onerror=alert(1)>",
	"<a href=\"javascript:alert(1)\">x</a>",
	"<svg onload=alert(1)>",
	"<body onload=alert(1)>",
	"<iframe src=\"javascript:alert(1)\"></iframe>",
	"\"><script>alert(1)</script>",
	"' onmouseover='alert(1)",
	"<ScRiPt>alert(1)</sCrIpT>",
	"<div style=\"background:url(javascript:alert(1))\">",
};

/* A whole value that is a script URL: reported by our scheme check. */
static const char *const url_attacks[] = {
	"javascript:alert(1)",
	"JavaScript:alert(1)",
	"JAVASCRIPT:alert(document.cookie)",
	"  javascript:alert(1)",
	"\x01\x1f javascript:alert(1)",
	"java\tscript:alert(1)",
	"jav\na\rscript:alert(1)",
	"javascript:alert`1`",
	"javascript:location='//evil.example'",
	"javascript://%0aalert(1)",
	"vbscript:msgbox(1)",
	"data:text/html,<p>x</p>",
	"data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==",
	"DATA:Text/HTML,x",
	"data:image/svg+xml;base64,PHN2Zz4=",
	"data:application/xhtml+xml,x",
	"d\tata:text/html,x",
};

static const char *const benign[] = {
	"hello world",
	"<b>bold</b>",
	"<i>it</i> and <p>para</p>",
	"<a href=\"https://example.com\">link</a>",
	"2 < 3 and 5 > 4",
	"O'Reilly",
	"Привет, <b>мир</b>",
	"<img src=\"cat.png\" alt=\"cat\">",
	"",
	"a=b&c=d",
	"<p class=\"x\">text</p>",
	"I <3 you",
	"JavaScript: The Good Parts",
	"javascript is fun",
	"javascript",
	"https://example.com/?q=javascript:",
	"data:image/png;base64,iVBORw0KGgo=",
	"data:text/plain,hello",
	"my javascript:alert(1) note",
};

static int check(const char *s, size_t len, ig_verdict_t want, const char *fp)
{
	ig_result_t res;
	ig_verdict_t v;

	memset(&res, 'X', sizeof(res));
	v = ig_detect_xss(s, len, &res);
	if (v != want || res.verdict != want || strcmp(res.fingerprint, fp) != 0) {
		ig_test_fail(__FILE__, __LINE__,
			"\"%s\": verdict %d/%d, fingerprint \"%.8s\", expected %d \"%s\"", s ? s : "(null)",
			(int)v, (int)res.verdict, res.fingerprint, (int)want, fp);
		return 0;
	}
	return 1;
}

IG_TEST(xss_html_injection_is_reported_as_html)
{
	size_t i;

	for (i = 0; i < COUNT(html_attacks); i++) {
		if (!check(html_attacks[i], strlen(html_attacks[i]), IG_ATTACK, "html")) {
			return;
		}
	}
}

IG_TEST(xss_script_url_is_reported_as_url)
{
	size_t i;

	for (i = 0; i < COUNT(url_attacks); i++) {
		if (!check(url_attacks[i], strlen(url_attacks[i]), IG_ATTACK, "url")) {
			return;
		}
	}
}

IG_TEST(xss_benign_values_are_clean)
{
	size_t i;

	for (i = 0; i < COUNT(benign); i++) {
		if (!check(benign[i], strlen(benign[i]), IG_CLEAN, "")) {
			return;
		}
	}
}

IG_TEST(xss_embedded_nul_does_not_hide_an_attack)
{
	static const char html[] = "abc\0<script>alert(1)</script>";
	static const char url[] = "\0javascript:alert(1)";

	if (!check(html, sizeof(html) - 1, IG_ATTACK, "html")) {
		return;
	}
	check(url, sizeof(url) - 1, IG_ATTACK, "url");
}

IG_TEST(xss_scheme_needs_the_whole_prefix)
{
	check("javascript:", 11, IG_CLEAN, "");
	check("data:text/htm", 13, IG_CLEAN, "");
}

IG_TEST(xss_null_arguments)
{
	IG_ASSERT_INT_EQ(IG_ERROR, ig_detect_xss("<script>", 8, NULL));
	if (!check(NULL, 5, IG_ERROR, "")) {
		return;
	}
	check(NULL, 0, IG_CLEAN, "");
}
