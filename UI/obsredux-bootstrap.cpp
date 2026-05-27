#include "obsredux-bootstrap.h"
#include "obs-app.hpp"

#include <obs.h>
#include <util/base.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <util/config-file.h>

#include <obs-data.h>

#include <QApplication>
#include <QDesktopServices>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QUrl>

#include <ctime>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#endif

static bool path_under_system_protected_dir(const char *path)
{
#ifdef _WIN32
	if (!path)
		return false;

	wchar_t prog_files[MAX_PATH] = {0};
	wchar_t prog_files_x86[MAX_PATH] = {0};
	wchar_t windir[MAX_PATH] = {0};

	DWORD pf = GetEnvironmentVariableW(L"ProgramFiles", prog_files,
					   MAX_PATH);
	DWORD pf86 = GetEnvironmentVariableW(L"ProgramFiles(x86)",
					     prog_files_x86, MAX_PATH);
	UINT wd = GetWindowsDirectoryW(windir, MAX_PATH);

	wchar_t w_path[1024] = {0};
	if (os_utf8_to_wcs(path, 0, w_path, sizeof(w_path) / sizeof(wchar_t)) ==
	    0)
		return false;

	auto starts_with = [&](const wchar_t *prefix, size_t prefix_len) {
		if (prefix_len == 0)
			return false;
		size_t plen = wcslen(w_path);
		if (plen < prefix_len)
			return false;
		return _wcsnicmp(w_path, prefix, prefix_len) == 0;
	};

	if (pf > 0 && starts_with(prog_files, pf))
		return true;
	if (pf86 > 0 && starts_with(prog_files_x86, pf86))
		return true;
	if (wd > 0 && starts_with(windir, wd))
		return true;
	return false;
#else
	(void)path;
	return false;
#endif
}

write_probe_result_t probe_install_root_writable(const char *install_root)
{
#ifdef _WIN32
	if (!install_root || !*install_root)
		return WRITE_PROBE_UNKNOWN;

	GUID guid;
	if (CoCreateGuid(&guid) != S_OK)
		return WRITE_PROBE_UNKNOWN;

	wchar_t guid_str[64] = {0};
	StringFromGUID2(guid, guid_str, 64);

	struct dstr probe_path;
	dstr_init(&probe_path);
	dstr_copy(&probe_path, install_root);
	dstr_replace(&probe_path, "\\", "/");
	if (probe_path.len > 0 &&
	    probe_path.array[probe_path.len - 1] != '/')
		dstr_cat_ch(&probe_path, '/');
	dstr_cat(&probe_path, ".write_probe_");

	char guid_mbs[64] = {0};
	os_wcs_to_utf8(guid_str, 0, guid_mbs, sizeof(guid_mbs));
	for (char *p = guid_mbs; *p; ++p) {
		if (*p == '{' || *p == '}')
			*p = '_';
	}
	dstr_cat(&probe_path, guid_mbs);
	dstr_cat(&probe_path, ".tmp");

	wchar_t w_path[1024] = {0};
	os_utf8_to_wcs(probe_path.array, 0, w_path,
		       sizeof(w_path) / sizeof(wchar_t));

	HANDLE h = CreateFileW(w_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			       FILE_ATTRIBUTE_TEMPORARY, NULL);
	DWORD err = (h == INVALID_HANDLE_VALUE) ? GetLastError() : 0;

	if (h != INVALID_HANDLE_VALUE) {
		CloseHandle(h);
		DeleteFileW(w_path);
		dstr_free(&probe_path);
		return WRITE_PROBE_OK;
	}

	dstr_free(&probe_path);

	switch (err) {
	case ERROR_ACCESS_DENIED:
		return path_under_system_protected_dir(install_root)
			       ? WRITE_PROBE_SYSTEM_PROTECTED
			       : WRITE_PROBE_UNKNOWN;
	case ERROR_WRITE_PROTECT:
		return WRITE_PROBE_READ_ONLY_VOLUME;
	case ERROR_VIRUS_INFECTED:
	case ERROR_VIRUS_DELETED:
		return WRITE_PROBE_ANTIVIRUS_BLOCKED;
	default:
		return WRITE_PROBE_UNKNOWN;
	}
#else
	(void)install_root;
	return WRITE_PROBE_OK;
#endif
}

