/* SPDX-License-Identifier: MIT */
/* Glob matcher for exclusions (FR-5.4). */

#include "ig_test.h"
#include "core/ig_glob.h"

#include <string.h>

static int g(const char *pat, const char *s, unsigned int flags)
{
	return ig_glob_match(pat, strlen(pat), s, strlen(s), flags);
}

IG_TEST(glob_exact_match)
{
	IG_ASSERT_TRUE(g("user_id", "user_id", 0));
	IG_ASSERT_FALSE(g("user_id", "user_ids", 0));
	IG_ASSERT_FALSE(g("user_id", "user_i", 0));
	IG_ASSERT_TRUE(g("", "", 0));
	IG_ASSERT_FALSE(g("", "a", 0));
	IG_ASSERT_FALSE(g("a", "", 0));
}

IG_TEST(glob_star_positions)
{
	IG_ASSERT_TRUE(g("user_*", "user_", 0));
	IG_ASSERT_TRUE(g("user_*", "user_name", 0));
	IG_ASSERT_FALSE(g("user_*", "user", 0));
	IG_ASSERT_TRUE(g("*_token", "csrf_token", 0));
	IG_ASSERT_FALSE(g("*_token", "csrf_tokens", 0));
	IG_ASSERT_TRUE(g("a*z", "az", 0));
	IG_ASSERT_TRUE(g("a*z", "abcz", 0));
	IG_ASSERT_TRUE(g("*", "", 0));
	IG_ASSERT_TRUE(g("*", "anything", 0));
	IG_ASSERT_TRUE(g("***", "x", 0));
	IG_ASSERT_TRUE(g("a**b", "aXYb", 0));
}

IG_TEST(glob_star_crosses_slash)
{
	IG_ASSERT_TRUE(g("/admin/*", "/admin/a/b", 0));
	IG_ASSERT_TRUE(g("/api/*/upload", "/api/v1/x/upload", 0));
	IG_ASSERT_FALSE(g("/admin/*", "/administrator", 0));
}

IG_TEST(glob_backtracking)
{
	IG_ASSERT_TRUE(g("a*b*c", "aXbYbZc", 0));
	IG_ASSERT_TRUE(g("*ab", "aab", 0));
	IG_ASSERT_TRUE(g("*aab", "aaab", 0));
	IG_ASSERT_FALSE(g("a*b*c", "aXbYbZ", 0));
	IG_ASSERT_TRUE(g("*x*x*", "xx", 0));
	IG_ASSERT_FALSE(g("*x*x*", "x", 0));
}

IG_TEST(glob_escaping)
{
	IG_ASSERT_TRUE(g("a\\*b", "a*b", 0));
	IG_ASSERT_FALSE(g("a\\*b", "aXb", 0));
	IG_ASSERT_TRUE(g("a\\\\b", "a\\b", 0));
	IG_ASSERT_TRUE(g("a\\", "a\\", 0));
	IG_ASSERT_TRUE(g("\\a", "a", 0));
	IG_ASSERT_TRUE(g("*\\*", "x*", 0));
	IG_ASSERT_FALSE(g("*\\*", "x", 0));
}

IG_TEST(glob_other_metacharacters_are_literal)
{
	IG_ASSERT_TRUE(g("a?b", "a?b", 0));
	IG_ASSERT_FALSE(g("a?b", "aXb", 0));
	IG_ASSERT_TRUE(g("[ab]", "[ab]", 0));
	IG_ASSERT_FALSE(g("[ab]", "a", 0));
}

IG_TEST(glob_case)
{
	IG_ASSERT_FALSE(g("User-Agent", "user-agent", 0));
	IG_ASSERT_TRUE(g("User-Agent", "user-agent", IG_GLOB_NOCASE));
	IG_ASSERT_TRUE(g("X-*", "x-forwarded-for", IG_GLOB_NOCASE));
	IG_ASSERT_FALSE(g("X-*", "x-forwarded-for", 0));
	/* only ASCII folds: "Ж" (d0 96) vs "ж" (d0 b6) */
	IG_ASSERT_FALSE(g("\xd0\x96", "\xd0\xb6", IG_GLOB_NOCASE));
	IG_ASSERT_TRUE(g("\xd0\x96*", "\xd0\x96x", IG_GLOB_NOCASE));
}

IG_TEST(glob_binary_safe_and_null)
{
	IG_ASSERT_TRUE(ig_glob_match("a*", 2, "a\0b", 3, 0));
	IG_ASSERT_TRUE(ig_glob_match("a\0*", 3, "a\0b", 3, 0));
	IG_ASSERT_FALSE(ig_glob_match("a\0*", 3, "ab", 2, 0));
	IG_ASSERT_FALSE(ig_glob_match(NULL, 1, "a", 1, 0));
	IG_ASSERT_FALSE(ig_glob_match("a", 1, NULL, 1, 0));
	IG_ASSERT_TRUE(ig_glob_match(NULL, 0, NULL, 0, 0));
	IG_ASSERT_TRUE(ig_glob_match("*", 1, NULL, 0, 0));
}

IG_TEST(glob_many_stars_are_fast)
{
	static char s[20001];
	static const char pat[] = "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b";

	memset(s, 'a', sizeof(s) - 1);
	/* exponential matchers never finish this; ours is O(|pat| * |s|) */
	IG_ASSERT_FALSE(ig_glob_match(pat, sizeof(pat) - 1, s, sizeof(s) - 1, 0));
	s[sizeof(s) - 2] = 'b';
	IG_ASSERT_TRUE(ig_glob_match(pat, sizeof(pat) - 1, s, sizeof(s) - 1, 0));
}
