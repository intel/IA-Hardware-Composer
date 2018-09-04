#! /usr/bin/env bash

git fetch https://gitlab.freedesktop.org/drm-hwcomposer/drm-hwcomposer.git

git diff -U0 --no-color FETCH_HEAD...HEAD -- | clang-format-diff-5.0 -p 1 -style=file > format-fixup.patch
if [ -s format-fixup.patch ]; then
	cat format-fixup.patch
	exit 1
fi
