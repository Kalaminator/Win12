#!/bin/sh
# Run: sh tests/test_explorer_pkgbuild.sh   -- explorer/PKGBUILD and explorer/build.sh --version.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
sed 's/@VER@/0.1.abcdef012345/' "$HERE/explorer/PKGBUILD" > "$T/PKGBUILD"
cd "$T"
bash -n PKGBUILD
S=$(makepkg --printsrcinfo)
for l in 'pkgname = win12-explorer' 'arch = x86_64' 'depends = kio' 'depends = kitemmodels' 'depends = purpose' \
         'makedepends = extra-cmake-modules' 'makedepends = cmake'; do
  echo "$S" | grep -q "$l" || { echo "FAIL: srcinfo lacks '$l'"; exit 1; }
done
grep -q 'BUILD_TESTING=ON' PKGBUILD || { echo "FAIL: BUILD_TESTING=ON"; exit 1; }
grep -q "makepkg -sfC" "$HERE/explorer/build.sh" || { echo "FAIL: build.sh must clean srcdir (stale CMake cache across versions)"; exit 1; }
grep -c '^package() {' PKGBUILD | grep -qx 1 || { echo "FAIL: package() count"; exit 1; }
V=$(sh "$HERE/explorer/build.sh" --version)
echo "$V" | grep -Eqx '0\.1\.[0-9a-f]{12}' || { echo "FAIL: version '$V'"; exit 1; }
[ "$V" = "$(sh "$HERE/explorer/build.sh" --version)" ] || { echo "FAIL: version not stable"; exit 1; }
grep -q '^pkgver=@VER@' "$HERE/explorer/PKGBUILD" || { echo "FAIL: pkgver placeholder"; exit 1; }
echo "test_explorer_pkgbuild OK"
