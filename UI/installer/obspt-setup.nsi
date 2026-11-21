Unicode true
ManifestDPIAware true

!define APP_NAME "OBS-PT"
!define APP_VERSION "1.3.0"
!define COMPANY_NAME "OBS-PT"
!define INSTALLER_NAME "OBS-PT-${APP_VERSION}-Installer.exe"
!define SOURCE_DIR "..\..\.target\_pkg\OBS-PT"
; Packaging guard: both virtual-camera module architectures must be staged.
; The Win32 lane is a separate manual build, so it is easy to refresh
; staging and silently drop the 32-bit module - which previously shipped a
; filter that never registered (both regsvr32 calls share one command).
!if ! /FileExists "${SOURCE_DIR}\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll"
	!error "obs-virtualcam-module32.dll is missing from staging. Build the Win32 virtual-camera lane and copy it in (see AGENTS.md)."
!endif
!if ! /FileExists "${SOURCE_DIR}\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"
	!error "obs-virtualcam-module64.dll is missing from staging. Refresh staging (cmake --install) before packaging."
!endif
!define EXE_PATH "bin\OBS-PT.exe"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBS-PT"
; The configuration-format generation this installer ships. A User Data
; Root whose [OBSPT] BootstrapVersion is below this floor is legacy and
; must be reset and merged; a root at or above it is left untouched.
; Raise this floor only when a future release changes the config format.
!define CONFIG_GENERATION "3"

; Issue 075 (ADR 0009): the installer-driven default replacement. When this
; build upgrades a tree whose recorded product version is older, the
; installer records "pending" so the application performs one silent
; replacement on its next launch (dock layout, encoder re-probe, lenient
; Java detection). A fresh install records "done" so first launch does not
; replace anything. The marker is deliberately separate from
; BootstrapVersion, whose repair cargo (forcing PotPvP and FirstRun) this
; migration must not drag in.
; This release adds capture, not another default replacement.
!define MIGRATION_RELEASE "1.2.0"
!define MIGRATION_PENDING "0"
!define MIGRATION_DONE "1"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\..\.target\.distribution\installer\${INSTALLER_NAME}"
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "${APP_NAME}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=1033 "FileDescription" "OBS-PT Installer"
VIAddVersionKey /LANG=1033 "CompanyName" "${COMPANY_NAME}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (C) 2026 Sakyvo"
InstallDir ""
RequestExecutionLevel user
SetCompressor /SOLID lzma
CRCCheck force

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "WordFunc.nsh"

!insertmacro GetDrives
!insertmacro VersionCompare

Var ConfigBackupDir
Var ConfigBackupCreated
Var ForceResetChecked
Var ForceResetBacked
Var ForceResetCheckboxHwnd
Var VcamLocked
Var MigrationValue

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\cmake\winrc\obs-studio.ico"
!define MUI_UNICON "..\..\cmake\winrc\obs-studio.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "OBSHeader.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "OBSBanner.bmp"
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "$(OBSPT_Launch)"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchOBS"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
Page custom ConfigPageCreate ConfigPageLeave
Page custom ForceResetPageCreate ForceResetPageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

