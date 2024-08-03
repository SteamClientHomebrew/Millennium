# download python 3.11.8 source code
curl -o Python-3.11.8.tgz https://www.python.org/ftp/python/3.11.8/Python-3.11.8.tgz
# extract the tarball
tar -xzvf Python-3.11.8.tgz >nul 2>&1

Set-Location Python-3.11.8

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
msbuild PCBuild/pcbuild.sln /p:Configuration=Debug /p:Platform=Win32 /p:RuntimeLibrary=MT
# verify python is installed
PCbuild/win32/python.exe --version


mkdir D:/a/Millennium/Millennium/build/python
# Move the python311.dll binary to the release directory
Copy-Item D:/a/Millennium/Millennium/Python-3.11.8/PCbuild/win32/python311.dll D:/a/Millennium/Millennium/build/python/python311.dll
Copy-Item D:/a/Millennium/Millennium/Python-3.11.8/PCbuild/win32/python311_d.dll D:/a/Millennium/Millennium/build/python/python311_d.dll
Copy-Item D:/a/Millennium/Millennium/Python-3.11.8/PCbuild/win32/python311.lib D:/a/Millennium/Millennium/build/python/python311.lib
Copy-Item D:/a/Millennium/Millennium/Python-3.11.8/PCbuild/win32/python311_d.lib D:/a/Millennium/Millennium/build/python/python311_d.lib