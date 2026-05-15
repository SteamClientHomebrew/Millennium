{
  cmake,
  ninja,
  pkg-config,
  git,
  cacert,
  python3,

  lib,
  stdenv,

  inputs,
  millennium-shims,
  millennium-assets,
  millennium-frontend,
  ...
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "millennium-64";
  version = "2.36.0";

  src = inputs.millennium-src;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    git
    python3
  ];

  buildInputs = [
    cacert
  ];

  cmakeGenerator = "Ninja";
  cmakeBuildType = "Release";
  enableParallelBuilding = true;

  cmakeFlags = [
    "-DGITHUB_ACTION_BUILD=ON"
    "-DDISTRO_NIX=ON"
    "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    "-DMILLENNIUM_BUILD_TESTS=OFF"
    "-DFETCHCONTENT_SOURCE_DIR_SNARE=${inputs.snare-src}"
  ];

  postPatch = ''
    mkdir -p deps

    prepare_dep() {
      local name="$1"
      local src="$2"
      echo "[Nix Millennium Build Setup] Preparing dependency: $name"
      cp -r --no-preserve=mode "$src" "deps/$name"
      chmod -R u+w "deps/$name"
    }

    echo "[Nix Millennium Build Setup] Copying flake inputs to local writable directories"
    prepare_dep abseil "${inputs.abseil-src}"
    prepare_dep re2 "${inputs.re2-src}"

    echo "[Nix Millennium Build Setup] Initializing Git Repos and adding Dummy Commits"
    echo "[Nix Millennium Build Setup] Dummy commits are used to determine versions, but flake inputs strip git history, causing issues"

    export HOME=$(pwd)

    git config --global init.defaultBranch main
    git config --global user.email "nix-build@localhost"
    git config --global user.name "Nix Build"

    git init
    git add .
    git commit -m "Dummy commit for build" > /dev/null 2>&1

    chmod -R u+rwx deps/

    echo "[Nix] Patching CMakeLists to IGNORE 32-bit source..."
    sed -i '/add_subdirectory.*src)/s/^/#/' CMakeLists.txt

    echo "[Nix] Writing cmake fragment for abseil + re2 (normally from bootstrap_deps.cmake via src/)..."
    # Patch re2 to skip install(EXPORT) which fails because absl targets aren't installed
    python3 - deps/re2/CMakeLists.txt <<'PEOF'
import sys, re
with open(sys.argv[1]) as f:
    content = f.read()
content = re.sub(r'install\(EXPORT.*?\)', "", content, flags=re.DOTALL)
with open(sys.argv[1], 'w') as f:
    f.write(content)
PEOF

    cat > nix_64bit_deps.cmake <<'NIXEOF'
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(RE2_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
FetchContent_Declare(absl SOURCE_DIR "''${MILLENNIUM_BASE}/deps/abseil" OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(absl)
FetchContent_Declare(re2 SOURCE_DIR "''${MILLENNIUM_BASE}/deps/re2" SOURCE_SUBDIR fakedir)
FetchContent_MakeAvailable(re2)
NIXEOF
    sed -i '/FetchContent_MakeAvailable(snare)/a include(''${CMAKE_SOURCE_DIR}/nix_64bit_deps.cmake)' CMakeLists.txt
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build .

    # pvs64 is a pure-stdlib 64-bit helper; compile it here since the 32-bit build cannot emit -m64
    g++ -m64 -std=c++17 -O2 -Wall -Wextra \
      ../src/bootstrap/linux/millennium_pvs64.cc \
      -o libmillennium_pvs64

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/lib/
    so=$(find . -name "libmillennium_hhx64.so" | head -1)
    install -Dm755 "$so" $out/lib/libmillennium_hhx64.so
    install -Dm755 libmillennium_pvs64 $out/lib/libmillennium_pvs64

    runHook postInstall
  '';

  meta = {
    homepage = "https://steambrew.app/";
    license = lib.licenses.mit;
    description = "Main Millennium Library used to load and apply plugins and themes";

    longDescription = "An open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead";

    maintainers = [
      lib.maintainers.trivaris
    ];

    platforms = [
      "x86_64-linux"
    ];
  };
})
