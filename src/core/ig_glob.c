/* SPDX-License-Identifier: MIT */

#include "core/ig_glob.h"

static unsigned char ig_fold(unsigned char c, unsigned int flags)
{
	if ((flags & IG_GLOB_NOCASE) && c >= 'A' && c <= 'Z') {
		return (unsigned char)(c - 'A' + 'a');
	}
	return c;
}

int ig_glob_match(const char *pat, size_t plen, const char *s, size_t slen, unsigned int flags)
{
	size_t p = 0;
	size_t i = 0;
	size_t star_p = 0; /* pattern position after the last '*' */
	size_t star_i = 0; /* string position that '*' currently ends at */
	int have_star = 0;

	if ((pat == NULL && plen > 0) || (s == NULL && slen > 0)) {
		return 0;
	}
	/* Greedy scan: on a mismatch, let the last '*' take one more byte.
	 * Earlier stars never need to grow, so no recursion is required. */
	while (i < slen) {
		if (p < plen && pat[p] == '*') {
			have_star = 1;
			star_p = ++p;
			star_i = i;
			continue;
		}
		if (p < plen) {
			size_t step = 1;
			unsigned char c = (unsigned char)pat[p];

			if (c == '\\' && p + 1 < plen) {
				c = (unsigned char)pat[p + 1];
				step = 2;
			}
			if (ig_fold(c, flags) == ig_fold((unsigned char)s[i], flags)) {
				p += step;
				i++;
				continue;
			}
		}
		if (!have_star) {
			return 0;
		}
		p = star_p;
		i = ++star_i;
	}
	while (p < plen && pat[p] == '*') {
		p++;
	}
	return p == plen;
}
