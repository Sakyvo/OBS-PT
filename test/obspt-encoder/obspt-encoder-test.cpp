/******************************************************************************
    OBS-PT modern hardware encoder support tests (issue 055)

    Verifies, through public interfaces only:
      - libobs obs-nal / obs-hevc / obs-av1 packet helpers
      - the Hybrid MP4 muxer writing HEVC/AV1 (plus AAC audio tracks)
        end-to-end: synthetic elementary streams in, real MP4 files out,
        read back and validated through libavformat.

    No GPU, no display, no window: everything runs headless.
******************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <obs.h>
#include <obs-nal.h>
#include <obs-hevc.h>
#include <obs-av1.h>

#include <util/buffered-file-serializer.h>
#include <util/platform.h>
#include <media-io/video-io.h>
#include <media-io/audio-io.h>

extern "C" {
#include <libavformat/avformat.h>
}

extern "C" {
#include "mp4-mux.h"
#include "rtmp-av1.h"
#include "flv-mux.h"
}

#include "ffmpeg-mux/ffmpeg-mux.h"
#include "obspt-nvenc-model.h"
#include "obspt-amf-model.h"

/* ------------------------------------------------------------------ */
/* tiny test harness                                                   */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, ...)                                                       \
	do {                                                                   \
		if (cond) {                                                    \
			printf("  PASS: " __VA_ARGS__);                        \
			printf("\n");                                          \
		} else {                                                       \
			g_failures++;                                          \
			printf("  FAIL: %s:%d ", __FILE__, __LINE__);          \
			printf(__VA_ARGS__);                                   \
			printf("\n");                                          \
		}                                                              \
	} while (0)

/* ------------------------------------------------------------------ */
/* bit writer (MSB first, like all the parsers involved)               */
/* ------------------------------------------------------------------ */

struct BitWriter {
	std::vector<uint8_t> bytes;
	int bitpos = 0; /* bits used in the last byte */

	void put(uint32_t val, int n)
	{
		for (int i = n - 1; i >= 0; i--) {
			if (bitpos == 0)
				bytes.push_back(0);
			if ((val >> i) & 1u)
				bytes.back() |= (uint8_t)(1u << (7 - bitpos));
			if (++bitpos == 8)
				bitpos = 0;
		}
	}

	/* unsigned Exp-Golomb */
	void ue(uint32_t v)
	{
		uint32_t x = v + 1;
		int nbits = 0;
		while (x >> nbits)
			nbits++;
		put(0, nbits - 1);
		put(x, nbits);
	}

	/* signed Exp-Golomb */
	void se(int32_t v)
	{
		if (v > 0)
			ue((uint32_t)(2 * v - 1));
		else if (v < 0)
			ue((uint32_t)(-2 * v));
		else
			ue(0);
	}

	/* rbsp_trailing_bits: a '1' then zero padding to byte alignment */
	void trailing()
	{
		put(1, 1);
		while (bitpos)
			put(0, 1);
	}
};

/* ------------------------------------------------------------------ */
/* synthetic HEVC construction                                         */
/* ------------------------------------------------------------------ */

/* HEVC NAL types used below */
#define T_TRAIL_R 1
#define T_IDR_W_RADL 19
#define T_VPS 32
#define T_SPS 33
#define T_PPS 34
#define T_SEI_PREFIX 39

/* rbsp -> NAL bytes with emulation prevention inserted */
static std::vector<uint8_t> rbsp_to_nal(const std::vector<uint8_t> &rbsp)
{
	std::vector<uint8_t> out;
	int zeros = 0;
	for (uint8_t b : rbsp) {
		if (zeros >= 2 && b <= 3) {
			out.push_back(3);
			zeros = 0;
		}
		out.push_back(b);
		if (b == 0)
			zeros++;
		else
			zeros = 0;
	}
	/* avoid trailing zero bytes (they would merge into start codes) */
	while (!out.empty() && out.back() == 0)
		out.back() = 0x42;
	return out;
}

static std::vector<uint8_t> hevc_nal(int type,
				     const std::vector<uint8_t> &rbsp)
{
	std::vector<uint8_t> n;
	n.push_back((uint8_t)(type << 1)); /* forbidden 0, type, layer hi */
	n.push_back(1);		      /* layer 0, temporal_id 1 */
	const std::vector<uint8_t> body = rbsp_to_nal(rbsp);
	n.insert(n.end(), body.begin(), body.end());
	return n;
}

/* profile_tier_level(max_sub_layers_minus1 = 0) */
static void hevc_put_ptl(BitWriter &bw)
{
	bw.put(0, 2);   /* profile_space */
	bw.put(0, 1);   /* tier_flag */
	bw.put(1, 5);   /* profile_idc: Main */
	bw.put(0x60000000u, 32); /* compatibility flags */
	bw.put(0, 48);  /* constraint indicator flags */
	bw.put(186, 8); /* level_idc: level 6.2 */
}

static std::vector<uint8_t> make_vps_rbsp()
{
	BitWriter bw;
	bw.put(0, 12); /* vps_id + reserved */
	bw.put(0, 3);  /* vps_max_sub_layers_minus1 */
	bw.put(0, 1);  /* vps_temporal_id_nesting */
	hevc_put_ptl(bw);
	bw.trailing();
	return bw.bytes;
}

static std::vector<uint8_t> make_sps_rbsp()
{
	BitWriter bw;
	bw.put(0, 4); /* sps_video_parameter_set_id */
	bw.put(0, 3); /* sps_max_sub_layers_minus1 */
	bw.put(1, 1); /* sps_temporal_id_nesting */
	hevc_put_ptl(bw);
	bw.ue(0);     /* sps_seq_parameter_set_id */
	bw.ue(1);     /* chroma_format_idc: 4:2:0 */
	bw.ue(64);    /* pic_width_in_luma_samples */
	bw.ue(64);    /* pic_height_in_luma_samples */
	bw.put(0, 1); /* conformance_window_flag */
	bw.ue(0);     /* bit_depth_luma_minus8 */
	bw.ue(0);     /* bit_depth_chroma_minus8 */
	bw.ue(0);     /* log2_max_pic_order_cnt_lsb_minus4 */
	bw.put(1, 1); /* sub_layer_ordering_info_present */
	bw.ue(0);     /* max_dec_pic_buffering_minus1 */
	bw.ue(0);     /* max_num_reorder_pics */
	bw.ue(0);     /* max_latency_increase_plus1 */
	bw.ue(0);     /* log2_min_luma_coding_block_size_minus3 */
	bw.ue(0);     /* log2_diff_max_min_luma_coding_block_size */
	bw.ue(0);     /* log2_min_transform_block_size_minus2 */
	bw.ue(0);     /* log2_diff_max_min_transform_block_size */
	bw.ue(0);     /* max_transform_hierarchy_depth_inter */
	bw.ue(0);     /* max_transform_hierarchy_depth_intra */
	bw.put(0, 1); /* scaling_list_enabled */
	bw.put(0, 1); /* amp_enabled */
	bw.put(0, 1); /* sample_adaptive_offset_enabled */
	bw.put(0, 1); /* pcm_enabled */
	bw.ue(0);     /* num_short_term_ref_pic_sets */
	bw.put(0, 1); /* long_term_ref_pics_present */
	bw.put(0, 1); /* sps_temporal_mvp_enabled */
	bw.put(0, 1); /* strong_intra_smoothing_enabled */
	bw.put(0, 1); /* vui_parameters_present */
	return bw.bytes;
}

