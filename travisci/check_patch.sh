#!/bin/bash

difference=$(git show origin/master..@ | clang-format-diff-3.9 -p 1 -style=file);
[ ! -z "$difference" ] && echo "Code formatting is not according to style guidelines. Read https://github.com/intel/IA-Hardware-Composer/wiki/Contributions#coding_style" && exit 1;
exit 0;