void ShowWritePermissionFailureDialog(write_probe_result_t reason)
{
	(void)reason;
	QMessageBox::critical(nullptr,
			      QTStr("OBSRedux.WriteProbeFailure.Title"),
			      QTStr("OBSRedux.WriteProbeFailure.Body"),
			      QMessageBox::Ok);
}

static const char *POTPVP_PROFILE = "PotPvP";

static bool g_late_bootstrap_needed = false;
static bool g_show_first_run_dialog = false;
static bool g_bootstrap_version_pending = false;

static bool is_dir(const char *path)
{
	os_dir_t *dir = os_opendir(path);
	if (!dir)
		return false;
	os_closedir(dir);
	return true;
}

static void copy_path(char *dst, size_t size, const char *path)
{
	if (!dst || size == 0)
		return;
	snprintf(dst, size, "%s", path ? path : "");
}

static bool validate_ini_file(const char *rel_path, char *failed_path,
			      size_t failed_path_size)
{
	char abs_path[512];
	get_portable_path(abs_path, sizeof(abs_path), rel_path);

	if (!os_file_exists(abs_path)) {
		copy_path(failed_path, failed_path_size, abs_path);
		return false;
	}

	config_t *cfg = nullptr;
	if (config_open(&cfg, abs_path, CONFIG_OPEN_EXISTING) !=
	    CONFIG_SUCCESS) {
		copy_path(failed_path, failed_path_size, abs_path);
		return false;
	}

	config_close(cfg);
	return true;
}

static bool validate_json_file(const char *rel_path, char *failed_path,
			       size_t failed_path_size)
{
	char abs_path[512];
	get_portable_path(abs_path, sizeof(abs_path), rel_path);

	if (!os_file_exists(abs_path)) {
		copy_path(failed_path, failed_path_size, abs_path);
		return false;
	}

	obs_data_t *data = obs_data_create_from_json_file(abs_path);
	if (!data) {
		copy_path(failed_path, failed_path_size, abs_path);
		return false;
	}

	obs_data_release(data);
	return true;
}

static bool validate_preset_files(char *failed_path,
				  size_t failed_path_size)
{
	if (!validate_ini_file("obs-studio/global.ini", failed_path,
			       failed_path_size))
		return false;
	if (!validate_ini_file(
		    "obs-studio/basic/profiles/PotPvP/basic.ini", failed_path,
		    failed_path_size))
		return false;
	if (!validate_json_file(
		    "obs-studio/basic/profiles/PotPvP/recordEncoder.json",
		    failed_path, failed_path_size))
		return false;
	if (!validate_json_file(
		    "obs-studio/basic/profiles/PotPvP/streamEncoder.json",
		    failed_path, failed_path_size))
		return false;
	if (!validate_json_file(
		    "obs-studio/basic/profiles/PotPvP/service.json",
		    failed_path, failed_path_size))
		return false;
	if (!validate_json_file("obs-studio/basic/scenes/PotPvP.json",
				failed_path, failed_path_size))
		return false;

	return true;
}

bool validate_obsredux_preset_files(char *failed_path,
				    size_t failed_path_size)
{
	return validate_preset_files(failed_path, failed_path_size);
}

void ShowPresetIntegrityFailureDialog(const char *failed_path)
{
	QString body = QTStr("OBSRedux.PresetFailure.Body")
			       .arg(QString::fromUtf8(failed_path ? failed_path
								   : ""));

	QMessageBox mb(QMessageBox::Critical,
		       QTStr("OBSRedux.PresetFailure.Title"), body,
		       QMessageBox::NoButton, nullptr);
	QPushButton *openButton = mb.addButton(
		QTStr("OBSRedux.PresetFailure.OpenInstallRoot"),
		QMessageBox::ActionRole);
	QPushButton *exitButton = mb.addButton(
		QTStr("OBSRedux.PresetFailure.Exit"),
		QMessageBox::RejectRole);
	mb.setDefaultButton(exitButton);

	mb.exec();

	if (mb.clickedButton() == openButton) {
		char install_root[512];
		get_portable_path(install_root, sizeof(install_root), "");
		QDesktopServices::openUrl(
			QUrl::fromLocalFile(QString::fromUtf8(install_root)));
	}
}

