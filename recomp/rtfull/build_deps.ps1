<#
Builds SDL3 3.4.10 (shared DLL, so the runtime exe's own imports stay auditable) and zlib 1.3.1 (static)
offline from the user-supplied, hash-checked source archives. Output: recomp\rtfull\_deps\ (gitignored).
#>
param([string]$Dl = "$PSScriptRoot\..\..\..\..\star-cean-second\tools\dl",
      [string]$Src = "$PSScriptRoot\..\..\..\..\star-cean-second\tools\ref-deps")
$ErrorActionPreference = 'Stop'
$want = @{ 'SDL3-3.4.10.tar.gz' = '12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785'
           'zlib-1.3.1.tar.gz'  = '9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23' }
foreach ($k in $want.Keys) {
    $h = (Get-FileHash "$Dl\$k" -Algorithm SHA256).Hash.ToLower()
    if ($h -ne $want[$k]) { throw "$k sha256 $h != pinned $($want[$k])" }
}
foreach ($d in 'SDL3-3.4.10','zlib-1.3.1') { if (-not (Test-Path "$Src\$d\CMakeLists.txt")) { throw "extract $d into $Src first" } }
$ErrorActionPreference = 'Continue'  # native tools write warnings to stderr; failures are checked via $LASTEXITCODE
$vs = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" >nul 2>nul && set" | ForEach-Object { if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] } }
$O = "$PSScriptRoot\_deps"; New-Item -ItemType Directory -Force $O | Out-Null
# FETCHCONTENT_FULLY_DISCONNECTED: belt and braces, neither project should fetch anything
$common = @('-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_COMPILER=cl','-DCMAKE_CXX_COMPILER=cl',"-DCMAKE_INSTALL_PREFIX=$O\install",'-DFETCHCONTENT_FULLY_DISCONNECTED=ON','-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DCMAKE_POLICY_DEFAULT_CMP0091=NEW','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')
& cmake -S "$Src\zlib-1.3.1" -B "$O\zlib_mt" @common *> "$O\zlib.log"; if ($LASTEXITCODE) { throw "zlib configure" }
& cmake --build "$O\zlib_mt" --target install *>> "$O\zlib.log"; if ($LASTEXITCODE) { throw "zlib build" }
& cmake -S "$Src\SDL3-3.4.10" -B "$O\sdl_mt" @common -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF *> "$O\sdl.log"; if ($LASTEXITCODE) { throw "SDL configure" }
& cmake --build "$O\sdl_mt" --target install *>> "$O\sdl.log"; if ($LASTEXITCODE) { throw "SDL build" }
"deps OK: $O\install"
