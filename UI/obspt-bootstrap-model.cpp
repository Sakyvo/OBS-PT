#include "obspt-bootstrap-model.h"

#include <string.h>

#define OBSPT_VIDEO_REPAIR_MAX_VERSION 3

bool obspt_bootstrap_needs_video_repair(int previous_bootstrap_version)
{
	return previous_bootstrap_version < OBSPT_VIDEO_REPAIR_MAX_VERSION;
}

bool obspt_bootstrap_needs_encoder_repair(int previous_bootstrap_version,
					  bool first_run_completed)
{
	return !first_run_completed ||
	       previous_bootstrap_version < OBSPT_BOOTSTRAP_VERSION;
}

bool obspt_amf_id_is_legacy(const char *encoder_id)
{
	return encoder_id && strcmp(encoder_id, "amd_amf_h264") == 0;
}

bool obspt_recording_default_is_hevc(void)
{
	return true;
}

const char *obspt_recording_hevc_preference(int index)
{
	static const char *const hevc_ids[] = {
		"jim_hevc_nvenc",
		"h265_texture_amf",
		nullptr,
	};

	return (index >= 0 && hevc_ids[index]) ? hevc_ids[index] : nullptr;
}

int obspt_recording_keyint_sec(void)
{
	return 0;
}

int obspt_streaming_bitrate_kbps(void)
{
	return 4500;
}

bool obspt_installer_migration_needed(int stored_version)
{
	/* A negative or missing marker (both read as 0 or below) needs the
	 * replacement; an equal or newer one has already had it. */
	return stored_version < OBSPT_INSTALLER_MIGRATION_VERSION;
}
