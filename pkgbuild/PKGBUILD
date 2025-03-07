pkgver=v2.17.2
pkgname=millennium
pkgrel=6
pkgdesc="Millennium is an open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead."
arch=('x86_64')
url="https://github.com/shdwmtr/millennium"
license=('MIT')
depends=('gtk3' 'ninja' 'cmake' 'gcc-multilib' 'glibc' 'zlib' 'ncurses' 'gdbm' 'nss' 'openssl' 'readline' 'libffi' 'sqlite' 'xz' 'python-i686-bin' 'git')
makedepends=('git' 'nodejs' 'npm')
source=("git+$url.git")
sha256sums=('SKIP')
validpgpkeys=('D4A49B8AB39D704F')
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
    destinationBase="$pkgdir$HOME/.local/share/millennium/lib/assets"

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
    mkdir -p "$pkgdir$HOME/.local/share/millennium/lib/shims"

    cp -r ./node_modules/@steambrew/api/dist/webkit_api.js "$pkgdir$HOME/.local/share/millennium/lib/shims/webkit_api.js"
    cp -r ./node_modules/@steambrew/api/dist/client_api.js "$pkgdir$HOME/.local/share/millennium/lib/shims/client_api.js"

    echo -e "\e[1m\e[92m==>\e[0m \e[1mCreating virtual environment...\e[0m"
    /opt/python-i686-3.11.8/bin/python3.11 -m venv "$pkgdir$HOME/.local/share/millennium/lib/cache/" --system-site-packages --symlinks

    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}

post_install() {
    echo -e "\e[1m\e[92m==>\e[0m \e[1mSetting permissions...\e[0m"
    sudo chown -R $USER:$USER $HOME/.local/share/millennium
    echo -e "\e[1m\e[92m==>\e[0m \e[1mPlease run 'millennium patch' setup Millennium.\e[0m"
}
