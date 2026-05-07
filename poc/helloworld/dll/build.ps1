<#
.SYNOPSIS
    poc/helloworld/dll/build.ps1
    Build script for Windows DLL and Test Native Loader.
#>

param (
    [ValidateSet("debug", "release")]
    [string]$Mode = "release"
)

$ErrorActionPreference = "Stop"
$SrcDir = $PSScriptRoot

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "[*] Building Windows PoC DLL & Test EXE" -ForegroundColor Cyan
Write-Host "========================================="

# Locate MSVC for SCons
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not [string]::IsNullOrEmpty($vsPath)) {
        $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars) {
            Write-Host "[+] Found MSVC Env: $vcvars" -ForegroundColor Green
            Push-Location $SrcDir
            
            # Execute vcvars64 and scons in the same cmd instance
            $cmd = "`"$vcvars`" && scons mode=$Mode"
            cmd.exe /c $cmd
            
            if ($LASTEXITCODE -ne 0) {
                Pop-Location
                Write-Error "[-] SCons build failed with exit code $LASTEXITCODE"
                exit $LASTEXITCODE
            }
            Pop-Location
        } else { Write-Warning "[-] vcvars64.bat not found." }
    } else { Write-Warning "[-] MSVC Compiler not found." }
} else { Write-Warning "[-] vswhere.exe not found." }

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "[+] Tasks finished! Outputs are in build/bin/" -ForegroundColor Green