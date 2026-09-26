#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum obspt_nvenc_codec {
	OBSPT_NVENC_H264,
	OBSPT_NVENC_HEVC,
	OBSPT_NVENC_AV1,
};

/* OBS-PT throughput-tuned encoder defaults, shared by all NVENC codecs.
 * h264 keeps the values shipped since OBS-PT 1.0; hevc/av1 inherit the
 * same philosophy: speed preset, no multipass, no B-frames. */
struct obspt_nvenc_defaults {
	const char *preset2;
	const char *tune;
	const char *multipass;
	const char *rate_control;
	const char *profile;
	int cqp;
	int bf;
	bool psycho_aq;
	bool lookahead;
	bool repeat_headers;
};

const struct obspt_nvenc_defaults *
obspt_nvenc_defaults(enum obspt_nvenc_codec codec);

/* Bounded string-table lookups (case-insensitive, upstream semantics). */
bool obspt_nvenc_preset2_valid(const char *preset2);
const char *obspt_nvenc_preset2_fallback(void);
bool obspt_nvenc_tuning_valid(const char *tune);
const char *obspt_nvenc_tuning_fallback(void);
bool obspt_nvenc_multipass_valid(const char *multipass);
const char *obspt_nvenc_multipass_fallback(void);
const char *obspt_nvenc_profile_fallback(enum obspt_nvenc_codec codec);

/* Unified 0-51 CQP scale to per-codec NVENC QP. AV1 uses scale * 4 with
 * the result clamped to the 8-bit constQP range. */
int obspt_nvenc_map_cqp(enum obspt_nvenc_codec codec, int cqp);

#ifdef __cplusplus
}
#endif
