/* SPDX-License-Identifier: MIT */

#ifndef PHP_INLETGUARD_H
#define PHP_INLETGUARD_H

extern zend_module_entry inletguard_module_entry;
#define phpext_inletguard_ptr &inletguard_module_entry

#include "core/ig_version.h"

#define PHP_INLETGUARD_EXTNAME "inletguard"
#define PHP_INLETGUARD_VERSION IG_VERSION

#if defined(ZTS) && defined(COMPILE_DL_INLETGUARD)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif /* PHP_INLETGUARD_H */
