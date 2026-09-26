/******************************************************************************
    OBS-PT service codec-pointer lifetime regression (post-1.2.0 crash)

    Reported symptom: creating a profile, then switching profiles, crashes
    OBS-PT 1.2.0 (D:\OBS-PT install, all three captured sessions).

    Root cause hypothesis (058-introduced): update_supported_codecs stored
    json_string_value pointers (jansson-owned) into char** and bfree'd them
    in free_codecs_impl, plus wrote the NULL terminator one slot past the
    bmemdup'd buffer -> heap corruption -> crash on the FIRST or SECOND
    service creation.

    Harness: loads rtmp-services.dll through the regular libobs module
    machinery (obs_add_module_path + obs_load_all_modules) so
    obs_current_module() is valid inside the plugin, then creates the same
    rtmp_common service five times and queries supported codecs each time.

    Run from rundir/<config>/bin so plugin DLLs and data resolve.
******************************************************************************/

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

#include <obs.h>

#define STEPF(...)                              \
	do {                                    \
		printf(__VA_ARGS__);            \
		printf("\n");                   \
		fflush(stdout);                 \
	} while (false)

static int g_failures = 0;

#define CHECK(cond, ...)                                            \
	do {                                                        \
		if (cond) {                                          \
			printf("  PASS: " __VA_ARGS__);              \
			printf("\n");                                \
			fflush(stdout);                              \
		} else {                                             \
			g_failures++;                                \
			printf("  FAIL: " __VA_ARGS__);            \
			printf("\n");                              \
			fflush(stdout);                            \
		}                                                   \
	} while (0)

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	/* libobs always scans the default module path "../obs-plugins"
	 * (relative to CWD; obs-windows.c add_default_module_paths), so the
	 * sandbox below swaps CWD to a temp tree containing ONLY the
	 * rtmp-services plugin. Other UI-facing plugins crash headless. */
	char exe[MAX_PATH];
	GetModuleFileNameA(nullptr, exe, MAX_PATH);
	std::string exe_dir(exe);
	exe_dir = exe_dir.substr(0, exe_dir.find_last_of('\\'));

	std::string root = std::string(getenv("TEMP")) + "\\obspt-service-reload";
	SetCurrentDirectoryA(std::string(root + "\\..").c_str()); /* ensure parent exists */
	(void)CreateDirectoryA(root.c_str(), nullptr);
	(void)CreateDirectoryA((root + "\\bin").c_str(), nullptr);
	(void)CreateDirectoryA((root + "\\obs-plugins").c_str(), nullptr);
	(void)CreateDirectoryA((root + "\\data").c_str(), nullptr);
	(void)CreateDirectoryA((root + "\\data\\obs-plugins").c_str(), nullptr);
	(void)CreateDirectoryA((root + "\\data\\obs-plugins\\rtmp-services").c_str(), nullptr);

	CopyFileA((exe_dir + "\\..\\obs-plugins\\rtmp-services.dll").c_str(),
		  (root + "\\obs-plugins\\rtmp-services.dll").c_str(), FALSE);
	CopyFileA((exe_dir + "\\..\\data\\obs-plugins\\rtmp-services\\services.json").c_str(),
		  (root + "\\data\\obs-plugins\\rtmp-services\\services.json").c_str(), FALSE);

	SetCurrentDirectoryA((root + "\\bin").c_str());

	if (!obs_startup("en-US", nullptr, nullptr)) {
		STEPF("FATAL: obs_startup failed");
		return 1;
	}

	STEPF("[step] load modules from sandbox");
	obs_load_all_modules();
	for (int round = 0; round < 5; round++) {
		STEPF("[round %d] create service", round);

		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "service", "Bilibili Live");
		obs_service_t *svc = obs_service_create(
			"rtmp_common", "test_service", settings, nullptr);
		obs_data_release(settings);
		CHECK(svc != nullptr, "service created");

		const char **codecs =
			svc ? obs_service_get_supported_video_codecs(svc)
			    : nullptr;
		STEPF("  codecs=%p first=%s", (const void *)codecs,
		      (codecs && codecs[0]) ? codecs[0] : "(null)");
		CHECK(codecs && codecs[0] && strcmp(codecs[0], "h264") == 0 &&
			      codecs[1] == nullptr,
		      "Bilibili reports h264-only codecs");

		STEPF("[round %d] release service", round);
		if (svc)
			obs_service_release(svc);
		STEPF("[round %d] survived", round);
	}

	obs_shutdown();
	printf("\n%s: %d failure(s)\n",
	       g_failures ? "FAILED" : "ALL PASSED", g_failures);
	return g_failures ? 1 : 0;
}