; --- Installer UI strings (English + Simplified Chinese). Issue 046. ---
LangString OBSPT_Launch ${LANG_ENGLISH} "Launch OBS-PT"
LangString OBSPT_Launch ${LANG_SIMPCHINESE} "启动 OBS-PT"
LangString OBSPT_ResetTitle ${LANG_ENGLISH} "Reset Configuration"
LangString OBSPT_ResetTitle ${LANG_SIMPCHINESE} "重置配置"
LangString OBSPT_ResetSub ${LANG_ENGLISH} "OBS-PT ${APP_VERSION} requires the new unified audio configuration."
LangString OBSPT_ResetSub ${LANG_SIMPCHINESE} "OBS-PT ${APP_VERSION} 需要新的统一音频配置。"
LangString OBSPT_ResetBody1 ${LANG_ENGLISH} "Setup will replace the portable obs-studio configuration with the packaged defaults. This reset is required because the unified Audio and Routing model is not compatible with the previous source layout."
LangString OBSPT_ResetBody1 ${LANG_SIMPCHINESE} "安装程序将用打包的默认配置替换 portable obs-studio 配置。此重置是必需的，因为统一的音频与路由模型与之前的来源结构不兼容。"
LangString OBSPT_ResetBody2 ${LANG_ENGLISH} "Setup preserves only stream settings, recording paths, hotkeys, and replay buffer settings. Scenes, sources, audio routing, dock layout, video, audio, and recording encoder settings use the new defaults."
LangString OBSPT_ResetBody2 ${LANG_SIMPCHINESE} "安装程序仅保留串流设置、录制路径、热键和回放缓存设置。场景、来源、音频路由、停靠布局、视频、音频和录制编码器设置使用新的默认值。"
LangString OBSPT_ConfirmOverwriteMsg ${LANG_ENGLISH} "OBS-PT ${APP_VERSION} must replace the existing portable configuration.$\r$\n$\r$\nSetup preserves stream settings, recording paths, hotkeys, and replay buffer settings only.$\r$\n$\r$\nContinue?"
LangString OBSPT_ConfirmOverwriteMsg ${LANG_SIMPCHINESE} "OBS-PT ${APP_VERSION} 必须替换现有的 portable 配置。$\r$\n$\r$\n安装程序仅保留串流设置、录制路径、热键和回放缓存设置。$\r$\n$\r$\n是否继续?"
LangString OBSPT_OBSRunningMsg ${LANG_ENGLISH} "OBS-PT is still running.$\r$\n$\r$\nClose OBS-PT normally before continuing setup. Setup has not installed or replaced any files."
LangString OBSPT_OBSRunningMsg ${LANG_SIMPCHINESE} "OBS-PT 仍在运行。$\r$\n$\r$\n请先正常关闭 OBS-PT 再继续。安装程序尚未安装或替换任何文件。"
LangString OBSPT_OBSRunningUnknownMsg ${LANG_ENGLISH} "Setup could not determine whether OBS-PT is running.$\r$\n$\r$\nSetup will not install or replace any files. Click Retry to check again, or Cancel to exit."
LangString OBSPT_OBSRunningUnknownMsg ${LANG_SIMPCHINESE} "安装程序无法确定 OBS-PT 是否正在运行。$\r$\n$\r$\n安装程序不会安装或替换任何文件。点击“重试”重新检查，或“取消”退出。"
LangString OBSPT_MoveFailedMsg ${LANG_ENGLISH} "Setup could not move the existing OBS-PT configuration out of the way.$\r$\n$\r$\nClose OBS-PT and any tools using files under:$\r$\n$INSTDIR\obs-studio"
LangString OBSPT_MoveFailedMsg ${LANG_SIMPCHINESE} "安装程序无法移开现有的 OBS-PT 配置。$\r$\n$\r$\n请关闭 OBS-PT 及任何占用以下目录下文件的工具：$\r$\n$INSTDIR\obs-studio"
LangString OBSPT_MergeFailedMsg ${LANG_ENGLISH} "Setup replaced the OBS-PT configuration, but could not restore all preserved settings.$\r$\n$\r$\nThe previous configuration was kept at:$\r$\n$ConfigBackupDir"
LangString OBSPT_MergeFailedMsg ${LANG_SIMPCHINESE} "安装程序已替换 OBS-PT 配置，但无法恢复全部保留的设置。$\r$\n$\r$\n原配置保留在：$\r$\n$ConfigBackupDir"

LangString OBSPT_FRTitle ${LANG_ENGLISH} "Force-Reset Settings"
LangString OBSPT_FRTitle ${LANG_SIMPCHINESE} "覆盖安装设置"
LangString OBSPT_FRSub ${LANG_ENGLISH} "Back up and reset all OBS-PT configuration to factory defaults"
LangString OBSPT_FRSub ${LANG_SIMPCHINESE} "备份并把所有 OBS-PT 配置重置为出厂默认"
LangString OBSPT_FRBody ${LANG_ENGLISH} "Checking this option resets every OBS-PT setting to the packaged defaults. Your current configuration is preserved as a backup first."
LangString OBSPT_FRBody ${LANG_SIMPCHINESE} "勾选此选项会把所有 OBS-PT 设置重置为打包的默认值。操作前会先备份你当前的配置。"
LangString OBSPT_FRCheckbox ${LANG_ENGLISH} "Force-Reset Settings (restore factory defaults)"
LangString OBSPT_FRCheckbox ${LANG_SIMPCHINESE} "覆盖安装设置(恢复出厂默认)"
LangString OBSPT_FRConfirmMsg ${LANG_ENGLISH} "This will reset all OBS-PT configuration to factory defaults. Your current configuration will be backed up to obs-studio.__obspt_backup.$\r$\n$\r$\nAre you sure you want to continue?"
LangString OBSPT_FRConfirmMsg ${LANG_SIMPCHINESE} "此操作将把 OBS-PT 配置重置为出厂默认。操作前会先备份,当前配置将备份至 obs-studio.__obspt_backup。$\r$\n$\r$\n是否继续?"
LangString OBSPT_FRDoneMsg ${LANG_ENGLISH} "OBS-PT was reset to factory defaults. Your previous configuration was kept at:$\r$\n$ConfigBackupDir"
LangString OBSPT_FRDoneMsg ${LANG_SIMPCHINESE} "OBS-PT 已重置为出厂默认。你之前的配置保留在:$\r$\n$ConfigBackupDir"

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
	; Issue 046: the installer UI language follows NSIS built-in auto-
	; detection (GetUserDefaultUILanguage -> loaded-language match, else
	; the first language below which is English). No selection dialog.
	StrCpy $ConfigBackupCreated 0
	StrCpy $ForceResetChecked 0
	StrCpy $ForceResetBacked 0
	StrCpy $ConfigBackupDir ""

	${If} $INSTDIR != ""
		Return
	${EndIf}

	ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "InstallLocation"
	${If} $0 != ""
		IfFileExists "$0\${EXE_PATH}" 0 select_new_install_dir
		StrCpy $INSTDIR "$0"
		Return
	${EndIf}

