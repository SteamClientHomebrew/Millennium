# This file is responsible for building python 3.11.8 from source code
# It is a required dependency for building Millennium.
# This build script is different installing from pythons FTP, attempting to do so will result in a failed build/undefined behavior.

# download python 3.11.8 source code
Invoke-WebRequest -Uri "https://www.python.org/ftp/python/3.11.8/Python-3.11.8.tgz" -OutFile "Python-3.11.8.tgz"
# extract the tarball
tar -xzvf "Python-3.11.8.tgz"
# set the working directory to the extracted folder
Set-Location "Python-3.11.8"

$vcxprojPath = "PCbuild/pythoncore.vcxproj"
$content = Get-Content $vcxprojPath -Raw  # Read the entire content as a single string

# Force the runtime library to be MultiThreaded for Release and Debug configurations
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



# Move the python311.dll binary to the release directory
# Move the python311.dll binary to the temp directory
$tempDir = "C:\Temp\PythonBuild"
New-Item -ItemType Directory -Force -Path $tempDir

Copy-Item PCbuild/win32/python311.dll -Destination $tempDir
Copy-Item PCbuild/win32/python311_d.dll -Destination $tempDir
Copy-Item PCbuild/win32/python311.lib -Destination $tempDir
Copy-Item PCbuild/win32/python311_d.lib -Destination $tempDir

# remove all content from the Python-3.11.8 directory
Set-Location ../
Remove-Item -Recurse -Force -Path "Python-3.11.8"

# move the python binaries to the build directory
New-Item -ItemType Directory -Force -Path "Python-3.11.8"
Copy-Item -Recurse -Force -Path $tempDir -Destination "Python-3.11.8"
