#include "obspt-amf-model.h"

#include <util/dstr.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const struct obspt_amf_defaults defaults_avc = {
	.preset = "speed",
	.rate_control = "CQP",
	.profile = "high",
	.cqp = 27,
	.bf = 0,
	.keyint_sec = 2,
	.bitrate = 2500,
};

static const struct obspt_amf_defaults defaults_hevc = {
	.preset = "speed",
	.rate_control = "CQP",
	.profile = "main",
	.cqp = 27,
	.bf = 0,
	.keyint_sec = 2,
	.bitrate = 2500,
};

static const struct obspt_amf_defaults defaults_av1 = {
	.preset = "speed",
	.rate_control = "CQP",
	.profile = "main",
	.cqp = 27,
	.bf = 0,
	.keyint_sec = 2,
	.bitrate = 2500,
};

const struct obspt_amf_defaults *
obspt_amf_defaults(enum obspt_amf_codec codec)
{
	switch (codec) {
	case OBSPT_AMF_AVC:
		return &defaults_avc;
	case OBSPT_AMF_HEVC:
		return &defaults_hevc;
	case OBSPT_AMF_AV1:
		return &defaults_av1;
	}
	return NULL;
}

static bool str_in(const char *const *list, size_t count, const char *value)
{
	if (!value)
		return false;
	for (size_t i = 0; i < count; i++) {
		if (astrcmpi(value, list[i]) == 0)
			return true;
	}
	return false;
}

bool obspt_amf_preset_valid(enum obspt_amf_codec codec, const char *preset)
{
	static const char *const shared[] = {"speed", "balanced", "quality"};
	if (codec == OBSPT_AMF_AV1 &&
	    astrcmpi(preset, "highQuality") == 0)
		return true;
	return str_in(shared, ARRAY_SIZE(shared), preset);
}

const char *obspt_amf_preset_fallback(void)
{
	return "speed";
}

bool obspt_amf_rate_control_valid(const char *rc)
{
	static const char *const values[] = {
		"CBR", "CQP", "VBR", "VBR_LAT", "QVBR", "HQVBR", "HQCBR",
	};
	return str_in(values, ARRAY_SIZE(values), rc);
}

const char *obspt_amf_rate_control_fallback(void)
{
	return "CQP";
}

int obspt_amf_map_cqp(enum obspt_amf_codec codec, int cqp)
{
	if (cqp < 0)
		return 0;

	if (codec == OBSPT_AMF_AV1) {
		/* AV1 QVBR uses a quarter of the QP scale */
		if (cqp > 51)
			cqp = 51;
		return cqp / 4;
	}

	if (cqp > 51)
		cqp = 51;
	return cqp;
}