select_new_install_dir:
	${GetDrives} "HDD" "PickDefaultDrive"
	${If} $INSTDIR == ""
		StrCpy $INSTDIR "C:\OBS-PT"
	${EndIf}
FunctionEnd

; Pushes 1 when the existing User Data Root must be reset (legacy
; generation or unreadable), 0 when it is already current or absent.
Function NeedsConfigReset
	${If} $INSTDIR == ""
		Push 0
		Return
	${EndIf}

	IfFileExists "$INSTDIR\obs-studio\global.ini" 0 config_reset_none
	ClearErrors
	ReadINIStr $0 "$INSTDIR\obs-studio\global.ini" "OBSPT" "BootstrapVersion"
	${If} ${Errors}
		Push 1
		Return
	${EndIf}
	${If} $0 == ""
		Push 1
		Return
	${EndIf}
	${If} $0 < ${CONFIG_GENERATION}
		Push 1
	${Else}
		Push 0
	${EndIf}
	Return

config_reset_none:
	Push 0
FunctionEnd

Function ConfigPageCreate
	Call NeedsConfigReset
	Pop $0
	${If} $0 == 0
		Abort
	${EndIf}

	!insertmacro MUI_HEADER_TEXT "$(OBSPT_ResetTitle)" "$(OBSPT_ResetSub)"

	nsDialogs::Create 1018
	Pop $0
	${If} $0 == error
		Abort
	${EndIf}

	${NSD_CreateLabel} 0 0 100% 54u "$(OBSPT_ResetBody1)"
	Pop $0

	${NSD_CreateLabel} 0 68u 100% 58u "$(OBSPT_ResetBody2)"
	Pop $0

	nsDialogs::Show
FunctionEnd

Function ConfigPageLeave
	MessageBox MB_YESNO|MB_ICONEXCLAMATION "$(OBSPT_ConfirmOverwriteMsg)" IDYES confirm_config_overwrite
	Abort
confirm_config_overwrite:
FunctionEnd
Function ForceResetPageCreate
	; Force-reset is only offered on an existing, current-generation User
	; Data Root (upgrade/repair). Fresh installs and legacy resets use their
	; own flows: fresh simply deploys templates; legacy uses ConfigPageCreate.
	IfFileExists "$INSTDIR\obs-studio" 0 force_reset_page_skip
	Call NeedsConfigReset
	Pop $0
	${If} $0 == 1
		Goto force_reset_page_skip
	${EndIf}

	!insertmacro MUI_HEADER_TEXT "$(OBSPT_FRTitle)" "$(OBSPT_FRSub)"

	nsDialogs::Create 1018
	Pop $0
	${If} $0 == error
		Abort
	${EndIf}

	${NSD_CreateLabel} 0 0 100% 42u "$(OBSPT_FRBody)"
	Pop $0

	${NSD_CreateCheckBox} 0 50u 100% 12u "$(OBSPT_FRCheckbox)"
	Pop $0
	StrCpy $ForceResetCheckboxHwnd $0

	nsDialogs::Show
	Return

force_reset_page_skip:
	Abort
FunctionEnd

