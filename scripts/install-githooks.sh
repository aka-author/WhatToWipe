#!/bin/sh
set -e
# Directory containing this script (works regardless of caller cwd).
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
d=$HERE
root=
while [ -n "$d" ]; do
  if [ -d "$d/.git" ]; then
    root=$d
    break
  fi
  parent=$(dirname "$d")
  if [ "$parent" = "$d" ]; then
    break
  fi
  d=$parent
done
if [ -z "$root" ]; then
  echo "No Git repository found walking up from: $HERE" >&2
  exit 1
fi
cd "$root"
git config core.hooksPath .githooks
echo "Set core.hooksPath to .githooks in: $root"
