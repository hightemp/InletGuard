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
  IG_SOURCES="inletguard.c src/core/ig_version.c"

  IG_CFLAGS="-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -std=gnu99"
  if test "$PHP_INLETGUARD_DEV" != "no"; then
    IG_CFLAGS="$IG_CFLAGS -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers"
  fi

  PHP_NEW_EXTENSION(inletguard, $IG_SOURCES, $ext_shared,, $IG_CFLAGS)
  PHP_ADD_BUILD_DIR([$ext_builddir/src/core])
  PHP_ADD_INCLUDE([$ext_srcdir/src])

  dnl Vendored third-party code is built unmodified with its own, lenient
  dnl flags: our strict warnings and -Werror apply to our code only.
  IG_TP_SOURCES="libinjection_sqli.c libinjection_xss.c libinjection_html5.c"
  IG_TP_CFLAGS="-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -std=gnu99"
  PHP_ADD_BUILD_DIR([$ext_builddir/third_party/libinjection])
  PHP_ADD_INCLUDE([$ext_srcdir/third_party/libinjection])
  if test "$ext_shared" = "yes"; then
    PHP_ADD_SOURCES_X([third_party/libinjection], [$IG_TP_SOURCES], [$IG_TP_CFLAGS], [shared_objects_inletguard], [yes])
  else
    PHP_ADD_SOURCES([third_party/libinjection], [$IG_TP_SOURCES], [$IG_TP_CFLAGS])
  fi
  PHP_ADD_MAKEFILE_FRAGMENT
fi
