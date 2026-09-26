/******************************************************************************
    OBS-PT real module-load smoke (issue 056)

    Loads the actual obs-ffmpeg.dll from the runtime layout, runs its
    obs_module_load, and asserts the NVENC registration gating on the
    current hardware:
      - jim_nvenc registered (H264)
      - jim_hevc_nvenc registered (HEVC, ENABLE_HEVC + deps ffmpeg)
      - jim_av1_nvenc registered only when the hardware probe reports
        AV1 support (obs-nvenc-test.exe must sit next to this exe)
      - ffmpeg_nvenc fallback registered

    Must run from rundir/<config>/bin. Headless: module load performs
    codec checks, library loads and a hidden subprocess probe only.
******************************************************************************/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

#include <obs.h>

static int g_failures = 0;

#define CHECK(cond, ...)                                            \
	do {                                                        \
		if (cond) {                                          \
			printf("  PASS: " __VA_ARGS__);              \
			printf("\n");                                \
		} else {                                             \
			g_failures++;                                  \
			printf("  FAIL: " __VA_ARGS__);              \
			printf("\n");                                \
		}                                                   \
	} while (0)

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	if (!obs_startup("en-US", nullptr, nullptr)) {
		printf("FATAL: obs_startup failed\n");
		return 1;
	}

	/* resolve obs-ffmpeg.dll relative to this exe (rundir/<cfg>/bin) */
	char self[MAX_PATH];
	GetModuleFileNameA(nullptr, self, MAX_PATH);
	std::string bin_dir(self);
	bin_dir = bin_dir.substr(0, bin_dir.find_last_of('\\'));
	std::string plugin_path =
		bin_dir + "\\..\\obs-plugins\\obs-ffmpeg.dll";

	HMODULE lib = LoadLibraryA(plugin_path.c_str());
	CHECK(lib != nullptr, "obs-ffmpeg.dll loaded (%s)",
	      plugin_path.c_str());
	if (!lib) {
		printf("FAILED: cannot load plugin\n");
		return 1;
	}

	using set_pointer_t = void (*)(obs_module_t *);
	using load_t = bool (*)(void);

	auto set_pointer =
		(set_pointer_t)GetProcAddress(lib, "obs_module_set_pointer");
	auto module_load = (load_t)GetProcAddress(lib, "obs_module_load");
	CHECK(set_pointer && module_load,
	      "module exports resolved");
	if (!set_pointer || !module_load) {
		return 1;
	}

	set_pointer(nullptr);
	bool loaded = module_load();
	CHECK(loaded, "obs_module_load succeeded");

	std::vector<std::string> encoders;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++)
		encoders.push_back(id);

	auto has = [&](const char *name) {
		return std::any_of(encoders.begin(), encoders.end(),
				   [&](const std::string &e) {
					   return e == name;
				   });
	};

	CHECK(has("jim_nvenc"), "jim_nvenc registered");
	CHECK(has("jim_hevc_nvenc"), "jim_hevc_nvenc registered");
	CHECK(has("ffmpeg_nvenc"), "ffmpeg_nvenc fallback registered");

	/* issue 057: legacy enc-amf retired; texture-amf registers nothing
	 * on a machine whose primary adapter is not AMD */
	CHECK(!has("amd_amf_h264"), "legacy amd_amf_h264 absent");
	CHECK(!has("h264_texture_amf") && !has("h265_texture_amf") &&
		      !has("av1_texture_amf"),
	      "texture-amf encoders absent without AMD hardware");

	/* run the hardware probe ourselves and assert registration matches */
	bool probe_present = false;
	bool probe_av1 = false;
	{
		std::string probe = bin_dir + "\\obs-nvenc-test.exe";
		HANDLE h = CreateFileA(probe.c_str(), GENERIC_READ,
				       FILE_SHARE_READ, nullptr,
				       OPEN_EXISTING, 0, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			CloseHandle(h);
			probe_present = true;
		}
		if (probe_present) {
			FILE *p = _popen(probe.c_str(), "r");
			if (p) {
				char line[256];
				while (fgets(line, sizeof(line), p)) {
					if (strstr(line, "supports_av1=true"))
						probe_av1 = true;
				}
				_pclose(p);
			}
		}
	}
	if (probe_present) {
		CHECK(has("jim_av1_nvenc") == probe_av1,
		      "jim_av1_nvenc registration matches hardware probe (registered=%d, probe=%d)",
		      (int)has("jim_av1_nvenc"), (int)probe_av1);
	} else {
		printf("  SKIP: obs-nvenc-test.exe not found next to this "
		       "exe; cannot assert AV1 gate\n");
	}

obs_shutdown();

	printf("\n%s: %d failure(s)\n",
	       g_failures ? "FAILED" : "ALL PASSED", g_failures);
	return g_failures ? 1 : 0;
}
