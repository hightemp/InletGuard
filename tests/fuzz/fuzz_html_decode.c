/* SPDX-License-Identifier: MIT */
/* Fuzz target: HTML decoding never grows a value, introduces no NUL and gives
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

	out = malloc(size + 1);
	inplace = malloc(size + 1);
	if (out == NULL || inplace == NULL) {
		abort();
	}

	n = ig_html_decode((const char *)data, size, out);
	if (n > size) {
		abort();
	}
	if (memchr(data, '\0', size) == NULL && memchr(out, '\0', n) != NULL) {
		abort();
	}
	if (size > 0) {
		memcpy(inplace, data, size);
	}
	m = ig_html_decode(inplace, size, inplace);
	if (m != n || memcmp(out, inplace, n) != 0) {
		abort();
	}

	free(out);
	free(inplace);
	return 0;
}