void prepare_obsredux_global_config(config_t *global_config)
{
	if (!global_config)
		return;

	bool first_run_completed =
		config_get_bool(global_config, "OBSRedux", "FirstRunCompleted");
	int bootstrap_version =
		(int)config_get_int(global_config, "OBSRedux", "BootstrapVersion");
	bool needs_repair =
		!first_run_completed ||
		bootstrap_version < OBSREDUX_BOOTSTRAP_VERSION;

	g_late_bootstrap_needed = needs_repair;
	g_show_first_run_dialog = !first_run_completed;
	g_bootstrap_version_pending = needs_repair;

	if (needs_repair) {
		config_set_string(global_config, "Basic", "Profile",
				  POTPVP_PROFILE);
		config_set_string(global_config, "Basic", "ProfileDir",
				  POTPVP_PROFILE);
		config_set_string(global_config, "Basic", "SceneCollection",
				  POTPVP_PROFILE);
		config_set_string(global_config, "Basic",
				  "SceneCollectionFile", POTPVP_PROFILE);
	}

	if (needs_repair && config_has_user_value(global_config, "General",
						 "Language")) {
		const char *lang = config_get_string(global_config, "General",
						     "Language");
		if (lang && strcmp(lang, "en-US") == 0)
			config_remove_value(global_config, "General",
					    "Language");
	}

	if (needs_repair) {
		config_set_bool(global_config, "General", "FirstRun", true);
		config_set_int(global_config, "General", "LastVersion",
			       LIBOBS_API_VER);
		config_set_bool(global_config, "General", "EnableAutoUpdates",
				false);
		config_save_safe(global_config, "tmp", nullptr);
	}
}

bool run_obsredux_early_bootstrap(config_t *global_config)
{
	char failed_path[512] = {0};

	if (!validate_preset_files(failed_path, sizeof(failed_path))) {
		ShowPresetIntegrityFailureDialog(failed_path);
		return false;
	}

	char recordings_path[512];
	get_portable_path(recordings_path, sizeof(recordings_path),
			  "recordings");
	if (os_mkdirs(recordings_path) == MKDIR_ERROR &&
	    !is_dir(recordings_path)) {
		ShowPresetIntegrityFailureDialog(recordings_path);
		return false;
	}

	if (global_config && g_bootstrap_version_pending) {
		config_set_int(global_config, "OBSRedux", "BootstrapVersion",
			       OBSREDUX_BOOTSTRAP_VERSION);
		if (config_save_safe(global_config, "tmp", nullptr) ==
		    CONFIG_SUCCESS)
			g_bootstrap_version_pending = false;
	}

	return true;
}

static bool encoder_registered(const char *id)
{
	const char *val = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &val); ++i) {
		if (val && strcmp(val, id) == 0)
			return true;
	}
	return false;
}

static const char *kEncoderPriority[] = {
	"jim_nvenc",   "obs_qsv11",    "ffmpeg_nvenc",
	"amd_amf_h264", "obs_x264",    nullptr,
};

encoder_probe_result_t probe_record_encoder(void)
{
	encoder_probe_result_t r{};
	for (int i = 0; kEncoderPriority[i]; ++i) {
		if (encoder_registered(kEncoderPriority[i])) {
			r.encoder_id = kEncoderPriority[i];
			r.is_software_fallback =
				strcmp(kEncoderPriority[i], "obs_x264") == 0;
			return r;
		}
	}
	r.encoder_id = "obs_x264";
	r.is_software_fallback = true;
	return r;
}

static int64_t default_get_current_time_sec(void)
{
	return (int64_t)time(NULL);
}

