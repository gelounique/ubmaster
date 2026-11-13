; UBMaster — Windows installer (Inno Setup 6)
; by unique & MELO — UNIQUEBABY
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\..\build\UBMaster_artefacts\Release"
#endif
; pass /DWithAAX=1 when the (PACE-signed) AAX build exists in {#BuildDir}\AAX
#ifndef WithAAX
  #define WithAAX 0
#endif

[Setup]
AppId={{9C4D2B7E-1A3F-4E6B-8D21-5F7A0C3E9B12}
AppName=UBMaster
AppVersion={#AppVersion}
AppVerName=UBMaster {#AppVersion}
AppPublisher=UNIQUEBABY · unique & MELO
AppPublisherURL=https://ubmaster.uniquebaby.de
AppSupportURL=https://ubmaster.uniquebaby.de
AppCopyright=© 2026 unique & MELO – UNIQUEBABY
DefaultDirName={autopf}\UBMaster
DefaultGroupName=UBMaster
DisableProgramGroupPage=yes
OutputBaseFilename=UBMaster-Windows-Setup
OutputDir=..\..\dist
SetupIconFile=..\..\branding\UBMaster.ico
UninstallDisplayIcon={app}\UBMaster.ico
UninstallDisplayName=UBMaster {#AppVersion}
WizardStyle=modern
WizardImageFile=wizard-large-100.bmp,wizard-large.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small.bmp
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
CloseApplications=yes
VersionInfoVersion={#AppVersion}
VersionInfoCompany=UNIQUEBABY
VersionInfoDescription=UBMaster Setup

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
de.CompVst3=VST3-Plugin (FL Studio, Ableton Live, Cubase, Studio One, Reaper, Bitwig ...)
en.CompVst3=VST3 plugin (FL Studio, Ableton Live, Cubase, Studio One, Reaper, Bitwig ...)
de.CompAax=AAX-Plugin (Pro Tools)
en.CompAax=AAX plugin (Pro Tools)
de.CompApp=UBMaster Standalone-App (Dateien/Audio-Interface direkt messen)
en.CompApp=UBMaster standalone app
de.Desktop=Desktop-Verknüpfung für die Standalone-App
en.Desktop=Desktop shortcut for the standalone app
de.UninstallEntry=UBMaster deinstallieren
en.UninstallEntry=Uninstall UBMaster
de.Rescan=Fertig! Starte deine DAW neu bzw. lass sie nach neuen Plugins suchen – UBMaster findest du unter UNIQUEBABY. Tipp: als letztes Plugin auf den Master-Bus.
en.Rescan=Done! Restart your DAW or rescan plugins – UBMaster is listed under UNIQUEBABY. Tip: put it last on your master bus.

[Types]
Name: "full"; Description: "Komplett / Full"
Name: "custom"; Description: "Anpassen / Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVst3}"; Types: full custom; Flags: fixed
#if WithAAX
Name: "aax"; Description: "{cm:CompAax}"; Types: full
#endif
Name: "app"; Description: "{cm:CompApp}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:Desktop}"; Components: app; Flags: unchecked

[Files]
Source: "{#BuildDir}\VST3\UBMaster.vst3\*"; DestDir: "{commoncf64}\VST3\UBMaster.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#if WithAAX
Source: "{#BuildDir}\AAX\UBMaster.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\UBMaster.aaxplugin"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
Source: "{#BuildDir}\Standalone\UBMaster.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\..\branding\UBMaster.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\UBMaster"; Filename: "{app}\UBMaster.exe"; Components: app
Name: "{group}\{cm:UninstallEntry}"; Filename: "{uninstallexe}"; IconFilename: "{app}\UBMaster.ico"
Name: "{autodesktop}\UBMaster"; Filename: "{app}\UBMaster.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\UBMaster.exe"; Description: "UBMaster starten"; Components: app; Flags: postinstall nowait skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\UBMaster.vst3"
Type: filesandordirs; Name: "{commoncf64}\Avid\Audio\Plug-Ins\UBMaster.aaxplugin"
Type: filesandordirs; Name: "{app}"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssDone) and (not WizardSilent) then
    MsgBox(CustomMessage('Rescan'), mbInformation, MB_OK);
end;
