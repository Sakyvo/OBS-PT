/******************************************************************************
    OBS-PT bootstrap migration model tests (issue 059)

    Verifies the declared-repair decision model through its public
    interface: video-repair gating, encoder-repair gating, and the legacy
    AMD id migration table.
******************************************************************************/

#include <cstdio>
#include <cstring>
#include "obspt-bootstrap-model.h"

static int failures = 0;

#define CHECK(cond, ...)                                                     \
	do {                                                                 \
		if (cond) {                                                  \
			printf("  PASS: " __VA_ARGS__);                      \
			printf("\n");                                      \
		} else {                                                     \
			failures++;                                          \
			printf("  FAIL: %s:%d ", __FILE__, __LINE__);        \
			printf(__VA_ARGS__);                                 \
			printf("\n");                                      \
		}                                                            \
	} while (0)

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	printf("[video repair gating]\n");
	CHECK(obspt_bootstrap_needs_video_repair(0),
	      "pre-bootstrap tree (0) runs the video repair");
	CHECK(obspt_bootstrap_needs_video_repair(1),
	      "v1 tree runs the video repair");
	CHECK(obspt_bootstrap_needs_video_repair(2),
	      "v2 tree runs the video repair");
	CHECK(!obspt_bootstrap_needs_video_repair(3),
	      "v3 tree (1.1.0) does NOT re-run the video repair");
	CHECK(!obspt_bootstrap_needs_video_repair(4),
	      "v4 tree does not re-run the video repair");

	printf("[encoder repair gating]\n");
	CHECK(obspt_bootstrap_needs_encoder_repair(3, true),
	      "v3 completed tree gets the v4 encoder repair");
	CHECK(obspt_bootstrap_needs_encoder_repair(4, true),
	      "v4 completed tree gets the v5 HEVC-default recording repair");
	CHECK(obspt_bootstrap_needs_encoder_repair(5, true),
	      "v5 completed tree gets the v6 keyint/streaming repair");
	CHECK(!obspt_bootstrap_needs_encoder_repair(6, true),
	      "v6 completed tree does not re-run the encoder repair");
	CHECK(obspt_bootstrap_needs_encoder_repair(6, false),
	      "unfinished first-run always repairs");
	CHECK(obspt_bootstrap_needs_encoder_repair(0, false),
	      "fresh tree repairs");

	printf("[recording HEVC preference]\n");
	CHECK(strcmp(obspt_recording_hevc_preference(0), "jim_hevc_nvenc") == 0,
	      "NVIDIA HEVC is the first recording choice");
	CHECK(strcmp(obspt_recording_hevc_preference(1), "h265_texture_amf") == 0,
	      "AMD HEVC is the second recording choice");
	CHECK(obspt_recording_hevc_preference(2) == nullptr,
	      "the HEVC preference list is terminated");
	CHECK(obspt_recording_hevc_preference(-1) == nullptr,
	      "a negative index yields no encoder");

	printf("[defaults]\n");
	CHECK(obspt_recording_keyint_sec() == 0,
	      "recording keyframe interval defaults to 0 (encoder-chosen)");
	CHECK(obspt_streaming_bitrate_kbps() == 4500,
	      "streaming default bitrate is 4500 Kbps");

	printf("[installer-driven default replacement]\n");
	CHECK(obspt_installer_migration_needed(0),
	      "a missing marker (0) needs the replacement");
	CHECK(obspt_installer_migration_needed(-1),
	      "a negative marker needs the replacement");
	CHECK(!obspt_installer_migration_needed(1),
	      "a current marker does not re-run the replacement");
	CHECK(!obspt_installer_migration_needed(2),
	      "a newer marker (downgrade) does not run the replacement");

	printf("[legacy AMD id migration]\n");
	CHECK(obspt_amf_id_is_legacy("amd_amf_h264"),
	      "amd_amf_h264 is the legacy id");
	CHECK(!obspt_amf_id_is_legacy("h264_texture_amf"),
	      "h264_texture_amf is the new id");
	CHECK(!obspt_amf_id_is_legacy("jim_nvenc"),
	      "jim_nvenc is not migrated");
	CHECK(!obspt_amf_id_is_legacy("obs_x264"),
	      "obs_x264 is not migrated");
	CHECK(!obspt_amf_id_is_legacy(nullptr),
	      "NULL id is not migrated");
	CHECK(!obspt_amf_id_is_legacy(""),
	      "empty id is not migrated");

	printf("\n%s: %d failure(s)\n",
	       failures ? "FAILED" : "ALL PASSED", failures);
	return failures ? 1 : 0;
}
