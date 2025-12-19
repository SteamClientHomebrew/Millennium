# Maintainers: SteamClientHomebrew <https://github.com/SteamClientHomebrew>

pkgver=2.31.1
pkgname="millennium"
_pkgdir="Millennium"
pkgrel=4
pkgdesc="Millennium is an open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead."
arch=('x86_64')
url="https://github.com/SteamClientHomebrew/Millennium"
license=('MIT')
depends=('git' 'steam')
makedepends=('npm' 'curl' 'zip' 'unzip' 'tar' 'cmake' 'ninja' 'lib32-gcc-libs' 'pnpm')
depends_x86_64=('lib32-python311-bin')
conflicts=('python-i686-bin')
source=("git+$url.git#commit=a49b12326f030f0c10430e36fc0cb2bd24bb5edf") # TODO: update to commit on main branch when we merge.
sha256sums=('SKIP')
options=(!debug)
install=millennium.install

prepare() {
    cd      $srcdir/$_pkgdir
    echo -e "\e[1m\e[92m==>\e[0m \e[1mCloning submodules...\e[0m"
    git submodule update --init --recursive
}

build() {
    export NODE_NO_WARNINGS=1

    echo -e    "\e[1m\e[92m==>\e[0m \e[1mBuilding Millennium assets...\e[0m"

    pnpm --dir $srcdir/$_pkgdir/sdk           install
    pnpm --dir $srcdir/$_pkgdir/sdk           run build
    pnpm --dir $srcdir/$_pkgdir/src/frontend  install
    pnpm --dir $srcdir/$_pkgdir/src/frontend  run build

    mkdir -p   $srcdir/$_pkgdir/shims/build/
    cp -r      $srcdir/$_pkgdir/sdk/packages/loader/build "./shims/"

    echo -e    "\e[1m\e[92m==>\e[0m \e[1mBuilding Millennium...\e[0m"

    cd         $srcdir/$_pkgdir
    cmake --preset linux-release -G "Ninja" -DDISTRO_ARCH=ON
    cmake --build build --config Release
}

package() {
    # Create final directory structure
    mkdir -p       $pkgdir/usr/lib/millennium
    mkdir -p       $pkgdir/usr/share/millennium/shims
    mkdir -p       $pkgdir/usr/share/millennium/assets
    mkdir -p       $pkgdir/usr/share/licenses/$pkgname

    install -Dm755 $srcdir/$_pkgdir/build/src/millennium_x86-build/libmillennium_x86.so                      "$pkgdir/usr/lib/millennium/"
    install -Dm755 $srcdir/$_pkgdir/build/src/millennium_x86-build/boot/linux/libmillennium_bootstrap_86x.so "$pkgdir/usr/lib/millennium/"
    install -Dm755 $srcdir/$_pkgdir/build/src/hhx64-build/libmillennium_hhx64.so                             "$pkgdir/usr/lib/millennium/"
    cp -r          $srcdir/$_pkgdir/src/pipx                                                                 "$pkgdir/usr/share/millennium/assets/"
    cp -r          $srcdir/$_pkgdir/shims/build                                                              "$pkgdir/usr/share/millennium/shims/"
    install -Dm644 $srcdir/$_pkgdir/LICENSE.md                                                               "$pkgdir/usr/share/licenses/$pkgname/"
}
