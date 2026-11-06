# Building the XtreemRgbToggle Installer

## Prerequisites (Windows Only)

1. **Inno Setup 6.2.0 or later**
   - Download: https://jrsoftware.org/isdl.php
   - Install with default options
   - Adds ISCC.exe (Inno Setup Compiler) to PATH

2. **.NET 8 SDK** (for building the application)
   - Download: https://dotnet.microsoft.com/download/dotnet/8.0
   - Install "Desktop development" workload

3. **PawnIO Dependencies** (for bundling)
   - Download PawnIOLib.dll from PawnIO installation
   - Download SmbusPIIX4.bin from https://github.com/namazso/PawnIO.Modules

## Build Process

### Step 1: Build the Application

```powershell
# From repository root
cd XtreemRgbToggle
dotnet publish XtreemRgbToggle.csproj -c Release -r win-x64 --self-contained false -o ..\build-output
```

This creates framework-dependent binaries in `build-output/`:
- XtreemRgbToggle.exe
- XtreemRgbToggle.dll
- XtreemRgbToggle.runtimeconfig.json

### Step 2: Copy Dependencies

```powershell
# Copy PawnIO files to build output
copy <path-to-PawnIO>\PawnIOLib.dll ..\build-output\
copy <path-to-SmbusPIIX4>\SmbusPIIX4.bin ..\build-output\
```

### Step 3: Prepare License File

```powershell
# Create LICENSE.txt in repository root
# Include project license and dependency licenses
```

### Step 4: Build the Installer

```powershell
# From repository root
cd installer
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" XtreemRgbToggle-Setup.iss
```

Output: `installer-output\XtreemRgbToggle-Setup-1.1.0.exe`

## Build Script (Automated)

Create `build-installer.ps1`:

```powershell
#!/usr/bin/env powershell
# Build XtreemRgbToggle installer

$ErrorActionPreference = "Stop"

Write-Host "Building XtreemRgbToggle..."

# Clean previous build
Remove-Item -Path "build-output" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "installer-output" -Recurse -Force -ErrorAction SilentlyContinue

# Build application
Write-Host "Step 1: Building application..."
Set-Location XtreemRgbToggle
dotnet publish XtreemRgbToggle.csproj -c Release -r win-x64 --self-contained false -o ..\build-output
if ($LASTEXITCODE -ne 0) { throw "Application build failed" }
Set-Location ..

# Check dependencies
Write-Host "Step 2: Checking dependencies..."
if (-not (Test-Path "build-output\PawnIOLib.dll")) {
    throw "PawnIOLib.dll missing. Copy from PawnIO installation to build-output\"
}
if (-not (Test-Path "build-output\SmbusPIIX4.bin")) {
    throw "SmbusPIIX4.bin missing. Download from PawnIO.Modules to build-output\"
}

# Check license
if (-not (Test-Path "LICENSE.txt")) {
    Write-Warning "LICENSE.txt missing. Creating placeholder..."
    @"
XtreemRgbToggle License
========================

[Add your license here]

Includes:
- PawnIO (https://pawnio.eu) - See PawnIO license
- SmbusPIIX4.bin from PawnIO.Modules - See PawnIO.Modules license
- .NET 8 Desktop Runtime (downloaded during install) - Microsoft Software License
"@ | Out-File -FilePath "LICENSE.txt" -Encoding UTF8
}

# Build installer
Write-Host "Step 3: Building installer..."
$ISCCPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $ISCCPath)) {
    throw "Inno Setup not found at $ISCCPath. Install from https://jrsoftware.org/isdl.php"
}

& $ISCCPath "installer\XtreemRgbToggle-Setup.iss"
if ($LASTEXITCODE -ne 0) { throw "Installer build failed" }

Write-Host ""
Write-Host "SUCCESS!" -ForegroundColor Green
Write-Host "Installer: installer-output\XtreemRgbToggle-Setup-1.1.0.exe"
Write-Host ""
Write-Host "Test the installer on a clean VM before distribution."
```

## Installer Features

### Installation Process

1. **UAC Elevation**: Normal Windows elevation prompt (PrivilegesRequired=admin)
2. **Dependency Detection**: Checks for .NET 8 Desktop Runtime and PawnIO
3. **Bootstrap Downloads**: Downloads missing dependencies from official sources
4. **Dependency Tracking**: Records which dependencies were installed (vs preexisting)
5. **Shortcuts**: Creates desktop and start menu shortcuts
6. **Registry Entries**: Adds uninstall entry to Windows Apps

### Dependency Handling

**\.NET 8 Desktop Runtime**:
- Detection: Checks registry and `dotnet --list-runtimes`
- Download: https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe (~55 MB)
- Source: Official Microsoft CDN
- License: Microsoft Software License
- Installation: Silent install with `/install /quiet /norestart`

**PawnIO Driver**:
- Detection: Checks for PawnIOLib.dll in System32 or service registry
- Download: https://pawnio.eu/PawnIO_setup.exe (if available)
- Source: Official PawnIO website
- License: See PawnIO license
- Installation: Silent install with `/VERYSILENT`
- Fallback: Prompts for manual installation if download unavailable

### Uninstallation Process

1. **Application Removal**: Removes all installed files
2. **Scheduled Task**: Automatically removes XtreemRgbToggle-Off task
3. **Dependency Options**: Shows UNCHECKED boxes for removing dependencies WE installed
4. **Safety Warnings**: Warns that other apps may use shared dependencies
5. **Vendor Uninstallers**: Uses official uninstallers (not manual deletion)

