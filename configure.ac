#
# Copyright (c) 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

AC_PREREQ([2.64])
AC_INIT([intel-hw-composer],
	[0.01],
	[https://github.com/android-ia/hwcomposer],
	[intel-hw-composer])

AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_GNU_SOURCE
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.12 foreign subdir-objects dist-bzip2])
AM_PATH_PYTHON([3],, [:])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_LEX
AC_PROG_YACC

# Initialize libtool
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_SUBST(ASSEMBLER_WARN_CFLAGS)

# For splash screen dummy compositor
AC_ARG_ENABLE(dummy-compositor,
  AS_HELP_STRING([--enable-dummy-compositor],
    [Enable the dummy compositor (EXPERIMENTAL)]),
[if test x$enableval = xyes; then
  enable_dummy_compositor=yes
  AC_DEFINE(ENABLE_DUMMY_COMPOSITOR, 1, [Enable Dummy_Compositor])
fi])

AM_CONDITIONAL([ENABLE_DUMMY_COMPOSITOR], [test "x$enable_dummy_compositor" = "xyes"])

# For vulkan
AC_ARG_ENABLE(vulkan,
  AS_HELP_STRING([--enable-vulkan],
    [Enable Vulkan backend (EXPERIMENTAL)]),
[if test x$enableval = xyes; then
  enable_vulkan=yes
  AC_DEFINE(ENABLE_VULKAN, 1, [Enable Vulkan backend])
fi])

AM_CONDITIONAL([ENABLE_VULKAN], [test "x$enable_vulkan" = "xyes"])

# For prebuilt-shader
AC_DEFINE(ENABLE_PREBUILT_SHADER_BIN_ARRAY, 0, [Enable built-in prebuilt shader array])

AC_ARG_WITH([prebuilt-shader-pci-id],
  [AS_HELP_STRING([--with-prebuilt-shader-pci-id@<:@=PCI ID in hex@:>@],
     [PCI-ID of target GPU is required for using built-in pre-built shaders, e.g.
     "0x0412" @<:@default=no@:>@])],
     [prebuilt_shader_pci_id="$withval"],
     [prebuilt_shader_pci_id=no])

# check if shader-db exist
SHADERDB_PATH=common/compositor/gl/gl_shader_pre_built/shader-db

if test "x$prebuilt_shader_pci_id" != "xno"; then
    SHADERDB_PATH=common/compositor/gl/gl_shader_pre_built/shader-db
    AC_CHECK_FILES([$SHADERDB_PATH $SHADERDB_PATH/run
                    $SHADERDB_PATH/intel_stub.so],
                    shaderdb=yes, shaderdb=no)
    if test "x$shaderdb" = "xyes"; then
        PREBUILT_SHADER_FOR_PCI_ID="$withval"
        AC_SUBST([PREBUILT_SHADER_FOR_PCI_ID])
    else
        prebuilt_shader_pci_id=no
        AC_MSG_WARN([shader-db is not found at ($SHADERDB_PATH), ignoring prebuilt shaders])
    fi
fi

AM_CONDITIONAL(ENABLE_PREBUILT_SHADER_BIN_ARRAY, test "x$prebuilt_shader_pci_id" != "xno")

# For linux
AC_ARG_ENABLE(linux-frontend,
AS_HELP_STRING([--enable-linux-frontend],
[Compile linux frontend (EXPERIMENTAL) @<:@default=no@:>@]),
[enable_linux_frontend="$enableval"],
[enable_linux_frontend=no])

AM_CONDITIONAL([ENABLE_LINUX_FRONTEND], [test "x$enable_linux_frontend" = "xyes"])

# For hotplug
AC_ARG_ENABLE(hotplug-support,
  AS_HELP_STRING([--disable-hotplug-support],
    [Disable Hot Plug Support.]),
[if test x$enableval = xyes; then
  disable_hotplug_support=yes
  AC_DEFINE(DISABLE_HOTPLUG_SUPPORT, 1, [Disable Hot Plug Support])
fi])

AM_CONDITIONAL(DISABLE_HOTPLUG_SUPPORT, test "x$disable_hotplug_support" = "xyes")

if test "x$disable_hotplug_support" = "xyes"; then
    AC_MSG_RESULT([Hot Plug support is disabled.])
  else
    AC_MSG_RESULT([Hot Plug support is enabled.])
fi

# For json-c
AC_CONFIG_HEADER(tests/third_party/json-c/json_config.h)
AC_ARG_ENABLE(rdrand,
  AS_HELP_STRING([--enable-rdrand],
    [Enable RDRAND Hardware RNG Hash Seed generation on supported x86/x64 platforms.]),
[if test x$enableval = xyes; then
  enable_rdrand=yes
  AC_DEFINE(ENABLE_RDRAND, 1, [Enable RDRANR Hardware RNG Hash Seed])
fi])

if test "x$enable_rdrand" = "xyes"; then
  AC_MSG_RESULT([RDRAND Hardware RNG Hash Seed enabled on supported x86/x64 platforms])
else
  AC_MSG_RESULT([RDRAND Hardware RNG Hash Seed disabled. Use --enable-rdrand to enable])
fi

