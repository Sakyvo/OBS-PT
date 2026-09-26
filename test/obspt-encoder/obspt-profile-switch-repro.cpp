/******************************************************************************
    OBS-PT profile-switch HEVC crash repro (1.2.0 regression #1)

    Simulates the exact ChangeProfile -> ResetProfileData sequence with a
    jim_hevc_nvenc recording encoder and empty recordEncoder.json settings:

      obs_reset_video        (ResetVideo)
      destroy old encoder    (outputHandler.reset())
      create new encoder     (AdvancedOutput ctor)   <-- crash observed live

    Must run from rundir/<config>/bin (CWD-based DLL lookup).
    Every step flushes; on a crash the last printed step pinpoints the
    failing call.
******************************************************************************/

#include <cstdio>
#include <string>

#include <windows.h>

#include <obs.h>

#define STEPF(...)                              \
	do {                                    \
		printf(__VA_ARGS__);            \
		printf("\n");                   \
		fflush(stdout);                 \
	} while (false)

static bool load_plugins(void)
{
	char self[MAX_PATH];
	GetModuleFileNameA(nullptr, self, MAX_PATH);
	std::string bin_dir(self);
	bin_dir = bin_dir.substr(0, bin_dir.find_last_of('\\'));
	std::string plugin_path =
		bin_dir + "\\..\\obs-plugins\\obs-ffmpeg.dll";

	HMODULE lib = LoadLibraryA(plugin_path.c_str());
	if (!lib) {
		STEPF("FATAL: LoadLibrary failed for %s",
		      plugin_path.c_str());
		return false;
	}

	using set_pointer_t = void (*)(obs_module_t *);
	using load_t = bool (*)(void);

	auto set_pointer =
		(set_pointer_t)GetProcAddress(lib, "obs_module_set_pointer");
	auto module_load = (load_t)GetProcAddress(lib, "obs_module_load");
	if (!set_pointer || !module_load) {
		STEPF("FATAL: module exports missing");
		return false;
	}

	set_pointer(nullptr);
	if (!module_load()) {
		STEPF("FATAL: obs_module_load failed");
		return false;
	}
	return true;
}

static void print_video_state(const char *when)
{
	STEPF("  [%s] nv12_tex_active=%d", when,
	      obs_nv12_tex_active() ? 1 : 0);
}

static int reset_video(void)
{
	struct obs_video_info ovi = {};
	ovi.adapter = 0;
	ovi.graphics_module = "libobs-d3d11.dll";
	ovi.fps_num = 480;
	ovi.fps_den = 1;
	ovi.base_width = 1920;
	ovi.base_height = 1080;
	ovi.output_width = 1920;
	ovi.output_height = 1080;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BILINEAR;
	return obs_reset_video(&ovi);
}

static obs_encoder_t *create_hevc(const char *name)
{
	/* `{}` settings == the empty recordEncoder.json on the user's
	 * crashing profile */
	return obs_video_encoder_create("jim_hevc_nvenc", name, nullptr,
					nullptr);
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	STEPF("[step] obs_startup");
	if (!obs_startup("en-US", nullptr, nullptr)) {
		STEPF("FATAL: obs_startup failed");
		return 1;
	}

	STEPF("[step] load obs-ffmpeg.dll + module_load");
	if (!load_plugins())
		return 1;

	/* initial video bring-up (program start) */
	STEPF("[step] reset video (startup)");
	int r = reset_video();
	STEPF("    reset ret=%d", r);
	print_video_state("startup");

	STEPF("[step] create HEVC encoder A (initial profile load)");
	obs_encoder_t *encA = create_hevc("profA");
	STEPF("    encoder A = %p", (void *)encA);

	/* --- profile switch: video reset happens with A still alive --- */
	STEPF("[step] reset video again (profile switch ResetVideo)");
	r = reset_video();
	STEPF("    reset ret=%d", r);
	print_video_state("post-switch-reset");

	STEPF("[step] destroy old encoder A (outputHandler reset)");
	if (encA) {
		obs_encoder_release(encA);
	}
	STEPF("    old encoder destroyed");

	STEPF("[step] create HEVC encoder B (new AdvancedOutput ctor)");
	obs_encoder_t *encB = create_hevc("profB");
	STEPF("    encoder B = %p", (void *)encB);

	if (!encB) {
		STEPF("[RESULT] encoder create returned NULL on switch -> "
		      "AdvancedOutput ctor throws -> uncaught -> crash");
	} else {
		obs_encoder_release(encB);
		STEPF("[RESULT] create on switch succeeded");
	}

	obs_shutdown();
	STEPF("[done] PASS (process survived full switch cycle)");
	return encB ? 0 : 2;
}
