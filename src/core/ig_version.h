/* SPDX-License-Identifier: MIT */
/* Single source of truth for the InletGuard version. */

#ifndef IG_VERSION_H
#define IG_VERSION_H

#define IG_VERSION_MAJOR 0
#define IG_VERSION_MINOR 0
#define IG_VERSION_PATCH 0
#define IG_VERSION_SUFFIX "-dev"

#define IG_STRINGIFY_(x) #x
#define IG_STRINGIFY(x) IG_STRINGIFY_(x)

#define IG_VERSION                 \
	IG_STRINGIFY(IG_VERSION_MAJOR) \
	"." IG_STRINGIFY(IG_VERSION_MINOR) "." IG_STRINGIFY(IG_VERSION_PATCH) IG_VERSION_SUFFIX

const char *ig_version(void);

#endif /* IG_VERSION_H */
