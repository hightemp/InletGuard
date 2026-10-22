/* SPDX-License-Identifier: MIT */
/* Fuzz target: ig_detect_sqli() never crashes and keeps its result contract. */

#include "core/ig_detect.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	ig_result_t res;
	ig_verdict_t v;
	size_t fp_len;

	memset(&res, 'X', sizeof(res));
	v = ig_detect_sqli((const char *)data, size, &res);

	if (v != res.verdict || (v != IG_CLEAN && v != IG_ATTACK && v != IG_ERROR)) {
		abort();
	}
	fp_len = strnlen(res.fingerprint, sizeof(res.fingerprint));
	if (fp_len == sizeof(res.fingerprint) || (fp_len > 0) != (v == IG_ATTACK)) {
		abort();
	}
	return 0;
}