**Dependency Removal Rules**:
- Only offer to remove dependencies THIS installer installed
- Never offer to remove preexisting dependencies
- Default: UNCHECKED (keep shared components)
- .NET removal: Redirects to Windows "Add or Remove Programs" (safety)
- PawnIO removal: Uses PawnIO's official uninstaller if present

### Registry Tracking

Stored in `HKLM\Software\XtreemRgbToggle`:
- `InstallPath`: Installation directory
- `Version`: Installed version
- `InstalledDotNet`: 0 = preexisting, 1 = we installed it
- `InstalledPawnIO`: 0 = preexisting, 1 = we installed it

## Testing the Installer

### Test Environment

Use a **clean Windows 10/11 VM** without:
- .NET 8 Desktop Runtime
- PawnIO driver
- Previous XtreemRgbToggle installation

### Test Procedure

1. **Run Setup.exe**
   - Verify UAC prompt appears
   - Verify dependency detection messages
   - Verify download prompts with URLs shown
   - Verify installation completes

2. **Test Application**
   - Double-click desktop shortcut
   - Verify GUI opens
   - Test OFF/ON/Color picker (if hardware available)
   - Test CLI: `XtreemRgbToggle.exe --help`

3. **Test Uninstaller**
   - Open Windows Settings → Apps
   - Find "XtreemRgbToggle RGB Control"
   - Click Uninstall
   - Verify dependency removal options appear
   - Verify boxes are UNCHECKED by default
   - Complete uninstall
   - Verify application removed
   - Verify scheduled task removed
   - Verify dependencies kept (if unchecked)

4. **Test Dependency Tracking**
   - Install on system with preexisting .NET 8
   - Verify installer detects it
   - Verify uninstaller does NOT offer to remove it

## Limitations

### Cannot Detect Shared Usage Safely

The installer **cannot reliably detect if other applications use .NET or PawnIO**.

**Workaround implemented**:
- Only offer removal for dependencies WE installed
- Default: Keep all shared components (unchecked)
- .NET removal: Redirect to Windows control panel (safest)
- PawnIO removal: Use vendor uninstaller with warning

**Why this is safe**:
1. We never remove preexisting components
2. We warn about shared usage before removal
3. We use vendor uninstallers (not manual deletion)
4. Default behavior keeps all dependencies

### Alternative Approaches NOT Used

**Rejected: Registry scanning** - Unreliable, false negatives
**Rejected: File locking checks** - Doesn't detect inactive dependencies
**Rejected: Application inventory** - Requires elevated scanning, still incomplete
**Rejected: Aggressive removal** - Dangerous, may break other apps

## Distribution

### Before Release

1. **Test on clean VMs** (Windows 10 and 11)
2. **Test upgrade scenario** (install v1.0, upgrade to v1.1)
3. **Test uninstall completeness** (verify nothing left behind)
4. **Document system requirements** in README
5. **Sign the installer** (optional but recommended)

### Recommended Distribution

- **GitHub Releases**: Attach Setup.exe as binary asset
- **Checksum**: Provide SHA256 hash for verification
- **Size**: ~2-5 MB (bootstrap), or ~60 MB (bundled with dependencies)
- **Filename**: `XtreemRgbToggle-Setup-1.1.0.exe`

## Troubleshooting

### Build Errors

**"ISCC.exe not found"**
- Install Inno Setup from https://jrsoftware.org/isdl.php
- Ensure it's in PATH or use full path in build script

**"PawnIOLib.dll missing"**
- Copy from existing PawnIO installation
- Or install PawnIO first, then copy from `C:\Program Files\PawnIO\`

**"SmbusPIIX4.bin missing"**
- Download from https://github.com/namazso/PawnIO.Modules/releases
- Place in `build-output\` directory

### Runtime Errors

**"Application failed to start"**
- User needs .NET 8 Desktop Runtime (installer handles this)
- Verify PawnIOLib.dll is beside the exe

**"PawnIO driver not loaded"**
- PawnIO requires Administrator rights
- Service may not be started: Run `sc start PawnIO`

## License Compliance

### Bundled Components

**XtreemRgbToggle**:
- License: [Your chosen license]
- Source: [Your repository]

**PawnIOLib.dll**:
- License: See PawnIO website
- Source: https://pawnio.eu
- Distribution: Check PawnIO license terms before bundling

**SmbusPIIX4.bin**:
- License: See PawnIO.Modules repository
- Source: https://github.com/namazso/PawnIO.Modules
- Distribution: Check repository license

**\.NET 8 Desktop Runtime** (downloaded):
- License: Microsoft Software License
- Source: Official Microsoft CDN
- Distribution: User downloads during install (not bundled)

### Installer Script License

The Inno Setup script itself (`.iss` file) can be distributed under your project license.

## Advanced Options

### Silent Installation

```batch
XtreemRgbToggle-Setup-1.1.0.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR="C:\CustomPath"
```

### Bundled Installer (No Downloads)

To create a larger installer with dependencies bundled:

1. Download .NET 8 Desktop Runtime installer manually
2. Download PawnIO installer manually
3. Modify `.iss` file to include them in `[Files]` section
4. Update `[Code]` section to use bundled files instead of downloading

Result: ~60 MB installer, no internet required during install.

### Enterprise Deployment

For corporate environments, consider:
- MSI wrapper (Advanced Installer, WiX)
- Group Policy deployment
- SCCM/Intune packages
- Pre-install dependencies via separate package
