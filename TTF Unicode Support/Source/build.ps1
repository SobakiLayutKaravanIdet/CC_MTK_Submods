$ErrorActionPreference = 'Stop'
$sourceDir = $PSScriptRoot
$project = Join-Path $sourceDir 'ttf_unicode_support.vcxproj'
$pf86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
$vswhere = Join-Path $pf86 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if (-not $msbuild) {
    $cmd = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { $msbuild = $cmd.Source }
}
if (-not $msbuild -or -not (Test-Path -LiteralPath $msbuild)) { throw 'MSBuild was not found. Install Visual Studio C++ build tools.' }
& $msbuild $project '/t:Build' '/p:Configuration=Release' '/p:Platform=Win32' '/v:minimal'
if ($LASTEXITCODE -ne 0) { throw "MSBuild failed with exit code $LASTEXITCODE" }
Write-Host 'Build completed. Output is in Source\.build\bin; the Release package was not changed.'