static int default_list_files(const char *dir, const char *glob_pat,
			      char ***out, size_t *count)
{
	*out = NULL;
	*count = 0;
	if (!dir || !glob_pat)
		return -1;

	struct dstr pattern;
	dstr_init(&pattern);
	dstr_copy(&pattern, dir);
	if (pattern.len > 0 &&
	    pattern.array[pattern.len - 1] != '/' &&
	    pattern.array[pattern.len - 1] != '\\')
		dstr_cat_ch(&pattern, '/');
	dstr_cat(&pattern, glob_pat);

	os_glob_t *g = NULL;
	int ret = os_glob(pattern.array, 0, &g);
	dstr_free(&pattern);
	if (ret != 0 || !g)
		return 0;

	if (g->gl_pathc == 0) {
		os_globfree(g);
		return 0;
	}

	char **arr = (char **)bzalloc(sizeof(char *) * g->gl_pathc);
	size_t n = 0;
	for (size_t i = 0; i < g->gl_pathc; ++i) {
		if (g->gl_pathv[i].directory)
			continue;
		arr[n++] = bstrdup(g->gl_pathv[i].path);
	}
	os_globfree(g);
	*out = arr;
	*count = n;
	return 0;
}

static int default_get_mtime_sec(const char *path, int64_t *out)
{
	struct stat st = {0};
	if (os_stat(path, &st) != 0)
		return -1;
	*out = (int64_t)st.st_mtime;
	return 0;
}

static int default_unlink_file(const char *path)
{
	return os_unlink(path);
}

int sweep_retention(const char *dir, int retention_days, const char *glob_pat,
		    const sweep_deps_t *deps)
{
	if (!dir || retention_days <= 0 || !glob_pat)
		return 0;

	sweep_deps_t d;
	if (deps) {
		d = *deps;
	} else {
		d.get_current_time_sec = default_get_current_time_sec;
		d.list_files = default_list_files;
		d.get_mtime_sec = default_get_mtime_sec;
		d.unlink_file = default_unlink_file;
	}

	if (!os_file_exists(dir))
		return 0;

	char **files = NULL;
	size_t count = 0;
	if (d.list_files(dir, glob_pat, &files, &count) != 0 || count == 0) {
		if (files) {
			for (size_t i = 0; i < count; ++i)
				bfree(files[i]);
			bfree(files);
		}
		return 0;
	}

	const int64_t now = d.get_current_time_sec();
	const int64_t threshold = (int64_t)retention_days * 86400;
	int deleted = 0;

	for (size_t i = 0; i < count; ++i) {
		int64_t mtime = 0;
		if (d.get_mtime_sec(files[i], &mtime) != 0) {
			bfree(files[i]);
			continue;
		}
		if (now - mtime > threshold) {
			if (d.unlink_file(files[i]) == 0) {
				++deleted;
			} else {
				blog(LOG_WARNING,
				     "sweep_retention: failed to unlink %s",
				     files[i]);
			}
		}
		bfree(files[i]);
	}
	bfree(files);
	return deleted;
}

bool is_first_run(void)
{
	char path[512];
	get_portable_path(path, sizeof(path), "obs-studio/global.ini");
	config_t *cfg = nullptr;
	if (config_open(&cfg, path, CONFIG_OPEN_EXISTING) != CONFIG_SUCCESS)
		return true;
	bool completed =
		config_get_bool(cfg, "OBSRedux", "FirstRunCompleted");
	config_close(cfg);
	return !completed;
}

void apply_record_path_to_config(config_t *cfg, const char *abs_path)
{
	if (!cfg || !abs_path || !*abs_path)
		return;

	config_set_string(cfg, "AdvOut", "RecFilePath", abs_path);
	config_set_string(cfg, "SimpleOutput", "FilePath", abs_path);
}

