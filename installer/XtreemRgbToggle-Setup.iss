; XtreemRgbToggle Installer Script (Inno Setup 6.x)
; Requires: Inno Setup 6.2.0+ with InnoSetup Download Plugin (IDP)
;
; Features:
; - Bootstrap installer (downloads .NET 8 Desktop Runtime + PawnIO if missing)
; - UAC elevation (normal Windows elevation, not bypassed)
; - Desktop shortcut for GUI
; - Windows Apps uninstall entry
; - Tracks which dependencies were installed by us
; - Optional dependency removal at uninstall (unchecked by default)
; - Uses vendor uninstallers only (not manual deletion)

#define MyAppName "XtreemRgbToggle"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "OpenSource"
#define MyAppExeName "XtreemRgbToggle.exe"
#define MyAppURL "https://github.com/yourusername/XtreemRgbToggle"

[Setup]
AppId={{A5B6C7D8-E9F0-4A5B-8C9D-0E1F2A3B4C5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE.txt
OutputDir=..\installer-output
OutputBaseFilename=XtreemRgbToggle-Setup-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} RGB Control

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
; Main application
Source: "..\build-output\XtreemRgbToggle.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build-output\XtreemRgbToggle.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build-output\XtreemRgbToggle.runtimeconfig.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build-output\PawnIOLib.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build-output\SmbusPIIX4.bin"; DestDir: "{app}"; Flags: ignoreversion

; Documentation
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion

; Dependency tracking file (records what we installed)
Source: "..\installer\dependency-tracker.ini"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "XTREEM RGB Control"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; Comment: "XTREEM RGB Control"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Registry]
; Track application install
Root: HKLM; Subkey: "Software\{#MyAppName}"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\{#MyAppName}"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"
; Dependency tracking is updated in [Code] section AFTER successful installation

[Code]
var
  DependencyDownloadPage: TDownloadWizardPage;
  DotNetInstallerPath: String;
  PawnIOInstallerPath: String;
  NeedDotNet: Boolean;
  NeedPawnIO: Boolean;
  DotNetWasInstalled: Boolean;
  PawnIOWasInstalled: Boolean;
  
const
  // Official Microsoft .NET 8 Desktop Runtime x64 download (VERIFIED URL)
  DotNetDownloadURL = 'https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe';
  
  // PawnIO download URL (NEEDS VERIFICATION - may not be direct download link)
  // Check https://pawnio.eu for actual download location
  // If direct download unavailable, installer will prompt for manual install
  PawnIODownloadURL = 'https://pawnio.eu/download/PawnIO_setup.exe'; // TODO: Verify this URL

function DotNetAlreadyInstalled: Boolean;
var
  Output: AnsiString;
  ResultCode: Integer;
  TmpFile: String;
begin
  // Check if .NET 8 Desktop Runtime is installed
  // Registry check for Microsoft.WindowsDesktop.App 8.0.x
  if RegKeyExists(HKLM, 'SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedfx\Microsoft.WindowsDesktop.App') then
  begin
    // Check for version 8.0 or higher under this key
    // Note: This is simplified; production should enumerate subkeys
    Result := RegKeyExists(HKLM, 'SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedfx\Microsoft.WindowsDesktop.App\8.0');
  end
  else
    Result := False;
  
  // Alternative: Check via dotnet --list-runtimes and parse output for "Microsoft.WindowsDesktop.App 8."
  if not Result and FileExists(ExpandConstant('{pf}\dotnet\dotnet.exe')) then
  begin
    TmpFile := ExpandConstant('{tmp}\dotnet-runtimes.txt');
    Exec(ExpandConstant('{pf}\dotnet\dotnet.exe'), '--list-runtimes', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // NOTE: Full implementation would capture output and parse for "Microsoft.WindowsDesktop.App 8."
    // This is a simplified check - exit 0 only means dotnet.exe works
    // For production, use ExecWithOutput or similar to parse actual runtime list
    Result := False; // Conservative: assume not present unless registry confirms
  end;
end;

function PawnIOAlreadyInstalled: Boolean;
begin
  // Check if PawnIO driver is installed
  // PawnIO installs PawnIOLib.dll in System32 and creates a service
  Result := FileExists(ExpandConstant('{sys}\PawnIOLib.dll')) or 
            RegKeyExists(HKLM, 'SYSTEM\CurrentControlSet\Services\PawnIO');
end;

procedure InitializeWizard;
begin
  NeedDotNet := not DotNetAlreadyInstalled;
  NeedPawnIO := not PawnIOAlreadyInstalled;
  DotNetWasInstalled := False;
  PawnIOWasInstalled := False;
  
  if NeedDotNet or NeedPawnIO then
  begin
    DependencyDownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), 
      'Downloading Required Dependencies', nil);
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ResultCode: Integer;
  ErrorMsg: String;