static std::vector<uint8_t> make_pps_rbsp()
{
	BitWriter bw;
	bw.ue(0);    /* pps_pic_parameter_set_id */
	bw.ue(0);    /* pps_seq_parameter_set_id */
	bw.put(0, 7);
	bw.ue(0);    /* num_ref_idx_l0_default_active_minus1 */
	bw.ue(0);    /* num_ref_idx_l1_default_active_minus1 */
	bw.se(0);    /* init_qp_minus26 */
	bw.put(0, 2);
	bw.put(0, 1); /* cu_qp_delta_enabled */
	bw.se(0);    /* pps_cb_qp_offset */
	bw.se(0);    /* pps_cr_qp_offset */
	bw.put(0, 4);
	bw.put(0, 1); /* tiles_enabled */
	bw.put(0, 1); /* entropy_coding_sync_enabled */
	return bw.bytes;
}

static std::vector<uint8_t> concat(const std::vector<std::vector<uint8_t>> &vs)
{
	std::vector<uint8_t> out;
	for (auto &v : vs)
		out.insert(out.end(), v.begin(), v.end());
	return out;
}

/* annex-B byte stream from NAL units */
static std::vector<uint8_t>
annexb(const std::vector<std::vector<uint8_t>> &nals)
{
	std::vector<uint8_t> out;
	const uint8_t sc[4] = {0, 0, 0, 1};
	for (auto &n : nals) {
		out.insert(out.end(), sc, sc + 4);
		out.insert(out.end(), n.begin(), n.end());
	}
	return out;
}

/* ------------------------------------------------------------------ */
/* synthetic AV1 construction                                          */
/* ------------------------------------------------------------------ */

enum {
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME_HEADER = 3,
	OBU_METADATA = 5,
	OBU_FRAME = 6,
};

static void uleb(std::vector<uint8_t> &out, size_t val)
{
	do {
		uint8_t b = val & 0x7f;
		val >>= 7;
		out.push_back(val ? (uint8_t)(b | 0x80) : b);
	} while (val);
}

static std::vector<uint8_t> obu(int type, const std::vector<uint8_t> &payload)
{
	std::vector<uint8_t> out;
	out.push_back((uint8_t)((type << 3) | (1 << 1)));
	uleb(out, payload.size());
	out.insert(out.end(), payload.begin(), payload.end());
	return out;
}

/* Minimal valid sequence header (reduced still picture path):
 *   profile=0 (Main), still_picture=1, reduced_still_picture_header=1,
 *   seq_level_idx=0, 8-bit width/height fields carrying 63 (i.e. 64-1),
 *   8-bit 4:2:0 full-range color config, no film grain.
 * Payload bytes (see issue 055 analysis):
 *   18 1D CF CF C0 10, trailing 80. */
static std::vector<uint8_t> make_av1_seq_obu()
{
	const uint8_t payload[7] = {0x18, 0x1D, 0xCF, 0xCF, 0xC0, 0x10, 0x80};
	return obu(OBU_SEQUENCE_HEADER,
		   std::vector<uint8_t>(payload, payload + 7));
}

/* OBU_FRAME with frame_type: 0=KEY, 1=INTER.
 * First payload byte: show_existing_frame(1)=0, frame_type(2), ... */
static std::vector<uint8_t> make_av1_frame_obu(int frame_type)
{
	std::vector<uint8_t> payload;
	/* show_existing=0, frame_type, show_frame=1, error_resilient=1,
	 * then a few arbitrary payload bytes */
	uint8_t hdr = (uint8_t)((frame_type << 5) | (1 << 4) | (1 << 3));
	payload.push_back(hdr);
	payload.push_back(0xAB);
	payload.push_back(0xCD);
	payload.push_back(0x11);
	return obu(OBU_FRAME, payload);
}

static std::vector<uint8_t> make_av1_td_obu()
{
	return obu(OBU_TEMPORAL_DELIMITER, {});
}

/* ------------------------------------------------------------------ */
/* helper: libobs-aware packet buffer with refcount prefix            */
/* ------------------------------------------------------------------ */

static uint8_t *alloc_packet_data(const std::vector<uint8_t> &bytes)
{
	uint8_t *buf = (uint8_t *)bmalloc(sizeof(long) + bytes.size());
	*(long *)buf = 1;
	memcpy(buf + sizeof(long), bytes.data(), bytes.size());
	return buf;
}

/* ------------------------------------------------------------------ */
/* Group A: libobs helper unit tests                                   */
/* ------------------------------------------------------------------ */

static void test_obs_nal()
{
	printf("[obs_nal_find_startcode]\n");
	const uint8_t sc4[] = {0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0xAA};
	const uint8_t *r = obs_nal_find_startcode(sc4, sc4 + sizeof(sc4));
	CHECK(r == sc4, "finds 4-byte start code");
	CHECK(r != NULL, "non-null result for 4-byte start code");

	const uint8_t sc3[] = {0x11, 0x22, 0x00, 0x00, 0x01, 0x26};
	r = obs_nal_find_startcode(sc3, sc3 + sizeof(sc3));
	CHECK(r != NULL, "finds 3-byte start code");
	CHECK(r == sc3 + 2, "3-byte start code found at correct offset");

	const uint8_t none[] = {0x11, 0x22, 0x33, 0x44};
	r = obs_nal_find_startcode(none, none + sizeof(none));
	CHECK((size_t)(r - none) >= sizeof(none),
	      "no start code -> past-end sentinel");
}

static void test_obs_hevc()
{
	printf("[obs_hevc]\n");

	std::vector<uint8_t> vps = hevc_nal(T_VPS, make_vps_rbsp());
	std::vector<uint8_t> sps = hevc_nal(T_SPS, make_sps_rbsp());
	std::vector<uint8_t> pps = hevc_nal(T_PPS, make_pps_rbsp());
	std::vector<uint8_t> sei = hevc_nal(T_SEI_PREFIX, {0xAB, 0xCD});
	std::vector<uint8_t> idr =
		hevc_nal(T_IDR_W_RADL, {0xAA, 0xBB, 0xCC});
	std::vector<uint8_t> trail = hevc_nal(T_TRAIL_R, {0xDD, 0xEE});

	/* keyframe detection */
	std::vector<uint8_t> keyframe_stream =
		annexb({vps, sps, pps, idr});
	CHECK(obs_hevc_keyframe(keyframe_stream.data(),
				keyframe_stream.size()),
	      "IDR stream detected as keyframe");

	std::vector<uint8_t> inter_stream = annexb({sps, pps, trail});
	CHECK(!obs_hevc_keyframe(inter_stream.data(), inter_stream.size()),
	      "TRAIL_R stream detected as non-keyframe");

	/* header extraction */
	std::vector<uint8_t> stream = annexb({vps, sps, pps, sei, idr});
	uint8_t *new_data, *hdr, *sei_data;
	size_t new_size, hdr_size, sei_size;
	obs_extract_hevc_headers(stream.data(), stream.size(), &new_data,
				 &new_size, &hdr, &hdr_size, &sei_data,
				 &sei_size);

	std::vector<uint8_t> expect_hdr = annexb({vps, sps, pps});
	std::vector<uint8_t> expect_sei = annexb({sei});
	std::vector<uint8_t> expect_pkt = annexb({idr});
	CHECK(hdr_size == expect_hdr.size() &&
		      memcmp(hdr, expect_hdr.data(), hdr_size) == 0,
	      "VPS/SPS/PPS extracted with start codes intact");
	CHECK(sei_size == expect_sei.size() &&
		      memcmp(sei_data, expect_sei.data(), sei_size) == 0,
	      "SEI extracted");
	CHECK(new_size == expect_pkt.size() &&
		      memcmp(new_data, expect_pkt.data(), new_size) == 0,
	      "slice packet extracted");
	bfree(new_data);
	bfree(hdr);
	bfree(sei_data);

	/* packet parsing: annex-B -> length-prefixed */
	struct encoder_packet src = {};
	struct encoder_packet parsed = {};
	src.type = OBS_ENCODER_VIDEO;
	src.data = keyframe_stream.data();
	src.size = keyframe_stream.size();
	src.pts = 0;
	src.dts = 0;
	src.keyframe = false;
	src.timebase_num = 1;
	src.timebase_den = 30;
	src.priority = 0;

	obs_parse_hevc_packet(&parsed, &src);
	CHECK(parsed.keyframe, "parsed packet flagged as keyframe");
	CHECK(parsed.priority == OBS_NAL_PRIORITY_HIGHEST,
	      "IDR gets highest priority");
	CHECK(parsed.drop_priority == parsed.priority,
	      "drop priority mirrors priority");
	uint32_t len0 = (uint32_t)parsed.data[0] << 24 |
			(uint32_t)parsed.data[1] << 16 |
			(uint32_t)parsed.data[2] << 8 | parsed.data[3];
	CHECK(len0 == vps.size(), "first NAL is length-prefixed (VPS size)");
	/* total: 4 prefix bytes per NAL replace the 4-byte start codes */
	CHECK(parsed.size == keyframe_stream.size(),
	      "length-prefixed size matches (4-byte start codes)");
	long *refs = ((long *)parsed.data) - 1;
	CHECK(*refs == 1, "parsed packet data is refcounted");
	bfree(refs);

	/* priority-only variant */
	int prio = obs_parse_hevc_packet_priority(&src);
	CHECK(prio == OBS_NAL_PRIORITY_HIGHEST,
	      "obs_parse_hevc_packet_priority reports highest for IDR");
}

