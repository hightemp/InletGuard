/* SPDX-License-Identifier: MIT */
/* Attack detectors over a single value (FR-2). No Zend dependency. */

#ifndef IG_DETECT_H
#define IG_DETECT_H

#include <stddef.h>

typedef enum ig_verdict {
	IG_ERROR = -1, /* internal error: fail_mode decides (NFR-3) */
	IG_CLEAN = 0,
	IG_ATTACK = 1
} ig_verdict_t;

/* SQLi: libinjection fingerprint (up to 5 tokens); XSS: what fired. Includes
 * the terminating NUL. */
#define IG_FINGERPRINT_SIZE 8

typedef struct ig_result {
	ig_verdict_t verdict;
	/* Always NUL-terminated; non-empty only for IG_ATTACK. */
	char fingerprint[IG_FINGERPRINT_SIZE];
	/* Only a prefix of the value was checked (FR-1.4). */
	int truncated;
} ig_result_t;

/* Length to inspect: min(len, max_len), where max_len == 0 means no limit.
 * Sets *truncated (if not NULL) when the value is longer than the limit. */
size_t ig_limit_len(size_t len, size_t max_len, int *truncated);

/* Checks s[0..len) for SQL injection. Fills *res and returns res->verdict;
 * returns IG_ERROR without writing when res is NULL. */
ig_verdict_t ig_detect_sqli(const char *s, size_t len, ig_result_t *res);

/* Checks s[0..len) for XSS: injection into HTML (fingerprint "html") or a
 * script URL as the whole value (fingerprint "url"). Same contract as
 * ig_detect_sqli(). */
ig_verdict_t ig_detect_xss(const char *s, size_t len, ig_result_t *res);

#endif /* IG_DETECT_H */
