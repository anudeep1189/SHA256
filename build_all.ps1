# build_all.ps1
# Automates the build process for Legacy (CUDA 12.6), Modern (CUDA 13.3) and Launcher versions.

$ErrorActionPreference = "Stop"

# 1. Locate MSBuild.exe
$msbuildPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2026\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
    "C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"
)

$msbuild = $null
foreach ($path in $msbuildPaths) {
    if (Test-Path $path) {
        $msbuild = $path
        break
    }
}

if ($null -eq $msbuild) {
    # Try finding via registry/default paths
    $msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
    if (-not (Test-Path $msbuild)) {
        Write-Error "Could not locate MSBuild.exe. Please ensure Visual Studio is installed."
    }
}

Write-Host "Using MSBuild: $msbuild" -ForegroundColor Cyan

# 2. Check for CUDA installations
$cuda12Path = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6"
$cuda13Path = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.3"

if (-not (Test-Path $cuda12Path)) {
    Write-Warning "CUDA 12.6 not found at '$cuda12Path'. Compilation of Legacy target might fail if not installed elsewhere."
}
if (-not (Test-Path $cuda13Path)) {
    Write-Warning "CUDA 13.3 not found at '$cuda13Path'. Compilation of Modern target might fail if not installed elsewhere."
}

# 3. Build Legacy Version (CUDA 12.6)
Write-Host "`n--- Building SHA256 Legacy Version (CUDA 12.6) ---" -ForegroundColor Yellow
$legacyArgs = @(
    "SHA256.vcxproj",
    "/p:Configuration=Release",
    "/p:Platform=x64",
    "/p:CudaVersion=12.6",
    "/p:CudaToolkitCustomDir=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9",
    "/p:VCToolsVersion=14.44.35207",
    "/t:Rebuild",
    "/v:minimal"
)
& $msbuild $legacyArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to build Legacy version."
}

# 4. Build Modern Version (CUDA 13.3)
Write-Host "`n--- Building SHA256 Modern Version (CUDA 13.3) ---" -ForegroundColor Yellow
$modernArgs = @(
    "SHA256.vcxproj",
    "/p:Configuration=Release",
    "/p:Platform=x64",
    "/p:CudaVersion=13.3",
    "/t:Rebuild",
    "/v:minimal"
)
& $msbuild $modernArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to build Modern version."
}

# 5. Build Launcher
Write-Host "`n--- Building GPU-Detecting Launcher ---" -ForegroundColor Yellow
$launcherArgs = @(
    "Launcher.vcxproj",
    "/p:Configuration=Release",
    "/p:Platform=x64",
    "/t:Rebuild",
    "/v:minimal"
)
& $msbuild $launcherArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to build Launcher."
}

# 6. Verify and Package Release
Write-Host "`n--- Packaging Release ---" -ForegroundColor Yellow

$legacyExe = "x64\Release_12.6\SHA256.exe"
$modernExe = "x64\Release_13.3\SHA256.exe"
$launcherExe = "x64\Release_Launcher\Launcher.exe"

if (-not (Test-Path $legacyExe)) {
    Write-Error "Missing legacy build output: $legacyExe"
}
if (-not (Test-Path $modernExe)) {
    Write-Error "Missing modern build output: $modernExe"
}
if (-not (Test-Path $launcherExe)) {
    Write-Error "Missing launcher build output: $launcherExe"
}

# Ensure Release_Package exists
$releaseFolder = "Release_Package"
if (-not (Test-Path $releaseFolder)) {
    New-Item -ItemType Directory -Path $releaseFolder | Out-Null
}

Copy-Item $legacyExe -Destination "$releaseFolder\SHA256_Legacy.exe" -Force
Copy-Item $modernExe -Destination "$releaseFolder\SHA256_Modern.exe" -Force
Copy-Item $launcherExe -Destination "$releaseFolder\SHA256.exe" -Force

Write-Host "`nSuccessfully packaged release files in $releaseFolder\:" -ForegroundColor Green
Write-Host "  - SHA256_Legacy.exe (CUDA 12.6, CC 5.0 to 9.0)" -ForegroundColor Green
Write-Host "  - SHA256_Modern.exe (CUDA 13.3, CC 7.5 to 12.1)" -ForegroundColor Green
Write-Host "  - SHA256.exe        (GPU detecting Launcher)" -ForegroundColor Green