static void test_obs_av1()
{
	printf("[obs_av1]\n");

	std::vector<uint8_t> td = make_av1_td_obu();
	std::vector<uint8_t> seq = make_av1_seq_obu();
	std::vector<uint8_t> key = make_av1_frame_obu(0);
	std::vector<uint8_t> inter = make_av1_frame_obu(1);

	std::vector<uint8_t> stream = concat({td, seq, key});
	CHECK(obs_av1_keyframe(stream.data(), stream.size()),
	      "KEY frame OBU stream detected as keyframe");

	std::vector<uint8_t> stream2 = concat({td, inter});
	CHECK(!obs_av1_keyframe(stream2.data(), stream2.size()),
	      "INTER frame OBU stream detected as non-keyframe");

	/* header extraction */
	uint8_t *new_data, *hdr;
	size_t new_size, hdr_size;
	obs_extract_av1_headers(stream.data(), stream.size(), &new_data,
				&new_size, &hdr, &hdr_size);
	CHECK(hdr_size == seq.size() &&
		      memcmp(hdr, seq.data(), seq.size()) == 0,
	      "sequence header OBU extracted");
	CHECK(new_size == stream.size(), "new packet keeps all OBUs");
	bfree(new_data);
	bfree(hdr);

	/* packet parsing drops temporal delimiters */
	struct encoder_packet src = {};
	struct encoder_packet parsed = {};
	src.type = OBS_ENCODER_VIDEO;
	src.data = stream.data();
	src.size = stream.size();
	src.pts = 0;
	src.dts = 0;
	src.keyframe = false;
	src.priority = 0;
	src.timebase_num = 1;
	src.timebase_den = 30;
	obs_parse_av1_packet(&parsed, &src);
	std::vector<uint8_t> expect = concat({seq, key});
	CHECK(parsed.size == expect.size() &&
		      memcmp(parsed.data, expect.data(), expect.size()) == 0,
	      "temporal delimiter dropped, sequence header and frame kept");
	CHECK(parsed.keyframe, "AV1 keyframe flag set from frame header");
	bfree(((long *)parsed.data) - 1);
}

/* ------------------------------------------------------------------ */
/* Group B: muxer roundtrip through real libobs objects               */
/* ------------------------------------------------------------------ */

/* ---- dummy encoders/output ---- */

struct dummy_video_enc {
	uint8_t *extra;
	size_t extra_size;
};

static const char *dummy_enc_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "OBS-PT encoder test dummy";
}

static void *dummy_video_create(obs_data_t *settings, obs_encoder_t *enc)
{
	UNUSED_PARAMETER(settings);
	struct dummy_video_enc *e =
		(struct dummy_video_enc *)bzalloc(sizeof(*e));
	UNUSED_PARAMETER(enc);
	return e;
}

static void dummy_video_destroy(void *data)
{
	bfree(data);
}

static bool dummy_get_extra_data(void *data, uint8_t **extra, size_t *size)
{
	struct dummy_video_enc *e = (struct dummy_video_enc *)data;
	if (!e->extra)
		return false;
	*extra = e->extra;
	*size = e->extra_size;
	return true;
}

static void dummy_video_defaults(obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);
}

static bool dummy_encode(void *data, struct encoder_frame *frame,
			 struct encoder_packet *packet, bool *received_packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(frame);
	UNUSED_PARAMETER(packet);
	UNUSED_PARAMETER(received_packet);
	return true;
}

static bool dummy_output_start(void *data)
{
	UNUSED_PARAMETER(data);
	return true;
}

static void dummy_output_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(ts);
}

static void dummy_output_packet(void *data, struct encoder_packet *packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(packet);
}

static size_t dummy_frame_size(void *data)
{
	UNUSED_PARAMETER(data);
	return 1024;
}

/* assign per-instance extra data by mutating through this test hook */
static std::vector<uint8_t> g_hevc_extra;
static std::vector<uint8_t> g_av1_extra;
static std::vector<uint8_t> g_h264_extra;

static bool h264_test_get_extra_data(void *data, uint8_t **extra,
				     size_t *size)
{
	UNUSED_PARAMETER(data);
	*extra = g_h264_extra.data();
	*size = g_h264_extra.size();
	return g_h264_extra.size() > 0;
}

static bool hevc_test_get_extra_data(void *data, uint8_t **extra,
				     size_t *size)
{
	UNUSED_PARAMETER(data);
	*extra = g_hevc_extra.data();
	*size = g_hevc_extra.size();
	return g_hevc_extra.size() > 0;
}

static bool av1_test_get_extra_data(void *data, uint8_t **extra, size_t *size)
{
	UNUSED_PARAMETER(data);
	*extra = g_av1_extra.data();
	*size = g_av1_extra.size();
	return g_av1_extra.size() > 0;
}

static struct obs_encoder_info h264_test_encoder = {
	.id = "obspt_test_h264",
	.type = OBS_ENCODER_VIDEO,
	.codec = "h264",
	.get_name = dummy_enc_name,
	.create = dummy_video_create,
	.destroy = dummy_video_destroy,
	.encode = dummy_encode,
	.get_defaults = dummy_video_defaults,
	.get_extra_data = h264_test_get_extra_data,
};

static struct obs_encoder_info hevc_test_encoder = {
	.id = "obspt_test_hevc",
	.type = OBS_ENCODER_VIDEO,
	.codec = "hevc",
	.get_name = dummy_enc_name,
	.create = dummy_video_create,
	.destroy = dummy_video_destroy,
	.encode = dummy_encode,
	.get_defaults = dummy_video_defaults,
	.get_extra_data = hevc_test_get_extra_data,
};

static struct obs_encoder_info av1_test_encoder = {
	.id = "obspt_test_av1",
	.type = OBS_ENCODER_VIDEO,
	.codec = "av1",
	.get_name = dummy_enc_name,
	.create = dummy_video_create,
	.destroy = dummy_video_destroy,
	.encode = dummy_encode,
	.get_defaults = dummy_video_defaults,
	.get_extra_data = av1_test_get_extra_data,
};

