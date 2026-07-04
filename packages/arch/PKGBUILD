# Maintainer: SteamClientHomebrew <noreply@steambrew.app>

pkgname="millennium"
pkgver=3.4.0_beta.6
pkgrel=1
pkgdesc="Open-source modding framework for creating and managing Steam Client themes and plugins"
arch=('x86_64')
url="https://github.com/SteamClientHomebrew/Millennium"
license=('MIT')
depends=('steam')
makedepends=('git' 'bun' 'curl' 'zip' 'unzip' 'tar' 'cmake' 'ninja' 'rust' 'lib32-gcc-libs' 'lib32-openssl' 'lib32-libidn2' 'lib32-xz' 'lib32-zstd' 'lib32-brotli' 'lib32-libnghttp2' 'lib32-libpsl' 'libx11' 'libxtst')
install=millennium.install
source=("git+$url.git#commit=5b23e9e9f57925fb74ece7b0cb8a37bae037fa80")
sha256sums=('SKIP')

_pkgdir="Millennium"

build() {
    cd "$srcdir/$_pkgdir"

    cmake --preset linux-arch-pkgbuild
    cmake --build build
}

package() {
    cd "$srcdir/$_pkgdir"

    install -d "$pkgdir/usr/lib/millennium"
    install -m755 build/libmillennium_x86.so "$pkgdir/usr/lib/millennium/"
    install -m755 build/libmillennium_hhx64.so "$pkgdir/usr/lib/millennium/"
    install -m755 build/libmillennium_bootstrap_x86.so "$pkgdir/usr/lib/millennium/"
    install -m755 build/libmillennium_luavm_x86 "$pkgdir/usr/lib/millennium/"
    install -m755 build/libmillennium_bootstrap_hhx64.so "$pkgdir/usr/lib/millennium/"
    install -m755 build/libmillennium_pvs64 "$pkgdir/usr/lib/millennium/"
    install -Dm644 LICENSE.md "$pkgdir/usr/share/licenses/millennium/LICENSE.md"
}
