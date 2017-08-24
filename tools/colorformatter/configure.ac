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
AC_INIT([colorformatter],
	[0.01],
	[https://github.com/intel/IA-Hardware-Composer/tools/colorformatter],
	[colorformatter])

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

AC_SUBST(ASSEMBLER_WARN_CFLAGS)

AC_CONFIG_FILES([
		Makefile
		])
AC_OUTPUT