AM_PROG_CC_C_O
AC_PROG_CC_C99

AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h strings.h syslog.h unistd.h [sys/cdefs.h] [sys/param.h] stdarg.h locale.h xlocale.h endian.h)
AC_CHECK_HEADER(inttypes.h,[AC_DEFINE([JSON_C_HAVE_INTTYPES_H],[1],[Public define for json_inttypes.h])])
AC_C_CONST
AC_TYPE_SIZE_T

AC_FUNC_VPRINTF
AC_FUNC_MEMCMP
AC_CHECK_FUNCS([realloc])
AC_CHECK_FUNCS(strcasecmp strdup strerror snprintf vsnprintf vasprintf open vsyslog strncasecmp setlocale uselocale)
AC_CHECK_DECLS([INFINITY], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([nan], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([isnan], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([isinf], [], [], [[#include <math.h>]])
AC_CHECK_DECLS([_isnan], [], [], [[#include <float.h>]])
AC_CHECK_DECLS([_finite], [], [], [[#include <float.h>]])

if test "$ac_cv_have_decl_isnan" = "yes" ; then
  AC_TRY_LINK([#include <math.h>], [float f = 0.0; return isnan(f)], [], [LIBS="$LIBS -lm"])
fi

AC_LANG_PUSH([C])

AC_MSG_CHECKING([if .gnu.warning accepts long strings])
AC_LINK_IFELSE([AC_LANG_SOURCE([[
extern void json_object_get();
__asm__(".section .gnu.json_object_get,\n\t.ascii \"Please link against libjson-c instead of libjson\"\n\t.text");

int main(int c,char* v) {return 0;}
]])], [
  AC_DEFINE(HAS_GNU_WARNING_LONG, 1, [Define if .gnu.warning accepts long strings.])
  AC_MSG_RESULT(yes)
], [
  AC_MSG_RESULT(no)
])

AC_LANG_POP([C])

AC_ARG_ENABLE([Bsymbolic],
		[AS_HELP_STRING([--disable-Bsymbolic], [Avoid linking with -Bsymbolic-function])],
		[],
		[enable_Bsymbolic=check])

AS_IF([test "x$enable_Bsymbolic" = "xcheck"],
	[
	saved_LDFLAGS="${LDFLAGS}"
	AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
	LDFLAGS=-Wl,-Bsymbolic-functions
	AC_TRY_LINK([], [int main (void) { return 0; }],
		[
			AC_MSG_RESULT([yes])
			enable_Bsymbolic=yes
		],
		[
			AC_MSG_RESULT([no])
			enable_Bsymbolic=no
		])
	LDFLAGS="${saved_LDFLAGS}"
])

AS_IF([test "x$enable_Bsymbolic" = "xyes"], [JSON_BSYMBOLIC_LDFLAGS=-Wl[,]-Bsymbolic-functions])
AC_SUBST(JSON_BSYMBOLIC_LDFLAGS)

AX_APPEND_COMPILE_FLAGS([-Wall -Wcast-qual -Wno-error=deprecated-declarations])
AX_APPEND_COMPILE_FLAGS([-Wextra -Wwrite-string -Wno-unused-parameter])
AX_APPEND_COMPILE_FLAGS([-D_GNU_SOURCE -D_REENTRANT])

AX_COMPILE_CHECK_SIZEOF(int)
AX_COMPILE_CHECK_SIZEOF(long)
AX_COMPILE_CHECK_SIZEOF(long long)
AX_COMPILE_CHECK_SIZEOF(size_t, [#include <stdint.h>])
# End of json-c
#######################################################################

PKG_CHECK_MODULES(DRM, [libdrm])
PKG_CHECK_MODULES(GBM, [gbm])
PKG_CHECK_MODULES(EGL, [egl])
PKG_CHECK_MODULES(GLES2, [glesv2])
PKG_CHECK_MODULES(LIBVA, [libva])
PKG_CHECK_MODULES(LIBVA_DRM, [libva-drm])

AC_ARG_ENABLE(git-hash,
		AS_HELP_STRING([--disable-git-hash],
			[Do not use git hash in version]),
			[git_hash=$enableval], [git_hash=yes])
AC_SUBST(GIT_HASH, [$git_hash])

AC_CONFIG_FILES([
		common/Makefile
		wsi/Makefile
		Makefile
		tests/third_party/json-c/Makefile
		tests/Makefile
		iahwc.pc
		])
AC_OUTPUT

echo -e """\nEnabled options """

AC_MSG_RESULT([
     Dummy compositor         $enable_dummy_compositor
     Vulkan                   $enable_vulkan
     Linux frontend           $enable_linux_frontend
     Hotplug Support          $disable_hotplug_support
     Prebuilt Shader Target   PCI-ID($prebuilt_shader_pci_id)
])

# Test both compositors aren't enabled.
if [[ ! -z $enable_dummy_compositor ]] && [[ ! -z $enable_vulkan ]];
then
    if test $enable_dummy_compositor = yes && test $enable_vulkan = yes; then
        echo "Error:"
        echo -e "\tOnly up to one compositor may be enabled at a time." 1>&2
        exit 1
    fi
fi
