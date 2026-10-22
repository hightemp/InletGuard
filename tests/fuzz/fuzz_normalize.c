/* SPDX-License-Identifier: MIT */
/* Fuzz target: the normalizer never grows a value, leaves no NUL and gives
 * the same result in place and into a separate buffer. */

#include "core/ig_normalize.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *out;
	char *inplace;
	size_t n;
	size_t m;
	unsigned int flags;
	ig_norm_stats_t st;

	if (size == 0) {
		return 0;
	}
	/* the first byte selects the options */
	flags = (data[0] & 1u) ? IG_DECODE_PLUS : 0;
	data++;
	size--;

	out = malloc(size + 1);
	inplace = malloc(size + 1);
	if (out == NULL || inplace == NULL) {
		abort();
	}

	n = ig_url_decode((const char *)data, size, out, flags);
	if (n > size) {
		abort();
	}

	n = ig_normalize((const char *)data, size, 3, flags, out, &st);
	if (n > size || memchr(out, '\0', n) != NULL || st.rounds > 3) {
		abort();
	}
	memcpy(inplace, data, size);
	m = ig_normalize(inplace, size, 3, flags, inplace, NULL);
	if (m != n || memcmp(out, inplace, n) != 0) {
		abort();
	}

	free(out);
	free(inplace);
	return 0;
}
