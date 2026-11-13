# Maintainer: SteamClientHomebrew <noreply@steambrew.app>

_pkgname='millennium'
_Pkgname='Millennium'
pkgbase="${_pkgname}"
pkgname=("${_pkgname}" "${_pkgname}-x86")
pkgver=3.6.3
pkgrel=1
pkgdesc="${_Pkgname} is an open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead."
arch=('x86_64')
url="https://github.com/SteamClientHomebrew/${_Pkgname}"
license=('MIT')
depends=('steam')
makedepends=(
    'git'
    'bun'
    'curl'
    'zip'
    'unzip'
    'tar'
    'cmake'
    'ninja'
    'rust'
    'openssl'
    'lib32-gcc-libs'
    'lib32-openssl'
    'lib32-libidn2'
    'lib32-xz'
    'lib32-zstd'
    'lib32-brotli'
    'lib32-libnghttp2'
    'lib32-libpsl'
    'libx11'
    'libxi'
    'libxtst'
)
provides=("${_pkgname}=${pkgver}")
install=millennium.install
source=("git+$url.git#commit=ac5e1ad646b1d1dd76a04aecf32d9e5e41779201")
sha256sums=('SKIP')

build() {
    cd "${srcdir}/${_Pkgname}"

    cmake -GNinja --preset linux-arch-pkgbuild     .
    cmake -GNinja --preset linux-arch-pkgbuild-x86 .

    cmake --build build
    cmake --build build32
}

_install_millennium_variant() {
    install -Dm755 "${_build_dir}/lib${_pkgname}_${_arch}.so"           -t "${pkgdir}/usr/lib/${_pkgname}/"
    install -Dm755 "${_build_dir}/lib${_pkgname}_bootstrap_${_arch}.so" -t "${pkgdir}/usr/lib/${_pkgname}/"
    install -Dm755 "${_build_dir}/lib${_pkgname}_luavm_${_arch}"        -t "${pkgdir}/usr/lib/${_pkgname}/"
    install -Dm644 'LICENSE.md'                                         -t "${pkgdir}/usr/share/licenses/${pkgname}/"
}

package_millennium() {
    optdepends=("${_pkgname}-x86: Required for legacy 32-bit Steam clients")

    cd "${srcdir}/${_Pkgname}"

    _arch="x64"
    _build_dir="build"

    _install_millennium_variant

    install -Dm755 "build/lib${_pkgname}_hhx64.so"           -t "${pkgdir}/usr/lib/${_pkgname}/"
    install -Dm755 "build/lib${_pkgname}_bootstrap_hhx64.so" -t "${pkgdir}/usr/lib/${_pkgname}/"
    install -Dm755 "build/lib${_pkgname}_pvs64"              -t "${pkgdir}/usr/lib/${_pkgname}/"
}

package_millennium-x86() {
    depends+=("${_pkgname}=${pkgver}")
    provides=()
    install=millennium-x86.install

    cd "${srcdir}/${_Pkgname}"

    _arch="x86"
    _build_dir="build32"

    _install_millennium_variant
}
