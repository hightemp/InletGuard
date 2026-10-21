/* SPDX-License-Identifier: MIT */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "php_inletguard.h"
#include "zend/php_compat.h"

IG_BEGIN_ARG_WITH_RETURN_TYPE_INFO(arginfo_inletguard_version, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* {{{ InletGuard\version(): string */
static ZEND_NAMED_FUNCTION(zif_inletguard_version)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}
	RETURN_STRING(ig_version());
}
/* }}} */

/* clang-format off */
static const zend_function_entry inletguard_functions[] = {
	ZEND_NS_NAMED_FE("InletGuard", version, zif_inletguard_version, arginfo_inletguard_version)
	ZEND_FE_END
};
/* clang-format on */

static PHP_MINIT_FUNCTION(inletguard)
{
	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(inletguard)
{
	return SUCCESS;
}

static PHP_RINIT_FUNCTION(inletguard)
{
#if defined(ZTS) && defined(COMPILE_DL_INLETGUARD)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(inletguard)
{
	return SUCCESS;
}

static PHP_MINFO_FUNCTION(inletguard)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "InletGuard support", "enabled");
	php_info_print_table_row(2, "Version", PHP_INLETGUARD_VERSION);
	php_info_print_table_row(2, "Compiled-in modules", IG_COMPILED_MODULES);
	php_info_print_table_end();
}

/* clang-format off */
zend_module_entry inletguard_module_entry = {
	STANDARD_MODULE_HEADER,
	PHP_INLETGUARD_EXTNAME,
	inletguard_functions,
	PHP_MINIT(inletguard),
	PHP_MSHUTDOWN(inletguard),
	PHP_RINIT(inletguard),
	PHP_RSHUTDOWN(inletguard),
	PHP_MINFO(inletguard),
	PHP_INLETGUARD_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* clang-format on */

#ifdef COMPILE_DL_INLETGUARD
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(inletguard)
#endif
