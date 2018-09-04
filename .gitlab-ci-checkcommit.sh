#! /usr/bin/env bash

echoerr() {
	printf "ERROR: %s\n" "$*" >&2
}

git fetch https://gitlab.freedesktop.org/drm-hwcomposer/drm-hwcomposer.git

git log --pretty='%h' FETCH_HEAD..HEAD | while read h; do
	subject=$(git show -s --pretty='%s' "$h")
	if [[ $subject != drm_hwcomposer:* ]]; then
		echoerr "Invalid subject prefix: $subject"
		exit 1
	fi

	commit_body=$(git show -s --pretty=%b "$h")

	author=$(git show -s --format='%an <%ae>')
	sob=$(echo "$commit_body" | grep "Signed-off-by: $author")
	if [ -z "$sob" ] ; then
		echoerr "Author SoB tag is missing from commit $h"
		exit 1
	fi

	committer=$(git show -s --format='%cn <%ce>')
	sob=$(echo "$commit_body" | grep "Signed-off-by: $committer")
	if [ -z "$sob" ] ; then
		echoerr "Committer SoB tag is missing from commit $h"
		exit 1
	fi

	git show "$h" -- | clang-format-diff-5.0 -p 1 -style=file > format-fixup.patch
	if [ -s  format-fixup.patch ]; then
		cat format-fixup.patch >&2
		exit 1
	fi
done