Function ForceResetPageLeave
	Push $0
	${NSD_GetState} $ForceResetCheckboxHwnd $0
	${If} $0 == ${BST_CHECKED}
		MessageBox MB_YESNO|MB_ICONEXCLAMATION "$(OBSPT_FRConfirmMsg)" IDYES force_reset_confirmed
		Abort
force_reset_confirmed:
		StrCpy $ForceResetChecked 1
	${EndIf}
	Pop $0
FunctionEnd


Function LaunchOBS
	SetOutPath "$INSTDIR\bin"
	Exec '"$INSTDIR\${EXE_PATH}"'
FunctionEnd

Function IsOBSRunning
	nsExec::ExecToStack '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -Command "if (Get-Process -Name OBS-PT -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"'
	Pop $0
	Pop $1

	${If} $0 == 0
		Push 1
	${ElseIf} $0 == 1
		Push 0
	${Else}
		DetailPrint "OBS-PT process detection failed: $0 $1"
		Push 2
	${EndIf}
FunctionEnd

Function RequireOBSClosed
require_obs_closed_retry:
	Call IsOBSRunning
	Pop $0
	${If} $0 == 0
		Return
	${ElseIf} $0 == 1
		MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "$(OBSPT_OBSRunningMsg)" IDRETRY require_obs_closed_retry
		Quit
	${Else}
		MessageBox MB_RETRYCANCEL|MB_ICONSTOP "$(OBSPT_OBSRunningUnknownMsg)" IDRETRY require_obs_closed_retry
		Quit
	${EndIf}
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

	Call NeedsConfigReset
	Pop $0
	${If} $0 == 0
		Return
	${EndIf}

	Call ReserveConfigBackupDir
	Rename "$INSTDIR\obs-studio" "$ConfigBackupDir"
	IfErrors 0 prepare_existing_config_moved
		MessageBox MB_OK|MB_ICONSTOP "$(OBSPT_MoveFailedMsg)"
		Abort

prepare_existing_config_moved:
	StrCpy $ConfigBackupCreated 1

FunctionEnd

Function FinalizeExistingConfig
	${If} $ConfigBackupCreated != 1
		Return
	${EndIf}

	InitPluginsDir
	File /oname=$PLUGINSDIR\obspt-preserve-config.ps1 "obspt-preserve-config.ps1"
	DetailPrint "Merging preserved OBS-PT configuration values"
	StrCpy $1 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PLUGINSDIR\obspt-preserve-config.ps1" "$ConfigBackupDir" "$INSTDIR\obs-studio"'
	nsExec::ExecToLog '$1'
	Pop $0
	${If} $0 == 0
		Goto preserve_config_merged
	${EndIf}

	; A transient failure (e.g. antivirus scanning freshly extracted files)
	; deserves one retry before we bother the user.
	Sleep 2000
	DetailPrint "Retrying the configuration merge"
	nsExec::ExecToLog '$1'
	Pop $0
	${If} $0 == 0
		Goto preserve_config_merged
	${EndIf}

	MessageBox MB_OK|MB_ICONEXCLAMATION "$(OBSPT_MergeFailedMsg)"
	Return

preserve_config_merged:
	RMDir /r "$ConfigBackupDir"

FunctionEnd

; Issue 050: remove historical residue from mixed/dev installs so the flat
; layout stands alone. Strict allow-list - never touches the User Data Root
; (obs-studio) or the output folder (recordings).
; Virtual camera DirectShow filters are registered system-wide (HKLM), which
; requires elevation, while this installer itself stays user-level
; (RequestExecutionLevel user). Registration therefore runs as one elevated
; helper for both architectures: decline the UAC prompt and the filter simply
; stays unregistered (vcam button hidden), recoverable later via the shipped
; virtualcam-install.bat. The explicit SysWOW64/sysnative paths pin the child
; process architecture because NSIS is 32-bit and WOW64 redirection would
; otherwise pick the wrong regsvr32 for the 64-bit module.
Function RegisterVirtualCam
	; A missing module must not abort the pair: the two regsvr32 calls are
	; separated so a shipped-with-one-architecture package still registers
	; what it has. Chaining them with a conditional separator would silently
	; skip the 64-bit module whenever the 32-bit one is absent.
	ExecShell "runas" "$WINDIR\SysWOW64\cmd.exe" '/c "%WINDIR%\SysWOW64\regsvr32.exe" /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll" & "%WINDIR%\sysnative\regsvr32.exe" /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"' SW_HIDE
FunctionEnd

