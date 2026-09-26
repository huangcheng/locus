; Locus Windows installer (Inno Setup 6).
;
; Rebuild from a fresh checkout (MSVC + Qt msvc2022_64 kit on the machine):
;   cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
;   cmake --build cmake-build-release --target locus --parallel
;   cmake --install cmake-build-release --prefix cmake-build-release\stage
;   iscc installer.iss
; The install step runs windeployqt (see CMakeLists.txt), so the stage dir
; holds locus.exe plus the Qt runtime it needs. Output lands in
; cmake-build-release\dist\.

; keep in sync with project(locus VERSION ...)
#define AppVersion "0.1.0"

; CI overrides these with /D (release.yml builds into build\ instead).
#ifndef StageDir
#define StageDir "cmake-build-release\stage"
#endif
#ifndef OutputDirPath
#define OutputDirPath "cmake-build-release\dist"
#endif

[Setup]
AppId={{B7E2A14C-9D3F-4A58-8E6C-1F2B4D5A6C7E}
AppName=Locus
AppVersion={#AppVersion}
AppVerName=Locus {#AppVersion}
AppPublisher=HUANG Cheng
; Per-user install: no UAC prompt, no admin needed.
DefaultDirName={localappdata}\Programs\Locus
PrivilegesRequired=lowest
DefaultGroupName=Locus
OutputDir={#OutputDirPath}
OutputBaseFilename=Locus-{#AppVersion}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
WizardStyle=modern
MinVersion=10.0
CloseApplications=yes
UninstallDisplayIcon={app}\locus.exe
SetupIconFile=windows\locus.ico
VersionInfoVersion={#AppVersion}
VersionInfoCompany=HUANG Cheng
VersionInfoDescription=Locus installer
VersionInfoProductName=Locus
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright (c) 2026 HUANG Cheng
; Prefs live in HKCU\Software\Locus (QSettings) and survive uninstall.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "installer\lang\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Locus"; Filename: "{app}\locus.exe"
Name: "{autodesktop}\Locus"; Filename: "{app}\locus.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\locus.exe"; Description: "{cm:LaunchProgram,Locus}"; Flags: nowait postinstall skipifsilent
; Silent auto-update: relaunch after upgrade (the entry above skips silent
; installs, and the updater UI promises "Install & Restart").
Filename: "{app}\locus.exe"; Flags: nowait skipifnotsilent