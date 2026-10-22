/* SPDX-License-Identifier: MIT */
/* Adapter between libinjection results and InletGuard verdicts. */

#ifndef IG_LIBINJECTION_H
#define IG_LIBINJECTION_H

#include "core/ig_detect.h"
#include "libinjection.h"

/* Any value other than FALSE or TRUE is an error, never "clean". */
ig_verdict_t ig_li_verdict(injection_result_t r);

#endif /* IG_LIBINJECTION_H */
