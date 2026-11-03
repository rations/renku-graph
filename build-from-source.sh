#!/bin/sh
# SPDX-License-Identifier: MIT
# Build Renku Graph from source on RenkuOS and install it non-packaged.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)

pkgman install -y gcc make haiku_devel

cd "$HERE"
make clean
make

mkdir -p /boot/system/non-packaged/apps
cp RenkuGraph /boot/system/non-packaged/apps/RenkuGraph
echo ">> Installed RenkuGraph to /boot/system/non-packaged/apps/RenkuGraph"
