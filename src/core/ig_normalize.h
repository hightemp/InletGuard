/* SPDX-License-Identifier: MIT */
/* Normalization of a copy of an input value before detection (FR-1.3).
 * No allocation: the caller provides the output buffer. */

#ifndef IG_NORMALIZE_H
#define IG_NORMALIZE_H

#include <stddef.h>

/* '+' decodes to a space (query string and form body, like urldecode()). */
#define IG_DECODE_PLUS (1u << 0)

/* Returned instead of a length on invalid arguments. */
#define IG_NORM_ERROR ((size_t) - 1)

typedef struct ig_norm_stats {
	unsigned int rounds; /* rounds that changed the value */
	size_t nul_removed;  /* NUL bytes removed */
	int limit_hit;       /* the value would still decode after the last round */
} ig_norm_stats_t;

/* One round of URL decoding: %XX, %uXXXX (to UTF-8) and, with
 * IG_DECODE_PLUS, '+'. Invalid sequences are copied as is. The result is
 * never longer than len, so out needs len bytes; out may be in. */
size_t ig_url_decode(const char *in, size_t len, char *out, unsigned int flags);

/* Up to `rounds` rounds of URL decoding, each followed by NUL removal; stops
 * early when a round changes nothing. NUL is removed even with rounds == 0.
 * Same buffer rules as ig_url_decode(); stats may be NULL. */
size_t ig_normalize(const char *in, size_t len, unsigned int rounds, unsigned int flags, char *out,
	ig_norm_stats_t *stats);

#endif /* IG_NORMALIZE_H */
