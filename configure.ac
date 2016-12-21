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

AC_PREREQ([2.60])
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

PKG_CHECK_MODULES(DRM, [libdrm])
PKG_CHECK_MODULES(GBM, [gbm])
PKG_CHECK_MODULES(EGL, [egl])
PKG_CHECK_MODULES(GLES2, [glesv2])

AC_ARG_ENABLE(git-hash,
	      AS_HELP_STRING([--disable-git-hash],
			     [Do not use git hash in version]),
	      [git_hash=$enableval], [git_hash=yes])
AC_SUBST(GIT_HASH, [$git_hash])

AC_CONFIG_FILES([
		 Makefile
                 tests/Makefile
		 ])
AC_OUTPUT
