/* SPDX-License-Identifier: MIT */

#include "core/ig_libinjection.h"

ig_verdict_t ig_li_verdict(injection_result_t r)
{
	switch (r) {
	case LIBINJECTION_RESULT_FALSE:
		return IG_CLEAN;
	case LIBINJECTION_RESULT_TRUE:
		return IG_ATTACK;
	default:
		return IG_ERROR;
	}
}
