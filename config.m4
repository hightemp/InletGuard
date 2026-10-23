dnl SPDX-License-Identifier: MIT
dnl config.m4 for the InletGuard extension

PHP_ARG_ENABLE([inletguard],
  [whether to enable InletGuard support],
  [AS_HELP_STRING([--enable-inletguard], [Enable InletGuard support])],
  [no])

PHP_ARG_WITH([inletguard-modules],
  [which InletGuard attack modules to build],
  [AS_HELP_STRING([--with-inletguard-modules=LIST],
    [Attack modules to build: all (default), none, a comma-separated list, or all,-module])],
  [all], [no])

PHP_ARG_ENABLE([inletguard-dev],
  [whether to enable InletGuard developer build flags],
  [AS_HELP_STRING([--enable-inletguard-dev], [Developer build: strict warnings, -Werror])],
  [no], [no])

if test "$PHP_INLETGUARD" != "no"; then
  IG_SOURCES="inletguard.c src/core/ig_version.c src/core/ig_detect.c src/core/ig_libinjection.c src/core/ig_detector.c src/core/ig_normalize.c src/core/ig_glob.c"

  IG_CFLAGS="-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -std=gnu99"
  if test "$PHP_INLETGUARD_DEV" != "no"; then
    IG_CFLAGS="$IG_CFLAGS -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers"
  fi

  dnl Attack modules compiled into the extension (FR-10.2). The list of known
  dnl modules moves into the module registry in TASKS 2.8.
  IG_KNOWN_MODULES="sqli xss"
  AC_MSG_CHECKING([for InletGuard attack modules])
  ig_arg="$PHP_INLETGUARD_MODULES"
  test "$ig_arg" = "yes" && ig_arg=all
  ig_items=`echo "$ig_arg" | tr ',' ' '`
  if test -z "$ig_items" || test "$ig_arg" = "no"; then
    AC_MSG_ERROR([--with-inletguard-modules needs a value: all, none, a list or all,-module (known modules: $IG_KNOWN_MODULES)])
  fi
  ig_selected=""
  for ig_item in $ig_items; do
    ig_name=`echo "$ig_item" | sed 's/^-//'`
    case "$ig_name" in
      all|none) ;;
      *)
        case " $IG_KNOWN_MODULES " in
          *" $ig_name "*) ;;
          *) AC_MSG_ERROR([unknown InletGuard module '$ig_name' (known modules: $IG_KNOWN_MODULES)]) ;;
        esac
        ;;
    esac
    case "$ig_item" in
      all) ig_selected="$IG_KNOWN_MODULES" ;;
      none) ig_selected="" ;;
      -*)
        ig_rest=""
        for ig_m in $ig_selected; do
          test "$ig_m" != "$ig_name" && ig_rest="$ig_rest $ig_m"
        done
        ig_selected="$ig_rest"
        ;;
      *) ig_selected="$ig_selected $ig_name" ;;
    esac
  done
  IG_COMPILED_MODULES=""
  for ig_m in $IG_KNOWN_MODULES; do
    case " $ig_selected " in
      *" $ig_m "*) IG_COMPILED_MODULES="${IG_COMPILED_MODULES:+$IG_COMPILED_MODULES, }$ig_m" ;;
    esac
  done
  test -z "$IG_COMPILED_MODULES" && IG_COMPILED_MODULES=none
  AC_MSG_RESULT([$IG_COMPILED_MODULES])
  AC_DEFINE_UNQUOTED([IG_COMPILED_MODULES], ["$IG_COMPILED_MODULES"], [InletGuard attack modules compiled in])
  case " $ig_selected " in
    *" sqli "*) AC_DEFINE([IG_MODULE_SQLI], [1], [Build the sqli module]) ;;
    *) AC_DEFINE([IG_MODULE_SQLI], [0], [Build the sqli module]) ;;
  esac
  case " $ig_selected " in
    *" xss "*) AC_DEFINE([IG_MODULE_XSS], [1], [Build the xss module]) ;;
    *) AC_DEFINE([IG_MODULE_XSS], [0], [Build the xss module]) ;;
  esac

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
