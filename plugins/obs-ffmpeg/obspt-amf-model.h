#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum obspt_amf_codec {
	OBSPT_AMF_AVC,
	OBSPT_AMF_HEVC,
	OBSPT_AMF_AV1,
};

/* OBS-PT throughput-tuned defaults shared by all AMF codecs */
struct obspt_amf_defaults {
	const char *preset;      /* speed */
	const char *rate_control;/* CQP */
	const char *profile;     /* high/main */
	int cqp;
	int bf;
	int keyint_sec;
	int bitrate;
};

const struct obspt_amf_defaults *
obspt_amf_defaults(enum obspt_amf_codec codec);

/* preset string table: speed/balanced/quality (+ highQuality for AV1) */
bool obspt_amf_preset_valid(enum obspt_amf_codec codec, const char *preset);
const char *obspt_amf_preset_fallback(void);

/* rate control string table: CBR/CQP/VBR/VBR_LAT/QVBR/HQVBR/HQCBR */
bool obspt_amf_rate_control_valid(const char *rc);
const char *obspt_amf_rate_control_fallback(void);

/* AMF AV1 QVBR uses a quarter QP range; map a unified 0-51 value. */
int obspt_amf_map_cqp(enum obspt_amf_codec codec, int cqp);

#ifdef __cplusplus
}
#endif
