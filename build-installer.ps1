#!/usr/bin/env powershell
# Build XtreemRgbToggle installer
# Requires: Inno Setup 6.2.0+, .NET 8 SDK

param(
    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Stop"

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "XtreemRgbToggle Installer Build Script" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Check prerequisites
Write-Host "Checking prerequisites..." -ForegroundColor Yellow

# Check .NET SDK
try {
    $dotnetVersion = & dotnet --version 2>&1
    Write-Host "[OK] .NET SDK: $dotnetVersion" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] .NET SDK not found. Install from https://dotnet.microsoft.com/download/dotnet/8.0" -ForegroundColor Red
    exit 1
}

# Check Inno Setup
$ISCCPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $ISCCPath)) {
    Write-Host "[ERROR] Inno Setup not found at $ISCCPath" -ForegroundColor Red
    Write-Host "        Install from https://jrsoftware.org/isdl.php" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Inno Setup found" -ForegroundColor Green

Write-Host ""

# Build application
if (-not $SkipBuild) {
    Write-Host "Step 1/4: Building application..." -ForegroundColor Yellow
    
    # Clean previous build
    Remove-Item -Path "build-output" -Recurse -Force -ErrorAction SilentlyContinue
    
    # Build
    Set-Location XtreemRgbToggle
    & dotnet publish XtreemRgbToggle.csproj -c Release -r win-x64 --self-contained false -o ..\build-output
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] Application build failed" -ForegroundColor Red
        Set-Location ..
        exit 1
    }
    Set-Location ..
    
    Write-Host "[OK] Application built" -ForegroundColor Green
} else {
    Write-Host "Step 1/4: Skipping application build (--SkipBuild)" -ForegroundColor Gray
}

Write-Host ""

# Check dependencies
Write-Host "Step 2/4: Checking dependencies..." -ForegroundColor Yellow

$missingDeps = @()

if (-not (Test-Path "build-output\PawnIOLib.dll")) {
    $missingDeps += "PawnIOLib.dll"
}

if (-not (Test-Path "build-output\SmbusPIIX4.bin")) {
    $missingDeps += "SmbusPIIX4.bin"
}

if ($missingDeps.Count -gt 0) {
    Write-Host "[WARNING] Missing dependencies:" -ForegroundColor Yellow
    foreach ($dep in $missingDeps) {
        Write-Host "  - $dep" -ForegroundColor Yellow
    }
    Write-Host ""
    Write-Host "Instructions:" -ForegroundColor Cyan
    Write-Host "  1. Install PawnIO from https://pawnio.eu" -ForegroundColor Cyan
    Write-Host "  2. Copy PawnIOLib.dll from PawnIO installation to build-output\" -ForegroundColor Cyan
    Write-Host "  3. Download SmbusPIIX4.bin from https://github.com/namazso/PawnIO.Modules" -ForegroundColor Cyan
    Write-Host "  4. Copy SmbusPIIX4.bin to build-output\" -ForegroundColor Cyan
    Write-Host ""
    
    $continue = Read-Host "Continue anyway? (installer will be incomplete) [y/N]"
    if ($continue -ne "y" -and $continue -ne "Y") {
        Write-Host "Build cancelled." -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "[OK] All dependencies present" -ForegroundColor Green
}

Write-Host ""

# Check license
Write-Host "Step 3/4: Checking license file..." -ForegroundColor Yellow

if (-not (Test-Path "LICENSE.txt")) {
    Write-Host "[WARNING] LICENSE.txt missing. Creating placeholder..." -ForegroundColor Yellow
    @"
XtreemRgbToggle License
========================

MIT License (or your chosen license)

Copyright (c) 2024 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Third-Party Components
----------------------

PawnIO (https://pawnio.eu)
- License: See PawnIO website and installation
- Used for SMBus access via signed kernel driver

PawnIO.Modules - SmbusPIIX4.bin
- Source: https://github.com/namazso/PawnIO.Modules
- License: See repository (GPL-3.0 or other - verify before distribution)

.NET 8 Desktop Runtime (downloaded during installation)
- Source: Official Microsoft CDN
- License: Microsoft Software License
- URL: https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe
"@ | Out-File -FilePath "LICENSE.txt" -Encoding UTF8
    
    Write-Host "[OK] Created placeholder LICENSE.txt" -ForegroundColor Green
    Write-Host "     IMPORTANT: Review and update before distribution!" -ForegroundColor Yellow
} else {
    Write-Host "[OK] LICENSE.txt found" -ForegroundColor Green
}

Write-Host ""

# Build installer
Write-Host "Step 4/4: Building installer..." -ForegroundColor Yellow

# Clean previous installer output
Remove-Item -Path "installer-output" -Recurse -Force -ErrorAction SilentlyContinue

# Compile installer
& $ISCCPath "installer\XtreemRgbToggle-Setup.iss"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Installer build failed" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] Installer built" -ForegroundColor Green

Write-Host ""
Write-Host "=====================================" -ForegroundColor Green
Write-Host "          BUILD SUCCESSFUL           " -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green
Write-Host ""
Write-Host "Output: installer-output\XtreemRgbToggle-Setup-1.1.0.exe" -ForegroundColor Cyan
Write-Host ""

# Show file info
$installerPath = "installer-output\XtreemRgbToggle-Setup-1.1.0.exe"
if (Test-Path $installerPath) {
    $fileInfo = Get-Item $installerPath
    $fileSizeMB = [math]::Round($fileInfo.Length / 1MB, 2)
    Write-Host "Size: $fileSizeMB MB" -ForegroundColor Cyan
    
    # Calculate SHA256
    Write-Host "Calculating SHA256..." -ForegroundColor Gray
    $hash = Get-FileHash -Path $installerPath -Algorithm SHA256
    Write-Host "SHA256: $($hash.Hash)" -ForegroundColor Cyan
    Write-Host ""
}

Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Test installer on a clean Windows VM" -ForegroundColor White
Write-Host "  2. Verify dependency downloads work" -ForegroundColor White
Write-Host "  3. Test uninstaller and dependency removal options" -ForegroundColor White
Write-Host "  4. Review LICENSE.txt compliance" -ForegroundColor White
Write-Host "  5. Optionally sign the installer" -ForegroundColor White
Write-Host ""
