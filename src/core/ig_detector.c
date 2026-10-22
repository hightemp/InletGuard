/* SPDX-License-Identifier: MIT */

#include "core/ig_detector.h"

#include <string.h>

/* Invariants (unique names, one unique bit each) are checked by the unit
 * tests, not at runtime. */
static const ig_detector_t ig_detectors[] = {
	{"sqli", IG_DETECT_SQLI, ig_detect_sqli},
	{"xss", IG_DETECT_XSS, ig_detect_xss},
};

#define IG_DETECTOR_COUNT (sizeof(ig_detectors) / sizeof(ig_detectors[0]))

size_t ig_detector_count(void)
{
	return IG_DETECTOR_COUNT;
}

const ig_detector_t *ig_detector_at(size_t i)
{
	return i < IG_DETECTOR_COUNT ? &ig_detectors[i] : NULL;
}

const ig_detector_t *ig_detector_find(const char *name, size_t len)
{
	size_t i;

	if (name == NULL) {
		return NULL;
	}
	for (i = 0; i < IG_DETECTOR_COUNT; i++) {
		if (strlen(ig_detectors[i].name) == len && memcmp(ig_detectors[i].name, name, len) == 0) {
			return &ig_detectors[i];
		}
	}
	return NULL;
}

const ig_detector_t *ig_detector_by_mask(unsigned int bit)
{
	size_t i;

	for (i = 0; i < IG_DETECTOR_COUNT; i++) {
		if (ig_detectors[i].mask == bit) {
			return &ig_detectors[i];
		}
	}
	return NULL;
}