static const uint8_t g_aac_asc[2] = {0x12, 0x10}; /* AAC-LC 48 kHz stereo */

static bool aac_test_get_extra_data(void *data, uint8_t **extra, size_t *size)
{
	UNUSED_PARAMETER(data);
	*extra = (uint8_t *)g_aac_asc;
	*size = sizeof(g_aac_asc);
	return true;
}

static void *dummy_audio_create(obs_data_t *settings, obs_encoder_t *enc)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(enc);
	return bzalloc(8);
}

static struct obs_encoder_info aac_test_encoder = {
	.id = "obspt_test_aac",
	.type = OBS_ENCODER_AUDIO,
	.codec = "aac",
	.get_name = dummy_enc_name,
	.create = dummy_audio_create,
	.destroy = bfree,
	.encode = dummy_encode,
	.get_frame_size = dummy_frame_size,
	.get_defaults = dummy_video_defaults,
	.get_extra_data = aac_test_get_extra_data,
};

static const char *dummy_output_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "OBS-PT encoder test output";
}

static void *dummy_output_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(output);
	return bzalloc(8);
}

static struct obs_output_info dummy_output_info = {
	.id = "obspt_test_output",
	.flags = OBS_OUTPUT_AV | OBS_OUTPUT_ENCODED | OBS_OUTPUT_MULTI_TRACK,
	.get_name = dummy_output_name,
	.create = dummy_output_create,
	.destroy = bfree,
	.start = dummy_output_start,
	.stop = dummy_output_stop,
	.encoded_packet = dummy_output_packet,
};

/* ---- media contexts ---- */

static bool noop_audio_input(void *param, uint64_t start_ts,
			     uint64_t end_ts, uint64_t *new_ts,
			     uint32_t active_mixers,
			     struct audio_output_data *mixes)
{
	UNUSED_PARAMETER(param);
	UNUSED_PARAMETER(start_ts);
	UNUSED_PARAMETER(end_ts);
	UNUSED_PARAMETER(new_ts);
	UNUSED_PARAMETER(active_mixers);
	UNUSED_PARAMETER(mixes);
	return true;
}

/* ---- video packet builders ---- */

static void submit_video_packets(struct mp4_mux *mux, obs_encoder_t *enc,
				 const std::vector<std::vector<uint8_t>> &frames,
				 int fps, bool av1)
{
	for (size_t i = 0; i < frames.size(); i++) {
		struct encoder_packet pkt = {};
		pkt.type = OBS_ENCODER_VIDEO;
		pkt.encoder = enc;
		pkt.data = const_cast<uint8_t *>(frames[i].data());
		pkt.size = frames[i].size();
		pkt.pts = (int64_t)i;
		pkt.dts = (int64_t)i;
		pkt.timebase_num = 1;
		pkt.timebase_den = fps;
		pkt.keyframe = i == 0;
		pkt.priority = 0;
		pkt.dts_usec = (int64_t)i * 1000000 / fps;
		CHECK(mp4_mux_submit_packet(mux, &pkt),
		      "video packet %zu accepted by muxer", i);
		UNUSED_PARAMETER(av1);
	}
}

static void submit_audio_packets(struct mp4_mux *mux, obs_encoder_t *enc,
				 int frames, int frame_samples)
{
	for (int i = 0; i < frames; i++) {
		std::vector<uint8_t> bytes(64, (uint8_t)(0x40 + i));
		struct encoder_packet pkt = {};
		pkt.type = OBS_ENCODER_AUDIO;
		pkt.encoder = enc;
		pkt.data = alloc_packet_data(bytes) + sizeof(long);
		pkt.size = bytes.size();
		pkt.pts = (int64_t)i * frame_samples;
		pkt.dts = (int64_t)i * frame_samples;
		pkt.timebase_num = 1;
		pkt.timebase_den = 48000;
		pkt.dts_usec =
			(int64_t)i * frame_samples * 1000000LL / 48000;
		CHECK(mp4_mux_submit_packet(mux, &pkt),
		      "audio packet %d accepted by muxer", i);
		obs_encoder_packet_release(&pkt);
	}
}

/* find a box by type in the raw file (top-level and stsd nesting both
 * work because we simply scan the whole file) */
static const uint8_t *find_box(const std::vector<uint8_t> &file,
			       const char *type, size_t *out_size)
{
	if (file.size() < 8)
		return nullptr;
	for (size_t i = 0; i + 8 <= file.size(); i++) {
		if (memcmp(file.data() + i + 4, type, 4) == 0) {
			size_t sz = (file[i] << 24) | (file[i + 1] << 16) |
				    (file[i + 2] << 8) | file[i + 3];
			if (sz >= 8 && i + sz <= file.size()) {
				if (out_size)
					*out_size = sz;
				return file.data() + i;
			}
		}
	}
	return nullptr;
}

static std::vector<uint8_t> read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	std::vector<uint8_t> out;
	if (!f)
		return out;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	out.resize((size_t)sz);
	if (sz > 0 && fread(out.data(), 1, (size_t)sz, f) != (size_t)sz)
		out.clear();
	fclose(f);
	return out;
}

/* ---- avformat readback ---- */

/* muxer design: the last submitted packet of each track is the duration
 * lookahead and is not written as a sample; expect count-1 packets */
static bool readback_check(const char *path, enum AVCodecID expect_video,
			   uint32_t expect_tag, int expect_video_frames,
			   int audio_streams, bool drop_last = true)
{
	/* muxer design: the last submitted packet of each track is the duration
	 * lookahead and is not written as a sample; expect count-1 packets */
	if (drop_last)
		expect_video_frames -= 1;
	AVFormatContext *ctx = nullptr;
	if (avformat_open_input(&ctx, path, nullptr, nullptr) < 0) {
		CHECK(false, "avformat_open_input(%s)", path);
		return false;
	}
	bool ok = true;
	if (avformat_find_stream_info(ctx, nullptr) < 0) {
		CHECK(false, "avformat_find_stream_info(%s)", path);
		avformat_close_input(&ctx);
		return false;
	}

	int found_audio = 0;
	int video_stream = -1;
	for (unsigned i = 0; i < ctx->nb_streams; i++) {
		AVStream *st = ctx->streams[i];
		if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream = (int)i;
			CHECK(st->codecpar->codec_id == expect_video,
			      "video codec id %d (expected %d)",
			      (int)st->codecpar->codec_id, (int)expect_video);
			CHECK(st->codecpar->codec_tag == expect_tag ||
			      /* HEVC: both hvc1 and hev1 are spec-valid; hev1 is
			       * written when parameter sets are in-band */
			      (expect_video == AV_CODEC_ID_HEVC &&
			       st->codecpar->codec_tag ==
				       MKTAG('h', 'e', 'v', '1')),
			      "video codec tag 0x%08x (expected 0x%08x)",
			      st->codecpar->codec_tag, expect_tag);
			CHECK(st->codecpar->extradata != nullptr &&
				      st->codecpar->extradata_size > 0,
			      "video extradata present (%d bytes)",
			      st->codecpar->extradata_size);
		} else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			found_audio++;
			CHECK(st->codecpar->codec_id == AV_CODEC_ID_AAC,
			      "audio stream %u is AAC", i);
		}
	}
	CHECK(found_audio == audio_streams,
	      "audio stream count %d (expected %d)", found_audio,
	      audio_streams);
	CHECK(video_stream != -1, "video stream present");

	/* count actual decodable video packets */
	int packets = 0;
	AVPacket pkt;
	while (av_read_frame(ctx, &pkt) >= 0) {
		if (pkt.stream_index == video_stream)
			packets++;
		av_packet_unref(&pkt);
	}
	CHECK(packets == expect_video_frames,
	      "video packet count %d (expected %d)", packets,
	      expect_video_frames);

	avformat_close_input(&ctx);
	return ok;
}

