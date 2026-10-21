/* SPDX-License-Identifier: MIT */

#ifndef PHP_INLETGUARD_H
#define PHP_INLETGUARD_H

extern zend_module_entry inletguard_module_entry;
#define phpext_inletguard_ptr &inletguard_module_entry

#define PHP_INLETGUARD_EXTNAME "inletguard"
#define PHP_INLETGUARD_VERSION "0.0.0-dev"

#if defined(ZTS) && defined(COMPILE_DL_INLETGUARD)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif /* PHP_INLETGUARD_H */
