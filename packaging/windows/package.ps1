# SPDX-License-Identifier: GPL-2.0-or-later

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$OutputDir = "",

    [string]$QtBinDir = ""
)

$ErrorActionPreference = "Stop"

# Run an external tool with its stdin closed and a hard timeout. windeployqt (and
# the helpers it spawns, such as qmlimportscanner) can block forever in a
# non-interactive CI shell while waiting on inherited stdin; closing stdin gives
# them an immediate EOF. The timeout converts any remaining wedge into a fast,
# clearly located failure instead of letting the job hang for hours. Output is
# captured and echoed so the step log shows progress and the failing command.
function Invoke-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $PWD.Path,
        [int]$TimeoutSeconds = 600,
        [string]$Label = ""
    )

    if ([string]::IsNullOrEmpty($Label)) {
        $Label = [System.IO.Path]::GetFileName($FilePath)
    }

    $quoted = $Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }

    Write-Host "==> $Label (timeout ${TimeoutSeconds}s)"

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = ($quoted -join ' ')
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    [void]$proc.Start()
    $proc.StandardInput.Close()

    # Drain both pipes asynchronously so a full pipe buffer cannot deadlock the child.
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()

    if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
        Write-Host "$Label exceeded ${TimeoutSeconds}s; terminating the process tree."
        & taskkill.exe /PID $proc.Id /T /F *> $null
        try { [void]$proc.WaitForExit(5000) } catch { }
        throw "$Label timed out after $TimeoutSeconds seconds."
    }

    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    if (-not [string]::IsNullOrWhiteSpace($stdout)) { Write-Host $stdout.TrimEnd() }
    if (-not [string]::IsNullOrWhiteSpace($stderr)) { Write-Host $stderr.TrimEnd() }
    Write-Host "<== $Label exit $($proc.ExitCode)"

    return $proc.ExitCode
}

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

$guiDeployExit = Invoke-Tool -FilePath $deployTool -Label "windeployqt lumacore.exe" -TimeoutSeconds 900 -Arguments @(
    "--release",
    "--compiler-runtime",
    "--no-translations",
    "--skip-plugin-types", "qmltooling",
    "--verbose", "1",
    "--dir", $stageDir,
    "--qmldir", (Join-Path $sourceRoot "ui\qml"),
    (Join-Path $stageDir "lumacore.exe")
)
if ($guiDeployExit -ne 0) {
    throw "windeployqt failed for lumacore.exe with exit code $guiDeployExit."
}

$daemonDeployExit = Invoke-Tool -FilePath $deployTool -Label "windeployqt lumacore-daemon.exe" -TimeoutSeconds 600 -Arguments @(
    "--release",
    "--compiler-runtime",
    "--no-translations",
    "--no-plugins",
    "--verbose", "1",
    "--dir", $stageDir,
    (Join-Path $stageDir "lumacore-daemon.exe")
)
if ($daemonDeployExit -ne 0) {
    throw "windeployqt failed for lumacore-daemon.exe with exit code $daemonDeployExit."
}

# windeployqt --compiler-runtime can copy MinGW runtime DLLs that do not match the
# compiler CMake actually built with. When Qt ships an older bundled toolchain than the
# standalone MinGW used for the build (e.g. Qt 6.7.3's GCC 11 runtime vs. a GCC 13
# build), the deployed libstdc++ is missing newer symbols and the GUI fails to start
# with STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139). Overwrite the runtime with the build
# compiler's own DLLs so the package matches the binaries.
$cmakeCache = Join-Path $resolvedBuildDir "CMakeCache.txt"
if (Test-Path -LiteralPath $cmakeCache) {
    $compilerMatch = Select-String -LiteralPath $cmakeCache -Pattern '^CMAKE_CXX_COMPILER:[^=]*=(.+)$' |
        Select-Object -First 1
    if ($compilerMatch) {
        $compilerBin = Split-Path -Parent $compilerMatch.Matches[0].Groups[1].Value
        foreach ($runtimeDll in @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")) {
            $runtimeSource = Join-Path $compilerBin $runtimeDll
            if (Test-Path -LiteralPath $runtimeSource) {
                Copy-Item -LiteralPath $runtimeSource -Destination $stageDir -Force
                Write-Host "Synced MinGW runtime $runtimeDll from $compilerBin"
            }
        }
    } else {
        Write-Host "Note: CMAKE_CXX_COMPILER not found in CMakeCache.txt; left deployed runtime as-is."
    }
}

$runtimeSearchDirs = @()
if (-not [string]::IsNullOrWhiteSpace($QtBinDir)) {
    $runtimeSearchDirs += $QtBinDir
}
if (-not [string]::IsNullOrWhiteSpace($compilerBin)) {
    $runtimeSearchDirs += $compilerBin
}
$runtimeSearchDirs += ($env:PATH -split [System.IO.Path]::PathSeparator)
$runtimeSearchDirs = $runtimeSearchDirs |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
    Select-Object -Unique

foreach ($optionalDll in @("hidapi.dll", "libhidapi-0.dll", "libhidapi.dll")) {
    $runtimeSource = $runtimeSearchDirs |
        ForEach-Object { Join-Path $_ $optionalDll } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
    if ($runtimeSource) {
        Copy-Item -LiteralPath $runtimeSource -Destination $stageDir -Force
        Write-Host "Copied optional runtime $optionalDll from $(Split-Path -Parent $runtimeSource)"
    }
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
    $versionExit = Invoke-Tool `
        -FilePath (Join-Path $stageDir $executable) `
        -Arguments @("--version") `
        -WorkingDirectory $stageDir `
        -TimeoutSeconds 60 `
        -Label "$executable --version"
    if ($versionExit -ne 0) {
        throw "$executable --version failed with exit code $versionExit."
    }
}

$archiveTool = Get-Command tar.exe -ErrorAction Stop
$archiveExit = Invoke-Tool `
    -FilePath $archiveTool.Source `
    -Arguments @("-a", "-c", "-f", $zipPath, "-C", $resolvedOutputDir, (Split-Path -Leaf $stageDir)) `
    -TimeoutSeconds 300 `
    -Label "tar (portable ZIP)"
if ($archiveExit -ne 0) {
    throw "tar.exe failed to create the portable ZIP with exit code $archiveExit."
}
Write-Host "Created $zipPath"
