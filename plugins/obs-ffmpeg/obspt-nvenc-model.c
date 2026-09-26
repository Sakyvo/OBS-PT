#include "obspt-nvenc-model.h"

#include <string.h>

#include <util/c99defs.h>
#include <util/dstr.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const struct obspt_nvenc_defaults defaults_h264 = {
	.preset2 = "p1",
	.tune = "hq",
	.multipass = "disabled",
	.rate_control = "CQP",
	.profile = "high",
	.cqp = 27,
	.bf = 0,
	.psycho_aq = false,
	.lookahead = false,
	.repeat_headers = false,
};

static const struct obspt_nvenc_defaults defaults_hevc = {
	.preset2 = "p1",
	.tune = "hq",
	.multipass = "disabled",
	.rate_control = "CQP",
	.profile = "main",
	.cqp = 27,
	.bf = 0,
	.psycho_aq = false,
	.lookahead = false,
	.repeat_headers = false,
};

static const struct obspt_nvenc_defaults defaults_av1 = {
	.preset2 = "p1",
	.tune = "hq",
	.multipass = "disabled",
	.rate_control = "CQP",
	.profile = "main",
	.cqp = 27,
	.bf = 0,
	.psycho_aq = false,
	.lookahead = false,
	.repeat_headers = false,
};

const struct obspt_nvenc_defaults *
obspt_nvenc_defaults(enum obspt_nvenc_codec codec)
{
	switch (codec) {
	case OBSPT_NVENC_H264:
		return &defaults_h264;
	case OBSPT_NVENC_HEVC:
		return &defaults_hevc;
	case OBSPT_NVENC_AV1:
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

bool obspt_nvenc_preset2_valid(const char *preset2)
{
	static const char *const values[] = {"p1", "p2", "p3", "p4",
					     "p5", "p6", "p7"};
	return str_in(values, ARRAY_SIZE(values), preset2);
}

const char *obspt_nvenc_preset2_fallback(void)
{
	return "p5";
}

bool obspt_nvenc_tuning_valid(const char *tune)
{
	static const char *const values[] = {"hq", "ll", "ull"};
	return str_in(values, ARRAY_SIZE(values), tune);
}

const char *obspt_nvenc_tuning_fallback(void)
{
	return "hq";
}

bool obspt_nvenc_multipass_valid(const char *multipass)
{
	static const char *const values[] = {"disabled", "qres", "fullres"};
	return str_in(values, ARRAY_SIZE(values), multipass);
}

const char *obspt_nvenc_multipass_fallback(void)
{
	return "disabled";
}

const char *obspt_nvenc_profile_fallback(enum obspt_nvenc_codec codec)
{
	switch (codec) {
	case OBSPT_NVENC_HEVC:
	case OBSPT_NVENC_AV1:
		return "main";
	case OBSPT_NVENC_H264:
		break;
	}
	return "high";
}

int obspt_nvenc_map_cqp(enum obspt_nvenc_codec codec, int cqp)
{
	if (cqp < 0)
		return 0;

	if (codec == OBSPT_NVENC_AV1) {
		/* AV1 NVENC constQP uses the unified scale multiplied by 4;
		 * clamp to the 8-bit field range. */
		if (cqp > 63)
			cqp = 63;
		return cqp * 4;
	}

	if (cqp > 51)
		cqp = 51;
	return cqp;
}
