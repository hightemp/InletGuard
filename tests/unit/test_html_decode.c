/* SPDX-License-Identifier: MIT */
/* HTML character reference decoding for XSS detection (FR-1.3). */

#include "ig_test.h"
#include "core/ig_detect.h"
#include "core/ig_normalize.h"

#include <string.h>

/* html <in> <expected>: one pass into a separate buffer */
static int html(const char *in, const char *want)
{
	char out[128];
	size_t n = ig_html_decode(in, strlen(in), out);

	if (n != strlen(want) || memcmp(out, want, n) != 0) {
		ig_test_fail(__FILE__, __LINE__, "html \"%s\": got \"%.*s\" (%zu), expected \"%s\"", in,
			(int)(n < sizeof(out) ? n : 0), out, n, want);
		return 0;
	}
	return 1;
}

/* xss_after_decode <in> <verdict>: ig_detect_xss on the decoded copy */
static ig_verdict_t xss_after_decode(const char *in)
{
	char out[128];
	ig_result_t res;
	size_t n = ig_html_decode(in, strlen(in), out);

	return ig_detect_xss(out, n, &res);
}

IG_TEST(html_decode_decimal_references)
{
	if (!html("&#106;avascript:", "javascript:")) {
		return;
	}
	if (!html("&#60&#62", "<>")) {
		return;
	}
	if (!html("&#0000106;&#00039", "j'")) {
		return;
	}
	html("&#1040;&#8364;", "\xd0\x90\xe2\x82\xac"); /* "А€" */
}

IG_TEST(html_decode_hex_references)
{
	if (!html("&#x6A;&#X6a;&#x6a", "jjj")) {
		return;
	}
	if (!html("&#x0000003c;", "<")) {
		return;
	}
	if (!html("&#x20AC;", "\xe2\x82\xac")) {
		return;
	}
	html("&#x1F600;", "\xf0\x9f\x98\x80");
}

IG_TEST(html_decode_invalid_code_points_become_replacement)
{
	if (!html("&#0;", "\xef\xbf\xbd")) {
		return;
	}
	if (!html("&#xD800;", "\xef\xbf\xbd")) {
		return;
	}
	if (!html("&#x110000;", "\xef\xbf\xbd")) {
		return;
	}
	if (!html("&#99999999999999999999;", "\xef\xbf\xbd")) {
		return;
	}
	html("&#x10FFFF;", "\xf4\x8f\xbf\xbf");
}

IG_TEST(html_decode_keeps_references_without_digits)
{
	if (!html("&#;&#x;&#xg;&#a", "&#;&#x;&#xg;&#a")) {
		return;
	}
	html("&#", "&#");
}

IG_TEST(html_decode_legacy_names_with_and_without_semicolon)
{
	if (!html("&lt;script&gt;", "<script>")) {
		return;
	}
	if (!html("&ltscript&gt", "<script>")) {
		return;
	}
	if (!html("&quot;&quot&amp;&amp", "\"\"&&")) {
		return;
	}
	if (!html("&LT;&GT&QUOT;&AMP", "<>\"&")) {
		return;
	}
	html("&nbsp;&nbsp", "\xc2\xa0\xc2\xa0");
}

IG_TEST(html_decode_names_that_need_a_semicolon)
{
	if (!html("&colon;&lpar;&rpar;&Tab;&NewLine;&apos;", ":()\t\n'")) {
		return;
	}
	if (!html("&grave;&equals;&sol;&bsol;&semi;&comma;", "`=/\\;,")) {
		return;
	}
	if (!html("&lsqb;&rsqb;&lcub;&rcub;&lbrack;&rbrack;&lbrace;&rbrace;", "[]{}[]{}")) {
		return;
	}
	if (!html("&period;&excl;&quest;&num;&percnt;&plus;&ast;&commat;", ".!?#%+*@")) {
		return;
	}
	if (!html("&lowbar;&verbar;&vert;&dollar;&Hat;", "_||$^")) {
		return;
	}
	/* without ';' they stay */
	html("&colon&lpar1&rpar", "&colon&lpar1&rpar");
}

IG_TEST(html_decode_names_are_case_sensitive_and_unknown_kept)
{
	if (!html("&Lt;&COLON;&tab;", "&Lt;&COLON;&tab;")) {
		return;
	}
	if (!html("AT&T &copy; &foo;", "AT&T &copy; &foo;")) {
		return;
	}
	if (!html("a&", "a&")) {
		return;
	}
	if (!html("&&lt;", "&<")) {
		return;
	}
	/* one pass: double encoding is expanded once */
	html("&amp;#106;&amp;lt;", "&#106;&lt;");
}

IG_TEST(html_decode_in_place_empty_and_null)
{
	char buf[] = "&#106;&lt;x";
	char out[8];
	size_t n = ig_html_decode(buf, strlen(buf), buf);

	IG_ASSERT_INT_EQ(3, n);
	IG_ASSERT_TRUE(memcmp(buf, "j<x", 3) == 0);
	IG_ASSERT_INT_EQ(0, ig_html_decode("", 0, out));
	IG_ASSERT_INT_EQ(0, ig_html_decode(NULL, 0, out));
	IG_ASSERT_TRUE(ig_html_decode(NULL, 3, out) == IG_NORM_ERROR);
	IG_ASSERT_TRUE(ig_html_decode("abc", 3, NULL) == IG_NORM_ERROR);
}

IG_TEST(html_entity_table_values_are_shorter_than_names)
{
	size_t count = 0;
	const ig_html_entity_t *t = ig_html_entities(&count);
	size_t i;

	IG_ASSERT_TRUE(t != NULL);
	IG_ASSERT_TRUE(count >= 40);
	for (i = 0; i < count; i++) {
		IG_ASSERT_TRUE(t[i].name != NULL && t[i].value != NULL);
		/* '&' + name (no ';' for legacy names) holds the value */
		IG_ASSERT_TRUE(strlen(t[i].value) <= strlen(t[i].name) + 1);
		IG_ASSERT_TRUE(strlen(t[i].value) > 0);
	}
}

IG_TEST(html_decode_reveals_xss)
{
	IG_ASSERT_INT_EQ(IG_CLEAN, xss_after_decode("Tom &amp; Jerry"));
	IG_ASSERT_INT_EQ(IG_ATTACK, xss_after_decode("&#106;avascript:alert(1)"));
	IG_ASSERT_INT_EQ(IG_ATTACK, xss_after_decode("java&Tab;script&colon;alert&lpar;1&rpar;"));
	IG_ASSERT_INT_EQ(IG_ATTACK, xss_after_decode("&#x6a&#x61&#x76&#x61script:alert(1)"));
	IG_ASSERT_INT_EQ(IG_ATTACK, xss_after_decode("&lt;script&gt;alert(1)&lt;/script&gt;"));
}