!macro OBSPT_UNREGISTER_VIRTUALCAM
	ExecShell "runas" "$WINDIR\SysWOW64\cmd.exe" '/c "%WINDIR%\SysWOW64\regsvr32.exe" /u /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll" & "%WINDIR%\sysnative\regsvr32.exe" /u /s "$INSTDIR\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll"' SW_HIDE
	; ExecShell is asynchronous; give the elevated regsvr32 a moment to
	; release the DLLs before the uninstaller deletes them.
	Sleep 1500
!macroend

Function un.UnregisterVirtualCam
	!insertmacro OBSPT_UNREGISTER_VIRTUALCAM
FunctionEnd

Function CleanupLegacyResidue
	; old arch-suffixed plugin folder from pre-flat installs
	RMDir /r "$INSTDIR\obs-plugins\64bit"
	; legacy top-level executable folder from an old dev install
	RMDir /r "$INSTDIR\64bit"
	; legacy arch layer under bin from pre-flat installs
	RMDir /r "$INSTDIR\bin\64bit"
	; issue 057: legacy AMD encoder plugin replaced by texture-amf
	Delete "$INSTDIR\obs-plugins\enc-amf.dll"
	RMDir /r "$INSTDIR\data\obs-plugins\enc-amf"
	; source-tree debris from an old dev install at the Install Root
	RMDir /r "$INSTDIR\libobs"
	RMDir /r "$INSTDIR\media-io"
	RMDir /r "$INSTDIR\audio-monitoring"
	RMDir /r "$INSTDIR\callback"
	RMDir /r "$INSTDIR\graphics"
	RMDir /r "$INSTDIR\obs-scripting"
	RMDir /r "$INSTDIR\cmake"
	RMDir /r "$INSTDIR\include"
	Delete "$INSTDIR\obs-*.h"
	Delete "$INSTDIR\obs.hpp"
	; note: obs-studio and recordings are deliberately NOT listed here
FunctionEnd

Function ReportLockedVirtualCam
	; ADR-0006: browsers keep the virtual camera filter DLLs mapped for their
	; whole lifetime, so an upgrade can find them locked. The payload copy
	; runs with SetOverwrite try and simply skips such a file; this probe only
	; reports which module kept its previous version, so an out-of-date
	; filter is explainable. No prompt, no process is touched.
	StrCpy $VcamLocked ""
	StrCpy $R0 "obs-virtualcam-module32.dll"
	Call ProbeLockedVirtualCam
	StrCmp $VcamLocked "" 0 vcam_probe_report
	StrCpy $R0 "obs-virtualcam-module64.dll"
	Call ProbeLockedVirtualCam
vcam_probe_report:
	StrCmp $VcamLocked "" vcam_probe_done
	DetailPrint "The installed virtual camera filter is in use by another application, so it kept its previous version ($VcamLocked). Close that application and run Setup again to update it; OBS-PT itself was updated."
vcam_probe_done:
FunctionEnd

Function ProbeLockedVirtualCam
	; $R0 = module file name to probe; sets $VcamLocked when it is unwritable.
	; Absence is normal (fresh install) and is not reported.
	IfFileExists "$INSTDIR\data\obs-plugins\win-dshow\$R0" 0 vcam_probe_right
	ClearErrors
	FileOpen $0 "$INSTDIR\data\obs-plugins\win-dshow\$R0" a
	IfErrors vcam_probe_locked vcam_probe_release
vcam_probe_release:
	FileClose $0
	Goto vcam_probe_right
vcam_probe_locked:
	StrCpy $VcamLocked "$R0"
vcam_probe_right:
FunctionEnd

; Issue 075 (ADR 0009): record whether the application must perform the
; installer-driven default replacement on its next launch.
;
; PENDING when this build upgrades a tree whose recorded product version is
; older; DONE for a fresh install, a same-version reinstall, or a downgrade
; over a newer build. Read the recorded version BEFORE DisplayVersion is
; rewritten below.
Function MarkInstallerMigration
	StrCpy $MigrationValue "${MIGRATION_DONE}"

	ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "DisplayVersion"
	${If} $0 != ""
		; VersionCompare yields 2 when the second operand is newer.
		${VersionCompare} $0 "${MIGRATION_RELEASE}" $1
		${If} $1 == 2
			StrCpy $MigrationValue "${MIGRATION_PENDING}"
		${EndIf}
	${EndIf}

	DetailPrint "[OBSPT] InstallerMigrationVersion = $MigrationValue"

	; The edit must preserve the file's BOM, CRLF/LF style and the
	; 528-character DockState byte for byte, which NSIS cannot do safely.
	InitPluginsDir
	File /oname=$PLUGINSDIR\obspt-mark-installer-migration.ps1 "obspt-mark-installer-migration.ps1"
	nsExec::ExecToLog '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\obspt-mark-installer-migration.ps1" -IniPath "$INSTDIR\obs-studio\global.ini" -Value $MigrationValue'
	Pop $0
	${If} $0 != 0
		; Never fatal: the marker only decides whether one default
		; replacement happens; Setup continues regardless (ADR-0006
		; posture: no prompt, no failure escalation).
		DetailPrint "Could not record the installer default replacement marker ($0); all other installation steps are unaffected."
	${EndIf}
