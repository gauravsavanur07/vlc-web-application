; VideoLAN Movie Creator
;
; NSIS Script by:
; Ludovic Fauvet <etix@l0cal.com>

!include "FileAssociation.nsh"

;--------------------------------
; General

; Name
Name "VideoLAN Movie Creator"

; Output file
OutFile "@NSIS_OUTPUT_FILE@"

; Get installation folder from registry if available
InstallDirRegKey HKLM "Software\@PROJECT_NAME_SHORT@" "Install_Dir"

; Install directory
InstallDir "$PROGRAMFILES\VideoLAN\@PROJECT_NAME_SHORT@"

; Request admin permissions for Vista and higher
RequestExecutionLevel admin

; Compression method
SetCompressor /SOLID lzma

;--------------------------------
; Interface

; Warn the user if he try to close the installer
!define MUI_ABORTWARNING

LicenseText "License"
LicenseData "@CMAKE_SOURCE_DIR@/COPYING"

;--------------------------------
; Pages

; Install
Page license
Page components
Page directory
Page instfiles

; Uninstall
UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------
; Installer sections

Section "@PROJECT_NAME_SHORT@ (required)"

  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; Put file there
  File "libvlc.dll"
  File "libvlccore.dll"
  File "libeay32.dll"
  File "ssleay32.dll"
;  File "mingwm10.dll"
;  File "libgcc_s_dw2-1.dll"
;  File "QtCore4.dll"
;  File "QtGui4.dll"
;  File "QtSvg4.dll"
;  File "QtXml4.dll"
  File "vlmc.exe"
  File "@CMAKE_SOURCE_DIR@/share/vlmc.ico"
  File "@CMAKE_SOURCE_DIR@/COPYING"
  File "@CMAKE_SOURCE_DIR@/AUTHORS"
  File "@CMAKE_SOURCE_DIR@/TRANSLATORS"
  File "@CMAKE_SOURCE_DIR@/NEWS"
  File /r "plugins"

  ; Write the installation path into the registry
  WriteRegStr HKLM "Software\@PROJECT_NAME_SHORT@" "Install_Dir" "$INSTDIR"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "DisplayName" "@PROJECT_NAME_LONG@"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "DisplayIcon" '"$INSTDIR\vlmc.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "DisplayVersion" "@PROJECT_VERSION@"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "Publisher" "VideoLAN"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "URLInfoAbout" "http://www.videolan.org/vlmc/"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@" "NoRepair" 1
  WriteUninstaller "uninstall.exe"

  ; File association
  ${registerExtension} "$INSTDIR\vlmc.exe" ".vlmc" "VLMC Project"

SectionEnd

Section "Frei0r effects & Qt translations"
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  File /r "effects"
  File /r "ts"
SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\@PROJECT_NAME_LONG@"
  CreateShortCut "$SMPROGRAMS\@PROJECT_NAME_LONG@\@PROJECT_NAME_SHORT@.lnk" "$INSTDIR\vlmc.exe" "" "$INSTDIR\vlmc.ico" 0
  CreateShortCut "$SMPROGRAMS\@PROJECT_NAME_LONG@\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

Section "Desktop Shortcut"

  CreateShortCut "$DESKTOP\@PROJECT_NAME_LONG@.lnk" "$INSTDIR\vlmc.exe" "" "$INSTDIR\vlmc.ico" 0

SectionEnd

;--------------------------------
; Uninstaller sections

Section "Uninstall"

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\@PROJECT_NAME_SHORT@"
  DeleteRegKey HKLM "Software\@PROJECT_NAME_SHORT@"

  ; Remove files and uninstaller
  Delete "$INSTDIR\vlmc.exe"
  Delete "$INSTDIR\vlmc.ico"
  Delete "$INSTDIR\uninstall.exe"
  Delete "$INSTDIR\libvlc.dll"
  Delete "$INSTDIR\libvlccore.dll"
  Delete "$INSTDIR\libeay32.dll"
  Delete "$INSTDIR\ssleay32.dll"
;  Delete "$INSTDIR\mingwm10.dll"
;  Delete "$INSTDIR\libgcc_s_dw2-1.dll"
;  Delete "$INSTDIR\QtCore4.dll"
;  Delete "$INSTDIR\QtGui4.dll"
;  Delete "$INSTDIR\QtSvg4.dll"
;  Delete "$INSTDIR\QtXml4.dll"
  Delete "$INSTDIR\COPYING"
  Delete "$INSTDIR\AUTHORS"
  Delete "$INSTDIR\TRANSLATORS"
  Delete "$INSTDIR\NEWS"
  Delete "$INSTDIR\plugins\*.*"
  Delete "$INSTDIR\effects\*.*"
  Delete "$INSTDIR\ts\*.*"
  RMDir "$INSTDIR\plugins"
  RMDir "$INSTDIR\effects"
  RMDir "$INSTDIR\ts"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\@PROJECT_NAME_LONG@\*.*"
  Delete "$DESKTOP\@PROJECT_NAME_LONG@.lnk"

  ; Remove directories used
  RMDir "$SMPROGRAMS\@PROJECT_NAME_LONG@"
  RMDir "$INSTDIR"

  ; Remove file association
  ${unregisterExtension} ".vlmc" "VLMC Project"

SectionEnd