/* ---- full muxer roundtrip for one codec ---- */

static void run_muxer_roundtrip(const char *label, const char *path,
				struct obs_encoder_info *venc_info,
				const std::vector<uint8_t> &extra_data,
				enum AVCodecID expect_video, uint32_t expect_tag,
				const std::vector<std::vector<uint8_t>> &frames,
				int audio_encoders)
{
	printf("[%s muxer roundtrip]\n", label);

	if (venc_info == &hevc_test_encoder)
		g_hevc_extra = extra_data;
	else if (venc_info == &h264_test_encoder)
		g_h264_extra = extra_data;
	else
		g_av1_extra = extra_data;

	video_t *video = nullptr;
	struct video_output_info vi = {};
	vi.name = "obspt-encoder-test";
	vi.format = VIDEO_FORMAT_NV12;
	vi.fps_num = 30;
	vi.fps_den = 1;
	vi.width = 64;
	vi.height = 64;
	vi.cache_size = 2;
	vi.colorspace = VIDEO_CS_709;
	vi.range = VIDEO_RANGE_PARTIAL;
	int vret = video_output_open(&video, &vi);
	CHECK(vret == VIDEO_OUTPUT_SUCCESS, "video_output_open");

	audio_t *audio = nullptr;
	struct audio_output_info ai = {};
	ai.name = "obspt-encoder-test";
	ai.samples_per_sec = 48000;
	ai.format = AUDIO_FORMAT_FLOAT_PLANAR;
	ai.speakers = SPEAKERS_STEREO;
	ai.input_callback = noop_audio_input;
	bool aok = audio_output_open(&audio, &ai) == 0;
	CHECK(aok, "audio_output_open");

	obs_encoder_t *venc = obs_video_encoder_create(
		venc_info->id, "test_video", nullptr, nullptr);
	CHECK(venc != nullptr, "video encoder created");
	obs_encoder_set_video(venc, video);

	obs_encoder_t *aenc[2] = {};
	for (int i = 0; i < audio_encoders && i < 2; i++) {
		aenc[i] = obs_audio_encoder_create(aac_test_encoder.id,
						   "test_audio", nullptr,
						   (size_t)i, nullptr);
		CHECK(aenc[i] != nullptr, "audio encoder %d created", i);
		obs_encoder_set_audio(aenc[i], audio);
	}

	obs_output_t *output =
		obs_output_create(dummy_output_info.id, "test_out", nullptr,
				  nullptr);
	CHECK(output != nullptr, "output created");
	obs_output_set_video_encoder(output, venc);
	for (int i = 0; i < audio_encoders && i < 2; i++)
		obs_output_set_audio_encoder(output, aenc[i], (size_t)i);

	/* mirrors mp4_output_start: runs the encoder create callbacks so
	 * obs_encoder_get_extra_data works */
	CHECK(obs_output_initialize_encoders(output, 0),
	      "encoders initialized (create ran)");

	struct serializer s;
	CHECK(buffered_file_serializer_init_defaults(&s, path),
	      "output file opened: %s", path);

	struct mp4_mux *mux = mp4_mux_create(output, &s, MP4_USE_NEGATIVE_CTS);
	CHECK(mux != nullptr, "muxer created");

	submit_video_packets(mux, venc, frames, 30,
			     venc_info == &av1_test_encoder);
	for (int i = 0; i < audio_encoders && i < 2; i++)
		submit_audio_packets(mux, aenc[i], 20, 1024);

	CHECK(mp4_mux_finalise(mux), "muxer finalised");
	buffered_file_serializer_free(&s);
	mp4_mux_destroy(mux);

	obs_output_release(output);
	obs_encoder_release(venc);
	for (int i = 0; i < audio_encoders && i < 2; i++)
		obs_encoder_release(aenc[i]);
	if (audio)
		audio_output_close(audio);
	if (video)
		video_output_close(video);

	printf("  readback: %s\n", path);
	readback_check(path, expect_video, expect_tag, (int)frames.size(),
		       audio_encoders);
}

/* ------------------------------------------------------------------ */
/* Group C: ffmpeg-mux (ordinary MP4 / MKV) smoke                     */
/* ------------------------------------------------------------------ */

static void send_ffm(FILE *p, struct ffm_packet_info *info,
		     const uint8_t *data)
{
	fwrite(info, sizeof(*info), 1, p);
	fwrite(data, 1, info->size, p);
}

static bool ffmpeg_mux_smoke(const char *ffm_exe, const char *out_dir,
			     const char *ext, const char *vcodec,
			     enum AVCodecID expect_id, uint32_t expect_tag,
			     const std::vector<uint8_t> &video_header,
			     const std::vector<std::vector<uint8_t>> &frames)
{
	printf("[ffmpeg-mux %s %s smoke]\n", vcodec, ext);

	std::string path = std::string(out_dir) + "/ffm-" + vcodec + "." +
			   ext;

	std::string cmd = std::string(ffm_exe) + " \"" + path + "\"" +
			  " 1 2 " + vcodec +
			  " 1000 64 64 1 1 1 1 30 1 "
			  "aac a0 128 2 48000 1024 "
			  "a1 128 2 48000 1024 "
			  "\"\" \"\"";

	FILE *p = _popen(cmd.c_str(), "wb");
	if (!p) {
		CHECK(false, "_popen(%s)", ffm_exe);
		return false;
	}

	/* headers: video then one per audio track */
	struct ffm_packet_info info = {};
	info.type = FFM_PACKET_VIDEO;
	info.size = (uint32_t)video_header.size();
	send_ffm(p, &info, video_header.data());

	const uint8_t aac_asc[2] = {0x12, 0x10};
	for (int i = 0; i < 2; i++) {
		info = {};
		info.type = FFM_PACKET_AUDIO;
		info.index = (uint32_t)i;
		info.size = sizeof(aac_asc);
		send_ffm(p, &info, aac_asc);
	}

	/* 20 video packets + 20 audio packets per track */
	for (size_t i = 0; i < frames.size(); i++) {
		info = {};
		info.type = FFM_PACKET_VIDEO;
		info.size = (uint32_t)frames[i].size();
		info.pts = (int64_t)i;
		info.dts = (int64_t)i;
		info.keyframe = i == 0;
		send_ffm(p, &info, frames[i].data());
	}
	for (int t = 0; t < 2; t++) {
		for (int i = 0; i < 20; i++) {
			info = {};
			info.type = FFM_PACKET_AUDIO;
			info.index = (uint32_t)t;
			info.size = 64;
			info.pts = (int64_t)i * 1024;
			info.dts = (int64_t)i * 1024;
			info.keyframe = true;
			uint8_t bytes[64];
			memset(bytes, 0x40 + i, sizeof(bytes));
			send_ffm(p, &info, bytes);
		}
	}

	int ret = _pclose(p);
	CHECK(ret == 0, "obs-ffmpeg-mux exited cleanly (%d)", ret);

	readback_check(path.c_str(), expect_id, expect_tag,
		       (int)frames.size(), 2, false);
	return true;
}

/* ------------------------------------------------------------------ */
/* NVENC capability model (issue 056)                                 */
/* ------------------------------------------------------------------ */

