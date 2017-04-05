; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{C414BE13-5F10-48B7-AF92-9E4E7265D720}
AppName=Bibledit
AppVersion=5.0.145
AppPublisher=Teus Benschop
AppPublisherURL=http://bibledit.org
AppSupportURL=http://bibledit.org
AppUpdatesURL=http://bibledit.org
; Spaces in paths have not been tested on Windows.
; Therefore install Bibledit in C:\bibledit and do not allow the user to change the location.
DefaultDirName=C:\bibledit
DisableDirPage=yes
DefaultGroupName=Bibledit
LicenseFile=COPYING
; OutputDir=C:\bibledit-windows-packager
OutputBaseFilename=bibledit-5.0.145
Compression=lzma
SolidCompression=yes
; Create a log file in the user's TEMP directory detailing file installation and [Run] actions taken during the installation process.
; To find the location: Search %temp% in the Programs and Folders.
SetupLogging=yes
; Requires Microsoft Vista or higher.
MinVersion=0,6.0.6000

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
Source: "*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Dirs]
Name: "{app}"; Permissions: everyone-full

[Icons]
Name: "{group}\Bibledit"; Filename: "{app}\bibledit.exe"
Name: "{commondesktop}\Bibledit"; Filename: "{app}\bibledit.exe"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Bibledit"; Filename: "{app}\bibledit.exe"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\bibledit.exe"; Description: "{cm:LaunchProgram,Bibledit}"; Flags: nowait postinstall skipifsilent
; CefSharp, the WebKit component, needs C++ 2013.
Filename: "{app}\vcredist_x86.exe"; Parameters: "/install /passive /norestart"; Description: "Install Visual C++ Redistributable 2013"; Flags: runhidden skipifsilent
; The server.exe needs C++ 2015.
Filename: "{app}\vc_redist.x86.exe"; Parameters: "/install /passive /norestart"; Description: "Install Visual C++ Redistributable 2015"; Flags: runhidden skipifsilent

[Code]

procedure ReplaceValue (const FileName, Search, Replace: String);
var
  I: Integer;
  Line: String;
  FileLines: TStringList;
begin
  FileLines := TStringList.Create;
  try
    FileLines.LoadFromFile(FileName);
    for I := 0 to FileLines.Count - 1 do
    begin
      Line := FileLines[I];
      StringChangeEx (Line, Search, Replace, False);
      FileLines[I] := Line;
    end;
  finally
    FileLines.SaveToFile(FileName);
    FileLines.Free;
  end;
end;

function GetFileAttributes(lpFileName: PAnsiChar): DWORD;
external 'GetFileAttributesA@kernel32.dll stdcall';

function SetFileAttributes(lpFileName: PAnsiChar; dwFileAttributes: DWORD): BOOL;
external 'SetFileAttributesA@kernel32.dll stdcall';

procedure RemoveReadOnly(FileName : String);
var
  Attr : DWord;
begin
  Attr := GetFileAttributes(FileName);
  if (Attr and 1) = 1  then          
  begin
    Attr := Attr -1;
    SetFileAttributes(FileName,Attr);
  end;
end;

procedure DoPostInstall();
var
  appPath: String;
  bibleditWebSetupPath: String;
begin
  appPath := ExpandConstant ('{app}');

  // Set the setup folder to read write mode.
  RemoveReadOnly (bibleditWebSetupPath);
end;


// Utility functions for Inno Setup
// used to add/remove programs from the windows firewall rules
// Code originally from http://news.jrsoftware.org/news/innosetup/msg43799.html

const
  NET_FW_SCOPE_ALL = 0;
  NET_FW_IP_VERSION_ANY = 2;

procedure SetFirewallException(AppName,FileName:string);
var
  FirewallObject: Variant;
  FirewallManager: Variant;
  FirewallProfile: Variant;
begin
  try
    FirewallObject := CreateOleObject('HNetCfg.FwAuthorizedApplication');
    FirewallObject.ProcessImageFileName := FileName;
    FirewallObject.Name := AppName;
    FirewallObject.Scope := NET_FW_SCOPE_ALL;
    FirewallObject.IpVersion := NET_FW_IP_VERSION_ANY;
    FirewallObject.Enabled := True;
    FirewallManager := CreateOleObject('HNetCfg.FwMgr');
    FirewallProfile := FirewallManager.LocalPolicy.CurrentProfile;
    FirewallProfile.AuthorizedApplications.Add(FirewallObject);
  except
  end;
end;

procedure RemoveFirewallException( FileName:string );
var
  FirewallManager: Variant;
  FirewallProfile: Variant;
begin
  try
    FirewallManager := CreateOleObject('HNetCfg.FwMgr');
    FirewallProfile := FirewallManager.LocalPolicy.CurrentProfile;
    FireWallProfile.AuthorizedApplications.Remove(FileName);
  except
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep=ssPostInstall then
     SetFirewallException('Bibledit', ExpandConstant('{app}\')+'server.exe');
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep=usPostUninstall then
     RemoveFirewallException(ExpandConstant('{app}\')+'server.exe');
end;
