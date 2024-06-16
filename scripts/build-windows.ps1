# this script is still in development

# download python 3.11.8 source code
curl -o Python-3.11.8.tgz https://www.python.org/ftp/python/3.11.8/Python-3.11.8.tgz
# extract the tarball
tar -xzvf Python-3.11.8.tgz

cd Python-3.11.8

$vcxprojPath = "PCbuild/pythoncore.vcxproj"
$content = Get-Content $vcxprojPath -Raw  # Read the entire content as a single string

$pattern = '</ClCompile>'
$replacement = @"
<RuntimeLibrary Condition="'`$(Configuration)|`$(Platform)'=='Release|Win32'">MultiThreaded</RuntimeLibrary>
<RuntimeLibrary Condition="'`$(Configuration)|`$(Platform)'=='Debug|Win32'">MultiThreadedDebug</RuntimeLibrary>
</ClCompile>
"@

$modifiedContent = $content -replace [regex]::Escape($pattern), $replacement
$modifiedContent | Set-Content $vcxprojPath

# get python external libs before build
./PCbuild/get_externals.bat
# build python 3.11.8 as win32 and release
msbuild PCBuild/pcbuild.sln /p:Configuration=Release /p:Platform=Win32 /p:RuntimeLibrary=MT
# verify python is installed
PCbuild\win32\python.exe --version

# Integrate vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

cmake --preset=windows -G "Visual Studio 17 2022" -A Win32 -DGITHUB_ACTION_BUILD=ON
cmake --build build --config Release