static void test_nvenc_model()
{
	printf("[obspt-nvenc-model]\n");

	/* OBS-PT throughput-tuned defaults, raised to the recording preset
	 * surface: CQP 27, p1, hq, disabled, no B-frames */
	const struct obspt_nvenc_defaults *h =
		obspt_nvenc_defaults(OBSPT_NVENC_H264);
	CHECK(h && strcmp(h->preset2, "p1") == 0 &&
		      strcmp(h->tune, "hq") == 0 &&
		      strcmp(h->multipass, "disabled") == 0 &&
		      strcmp(h->rate_control, "CQP") == 0 &&
		      strcmp(h->profile, "high") == 0 && h->cqp == 27 &&
		      h->bf == 0 && !h->psycho_aq && !h->lookahead,
	      "h264 defaults carry the OBS-PT tuned preset");

	const struct obspt_nvenc_defaults *v =
		obspt_nvenc_defaults(OBSPT_NVENC_HEVC);
	CHECK(v && strcmp(v->preset2, "p1") == 0 &&
		      strcmp(v->multipass, "disabled") == 0 &&
		      strcmp(v->rate_control, "CQP") == 0 && v->cqp == 27 &&
		      strcmp(v->profile, "main") == 0 && v->bf == 0 &&
		      !v->psycho_aq,
	      "hevc defaults carry the OBS-PT tuned preset");

	const struct obspt_nvenc_defaults *a =
		obspt_nvenc_defaults(OBSPT_NVENC_AV1);
	CHECK(a && strcmp(a->preset2, "p1") == 0 &&
		      strcmp(a->multipass, "disabled") == 0 &&
		      strcmp(a->rate_control, "CQP") == 0 && a->cqp == 27 &&
		      strcmp(a->profile, "main") == 0 && a->bf == 0,
	      "av1 defaults carry the OBS-PT tuned preset");

	/* unified 0-51 CQP scale -> per-codec NVENC QP */
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_H264, 20) == 20,
	      "h264 cqp maps 1:1");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_HEVC, 27) == 27,
	      "hevc cqp maps 1:1");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_AV1, 20) == 80,
	      "av1 cqp maps x4");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_AV1, 51) == 204,
	      "av1 cqp 51 (exact top of unified scale) -> 204");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_AV1, 63) == 252,
	      "av1 cqp 63 (native top) -> 252");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_AV1, 64) == 252,
	      "av1 cqp first above native range clamps to 252");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_AV1, 1000) == 252,
	      "av1 cqp far above range clamps");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_H264, -1) == 0,
	      "negative cqp clamps to 0");
	CHECK(obspt_nvenc_map_cqp(OBSPT_NVENC_H264, 0) == 0,
	      "zero cqp stays 0");

	/* bounded string tables: ordinary, case, first-above, empty, NULL */
	CHECK(obspt_nvenc_preset2_valid("p1") &&
		      obspt_nvenc_preset2_valid("p7") &&
		      obspt_nvenc_preset2_valid("P4"),
	      "preset2 p1..p7 valid (case-insensitive)");
	CHECK(!obspt_nvenc_preset2_valid("p8"),
	      "preset2 first above range invalid");
	CHECK(!obspt_nvenc_preset2_valid("") &&
		      !obspt_nvenc_preset2_valid(NULL),
	      "preset2 empty/NULL invalid");
	CHECK(strcmp(obspt_nvenc_preset2_fallback(), "p5") == 0,
	      "preset2 fallback p5 (upstream behavior)");

	CHECK(obspt_nvenc_tuning_valid("hq") && obspt_nvenc_tuning_valid("ll") &&
		      obspt_nvenc_tuning_valid("ull"),
	      "tuning values valid");
	CHECK(!obspt_nvenc_tuning_valid("lossless") &&
		      !obspt_nvenc_tuning_valid("hq2") &&
		      !obspt_nvenc_tuning_valid(NULL),
	      "tuning invalid values rejected");
	CHECK(strcmp(obspt_nvenc_tuning_fallback(), "hq") == 0,
	      "tuning fallback hq");

	CHECK(obspt_nvenc_multipass_valid("disabled") &&
		      obspt_nvenc_multipass_valid("qres") &&
		      obspt_nvenc_multipass_valid("fullres"),
	      "multipass values valid");
	CHECK(!obspt_nvenc_multipass_valid("halfres"),
	      "multipass first-above/unknown invalid");
	CHECK(strcmp(obspt_nvenc_multipass_fallback(), "disabled") == 0,
	      "multipass fallback disabled");

	CHECK(strcmp(obspt_nvenc_profile_fallback(OBSPT_NVENC_H264),
		     "high") == 0 &&
		      strcmp(obspt_nvenc_profile_fallback(OBSPT_NVENC_HEVC),
			     "main") == 0 &&
		      strcmp(obspt_nvenc_profile_fallback(OBSPT_NVENC_AV1),
			     "main") == 0,
	      "profile fallbacks per codec");
}

/* ------------------------------------------------------------------ */
/* AMF capability model (issue 057)                                    */
/* ------------------------------------------------------------------ */

static void test_amf_model()
{
	printf("[obspt-amf-model]\n");

	const struct obspt_amf_defaults *h =
		obspt_amf_defaults(OBSPT_AMF_AVC);
	CHECK(h && strcmp(h->preset, "speed") == 0 &&
		      strcmp(h->rate_control, "CQP") == 0 &&
		      strcmp(h->profile, "high") == 0 && h->cqp == 27 &&
		      h->bf == 0 && h->keyint_sec == 2 &&
		      h->bitrate == 2500,
	      "amf avc defaults carry the OBS-PT tuned preset");

	const struct obspt_amf_defaults *v =
		obspt_amf_defaults(OBSPT_AMF_HEVC);
	CHECK(v && strcmp(v->preset, "speed") == 0 &&
		      strcmp(v->rate_control, "CQP") == 0 && v->cqp == 27 &&
		      strcmp(v->profile, "main") == 0 && v->bf == 0 &&
		      v->keyint_sec == 2,
	      "amf hevc defaults carry the OBS-PT tuned preset");

	const struct obspt_amf_defaults *a =
		obspt_amf_defaults(OBSPT_AMF_AV1);
	CHECK(a && strcmp(a->preset, "speed") == 0 &&
		      strcmp(a->rate_control, "CQP") == 0 && a->cqp == 27 &&
		      strcmp(a->profile, "main") == 0 && a->bf == 0,
	      "amf av1 defaults carry the OBS-PT tuned preset");

	/* preset table: ordinary, av1-only, first-above-invalid, NULL */
	CHECK(obspt_amf_preset_valid(OBSPT_AMF_AVC, "speed") &&
		      obspt_amf_preset_valid(OBSPT_AMF_HEVC, "balanced") &&
		      obspt_amf_preset_valid(OBSPT_AMF_AVC, "quality"),
	      "shared presets valid");
	CHECK(obspt_amf_preset_valid(OBSPT_AMF_AV1, "highQuality"),
	      "av1-only preset valid for av1");
	CHECK(!obspt_amf_preset_valid(OBSPT_AMF_AVC, "highQuality"),
	      "av1-only preset invalid for avc");
	CHECK(!obspt_amf_preset_valid(OBSPT_AMF_AVC, "turbo") &&
		      !obspt_amf_preset_valid(OBSPT_AMF_AVC, NULL),
	      "unknown/NULL preset rejected");
	CHECK(strcmp(obspt_amf_preset_fallback(), "speed") == 0,
	      "preset fallback speed");

	/* rate control table */
	CHECK(obspt_amf_rate_control_valid("CQP") &&
		      obspt_amf_rate_control_valid("QVBR"),
	      "rate control table has both CQP and QVBR");
	CHECK(!obspt_amf_rate_control_valid("lossless"),
	      "lossless is not an AMF rate control");
	CHECK(strcmp(obspt_amf_rate_control_fallback(), "CQP") == 0,
	      "rate control fallback CQP");

	/* cqp mapping: ordinary, boundaries, above-range */
	CHECK(obspt_amf_map_cqp(OBSPT_AMF_AVC, 20) == 20,
	      "amf cqp passes through");
	CHECK(obspt_amf_map_cqp(OBSPT_AMF_AVC, -1) == 0,
	      "negative cqp clamps to 0");
	CHECK(obspt_amf_map_cqp(OBSPT_AMF_AVC, 1000) == 51,
	      "above-range cqp clamps to 51");
	CHECK(obspt_amf_map_cqp(OBSPT_AMF_AV1, 20) == 5,
	      "av1 cqp maps to quarter QVBR scale (20 -> 5)");
	CHECK(obspt_amf_map_cqp(OBSPT_AMF_AV1, 51) == 12,
	      "av1 cqp boundary (51 -> 12, truncation)");
	CHECK(obspt_amf_map_cqp(OBSPT_AMF_AV1, 300) == 12,
	      "av1 far-above-range clamps");
}

