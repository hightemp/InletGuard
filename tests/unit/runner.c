/* SPDX-License-Identifier: MIT */

#include "ig_test.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define IG_TEST_MAX 4096

typedef struct {
	const char *name;
	const char *file;
	ig_test_fn fn;
} ig_test_case;

static ig_test_case ig_tests[IG_TEST_MAX];
static size_t ig_test_count;
static int ig_current_failed;

void ig_test_register(const char *name, const char *file, ig_test_fn fn)
{
	if (ig_test_count >= IG_TEST_MAX) {
		fprintf(stderr, "too many unit tests, raise IG_TEST_MAX\n");
		exit(2);
	}
	ig_tests[ig_test_count].name = name;
	ig_tests[ig_test_count].file = file;
	ig_tests[ig_test_count].fn = fn;
	ig_test_count++;
}

void ig_test_fail(const char *file, int line, const char *fmt, ...)
{
	va_list ap;

	ig_current_failed = 1;
	fprintf(stderr, "    %s:%d: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

int main(int argc, char **argv)
{
	const char *filter = argc > 1 ? argv[1] : NULL;
	size_t i, run = 0, failed = 0;

	for (i = 0; i < ig_test_count; i++) {
		if (filter != NULL && strstr(ig_tests[i].name, filter) == NULL) {
			continue;
		}
		ig_current_failed = 0;
		ig_tests[i].fn();
		run++;
		if (ig_current_failed) {
			failed++;
			printf("FAIL %s (%s)\n", ig_tests[i].name, ig_tests[i].file);
		} else {
			printf("PASS %s\n", ig_tests[i].name);
		}
	}

	printf("\n%zu tests, %zu passed, %zu failed\n", run, run - failed, failed);
	return failed == 0 ? 0 : 1;
}
