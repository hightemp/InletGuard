/* SPDX-License-Identifier: MIT */

#include "core/ig_detect.h"
#include "core/ig_libinjection.h"

#include <string.h>

/* Common argument handling. Returns 0 when the verdict is already final. */
static int ig_detect_begin(const char *s, size_t len, ig_result_t *res)
{
	res->fingerprint[0] = '\0';
	if (s == NULL) {
		res->verdict = len == 0 ? IG_CLEAN : IG_ERROR;
		return 0;
	}
	return 1;
}

static ig_verdict_t ig_detect_attack(ig_result_t *res, const char *fingerprint)
{
	res->verdict = IG_ATTACK;
	strncpy(res->fingerprint, fingerprint, IG_FINGERPRINT_SIZE - 1);
	res->fingerprint[IG_FINGERPRINT_SIZE - 1] = '\0';
	return IG_ATTACK;
}

ig_verdict_t ig_detect_sqli(const char *s, size_t len, ig_result_t *res)
{
	char fp[IG_FINGERPRINT_SIZE];

	if (res == NULL) {
		return IG_ERROR;
	}
	if (!ig_detect_begin(s, len, res)) {
		return res->verdict;
	}

	/* libinjection keeps its state on the stack: safe under ZTS. */
	fp[0] = '\0';
	res->verdict = ig_li_verdict(libinjection_sqli(s, len, fp));
	if (res->verdict == IG_ATTACK) {
		fp[IG_FINGERPRINT_SIZE - 1] = '\0';
		return ig_detect_attack(res, fp);
	}
	return res->verdict;
}

static int ig_lower(unsigned char c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* Matches the lowercase word at s[*pos], skipping ASCII tab, LF and CR the
 * way URL parsers do (WHATWG). Advances *pos past the match. */
static int ig_match_url_word(const char *s, size_t len, size_t *pos, const char *word)
{
	size_t i = *pos;

	for (; *word != '\0'; word++) {
		while (i < len && (s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
			i++;
		}
		if (i == len || ig_lower((unsigned char)s[i]) != (unsigned char)*word) {
			return 0;
		}
		i++;
	}
	*pos = i;
	return 1;
}

static int ig_has_code_char(const char *s, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (s[i] == '(' || s[i] == '`' || s[i] == '=') {
			return 1;
		}
	}
	return 0;
}

/* A value that is a script URL (FR-2.2): leading C0 controls and spaces are
 * skipped like browsers do; "javascript:"/"vbscript:" count only with a code
 * character after them, so prose such as "JavaScript: The Good Parts" stays
 * clean; "data:" only with a MIME type that runs script. */
static int ig_is_script_url(const char *s, size_t len)
{
	static const char *const script_schemes[] = {"javascript:", "vbscript:"};
	static const char *const data_types[] = {"text/html", "image/svg+xml", "application/xhtml+xml"};
	size_t start = 0;
	size_t pos;
	size_t i;

	while (start < len && (unsigned char)s[start] <= 0x20) {
		start++;
	}
	for (i = 0; i < sizeof(script_schemes) / sizeof(script_schemes[0]); i++) {
		pos = start;
		if (ig_match_url_word(s, len, &pos, script_schemes[i])) {
			return ig_has_code_char(s + pos, len - pos);
		}
	}
	pos = start;
	if (ig_match_url_word(s, len, &pos, "data:")) {
		for (i = 0; i < sizeof(data_types) / sizeof(data_types[0]); i++) {
			size_t p = pos;

			if (ig_match_url_word(s, len, &p, data_types[i])) {
				return 1;
			}
		}
	}
	return 0;
}

ig_verdict_t ig_detect_xss(const char *s, size_t len, ig_result_t *res)
{
	if (res == NULL) {
		return IG_ERROR;
	}
	if (!ig_detect_begin(s, len, res)) {
		return res->verdict;
	}

	res->verdict = ig_li_verdict(libinjection_xss(s, len));
	if (res->verdict == IG_ATTACK) {
		return ig_detect_attack(res, "html");
	}
	/* Evidence of an attack beats a libinjection error. */
	if (ig_is_script_url(s, len)) {
		return ig_detect_attack(res, "url");
	}
	return res->verdict;
}