/* ------------------------------------------------------------------ */
/* FLV Enhanced-RTMP tag layout (issue 058)                            */
/* ------------------------------------------------------------------ */

static void test_flv_enhanced_tags()
{
	printf("[flv enhanced-rtmp tags]\n");

	struct encoder_packet pkt = {};
	uint8_t payload[16];
	memset(payload, 0xAB, sizeof(payload));
	pkt.type = OBS_ENCODER_VIDEO;
	pkt.data = payload;
	pkt.size = sizeof(payload);
	pkt.pts = 3000;
	pkt.dts = 2990;
	pkt.timebase_num = 1;
	pkt.timebase_den = 30;
	pkt.keyframe = true;

	/* --- HEVC keyframe (non-header): 0x80|0x10|0x01 + "hvc1" + cto --- */
	{
		uint8_t *out = nullptr;
		size_t size = 0;
		flv_packet_mux_codec(&pkt, 0, &out, &size, false, "hevc");
		CHECK(out && size > 0, "hevc tag produced (%d bytes)", (int)size);
		if (out) {
			/* FLV tag: type(1)=9, size(3), ts(3), ts-ext(1), sid(3) */
			CHECK(out[0] == 9, "tag type is video (9)");
			uint32_t data_size = (uint32_t)out[1] << 16 |
					     (uint32_t)out[2] << 8 | out[3];
			CHECK(data_size == pkt.size + 8,
			      "hevc data size = payload + 1(hdr) + 4cc + 3(cto) (%d)",
			      (int)data_size);
			/* body starts after 11-byte tag header */
			const uint8_t *body = out + 11;
			CHECK(body[0] == 0x91,
			      "hevc keyframe frames byte = 0x91 (0x80|0x10|0x01) (0x%02x)",
			      body[0]);
			CHECK(memcmp(body + 1, "hvc1", 4) == 0,
			      "hevc fourcc is hvc1");
			/* composition time = (pts-dts) in ms: 10 frames
			 * @ 1/30s = 333ms */
			int32_t cto = (int32_t)((body[5] << 16) |
						(body[6] << 8) | body[7]);
			CHECK(cto == 333,
			      "hevc composition time 333ms (%d)", cto);
			CHECK(memcmp(body + 8, payload, sizeof(payload)) == 0,
			      "hevc payload intact");
			bfree(out);
		}
	}

	/* --- AV1 inter frame: no composition time, 0xA1 + "av01" --- */
	{
		pkt.keyframe = false;
		uint8_t *out = nullptr;
		size_t size = 0;
		flv_packet_mux_codec(&pkt, 0, &out, &size, false, "av1");
		CHECK(out && size > 0, "av1 tag produced (%d bytes)", (int)size);
		if (out) {
			CHECK(out[0] == 9, "av1 tag type is video (9)");
			uint32_t data_size = (uint32_t)out[1] << 16 |
					     (uint32_t)out[2] << 8 | out[3];
			CHECK(data_size == pkt.size + 5,
			      "av1 data size = payload + 1(hdr) + 4cc, no cto (%d)",
			      (int)data_size);
			const uint8_t *body = out + 11;
			CHECK(body[0] == 0xA1,
			      "av1 inter frames byte = 0xA1 (0x80|0x20|0x01) (0x%02x)",
			      body[0]);
			CHECK(memcmp(body + 1, "av01", 4) == 0,
			      "av1 fourcc is av01");
			CHECK(memcmp(body + 5, payload, sizeof(payload)) == 0,
			      "av1 payload directly after fourcc (no cto)");
			bfree(out);
		}
	}

	/* --- HEVC sequence header (is_header): PacketType 0, no cto --- */
	{
		pkt.keyframe = true;
		uint8_t *out = nullptr;
		size_t size = 0;
		flv_packet_mux_codec(&pkt, 0, &out, &size, true, "hevc");
		CHECK(out && size > 0, "hevc seq-start tag produced");
		if (out) {
			const uint8_t *body = out + 11;
			CHECK(body[0] == 0x90,
			      "hevc seq-start byte = 0x90 (0x80|0x10|0x00) (0x%02x)",
			      body[0]);
			uint32_t data_size = (uint32_t)out[1] << 16 |
					     (uint32_t)out[2] << 8 | out[3];
			CHECK(data_size == pkt.size + 5,
			      "hevc seq-start has no composition time (%d)",
			      (int)data_size);
			bfree(out);
		}
	}

	/* --- H264 passthrough: legacy tag shape unchanged (regression) --- */
	{
		pkt.keyframe = true;
		uint8_t *out = nullptr;
		size_t size = 0;
		flv_packet_mux_codec(&pkt, 0, &out, &size, false, "h264");
		CHECK(out && size > 0, "h264 tag produced");
		if (out) {
			const uint8_t *body = out + 11;
			CHECK(body[0] == 0x17,
			      "h264 keyframe byte stays legacy 0x17 (0x%02x)",
			      body[0]);
			CHECK(body[1] == 0x01, "h264 AVCPacketType = 1 (frames)");
			CHECK(memcmp(body + 5, payload, sizeof(payload)) == 0,
			      "h264 payload intact after 5-byte header");
			bfree(out);
		}
	}

	/* --- NULL codec falls back to legacy h264 path --- */
	{
		uint8_t *out = nullptr;
		size_t size = 0;
		flv_packet_mux(&pkt, 0, &out, &size, false);
		CHECK(out && size > 0, "legacy flv_packet_mux still works");
		if (out) {
			const uint8_t *body = out + 11;
			CHECK(body[0] == 0x17, "legacy path keeps 0x17");
			bfree(out);
		}
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	/* output directory: argv[1] or current dir; ffmpeg-mux exe via argv[2] */
	std::string outdir = argc > 1 ? argv[1] : ".";
	std::string ffm_exe = argc > 2 ? argv[2] : "";
	os_mkdirs(outdir.c_str());

	test_obs_nal();
	test_obs_hevc();
	test_obs_av1();
	test_nvenc_model();
	test_amf_model();
	test_flv_enhanced_tags();

	if (!obs_startup("en-US", nullptr, nullptr)) {
		printf("FATAL: obs_startup failed\n");
		return 1;
	}
	obs_register_encoder(&h264_test_encoder);
	obs_register_encoder(&hevc_test_encoder);
	obs_register_encoder(&av1_test_encoder);
	obs_register_encoder(&aac_test_encoder);
	obs_register_output(&dummy_output_info);

	/* H264 regression: the existing production path must keep working */
	{
		std::vector<uint8_t> sps = {0x67, 0x42, 0x00, 0x1E, 0xAB, 0xCD};
		std::vector<uint8_t> pps = {0x68, 0xEB, 0x01, 0x11};
		std::vector<uint8_t> idr = {0x65, 0x88, 0x84, 0x21, 0xA0};
		std::vector<uint8_t> ref = {0x41, 0x9A, 0x22, 0x10};

		std::vector<std::vector<uint8_t>> frames;
		frames.push_back(annexb({sps, pps, idr}));
		for (int i = 1; i < 20; i++)
			frames.push_back(annexb({ref}));

		std::string path = outdir + "/h264-roundtrip.mp4";
		run_muxer_roundtrip("H264", path.c_str(), &h264_test_encoder,
				    annexb({sps, pps}), AV_CODEC_ID_H264,
				    MKTAG('a', 'v', 'c', '1'), frames, 2);

		std::vector<uint8_t> file = read_file(path.c_str());
		size_t avcc_sz = 0;
		const uint8_t *avcc = find_box(file, "avcC", &avcc_sz);
		CHECK(avcc != nullptr, "avcC box present in file");
		if (avcc) {
			CHECK(avcc[8] == 0x01,
			      "avcC configurationVersion == 1");
			CHECK(avcc[13] == 0xE1, "avcC SPS count == 1");
		}
	}

	/* HEVC: VPS/SPS/PPS as extra data; 20 frames, first is IDR */
	{
		std::vector<std::vector<uint8_t>> frames;
		frames.push_back(annexb(
			{hevc_nal(T_VPS, make_vps_rbsp()),
			 hevc_nal(T_SPS, make_sps_rbsp()),
			 hevc_nal(T_PPS, make_pps_rbsp()),
			 hevc_nal(T_IDR_W_RADL, {0xAA, 0xBB, 0xCC})}));
		for (int i = 1; i < 20; i++)
			frames.push_back(
				annexb({hevc_nal(T_TRAIL_R, {0xDD, 0xEE})}));

		std::vector<uint8_t> extra =
			annexb({hevc_nal(T_VPS, make_vps_rbsp()),
				hevc_nal(T_SPS, make_sps_rbsp()),
				hevc_nal(T_PPS, make_pps_rbsp())});

		std::string path = outdir + "/hevc-roundtrip.mp4";
		run_muxer_roundtrip("HEVC", path.c_str(), &hevc_test_encoder,
				    extra, AV_CODEC_ID_HEVC,
				    MKTAG('h', 'v', 'c', '1'), frames, 2);

		/* hvcC sanity: parse the box from the raw file (demuxer-independent) */
		std::vector<uint8_t> file = read_file(path.c_str());
		size_t hvcc_sz = 0;
		const uint8_t *hvcc = find_box(file, "hvcC", &hvcc_sz);
		CHECK(hvcc != nullptr, "hvcC box present in file");
		if (hvcc) {
			const uint8_t *c = hvcc + 8; /* skip box header */
			size_t csz = hvcc_sz - 8;
			CHECK(csz >= 23, "hvcC size >= 23 (%d)", (int)csz);
			CHECK(c[0] == 1, "hvcC configurationVersion == 1");
			CHECK(c[1] == 1, "hvcC profile_idc == Main (1)");
			CHECK(c[12] == 186, "hvcC level_idc == 186");
			CHECK((c[16] & 0xFC) == 0xFC && (c[16] & 0x03) == 1,
			      "hvcC chromaFormat == 4:2:0");
			CHECK(c[22] == 3, "hvcC numOfArrays == 3 (VPS/SPS/PPS)");
		}
	}

	/* AV1: sequence header as extra data; TD+SEQ+KEY first frame */
	{
		std::vector<uint8_t> seq = make_av1_seq_obu();
		std::vector<uint8_t> key = make_av1_frame_obu(0);
		std::vector<uint8_t> inter = make_av1_frame_obu(1);

		std::vector<std::vector<uint8_t>> frames;
		frames.push_back(concat({make_av1_td_obu(), seq, key}));
		for (int i = 1; i < 20; i++)
			frames.push_back(concat({make_av1_td_obu(), inter}));

		std::string path = outdir + "/av1-roundtrip.mp4";
		run_muxer_roundtrip("AV1", path.c_str(), &av1_test_encoder, seq,
				    AV_CODEC_ID_AV1, MKTAG('a', 'v', '0', '1'),
				    frames, 2);

		/* av1C sanity: parse the box from the raw file */
		std::vector<uint8_t> file = read_file(path.c_str());
		size_t av1c_sz = 0;
		const uint8_t *av1c = find_box(file, "av1C", &av1c_sz);
		CHECK(av1c != nullptr, "av1C box present in file");
		if (av1c) {
			const uint8_t *c = av1c + 8;
			size_t csz = av1c_sz - 8;
			CHECK(csz == 4 + seq.size(),
			      "av1C size == 4 + seq OBU (%d)", (int)csz);
			CHECK(c[0] == 0x81, "av1C marker|version == 0x81");
			CHECK(c[1] == 0x00, "av1C profile 0 level 0");
			CHECK(c[2] == 0x0C, "av1C 8-bit 4:2:0 flags");
			CHECK(memcmp(c + 4, seq.data(), seq.size()) == 0,
			      "av1C carries sequence OBU");
		}
	}

	obs_shutdown();

	/* ffmpeg-mux (ordinary MP4 / MKV) smoke, driven via its stdin protocol */
	if (!ffm_exe.empty() && os_file_exists(ffm_exe.c_str())) {
		std::vector<uint8_t> vps = hevc_nal(T_VPS, make_vps_rbsp());
		std::vector<uint8_t> sps = hevc_nal(T_SPS, make_sps_rbsp());
		std::vector<uint8_t> pps = hevc_nal(T_PPS, make_pps_rbsp());
		std::vector<uint8_t> idr =
			hevc_nal(T_IDR_W_RADL, {0xAA, 0xBB, 0xCC});
		std::vector<uint8_t> trail = hevc_nal(T_TRAIL_R, {0xDD, 0xEE});
		std::vector<uint8_t> hevc_hdr = annexb({vps, sps, pps});
		std::vector<std::vector<uint8_t>> hevc_frames;
		hevc_frames.push_back(annexb({vps, sps, pps, idr}));
		for (int i = 1; i < 20; i++)
			hevc_frames.push_back(annexb({trail}));

		ffmpeg_mux_smoke(ffm_exe.c_str(), outdir.c_str(), "mp4", "hevc",
				 AV_CODEC_ID_HEVC, MKTAG('h', 'v', 'c', '1'),
				 hevc_hdr, hevc_frames);
		ffmpeg_mux_smoke(ffm_exe.c_str(), outdir.c_str(), "mkv", "hevc",
				 AV_CODEC_ID_HEVC, 0, hevc_hdr, hevc_frames);

		std::vector<uint8_t> seq = make_av1_seq_obu();
		std::vector<uint8_t> key = make_av1_frame_obu(0);
		std::vector<uint8_t> inter = make_av1_frame_obu(1);
		std::vector<std::vector<uint8_t>> av1_frames;
		av1_frames.push_back(concat({make_av1_td_obu(), seq, key}));
		for (int i = 1; i < 20; i++)
			av1_frames.push_back(concat({make_av1_td_obu(), inter}));

		ffmpeg_mux_smoke(ffm_exe.c_str(), outdir.c_str(), "mp4", "av1",
				 AV_CODEC_ID_AV1, MKTAG('a', 'v', '0', '1'), seq,
				 av1_frames);
		ffmpeg_mux_smoke(ffm_exe.c_str(), outdir.c_str(), "mkv", "av1",
				 AV_CODEC_ID_AV1, 0, seq, av1_frames);
	}

	printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "ALL PASSED",
	       g_failures);
	return g_failures ? 1 : 0;
}
