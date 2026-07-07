Unicode true
ManifestDPIAware true

!define APP_NAME "OBS-PT"
!define APP_VERSION "1.0.0"
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
CRCCheck force

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"

!insertmacro GetDrives

Var OverrideConfig
Var OverrideConfigCheckbox
Var ConfigBackupDir
Var ConfigBackupCreated

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\cmake\winrc\obs-studio.ico"
!define MUI_UNICON "..\..\cmake\winrc\obs-studio.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "OBSHeader.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "OBSBanner.bmp"
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Launch OBS-PT"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchOBS"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom ConfigPageCreate ConfigPageLeave
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
	StrCpy $OverrideConfig 0
	StrCpy $ConfigBackupCreated 0
	StrCpy $ConfigBackupDir ""

	${If} $INSTDIR != "C:\OBS-PT"
		Return
	${EndIf}

	StrCpy $INSTDIR ""
	${GetDrives} "HDD" "PickDefaultDrive"
	${If} $INSTDIR == ""
		StrCpy $INSTDIR "C:\OBS-PT"
	${EndIf}
FunctionEnd

Function ConfigPageCreate
	!insertmacro MUI_HEADER_TEXT "Overwrite Configuration" "Choose whether setup should replace existing OBS-PT configuration."

	nsDialogs::Create 1018
	Pop $0
	${If} $0 == error
		Abort
	${EndIf}

	${NSD_CreateLabel} 0 0 100% 34u "By default, setup installs or updates OBS-PT program files only and keeps the existing portable configuration unchanged."
	Pop $0

	${NSD_CreateCheckbox} 0 48u 100% 12u "Overwrite existing OBS-PT configuration with packaged defaults"
	Pop $OverrideConfigCheckbox

	${NSD_CreateLabel} 0 68u 100% 48u "Checked: setup replaces the portable obs-studio configuration, then preserves stream settings, recording paths, hotkeys, and replay buffer settings from the previous configuration."
	Pop $0

	${If} $OverrideConfig == 1
		${NSD_SetState} $OverrideConfigCheckbox ${BST_CHECKED}
	${Else}
		${NSD_SetState} $OverrideConfigCheckbox 0
	${EndIf}

	nsDialogs::Show
FunctionEnd

Function ConfigPageLeave
	${NSD_GetState} $OverrideConfigCheckbox $0
	${If} $0 == ${BST_CHECKED}
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "This will replace the existing OBS-PT portable configuration with packaged defaults.$\r$\n$\r$\nSetup will try to preserve stream settings, recording paths, hotkeys, and replay buffer settings from the previous configuration.$\r$\n$\r$\nContinue?" IDYES confirm_config_overwrite
		Abort
confirm_config_overwrite:
		StrCpy $OverrideConfig 1
	${Else}
		StrCpy $OverrideConfig 0
	${EndIf}
FunctionEnd

Function LaunchOBS
	SetOutPath "$INSTDIR\bin\64bit"
	Exec '"$INSTDIR\${EXE_PATH}"'
FunctionEnd

Function ReserveConfigBackupDir
	StrCpy $ConfigBackupDir "$INSTDIR\obs-studio.__obspt_backup"
	StrCpy $0 0

reserve_config_backup_loop:
	IfFileExists "$ConfigBackupDir\*.*" reserve_config_backup_next reserve_config_backup_check_file

reserve_config_backup_check_file:
	IfFileExists "$ConfigBackupDir" reserve_config_backup_next reserve_config_backup_done

reserve_config_backup_next:
	IntOp $0 $0 + 1
	StrCpy $ConfigBackupDir "$INSTDIR\obs-studio.__obspt_backup_$0"
	Goto reserve_config_backup_loop

reserve_config_backup_done:
FunctionEnd

Function PrepareExistingConfig
	StrCpy $ConfigBackupCreated 0
	StrCpy $ConfigBackupDir ""

	IfFileExists "$INSTDIR\obs-studio\*.*" 0 prepare_existing_config_done

	Call ReserveConfigBackupDir
	Rename "$INSTDIR\obs-studio" "$ConfigBackupDir"
	IfErrors 0 prepare_existing_config_moved
		MessageBox MB_OK|MB_ICONSTOP "Setup could not move the existing OBS-PT configuration out of the way.$\r$\n$\r$\nClose OBS-PT and any tools using files under:$\r$\n$INSTDIR\obs-studio"
		Abort

prepare_existing_config_moved:
	StrCpy $ConfigBackupCreated 1

prepare_existing_config_done:
FunctionEnd

Function FinalizeExistingConfig
	${If} $ConfigBackupCreated != 1
		Return
	${EndIf}

	${If} $OverrideConfig == 1
		InitPluginsDir
		File /oname=$PLUGINSDIR\obspt-preserve-config.ps1 "obspt-preserve-config.ps1"
		DetailPrint "Merging preserved OBS-PT configuration values"
		nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PLUGINSDIR\obspt-preserve-config.ps1" "$ConfigBackupDir" "$INSTDIR\obs-studio"'
		Pop $0
		${If} $0 == 0
			RMDir /r "$ConfigBackupDir"
		${Else}
			MessageBox MB_OK|MB_ICONEXCLAMATION "Setup replaced the OBS-PT configuration, but could not restore all preserved settings.$\r$\n$\r$\nThe previous configuration was kept at:$\r$\n$ConfigBackupDir"
		${EndIf}
		Return
	${EndIf}

	RMDir /r "$INSTDIR\obs-studio"
	Rename "$ConfigBackupDir" "$INSTDIR\obs-studio"
	IfErrors 0 finalize_existing_config_done
		MessageBox MB_OK|MB_ICONSTOP "Setup could not restore the previous OBS-PT configuration.$\r$\n$\r$\nThe backup is still available at:$\r$\n$ConfigBackupDir"
		Abort

finalize_existing_config_done:
FunctionEnd

Section "OBS-PT" SecMain
	Call PrepareExistingConfig

	SetOverwrite on
	SetOutPath "$INSTDIR"
	File /r "${SOURCE_DIR}\*.*"

	Call FinalizeExistingConfig

	SetOutPath "$INSTDIR"
	WriteUninstaller "$INSTDIR\uninstall.exe"

	Delete "$SMPROGRAMS\OBS-PT\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\Uninstall OBS-PT.lnk"
	RMDir "$SMPROGRAMS\OBS-PT"

	SetOutPath "$INSTDIR\bin\64bit"
	CreateShortCut "$SMPROGRAMS\OBS-PT.lnk" "$INSTDIR\${EXE_PATH}" "" "$INSTDIR\${EXE_PATH}" 0
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
	Delete "$SMPROGRAMS\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\Uninstall OBS-PT.lnk"
	RMDir "$SMPROGRAMS\OBS-PT"

	DeleteRegKey HKCU "${UNINSTALL_KEY}"
	RMDir /r "$INSTDIR"
SectionEnd
