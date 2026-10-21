dnl SPDX-License-Identifier: MIT
dnl config.m4 for the InletGuard extension

PHP_ARG_ENABLE([inletguard],
  [whether to enable InletGuard support],
  [AS_HELP_STRING([--enable-inletguard], [Enable InletGuard support])],
  [no])

PHP_ARG_ENABLE([inletguard-dev],
  [whether to enable InletGuard developer build flags],
  [AS_HELP_STRING([--enable-inletguard-dev], [Developer build: strict warnings, -Werror])],
  [no], [no])

if test "$PHP_INLETGUARD" != "no"; then
  IG_SOURCES="inletguard.c"

  IG_CFLAGS="-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -std=gnu99"
  if test "$PHP_INLETGUARD_DEV" != "no"; then
    IG_CFLAGS="$IG_CFLAGS -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers"
  fi

  PHP_NEW_EXTENSION(inletguard, $IG_SOURCES, $ext_shared,, $IG_CFLAGS)
  PHP_ADD_INCLUDE([$ext_srcdir/src])
  PHP_ADD_MAKEFILE_FRAGMENT
fi
