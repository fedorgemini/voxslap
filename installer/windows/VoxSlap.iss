; Inno Setup script for VoxSlap. Built in CI: ISCC.exe /DArtefacts=<Release dir> /DOutDir=<dir> VoxSlap.iss
#ifndef Artefacts
  #define Artefacts "..\..\build\VoxSlap_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6E2B5D7A-3C41-4F0B-9A8E-5D2C7F1B9E44}
AppName=VoxSlap
AppVersion={#AppVersion}
AppPublisher=Homebrew Audio
DefaultDirName={autopf}\VoxSlap
DefaultGroupName=VoxSlap
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=VoxSlap-Windows-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\VoxSlap.exe
UninstallDisplayName=VoxSlap

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (Ableton, FL Studio, Cubase, Reaper, Studio One...)"; Types: full custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#Artefacts}\VST3\VoxSlap.vst3\*"; DestDir: "{commoncf64}\VST3\VoxSlap.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\VoxSlap.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\VoxSlap"; Filename: "{app}\VoxSlap.exe"; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\VoxSlap.vst3"
