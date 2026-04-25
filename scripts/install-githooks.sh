#!/bin/sh
set -e
root=$(git rev-parse --show-toplevel)
cd "$root"
git config core.hooksPath .githooks
echo "Set core.hooksPath to .githooks in: $root"
