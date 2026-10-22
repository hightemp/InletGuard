/* SPDX-License-Identifier: MIT */
/* Detector interface and the registry of core detectors (FR-2.4). */

#ifndef IG_DETECTOR_H
#define IG_DETECTOR_H

#include "core/ig_detect.h"

#include <stddef.h>

/* Stable bits: also the values of the PHP API constants (InletGuard\SQLI,
 * InletGuard\XSS). A new detector takes the next free bit; never renumber. */
#define IG_DETECT_SQLI (1u << 0)
#define IG_DETECT_XSS (1u << 1)

typedef ig_verdict_t (*ig_detect_fn)(const char *s, size_t len, ig_result_t *res);

typedef struct ig_detector {
	const char *name;    /* module ID (FR-10.1) */
	unsigned int mask;   /* exactly one IG_DETECT_* bit */
	ig_detect_fn detect; /* stateless, ZTS-safe */
} ig_detector_t;

size_t ig_detector_count(void);

/* NULL when i is out of range. */
const ig_detector_t *ig_detector_at(size_t i);

/* Exact, case-sensitive match of name[0..len); NULL when unknown. */
const ig_detector_t *ig_detector_find(const char *name, size_t len);

/* NULL unless bit is exactly one known IG_DETECT_* bit. */
const ig_detector_t *ig_detector_by_mask(unsigned int bit);

#endif /* IG_DETECTOR_H */
