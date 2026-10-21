/* SPDX-License-Identifier: MIT */
/*
 * Compatibility layer for PHP 7.0 - 8.4.
 *
 * This is the only place where PHP_VERSION_ID checks are allowed. Every
 * wrapper here must be covered by a phpt that runs on all supported versions.
 */

#ifndef IG_PHP_COMPAT_H
#define IG_PHP_COMPAT_H

#include "php.h"

/*
 * arginfo with a return type. PHP 7.0 and 7.1 take an extra class_name
 * argument; PHP 7.2+ dropped it.
 */
#if PHP_VERSION_ID >= 70200
#define IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO(name, required, type, allow_null)  \
	ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(name, 0, required, type, allow_null)
#else
#define IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO(name, required, type, allow_null)  \
	ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(name, 0, required, type, NULL, allow_null)
#endif

#endif /* IG_PHP_COMPAT_H */