begin
  Result := True;
  
  if (CurPageID = wpReady) and (NeedDotNet or NeedPawnIO) then
  begin
    DependencyDownloadPage.Clear;
    
    // Add .NET 8 Desktop Runtime download
    if NeedDotNet then
    begin
      DotNetInstallerPath := ExpandConstant('{tmp}\windowsdesktop-runtime-8.0.exe');
      DependencyDownloadPage.Add(DotNetDownloadURL, 'windowsdesktop-runtime-8.0.exe', '');
      MsgBox('This installer will download .NET 8 Desktop Runtime (approximately 55 MB) from Microsoft.' + #13#10 + 
             'Source: ' + DotNetDownloadURL + #13#10#13#10 +
             'License: Microsoft Software License', mbInformation, MB_OK);
    end;
    
    // Add PawnIO download (or prompt for manual installation)
    if NeedPawnIO then
    begin
      if MsgBox('PawnIO driver is required for SMBus access.' + #13#10#13#10 +
                'Option 1: Download automatically (if available)' + #13#10 +
                'Option 2: Install manually from https://pawnio.eu' + #13#10#13#10 +
                'Attempt automatic download?', mbConfirmation, MB_YESNO) = IDYES then
      begin
        PawnIOInstallerPath := ExpandConstant('{tmp}\PawnIO_setup.exe');
        DependencyDownloadPage.Add(PawnIODownloadURL, 'PawnIO_setup.exe', '');
      end
      else
      begin
        MsgBox('Please download and install PawnIO manually from https://pawnio.eu before continuing.' + #13#10 + 
               'The installer will continue, but the application will not work without PawnIO.', 
               mbInformation, MB_OK);
        NeedPawnIO := False; // Don't try to install
      end;
    end;
    
    // Download dependencies
    if NeedDotNet or NeedPawnIO then
    begin
      DependencyDownloadPage.Show;
      try
        DependencyDownloadPage.Download;
        Result := True;
      except
        ErrorMsg := AddPeriod(GetExceptionMessage);
        MsgBox('Dependency download failed: ' + ErrorMsg + #13#10#13#10 +
               'Please download dependencies manually:', mbError, MB_OK);
        Result := False;
      end;
      DependencyDownloadPage.Hide;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    // Install .NET 8 Desktop Runtime if downloaded
    if NeedDotNet and FileExists(DotNetInstallerPath) then
    begin
      if MsgBox('Installing .NET 8 Desktop Runtime (required)...', mbInformation, MB_OKCANCEL) = IDOK then
      begin
        Exec(DotNetInstallerPath, '/install /quiet /norestart', '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
        if ResultCode = 0 then
        begin
          DotNetWasInstalled := True;
          // Record that WE installed it (after successful install)
          RegWriteDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledDotNet', 1);
          MsgBox('.NET 8 Desktop Runtime installed successfully.', mbInformation, MB_OK);
        end
        else
        begin
          RegWriteDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledDotNet', 0);
          MsgBox('.NET 8 Desktop Runtime installation failed (exit code ' + IntToStr(ResultCode) + ').' + #13#10 + 
                 'The application may not work correctly.', mbError, MB_OK);
        end;
      end;
    end
    else
    begin
      // .NET was already installed, mark as preexisting
      RegWriteDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledDotNet', 0);
    end;
    
    // Install PawnIO if downloaded
    if NeedPawnIO and FileExists(PawnIOInstallerPath) then
    begin
      if MsgBox('Installing PawnIO driver (required)...' + #13#10#13#10 +
                'This will install a signed kernel driver for SMBus access.' + #13#10 +
                'UAC elevation will be requested.', mbInformation, MB_OKCANCEL) = IDOK then
      begin
        Exec(PawnIOInstallerPath, '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART', '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
        if ResultCode = 0 then
        begin
          PawnIOWasInstalled := True;
          // Record that WE installed it (after successful install)
          RegWriteDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledPawnIO', 1);
          MsgBox('PawnIO driver installed successfully.', mbInformation, MB_OK);
        end
        else
        begin
          RegWriteDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledPawnIO', 0);
          MsgBox('PawnIO installation failed (exit code ' + IntToStr(ResultCode) + ').' + #13#10 + 
                 'You may need to install it manually from https://pawnio.eu', mbError, MB_OK);
        end;
      end;
    end
    else
    begin
      // PawnIO was already installed, mark as preexisting
      RegWriteDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledPawnIO', 0);
    end;
  end;
end;

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\XtreemRgbToggle"

// Uninstall-time variables (cannot use wizard pages in uninstall)
var
  UninstallRemoveDotNet: Boolean;
  UninstallRemovePawnIO: Boolean;

function InitializeUninstall(): Boolean;
var
  InstalledDotNet: Cardinal;
  InstalledPawnIO: Cardinal;
  Response: Integer;
  Msg: String;
begin
  Result := True;
  UninstallRemoveDotNet := False;
  UninstallRemovePawnIO := False;
  
  // Check if we installed dependencies
  RegQueryDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledDotNet', InstalledDotNet);
  RegQueryDWordValue(HKLM, 'Software\XtreemRgbToggle', 'InstalledPawnIO', InstalledPawnIO);
  
  // Only offer to remove dependencies WE installed
  if (InstalledDotNet = 1) or (InstalledPawnIO = 1) then
  begin
    Msg := 'This installer installed the following dependencies:' + #13#10;
    if InstalledDotNet = 1 then
      Msg := Msg + '  - .NET 8 Desktop Runtime' + #13#10;
    if InstalledPawnIO = 1 then
      Msg := Msg + '  - PawnIO Driver' + #13#10;
    Msg := Msg + #13#10 + 'By default, these will be KEPT (other apps may use them).' + #13#10#13#10;
    Msg := Msg + 'Do you want to remove these dependencies?' + #13#10;
    Msg := Msg + 'WARNING: Other applications may depend on them.';
    
    Response := MsgBox(Msg, mbConfirmation, MB_YESNO);
    
    if Response = IDYES then
    begin
      // Ask for each dependency separately if both are present
      if InstalledDotNet = 1 then
      begin
        Response := MsgBox('Remove .NET 8 Desktop Runtime?', mbConfirmation, MB_YESNO);
        UninstallRemoveDotNet := (Response = IDYES);
      end;
      
      if InstalledPawnIO = 1 then
      begin
        Response := MsgBox('Remove PawnIO Driver?', mbConfirmation, MB_YESNO);
        UninstallRemovePawnIO := (Response = IDYES);
      end;
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Remove scheduled task if exists
    Exec('schtasks.exe', '/Delete /TN "XtreemRgbToggle-Off" /F', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    
    // Remove .NET 8 Desktop Runtime if user chose to
    if UninstallRemoveDotNet then
    begin
      MsgBox('Please use Windows Settings → Apps to uninstall ".NET Desktop Runtime 8.x".' + #13#10 +
             'Automatic removal not supported for safety (shared component).', 
             mbInformation, MB_OK);
    end;
    
    // Remove PawnIO if user chose to
    if UninstallRemovePawnIO then
    begin
      // Use PawnIO's own uninstaller if available
      if FileExists(ExpandConstant('{pf}\PawnIO\unins000.exe')) then
      begin
        Exec(ExpandConstant('{pf}\PawnIO\unins000.exe'), '/VERYSILENT /SUPPRESSMSGBOXES', '', SW_SHOW, ewWaitUntilTerminated, ResultCode);
        if ResultCode <> 0 then
        begin
          MsgBox('PawnIO uninstaller failed. Please uninstall manually via Windows Settings → Apps.', 
                 mbInformation, MB_OK);
        end;
      end
      else
      begin
        MsgBox('PawnIO uninstaller not found. Please uninstall manually via Windows Settings → Apps.', 
               mbInformation, MB_OK);
      end;
    end;
  end;
end;
