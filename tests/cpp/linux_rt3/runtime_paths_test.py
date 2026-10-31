import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(sys.argv[1]).resolve()
cmake, readelf, mode = sys.argv[2:5]
with tempfile.TemporaryDirectory() as directory:
    work = pathlib.Path(directory)
    source = work / "source"
    build = work / "build"
    install = work / "install"
    source.mkdir()
    (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.21)
project(RuntimePathsTest LANGUAGES C)
include("{root}/scripts/cmake/linux_runtime_paths.cmake" OPTIONAL)
add_library(runtime_library SHARED "{root}/tests/cpp/linux_rt3/payload.c")
add_executable(runtime_executable "{root}/tests/cpp/linux_rt3/client.c")
foreach(target runtime_library runtime_executable)
    set_target_properties(${{target}} PROPERTIES BUILD_RPATH "${{CMAKE_BINARY_DIR}}/preserved" INSTALL_RPATH "${{CMAKE_INSTALL_PREFIX}}/preserved")
    if(COMMAND millennium_configure_linux_host_rpath)
        millennium_configure_linux_host_rpath(${{target}})
    endif()
endforeach()
install(TARGETS runtime_library runtime_executable LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
''')
    command = [cmake, "-S", str(source), "-B", str(build), "-G", "Ninja", "-DCMAKE_INSTALL_PREFIX=" + str(install)]
    paths = ["/run/host/usr/lib64", "/run/host/usr/lib", "/run/host/usr/lib/x86_64-linux-gnu"]
    if mode == "custom":
        paths = [str(work / "custom64"), str(work / "custom")]
        command.append("-DMILLENNIUM_LINUX_HOST_LIBRARY_PATHS=" + ";".join(paths))
    elif mode == "nix":
        command.append("-DDISTRO_NIX=ON")
    elif mode == "legacy-nix":
        command.append("-DMILLENNIUM_NIX_64BIT=ON")
    elif mode == "disabled":
        command.append("-DMILLENNIUM_LINUX_HOST_LIBRARY_PATHS=")
    subprocess.run(command, check=True)
    subprocess.run([cmake, "--build", str(build)], check=True)
    subprocess.run([cmake, "--install", str(build)], check=True)
    for artifact, preserved in [(build / "libruntime_library.so", build / "preserved"), (build / "runtime_executable", build / "preserved"), (install / "lib/libruntime_library.so", install / "preserved"), (install / "bin/runtime_executable", install / "preserved")]:
        dynamic = subprocess.run([readelf, "-dW", str(artifact)], check=True, capture_output=True, text=True).stdout
        assert str(preserved) in dynamic, dynamic
        if mode in ["default", "custom"]:
            assert "(RPATH)" in dynamic and "(RUNPATH)" not in dynamic, dynamic
            assert all(path in dynamic for path in paths), dynamic
        else:
            assert "/run/host/" not in dynamic, dynamic
