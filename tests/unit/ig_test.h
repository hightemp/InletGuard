/* SPDX-License-Identifier: MIT */
/*
 * Minimal dependency-free unit test framework for InletGuard core code.
 *
 * Usage:
 *   IG_TEST(my_feature_does_x) {
 *       IG_ASSERT_TRUE(1 + 1 == 2);
 *       IG_ASSERT_STR_EQ("a", "a");
 *   }
 *
 * Tests self-register through constructor functions (GCC/Clang), so a test
 * file only needs to be listed in tests/unit/Makefile.
 */

#ifndef IG_TEST_H
#define IG_TEST_H

#include <stddef.h>
#include <string.h>

typedef void (*ig_test_fn)(void);

void ig_test_register(const char *name, const char *file, ig_test_fn fn);
void ig_test_fail(const char *file, int line, const char *fmt, ...);

#define IG_TEST(name)                                                          \
	static void ig_test_##name(void);                                          \
	__attribute__((constructor)) static void ig_test_register_##name(void)     \
	{                                                                          \
		ig_test_register(#name, __FILE__, ig_test_##name);                     \
	}                                                                          \
	static void ig_test_##name(void)

#define IG_ASSERT_TRUE(expr)                                                   \
	do {                                                                       \
		if (!(expr)) {                                                         \
			ig_test_fail(__FILE__, __LINE__, "expected true: %s", #expr);      \
			return;                                                            \
		}                                                                      \
	} while (0)

#define IG_ASSERT_FALSE(expr) IG_ASSERT_TRUE(!(expr))

#define IG_ASSERT_INT_EQ(expected, actual)                                     \
	do {                                                                       \
		long long ig_e_ = (long long)(expected);                               \
		long long ig_a_ = (long long)(actual);                                 \
		if (ig_e_ != ig_a_) {                                                  \
			ig_test_fail(__FILE__, __LINE__, "%s: expected %lld, got %lld",    \
				#actual, ig_e_, ig_a_);                                        \
			return;                                                            \
		}                                                                      \
	} while (0)

#define IG_ASSERT_STR_EQ(expected, actual)                                     \
	do {                                                                       \
		const char *ig_e_ = (expected);                                        \
		const char *ig_a_ = (actual);                                          \
		if (ig_e_ == NULL || ig_a_ == NULL || strcmp(ig_e_, ig_a_) != 0) {     \
			ig_test_fail(__FILE__, __LINE__, "%s: expected \"%s\", got \"%s\"", \
				#actual, ig_e_ ? ig_e_ : "(null)", ig_a_ ? ig_a_ : "(null)");  \
			return;                                                            \
		}                                                                      \
	} while (0)

#endif /* IG_TEST_H */
