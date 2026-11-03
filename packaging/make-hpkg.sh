#!/bin/sh
# SPDX-License-Identifier: MIT
# Build the renkugraph .hpkg on a running RenkuOS (x86_64).
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
VERSION=0.1
REVISION=1
STAGE="$HERE/stage"

cd "$ROOT"
make clean
make

# A GUI app under apps/ plus a Deskbar menu entry.
rm -rf "$STAGE"
mkdir -p "$STAGE/apps" "$STAGE/data/deskbar/menu/Applications"
cp RenkuGraph "$STAGE/apps/RenkuGraph"
ln -sf ../../../../apps/RenkuGraph \
	"$STAGE/data/deskbar/menu/Applications/RenkuGraph"

cp "$HERE/renkugraph.PackageInfo" "$STAGE/.PackageInfo"
OUT="$HERE/renkugraph-$VERSION-$REVISION-x86_64.hpkg"
package create -C "$STAGE" "$OUT"
echo ">> built $OUT"
