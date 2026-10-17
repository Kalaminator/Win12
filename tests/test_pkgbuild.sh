#!/bin/sh
# Run: sh tests/test_pkgbuild.sh   -- kde/pkgbuild-win12.sh against a tiny PKGBUILD shaped like Arch's plasma ones.
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$T/patches"; printf 'diff --git a/x b/x\n' > "$T/patches/0001-win12-x.patch"
cat > "$T/PKGBUILD" <<'EOP'
pkgname=plasma-workspace
pkgver=6.8.0
pkgrel=1
arch=(x86_64)
source=(x.tar.xz)
sha256sums=('0')
build() {
  cmake -B build -S $pkgname-$pkgver \
    -DBUILD_TESTING=OFF
  cmake --build build
}
package() {
  DESTDIR="$pkgdir" cmake --install build
}
EOP
cd "$T" && bash "$HERE/kde/pkgbuild-win12.sh" plasma-workspace "$T/patches"
bash -n PKGBUILD
S=$(makepkg --printsrcinfo)
echo "$S" | grep -q 'source = 0001-win12-x.patch' \
  && echo "$S" | grep -q 'provides = plasma-workspace-win12=6.8.0-1' && grep -q 'BUILD_TESTING=ON' PKGBUILD \
  && echo "$S" | grep -q 'makedepends = qt6-wayland' \
  && ! echo "$S" | grep -q 'hook' && grep -c '^package() {' PKGBUILD | grep -qx 1 && echo "test_pkgbuild OK"
# second run must refuse (prepare() already there)
if bash "$HERE/kde/pkgbuild-win12.sh" plasma-workspace "$T/patches" 2>/dev/null; then echo "FAIL: edited twice"; exit 1; fi
