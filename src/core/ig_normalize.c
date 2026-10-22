/* SPDX-License-Identifier: MIT */

#include "core/ig_normalize.h"

#include <string.h>

static int ig_hex(unsigned char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

/* Value of n hex digits at s, or -1. */
static long ig_hex_n(const char *s, size_t n)
{
	long v = 0;
	size_t i;

	for (i = 0; i < n; i++) {
		int d = ig_hex((unsigned char)s[i]);

		if (d < 0) {
			return -1;
		}
		v = v * 16 + d;
	}
	return v;
}

/* Length of the escape at in[i] ('%XX' = 3, '%uXXXX' = 6) and its value in
 * *cp; 0 when there is no valid escape. */
static size_t ig_escape_at(const char *in, size_t len, size_t i, long *cp)
{
	if (in[i] != '%') {
		return 0;
	}
	if (i + 5 < len && (in[i + 1] == 'u' || in[i + 1] == 'U')) {
		*cp = ig_hex_n(in + i + 2, 4);
		/* surrogates are not characters */
		if (*cp >= 0 && (*cp < 0xD800 || *cp > 0xDFFF)) {
			return 6;
		}
	}
	if (i + 2 < len) {
		*cp = ig_hex_n(in + i + 1, 2);
		if (*cp >= 0) {
			return 3;
		}
	}
	return 0;
}

/* Writes code point cp (< 0x10000) as UTF-8; returns the byte count. */
static size_t ig_put_utf8(char *out, long cp)
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xC0 | (cp >> 6));
		out[1] = (char)(0x80 | (cp & 0x3F));
		return 2;
	}
	out[0] = (char)(0xE0 | (cp >> 12));
	out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
	out[2] = (char)(0x80 | (cp & 0x3F));
	return 3;
}

size_t ig_url_decode(const char *in, size_t len, char *out, unsigned int flags)
{
	size_t i = 0;
	size_t o = 0;

	if (len > 0 && (in == NULL || out == NULL)) {
		return IG_NORM_ERROR;
	}
	/* o <= i at every step, so decoding in place is safe */
	while (i < len) {
		long cp = 0;
		size_t esc = ig_escape_at(in, len, i, &cp);

		if (esc == 3) {
			out[o++] = (char)cp;
		} else if (esc == 6) {
			o += ig_put_utf8(out + o, cp);
		} else if (in[i] == '+' && (flags & IG_DECODE_PLUS)) {
			out[o++] = ' ';
			esc = 1;
		} else {
			out[o++] = in[i];
			esc = 1;
		}
		i += esc;
	}
	return o;
}

/* Would another decoding round change s? */
static int ig_url_decodable(const char *s, size_t len, unsigned int flags)
{
	size_t i;

	for (i = 0; i < len; i++) {
		long cp;

		if (ig_escape_at(s, len, i, &cp) != 0 || (s[i] == '+' && (flags & IG_DECODE_PLUS))) {
			return 1;
		}
	}
	return 0;
}

static size_t ig_remove_nul(char *s, size_t len, size_t *removed)
{
	size_t i;
	size_t o = 0;

	for (i = 0; i < len; i++) {
		if (s[i] != '\0') {
			s[o++] = s[i];
		}
	}
	*removed += len - o;
	return o;
}

size_t ig_normalize(const char *in, size_t len, unsigned int rounds, unsigned int flags, char *out,
	ig_norm_stats_t *stats)
{
	ig_norm_stats_t st = {0, 0, 0};
	size_t n;
	unsigned int r;

	if (len > 0 && (in == NULL || out == NULL)) {
		if (stats != NULL) {
			*stats = st;
		}
		return IG_NORM_ERROR;
	}
	if (len > 0 && out != in) {
		memmove(out, in, len);
	}
	n = ig_remove_nul(out, len, &st.nul_removed);

	/* A decodable value always changes in a round (an escape shrinks it,
	 * '+' becomes ' '), so the check also detects a stable value. */
	for (r = 0; r < rounds && ig_url_decodable(out, n, flags); r++) {
		n = ig_url_decode(out, n, out, flags);
		n = ig_remove_nul(out, n, &st.nul_removed);
		st.rounds++;
	}
	st.limit_hit = ig_url_decodable(out, n, flags);
	if (stats != NULL) {
		*stats = st;
	}
	return n;
}
