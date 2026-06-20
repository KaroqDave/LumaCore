[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$OutputDir = "",

    [string]$QtBinDir = ""
)

$ErrorActionPreference = "Stop"

$sourceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$resolvedBuildDir = (Resolve-Path $BuildDir).Path
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $sourceRoot "dist"
}
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$stageDir = Join-Path $resolvedOutputDir "LumaCore-Windows-x64"
$zipPath = Join-Path $resolvedOutputDir "LumaCore-Windows-x64.zip"

$guiPath = Join-Path $resolvedBuildDir "lumacore.exe"
$daemonPath = Join-Path $resolvedBuildDir "lumacore-daemon.exe"
if (!(Test-Path -LiteralPath $guiPath) -or !(Test-Path -LiteralPath $daemonPath)) {
    throw "Expected lumacore.exe and lumacore-daemon.exe in '$resolvedBuildDir'."
}

if ([string]::IsNullOrWhiteSpace($QtBinDir)) {
    if ($env:QT_ROOT_DIR) {
        $QtBinDir = Join-Path $env:QT_ROOT_DIR "bin"
    } else {
        $deployCommand = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
        if ($deployCommand) {
            $QtBinDir = Split-Path -Parent $deployCommand.Source
        }
    }
}

$deployTool = Join-Path $QtBinDir "windeployqt.exe"
if (!(Test-Path -LiteralPath $deployTool)) {
    throw "windeployqt.exe was not found. Pass -QtBinDir or set QT_ROOT_DIR."
}

New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null
if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
New-Item -ItemType Directory -Path $stageDir | Out-Null

Copy-Item -LiteralPath $guiPath, $daemonPath -Destination $stageDir
Copy-Item -LiteralPath (Join-Path $sourceRoot "LICENSE") -Destination $stageDir
Copy-Item -LiteralPath (Join-Path $sourceRoot "docs\windows-preview.md") -Destination (Join-Path $stageDir "README-Windows.md")

& $deployTool `
    --release `
    --compiler-runtime `
    --no-translations `
    --skip-plugin-types qmltooling `
    --verbose 0 `
    --dir $stageDir `
    --qmldir (Join-Path $sourceRoot "ui\qml") `
    (Join-Path $stageDir "lumacore.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed for lumacore.exe with exit code $LASTEXITCODE."
}

& $deployTool `
    --release `
    --compiler-runtime `
    --no-translations `
    --no-plugins `
    --verbose 0 `
    --dir $stageDir `
    (Join-Path $stageDir "lumacore-daemon.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed for lumacore-daemon.exe with exit code $LASTEXITCODE."
}

foreach ($requiredPath in @(
    "lumacore.exe",
    "lumacore-daemon.exe",
    "platforms\qwindows.dll",
    "qml\QtQuick"
)) {
    if (!(Test-Path -LiteralPath (Join-Path $stageDir $requiredPath))) {
        throw "Portable package is missing '$requiredPath'."
    }
}

foreach ($executable in @("lumacore.exe", "lumacore-daemon.exe")) {
    $process = Start-Process `
        -FilePath (Join-Path $stageDir $executable) `
        -ArgumentList "--version" `
        -WorkingDirectory $stageDir `
        -Wait `
        -PassThru `
        -WindowStyle Hidden
    if ($process.ExitCode -ne 0) {
        throw "$executable --version failed with exit code $($process.ExitCode)."
    }
}

$archiveTool = Get-Command tar.exe -ErrorAction Stop
& $archiveTool.Source -a -c -f $zipPath -C $resolvedOutputDir (Split-Path -Leaf $stageDir)
if ($LASTEXITCODE -ne 0) {
    throw "tar.exe failed to create the portable ZIP with exit code $LASTEXITCODE."
}
Write-Host "Created $zipPath"
