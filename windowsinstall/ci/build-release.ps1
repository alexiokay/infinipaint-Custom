$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'This provisioning script is for disposable GitHub Actions runners only.' }
Set-Location (Resolve-Path "$PSScriptRoot/../..")
if (Test-Path build-arm64) { throw 'Refusing to reuse an application build directory.' }
New-Item -ItemType Directory ci-logs, release-output, ci-package, ci-symbols | Out-Null
Start-Transcript -Path ci-logs/build.txt
try {
    function Invoke-Checked([string]$Program, [string[]]$Arguments) {
        & $Program @Arguments
        if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
    }
    Invoke-Checked conan @('profile','detect','--force')
    $recipes = @(
        @('conan/conan-skia/recipes/skia/all/conanfile.py','143.20251028.0'),
        @('conan/emsdk/all/conanfile.py','4.0.22'),
        @('conan/icu/all/conanfile.py','77.1'),
        @('conan/sdl/3.x/conanfile.py','3.4.16')
    )
    foreach ($recipe in $recipes) {
        Invoke-Checked conan @('export',$recipe[0],"--version=$($recipe[1])")
    }
    Invoke-Checked conan @('install','.', '-of=build-arm64','--build=missing',
        '-pr:h=conan/profiles/win-arm64','-pr:b=default','-s:b','compiler.cppstd=23',
        '-c','tools.build:jobs=3',
        '--deployer=runtime_deploy','--deployer-folder=ci-runtime',
        '--lockfile-out=ci-logs/conan.lock')
    Invoke-Checked cmd.exe @('/d','/c','windowsinstall\ci\build-app.cmd')
    $exe = 'build-arm64/build/Release/infinipaint.exe'
    $pdb = 'build-arm64/build/Release/infinipaint.pdb'
    Invoke-Checked python @('windowsinstall/ci/verify-symbols.py',$exe,$pdb)
    Copy-Item $exe ci-package
    Copy-Item $pdb ci-symbols
    Copy-Item assets/data ci-package/data -Recurse
    Copy-Item COPYING ci-package
    Invoke-Checked conan @('cache','clean','*')
    $runtimeDlls = @(Get-ChildItem ci-runtime -Recurse -Filter '*.dll')
    if ($runtimeDlls.Count -eq 0) { throw 'Conan did not deploy runtime DLLs.' }
    foreach ($dll in $runtimeDlls) {
        $destination = Join-Path ci-package $dll.Name
        if ((Test-Path $destination) -and
            (Get-FileHash $destination).Hash -ne (Get-FileHash $dll.FullName).Hash) {
            throw "Conflicting runtime DLL: $($dll.Name)"
        }
        Copy-Item $dll.FullName $destination
    }
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vsRoot) { throw 'Visual Studio installation not found.' }
    $crt = Get-ChildItem "$vsRoot/VC/Redist/MSVC" -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'arm64/Microsoft.VC143.CRT' } |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if (!$crt) { throw 'ARM64 app-local MSVC runtime not found.' }
    Copy-Item "$crt/*.dll" ci-package
    Remove-Item (Join-Path ci-package 'vcruntime140_1.dll') -Force -ErrorAction SilentlyContinue
    foreach ($dll in Get-ChildItem ci-package -Filter '*.dll') {
        Invoke-Checked python @('windowsinstall/ci/verify-symbols.py',$dll.FullName)
    }
    $requiredDlls = @('infinipaint.exe', 'hwloc.dll', 'vcruntime140.dll', 'msvcp140.dll')
    foreach ($req in $requiredDlls) {
        if (!(Test-Path (Join-Path ci-package $req))) { throw "Missing required package file: $req" }
    }
    $sha = (& git rev-parse HEAD).Trim()
    $pins = & git submodule status
    $metadata = @"
Source: $sha
Branch: $env:GITHUB_REF_NAME
Build: https://github.com/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID
Platform: Windows ARM64; Release; portable; Vulkan / Skia Ganesh
Application objects: fresh GitHub runner; no application build cache restored
Symbols: matching PDB verified against executable CodeView identity
Submodules:
$pins
"@
    $metadata | Set-Content ci-package/SOURCE_COMMIT.txt
    $metadata | Set-Content ci-symbols/SOURCE_COMMIT.txt
    @"
Extract this entire ZIP to a new writable folder and run infinipaint.exe.
Configuration stays in its local config folder. Do not mix files with an older package.
Includes graphite UI, movable inspector, pressure modes and optional pen correction.
Vulkan support must be supplied by the device's graphics driver.
No driver, registry, or system configuration change is performed by this package.
Unsigned experimental build: Windows may display a reputation warning.
CI cross-compiles ARM64 on x64. Successful build is NOT a Surface startup/pen test.
"@ | Set-Content ci-package/PORTABLE_README.txt
    $version = $sha.Substring(0,8)
    Compress-Archive -Path ci-package/* -DestinationPath "release-output/infinipaint-$version-win-arm64-portable.zip"
    Compress-Archive -Path ci-symbols/* -DestinationPath "release-output/infinipaint-$version-win-arm64-symbols.zip"
    Get-ChildItem release-output -Filter '*.zip' | ForEach-Object {
        "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower())  $($_.Name)"
    } | Set-Content release-output/SHA256SUMS.txt
    @"
Clean Windows ARM64 portable build of the graphite-ui fork, source **$sha**.

Includes graphite UI/movable inspector and the current pen/pressure features, plus
the CMake integration repair and startup-hardening change from the PR branch.
Uses the creator's ARM64 Conan profile, Release, Vulkan/Skia Ganesh, portable ON.
All application objects are compiled on a fresh GitHub runner.

Download the **portable.zip** to run the app. The separate **symbols.zip** is for diagnosis.
The ZIP includes runtime DLLs, assets, licenses and SOURCE_COMMIT.txt.
SHA256SUMS.txt verifies downloads. No raw pen recordings are included.

Automated core/replay checks and the cross-build must pass before publication.
This is an unsigned **prerelease**: ARM64 GUI startup and pen interaction have not
been tested by this x64 build job. Extract into a NEW folder; do not overwrite an
existing package or mix its DLLs. Your existing drawings/configuration are not changed.

Corresponding application source: https://github.com/$env:GITHUB_REPOSITORY/tree/$sha
Clone that revision and initialize its pinned submodules to rebuild; automatic
GitHub source ZIPs do not contain submodule contents.
Build logs: https://github.com/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID
"@ | Set-Content release-output/RELEASE_NOTES.md
} finally {
    Stop-Transcript
}
