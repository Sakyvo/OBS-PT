Unicode true
ManifestDPIAware true

!define APP_NAME "OBS-PT"
!define APP_VERSION "0.0.1-alpha"
!define COMPANY_NAME "OBS-PT"
!define INSTALLER_NAME "OBS-PT-${APP_VERSION}-Installer.exe"
!define SOURCE_DIR "..\..\build-v143\_pkg\OBS-PT"
!define EXE_PATH "bin\64bit\OBS-PT.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS-PT"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\..\build-v143\_pkg\${INSTALLER_NAME}"
InstallDir "C:\OBS-PT"
RequestExecutionLevel user
SetCompressor /SOLID lzma

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

!insertmacro GetDrives

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\cmake\winrc\obs-studio.ico"
!define MUI_UNICON "..\..\cmake\winrc\obs-studio.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "OBSHeader.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "OBSBanner.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${EXE_PATH}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch OBS-PT"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function PickDefaultDrive
	${If} $INSTDIR != ""
		Push "StopGetDrives"
		Return
	${EndIf}

	StrCpy $0 $9 1
	${If} $0 == "A"
	${OrIf} $0 == "B"
	${OrIf} $0 == "C"
		Push ""
		Return
	${EndIf}

	StrCpy $INSTDIR "$9OBS-PT"
	Push "StopGetDrives"
FunctionEnd

Function .onInit
	${If} $INSTDIR != "C:\OBS-PT"
		Return
	${EndIf}

	StrCpy $INSTDIR ""
	${GetDrives} "HDD" "PickDefaultDrive"
	${If} $INSTDIR == ""
		StrCpy $INSTDIR "C:\OBS-PT"
	${EndIf}
FunctionEnd

Section "OBS-PT" SecMain
	SetOutPath "$INSTDIR"
	File /r "${SOURCE_DIR}\*.*"

	SetOutPath "$INSTDIR"
	WriteUninstaller "$INSTDIR\uninstall.exe"

	CreateDirectory "$SMPROGRAMS\OBS-PT"
	CreateShortCut "$SMPROGRAMS\OBS-PT\OBS-PT.lnk" "$INSTDIR\${EXE_PATH}" "" "$INSTDIR\${EXE_PATH}" 0
	CreateShortCut "$SMPROGRAMS\OBS-PT\Uninstall OBS-PT.lnk" "$INSTDIR\uninstall.exe"
	CreateShortCut "$DESKTOP\OBS-PT.lnk" "$INSTDIR\${EXE_PATH}" "" "$INSTDIR\${EXE_PATH}" 0

	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "OBS-PT"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${APP_VERSION}"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "${COMPANY_NAME}"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${EXE_PATH}"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
	WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
	Delete "$DESKTOP\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\Uninstall OBS-PT.lnk"
	RMDir "$SMPROGRAMS\OBS-PT"

	DeleteRegKey HKCU "${UNINSTALL_KEY}"
	RMDir /r "$INSTDIR"
SectionEnd
