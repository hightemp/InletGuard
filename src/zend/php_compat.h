/* SPDX-License-Identifier: MIT */
/*
 * Compatibility layer for PHP 7.0 - 8.4.
 *
 * Rules (enforced by scripts/check-compat.sh in CI):
 *  - this is the only file allowed to check PHP/Zend version macros;
 *  - wrappers are added by the task that needs them, not in advance;
 *  - every wrapper uses the IG_ prefix, has a comment saying what differs
 *    and since which version, and is covered by a phpt that runs on the
 *    whole matrix.
 */

#ifndef IG_PHP_COMPAT_H
#define IG_PHP_COMPAT_H

#include "php.h"

/*
 * arginfo with a return type. PHP 7.0 and 7.1 take an extra class_name
 * argument; PHP 7.2+ dropped it.
 */
#if PHP_VERSION_ID >= 70200
#define IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO(name, required, type, allow_null) \
	ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(name, 0, required, type, allow_null)
#else
#define IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO(name, required, type, allow_null) \
	ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(name, 0, required, type, NULL, allow_null)
#endif

#endif /* IG_PHP_COMPAT_H */
