/* SPDX-License-Identifier: MIT */
/* Glob matching for exclusions (FR-5.4). No Zend dependency. */

#ifndef IG_GLOB_H
#define IG_GLOB_H

#include <stddef.h>

/* Compare ASCII letters without case (header names). */
#define IG_GLOB_NOCASE (1u << 0)

/* Does pat[0..plen) match the whole s[0..slen)? '*' matches any sequence,
 * '/' included; '\' escapes the next character (a trailing '\' is literal);
 * everything else, '?' and '[' too, is literal. Binary-safe, no recursion,
 * O(plen * slen) worst case. NULL with a non-zero length never matches. */
int ig_glob_match(const char *pat, size_t plen, const char *s, size_t slen, unsigned int flags);

#endif /* IG_GLOB_H */
