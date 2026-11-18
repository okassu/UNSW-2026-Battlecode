#!/bin/sh
# Usage: tools/snap.sh NAME
# Copies mybot's sources to snapshots/NAME and builds it there, so later edits to mybot
# don't change what a variant (tools/variant.sh NAME snapshots/NAME/.unswbc-build/bot ...) runs.
set -e
name=$1
mkdir -p snapshots/$name
cp mybot/main.cpp mybot/helper.hpp mybot/maplib.hpp mybot/bot.toml snapshots/$name/
~/.local/share/uv/tools/unswbc/bin/python -c "
from unswbc.run import _resolve
print(_resolve('snapshots/$name')[0][0])"
