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

/* Writes code point cp (<= 0x10FFFF) as UTF-8; returns the byte count. */
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
	if (cp < 0x10000) {
		out[0] = (char)(0xE0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2] = (char)(0x80 | (cp & 0x3F));
		return 3;
	}
	out[0] = (char)(0xF0 | (cp >> 18));
	out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
	out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
	out[3] = (char)(0x80 | (cp & 0x3F));
	return 4;
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

static const ig_html_entity_t ig_entities[] = {
	{"amp", "&", 1},
	{"AMP", "&", 1},
	{"lt", "<", 1},
	{"LT", "<", 1},
	{"gt", ">", 1},
	{"GT", ">", 1},
	{"quot", "\"", 1},
	{"QUOT", "\"", 1},
	{"nbsp", "\xc2\xa0", 1},
	{"apos", "'", 0},
	{"colon", ":", 0},
	{"lpar", "(", 0},
	{"rpar", ")", 0},
	{"Tab", "\t", 0},
	{"NewLine", "\n", 0},
	{"sol", "/", 0},
	{"bsol", "\\", 0},
	{"grave", "`", 0},
	{"equals", "=", 0},
	{"semi", ";", 0},
	{"comma", ",", 0},
	{"period", ".", 0},
	{"excl", "!", 0},
	{"quest", "?", 0},
	{"num", "#", 0},
	{"percnt", "%", 0},
	{"plus", "+", 0},
	{"ast", "*", 0},
	{"commat", "@", 0},
	{"lowbar", "_", 0},
	{"verbar", "|", 0},
	{"vert", "|", 0},
	{"lsqb", "[", 0},
	{"lbrack", "[", 0},
	{"rsqb", "]", 0},
	{"rbrack", "]", 0},
	{"lcub", "{", 0},
	{"lbrace", "{", 0},
	{"rcub", "}", 0},
	{"rbrace", "}", 0},
	{"dollar", "$", 0},
	{"Hat", "^", 0},
};

#define IG_ENTITY_COUNT (sizeof(ig_entities) / sizeof(ig_entities[0]))

const ig_html_entity_t *ig_html_entities(size_t *count)
{
	if (count != NULL) {
		*count = IG_ENTITY_COUNT;
	}
	return ig_entities;
}

static int ig_is_alnum(unsigned char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Numeric reference at in[i] ("&#..."): length consumed and code point in
 * *cp; 0 when there are no digits. */
static size_t ig_numeric_ref(const char *in, size_t len, size_t i, long *cp)
{
	size_t j = i + 2;
	size_t digits = 0;
	int base = 10;
	long v = 0;

	if (j < len && (in[j] == 'x' || in[j] == 'X')) {
		base = 16;
		j++;
	}
	for (; j < len; j++, digits++) {
		int d = ig_hex((unsigned char)in[j]);

		if (base == 10 && d > 9) {
			d = -1;
		}
		if (d < 0) {
			break;
		}
		/* saturate: any value above 10FFFF is invalid anyway */
		v = v > 0x10FFFF ? v : v * base + d;
	}
	if (digits == 0) {
		return 0;
	}
	if (j < len && in[j] == ';') {
		j++;
	}
	/* HTML5: NUL, surrogates and out-of-range values -> U+FFFD */
	if (v == 0 || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) {
		v = 0xFFFD;
	}
	*cp = v;
	return j - i;
}

/* Named reference at in[i] ("&name"): length consumed and the entity in
 * *ent; 0 when the name is not known. */
static size_t ig_named_ref(const char *in, size_t len, size_t i, const ig_html_entity_t **ent)
{
	size_t run = 0;
	size_t k;
	size_t best = 0;

	while (i + 1 + run < len && ig_is_alnum((unsigned char)in[i + 1 + run])) {
		run++;
	}
	for (k = 0; k < IG_ENTITY_COUNT; k++) {
		size_t nl = strlen(ig_entities[k].name);

		if (nl > run || memcmp(in + i + 1, ig_entities[k].name, nl) != 0) {
			continue;
		}
		if (nl == run && i + 1 + nl < len && in[i + 1 + nl] == ';') {
			*ent = &ig_entities[k];
			return nl + 2;
		}
		/* legacy names also end without ';': the longest prefix wins */
		if (ig_entities[k].legacy && nl + 1 > best) {
			*ent = &ig_entities[k];
			best = nl + 1;
		}
	}
	return best;
}

size_t ig_html_decode(const char *in, size_t len, char *out)
{
	size_t i = 0;
	size_t o = 0;

	if (len > 0 && (in == NULL || out == NULL)) {
		return IG_NORM_ERROR;
	}
	/* every reference is at least as long as its value, so o <= i */
	while (i < len) {
		size_t ref = 0;

		if (in[i] == '&' && i + 1 < len) {
			long cp = 0;
			const ig_html_entity_t *ent = NULL;

			if (in[i + 1] == '#') {
				ref = ig_numeric_ref(in, len, i, &cp);
				if (ref > 0) {
					o += ig_put_utf8(out + o, cp);
				}
			} else {
				ref = ig_named_ref(in, len, i, &ent);
				if (ref > 0) {
					size_t vl = strlen(ent->value);

					memmove(out + o, ent->value, vl);
					o += vl;
				}
			}
		}
		if (ref == 0) {
			out[o++] = in[i];
			ref = 1;
		}
		i += ref;
	}
	return o;
}