FunctionEnd

Section "OBS-PT" SecMain
	Call RequireOBSClosed
	Call ReportLockedVirtualCam
	Call PrepareExistingConfig

	; ADR-0006: only the two virtual camera module DLLs are expected to be
	; locked (a browser has them mapped); everything else should be free
	; after RequireOBSClosed. `try` keeps Setup progressing instead of
	; raising the built-in overwrite dialog for such a file.
	SetOverwrite try

	; Issue 047: explicit force-reset on an existing current-gen config.
	; Back the current User Data Root up (kept), then deploy fresh templates.
	${If} $ForceResetChecked != 0
		Call ReserveConfigBackupDir
		Rename "$INSTDIR\obs-studio" "$ConfigBackupDir"
		IfErrors 0 force_reset_backed_up
			MessageBox MB_OK|MB_ICONSTOP "$(OBSPT_MoveFailedMsg)"
			Abort
force_reset_backed_up:
		StrCpy $ForceResetBacked 1
	${EndIf}

	; Program files only - never the staged obs-studio templates on top of an
	; existing User Data Root (the root cause of "install swallowed my
	; settings"). obs-studio templates deploy separately below.
	SetOutPath "$INSTDIR"
	File /r "${SOURCE_DIR}\bin"
	File /r "${SOURCE_DIR}\data"
	File /r "${SOURCE_DIR}\obs-plugins"
	File /r "${SOURCE_DIR}\cmake"
	File /r "${SOURCE_DIR}\include"
	File /r "${SOURCE_DIR}\recordings"

	; Deploy obs-studio templates only when the User Data Root is absent
	; (fresh install, legacy reset which renamed it away, or force-reset).
	IfFileExists "$INSTDIR\obs-studio" 0 deploy_obs_studio_templates
	Goto after_config_deploy
deploy_obs_studio_templates:
	File /r "${SOURCE_DIR}\obs-studio"
after_config_deploy:

	; Legacy preserve-merge runs only when not doing a force-reset.
	${If} $ForceResetChecked == 0
		Call FinalizeExistingConfig
	${EndIf}

	; Issue 050: sweep legacy residue after files are in place.
	Call CleanupLegacyResidue

	; Issue 075: decide the migration marker while the OLD DisplayVersion is
	; still in the registry (WriteRegStr below overwrites it).
	Call MarkInstallerMigration

	SetOutPath "$INSTDIR"
	WriteUninstaller "$INSTDIR\uninstall.exe"

	Delete "$SMPROGRAMS\OBS-PT\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\Uninstall OBS-PT.lnk"
	RMDir "$SMPROGRAMS\OBS-PT"

	SetOutPath "$INSTDIR\bin"
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

	Call RegisterVirtualCam

	${If} $ForceResetBacked == 1
		MessageBox MB_OK|MB_ICONINFORMATION "$(OBSPT_FRDoneMsg)"
	${EndIf}
SectionEnd

Section "Uninstall"
	Call un.UnregisterVirtualCam

	Delete "$DESKTOP\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\OBS-PT.lnk"
	Delete "$SMPROGRAMS\OBS-PT\Uninstall OBS-PT.lnk"
	RMDir "$SMPROGRAMS\OBS-PT"

	DeleteRegKey HKCU "${UNINSTALL_KEY}"

	; Issue 050: never delete user data on uninstall. Remove only program
	; files; keep recordings\ (user output) and obs-studio\ (User Data Root).
	RMDir /r "$INSTDIR\bin"
	RMDir /r "$INSTDIR\data"
	RMDir /r "$INSTDIR\obs-plugins"
	RMDir /r "$INSTDIR\cmake"
	RMDir /r "$INSTDIR\include"
	Delete "$INSTDIR\uninstall.exe"
	; Remove the install root only if it is now empty (recordings/ and
	; obs-studio/ preserved).
	RMDir "$INSTDIR"
SectionEnd