int apply_encoder_to_profile(const char *profile_name, const char *encoder_id)
{
	char rel_path[512];
	snprintf(rel_path, sizeof(rel_path),
		 "obs-studio/basic/profiles/%s/recordEncoder.json",
		 profile_name);
	char abs_path[512];
	get_portable_path(abs_path, sizeof(abs_path), rel_path);

	obs_data_t *root = obs_data_create_from_json_file(abs_path);
	if (!root) {
		blog(LOG_WARNING,
		     "[OBSRedux] recordEncoder.json not found or invalid: %s",
		     abs_path);
		return -1;
	}
	obs_data_set_string(root, "encoder", encoder_id);
	bool ok = obs_data_save_json(root, abs_path);
	obs_data_release(root);
	return ok ? 0 : -1;
}

int apply_record_path_to_profile(const char *profile_name,
				  const char *abs_path)
{
	char rel_path[512];
	snprintf(rel_path, sizeof(rel_path),
		 "obs-studio/basic/profiles/%s/basic.ini", profile_name);
	char ini_path[512];
	get_portable_path(ini_path, sizeof(ini_path), rel_path);

	config_t *cfg = nullptr;
	if (config_open(&cfg, ini_path, CONFIG_OPEN_EXISTING) != CONFIG_SUCCESS) {
		blog(LOG_WARNING, "[OBSRedux] basic.ini not found: %s",
		     ini_path);
		return -1;
	}
	apply_record_path_to_config(cfg, abs_path);
	config_save_safe(cfg, "tmp", nullptr);
	config_close(cfg);
	return 0;
}

void mark_first_run_completed(void)
{
	config_t *global_config = qApp ? GetGlobalConfig() : nullptr;
	if (global_config) {
		config_set_bool(global_config, "OBSRedux", "FirstRunCompleted",
				true);
		config_set_int(global_config, "OBSRedux", "BootstrapVersion",
			       OBSREDUX_BOOTSTRAP_VERSION);
		config_save_safe(global_config, "tmp", nullptr);
		return;
	}

	char path[512];
	get_portable_path(path, sizeof(path), "obs-studio/global.ini");
	config_t *cfg = nullptr;
	if (config_open(&cfg, path, CONFIG_OPEN_ALWAYS) != CONFIG_SUCCESS)
		return;
	config_set_bool(cfg, "OBSRedux", "FirstRunCompleted", true);
	config_set_int(cfg, "OBSRedux", "BootstrapVersion",
		       OBSREDUX_BOOTSTRAP_VERSION);
	config_save_safe(cfg, "tmp", nullptr);
	config_close(cfg);
}

void ShowFirstRunRecommendationsDialog(bool is_software_encoder)
{
	char recordings_path[512];
	get_portable_path(recordings_path, sizeof(recordings_path), "recordings");
	QString body = QTStr("OBSRedux.FirstRun.Body")
			       .arg(QString::fromUtf8(recordings_path));
	if (is_software_encoder)
		body += "\n\n" +
			QTStr("OBSRedux.FirstRun.SoftwareEncoderWarning");
	QMessageBox::information(nullptr, QTStr("OBSRedux.FirstRun.Title"),
				 body, QTStr("OBSRedux.FirstRun.Start"));
}

void run_first_run_bootstrap_if_needed(const char *active_profile_name,
				       config_t *active_config)
{
	bool first_run = is_first_run();
	if (!first_run && !g_late_bootstrap_needed)
		return;

	if (!active_profile_name || !*active_profile_name)
		active_profile_name = POTPVP_PROFILE;

	encoder_probe_result_t enc = probe_record_encoder();
	apply_encoder_to_profile(active_profile_name, enc.encoder_id);

	char recordings_path[512];
	get_portable_path(recordings_path, sizeof(recordings_path), "recordings");
	apply_record_path_to_profile(active_profile_name, recordings_path);
	apply_record_path_to_config(active_config, recordings_path);
	if (active_config)
		config_save_safe(active_config, "tmp", nullptr);

	if (first_run || g_show_first_run_dialog)
		ShowFirstRunRecommendationsDialog(enc.is_software_fallback);

	mark_first_run_completed();
	g_late_bootstrap_needed = false;
	g_show_first_run_dialog = false;
}
