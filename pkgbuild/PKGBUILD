pkgname=millennium
pkgver=3.11.8
pkgrel=1
pkgdesc="A software project Millennium"
arch=('x86_64')
url="https://github.com/shdwmtr/millennium"
license=('MIT')
depends=('gtk3' 'ninja' 'cmake' 'gcc-multilib' 'glibc' 'zlib' 'ncurses' 'gdbm' 'nss' 'openssl' 'readline' 'libffi' 'sqlite' 'xz')
makedepends=('git' 'nodejs' 'npm')
source=("git+$url.git")
sha256sums=('SKIP')
options=(!debug)

build() {
    export NODE_NO_WARNINGS=1
    cd "$srcdir/millennium"

    echo -e "\e[1m\e[92m==>\e[0m \e[1mCloning submodules...\e[0m"
    git submodule update --init --recursive

    echo -e "\e[1m\e[92m==>\e[0m \e[1mInstalling dependencies...\e[0m"
    npm install @steambrew/api

    echo -e "\e[1m\e[92m==>\e[0m \e[1mBuilding Millennium core assets...\e[0m"

    cd assets
    npm install
    npm run build
    cd ..

    echo -e "\e[1m\e[92m==>\e[0m \e[1mBootstrapping VCPKG...\e[0m"

    ./vendor/vcpkg/bootstrap-vcpkg.sh
    ./vendor/vcpkg/vcpkg integrate install

    echo -e "\e[1m\e[92m==>\e[0m \e[1mBuilding Millennium...\e[0m"

    cmake --preset=linux-release -G "Ninja" -DDISTRO_ARCH=ON
    cmake --build build --config Release
}

package() {
    cd "$srcdir/millennium"
    destinationBase="$pkgdir/usr/share/millennium/assets"

    declare -A paths=(
        ["./assets/.millennium/Dist/index.js"]="$destinationBase/.millennium/Dist/index.js"
        ["./assets/core"]="$destinationBase/core"
        ["./assets/pipx"]="$destinationBase/pipx"
        ["./assets/requirements.txt"]="$destinationBase/requirements.txt"
        ["./assets/plugin.json"]="$destinationBase/plugin.json"
    )

    for source in "${!paths[@]}"; do
        destination="${paths[$source]}"
        destinationDir="$(dirname "$destination")"
        if [ ! -d "$destinationDir" ]; then
            echo "Creating directory $destinationDir"
            mkdir -p "$destinationDir"
        fi
        cp -r "$source" "$destination"
    done
    
    install -Dm755 build/libmillennium_x86.so "$pkgdir/usr/lib/millennium/libmillennium_x86.so"
    install -Dm755 build/cli/millennium "$pkgdir/usr/bin/millennium"
    
    mkdir -p "$pkgdir/usr/lib/millennium"
    cp "/opt/python-i686-3.11.8/lib/libpython-3.11.8.so" "$pkgdir/usr/lib/millennium/libpython-3.11.8.so"

    mkdir -p "$pkgdir/usr/share/millennium/shims"

    cp -r ./node_modules/@steambrew/api/dist/webkit_api.js "$pkgdir/usr/share/millennium/shims/webkit_api.js"
    cp -r ./node_modules/@steambrew/api/dist/client_api.js "$pkgdir/usr/share/millennium/shims/client_api.js"

    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
