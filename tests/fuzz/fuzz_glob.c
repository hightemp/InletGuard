/* SPDX-License-Identifier: MIT */
/* Fuzz target: the glob matcher agrees with a naive recursive reference on
 * short inputs, "*" matches everything, and a case-sensitive match implies a
 * case-insensitive one. */

#include "core/ig_glob.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static int ref_eq(unsigned char a, unsigned char b, int nocase)
{
	if (nocase) {
		a = (unsigned char)(a >= 'A' && a <= 'Z' ? a - 'A' + 'a' : a);
		b = (unsigned char)(b >= 'A' && b <= 'Z' ? b - 'A' + 'a' : b);
	}
	return a == b;
}

/* Exponential in the number of stars: only for short inputs. */
static int ref_match(const char *p, size_t pl, const char *s, size_t sl, int nocase)
{
	if (pl == 0) {
		return sl == 0;
	}
	if (p[0] == '*') {
		size_t i;

		for (i = 0; i <= sl; i++) {
			if (ref_match(p + 1, pl - 1, s + i, sl - i, nocase)) {
				return 1;
			}
		}
		return 0;
	}
	if (p[0] == '\\' && pl > 1) {
		return sl > 0 && ref_eq((unsigned char)p[1], (unsigned char)s[0], nocase) &&
			   ref_match(p + 2, pl - 2, s + 1, sl - 1, nocase);
	}
	return sl > 0 && ref_eq((unsigned char)p[0], (unsigned char)s[0], nocase) &&
		   ref_match(p + 1, pl - 1, s + 1, sl - 1, nocase);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const char *pat;
	const char *s;
	size_t plen;
	size_t slen;
	int cs;
	int ci;

	if (size == 0) {
		return 0;
	}
	/* the first byte splits the rest into pattern and string */
	plen = data[0] % size;
	pat = (const char *)data + 1;
	s = pat + plen;
	slen = size - 1 - plen;

	cs = ig_glob_match(pat, plen, s, slen, 0);
	ci = ig_glob_match(pat, plen, s, slen, IG_GLOB_NOCASE);
	if ((cs != 0 && cs != 1) || (cs && !ci)) {
		abort();
	}
	if (!ig_glob_match("*", 1, s, slen, 0)) {
		abort();
	}
	if (plen <= 12 && slen <= 24 &&
		(cs != ref_match(pat, plen, s, slen, 0) || ci != ref_match(pat, plen, s, slen, 1))) {
		abort();
	}
	return 0;
}
