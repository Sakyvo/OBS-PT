#include <windows.h>
#include <obs.h>
#include <cstdio>
#include "client-capture.h"
#include "client-capture-policy.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv)
{
    CHECK(client_capture_should_attempt(false, false, 0));
    CHECK(client_capture_should_attempt(false, false, 1));
    CHECK(!client_capture_should_attempt(false, false, 2));
    CHECK(!client_capture_should_attempt(true, false, 0));
    CHECK(!client_capture_should_attempt(false, true, 0));
    CHECK(obs_startup("en-US", nullptr, nullptr));
    obs_video_info video = {};
    video.graphics_module = "libobs-d3d11.dll";
    video.fps_num = 480; video.fps_den = 1;
    video.base_width = video.output_width = 64;
    video.base_height = video.output_height = 64;
    video.output_format = VIDEO_FORMAT_NV12;
    video.colorspace = VIDEO_CS_709; video.range = VIDEO_RANGE_PARTIAL;
    video.scale_type = OBS_SCALE_BILINEAR; video.gpu_conversion = true;
    CHECK(obs_reset_video(&video) == OBS_VIDEO_SUCCESS);
    obs_enter_graphics();
    const char *reason = nullptr;
    CHECK(client_capture_open(GetCurrentProcessId(), 2083333, &reason) == nullptr);
    CHECK(reason && strcmp(reason, "ClientCapture.NoEndpoint") == 0);
    if (argc == 2) {
        auto *capture = client_capture_open(strtoul(argv[1], nullptr, 10), 2083333, &reason);
        if (!capture) fprintf(stderr, "Open failed: %s\n", reason);
        CHECK(capture);
        gs_texture_t *frame = nullptr;
        bool received = false;
        for (int i = 0; i < 1500; ++i) {
            auto result = client_capture_tick(capture, 2083333, &frame);
            if (result == CLIENT_CAPTURE_LOST) fprintf(stderr, "Lost: %s\n", client_capture_reason(capture));
            CHECK(result != CLIENT_CAPTURE_LOST);
            if (result == CLIENT_CAPTURE_FRAME) { received = true; break; }
            Sleep(2);
        }
        CHECK(received && frame && gs_texture_get_width(frame) == 64);
        const char *busy_reason = nullptr;
        CHECK(client_capture_open(strtoul(argv[1], nullptr, 10), 2083333, &busy_reason) == nullptr);
        if (strcmp(busy_reason, "ClientCapture.Busy")) fprintf(stderr, "Busy rejection: %s\n", busy_reason);
        CHECK(strcmp(busy_reason, "ClientCapture.Busy") == 0);
        auto *stage = gs_stagesurface_create(64, 64, GS_BGRA);
        CHECK(stage);
        gs_stage_texture(stage, frame);
        uint8_t *bytes = nullptr; uint32_t stride = 0;
        CHECK(gs_stagesurface_map(stage, &bytes, &stride));
        const uint8_t *pixel = bytes + stride * 32 + 4 * 32;
        CHECK(pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 255);
        gs_stagesurface_unmap(stage); gs_stagesurface_destroy(stage);
        uint64_t produced, skipped, overwritten;
        client_capture_counters(capture, &produced, &skipped, &overwritten);
        CHECK(produced > 0);
        bool resized = false;
        for (int i = 0; i < 2500; ++i) {
            auto result = client_capture_tick(capture, 2083333, &frame);
            CHECK(result != CLIENT_CAPTURE_LOST);
            if (result == CLIENT_CAPTURE_FRAME && gs_texture_get_width(frame) == 32) {
                resized = true; break;
            }
            Sleep(2);
        }
        CHECK(resized);
        client_capture_close(capture);
        Sleep(100);
        capture = client_capture_open(strtoul(argv[1], nullptr, 10), 2083333, &reason);
        CHECK(capture);
        received = false;
        for (int i = 0; i < 1500; ++i) {
            auto result = client_capture_tick(capture, 2083333, &frame);
            CHECK(result != CLIENT_CAPTURE_LOST);
            if (result == CLIENT_CAPTURE_FRAME) { received = true; break; }
            Sleep(2);
        }
        CHECK(received);
        client_capture_close(capture);
        puts("PASS real Java producer -> NT handle -> libobs texture pixels/resize/reconnect");
    }
    obs_leave_graphics();
    obs_shutdown();
    puts("PASS absent cooperative endpoint is rejected before injection");
    return 0;
}
