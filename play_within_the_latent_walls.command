#!/bin/sh
# Finder double-click or Terminal; resource resolution is independent of cwd.
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ ! -x "$project_dir/build_macos/liminal_cornell_renderer" ]; then
    echo 'Build first: python3.12 scripts/setup_macos.py && python3 scripts/build_macos.py --publish'
    exit 1
fi
exec "$project_dir/build_macos/liminal_cornell_renderer" "$@"
