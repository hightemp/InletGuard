/* SPDX-License-Identifier: MIT */

#include "core/ig_detect.h"
#include "core/ig_libinjection.h"

#include <string.h>

ig_verdict_t ig_detect_sqli(const char *s, size_t len, ig_result_t *res)
{
	char fp[IG_FINGERPRINT_SIZE];

	if (res == NULL) {
		return IG_ERROR;
	}
	res->fingerprint[0] = '\0';
	if (s == NULL) {
		res->verdict = len == 0 ? IG_CLEAN : IG_ERROR;
		return res->verdict;
	}

	/* libinjection keeps its state on the stack: safe under ZTS. */
	fp[0] = '\0';
	res->verdict = ig_li_verdict(libinjection_sqli(s, len, fp));
	if (res->verdict == IG_ATTACK) {
		memcpy(res->fingerprint, fp, sizeof(fp));
		res->fingerprint[IG_FINGERPRINT_SIZE - 1] = '\0';
	}
	return res->verdict;
}
