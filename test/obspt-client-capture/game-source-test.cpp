#include <windows.h>
#include <obs.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <util/platform.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc, char **argv)
{
    wchar_t desktop[256]; DWORD bytes = 0;
    CHECK(GetUserObjectInformationW(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, desktop, sizeof(desktop), &bytes));
    CHECK(wcsncmp(desktop,L"CBZCaptureTest-",15)==0);
    CHECK(obs_startup("en-US",nullptr,nullptr));
    obs_video_info video = {};
    video.graphics_module="libobs-d3d11.dll"; video.fps_num=480; video.fps_den=1;
    video.base_width=video.output_width=1920; video.base_height=video.output_height=1080;
    video.output_format=VIDEO_FORMAT_NV12; video.colorspace=VIDEO_CS_709;
    video.range=VIDEO_RANGE_PARTIAL; video.scale_type=OBS_SCALE_BILINEAR; video.gpu_conversion=true;
    CHECK(obs_reset_video(&video)==OBS_VIDEO_SUCCESS);
    obs_module_t *module=nullptr;
    CHECK(obs_open_module(&module,"../obs-plugins/win-capture.dll","../data/obs-plugins/win-capture")==MODULE_SUCCESS);
    CHECK(obs_init_module(module));
    obs_data_t *settings=obs_data_create();
    obs_data_set_string(settings,"capture_mode","window");
    obs_data_set_string(settings,"window","CBZ capture fixture:CBZCaptureFixture:java.exe");
    obs_data_set_bool(settings,"capture_cursor",true);
    obs_source_t *source=obs_source_create("game_capture","CBZ integration",settings,nullptr);
    CHECK(source);
    obs_source_inc_showing(source);
    for (int i=0;i<150 && !obs_source_get_width(source);i++) Sleep(100);
    CHECK(obs_source_get_width(source)>0);
    obs_properties_t *props=obs_source_properties(source);
    obs_property_t *status=obs_properties_get(props,"obspt_capture_status");
    CHECK(status);
    printf("STATUS: %s\n",obs_property_description(status));
    CHECK(strstr(obs_property_description(status),"ClientCapture.")==nullptr);
    obs_properties_destroy(props);
    obs_enter_graphics();
    auto *render=gs_texrender_create(GS_BGRA,GS_ZS_NONE);
    auto *stage=gs_stagesurface_create(64,64,GS_BGRA);
    CHECK(gs_texrender_begin(render,64,64));
    gs_ortho(0,32,0,32,-100,100);
    obs_source_video_render(source);
    gs_texrender_end(render);
    gs_stage_texture(stage,gs_texrender_get_texture(render));
    uint8_t *pixels=nullptr; uint32_t stride=0;
    CHECK(gs_stagesurface_map(stage,&pixels,&stride));
    CHECK(pixels[32*stride+32*4+2]==255);
    gs_stagesurface_unmap(stage); gs_stagesurface_destroy(stage); gs_texrender_destroy(render);
    obs_leave_graphics();
    obs_output_t *recording = nullptr;
    obs_encoder_t *encoder = nullptr;
    if (argc >= 2) {
        obs_module_t *ffmpeg = nullptr, *outputs = nullptr;
        CHECK(obs_open_module(&ffmpeg,"../obs-plugins/obs-ffmpeg.dll","../data/obs-plugins/obs-ffmpeg")==MODULE_SUCCESS);
        CHECK(obs_init_module(ffmpeg));
        CHECK(obs_open_module(&outputs,"../obs-plugins/obs-outputs.dll","../data/obs-plugins/obs-outputs")==MODULE_SUCCESS);
        CHECK(obs_init_module(outputs));
        auto *config = obs_data_create_from_json("{\"rate_control\":\"CQP\",\"cqp\":27,\"preset2\":\"p1\",\"tune\":\"hq\",\"multipass\":\"disabled\",\"profile\":\"main\",\"keyint_sec\":0,\"bf\":0}");
        encoder = obs_video_encoder_create("jim_hevc_nvenc","capture recording",config,nullptr);
        obs_data_release(config); CHECK(encoder);
        obs_encoder_set_video(encoder,obs_get_video());
        config = obs_data_create(); obs_data_set_string(config,"path",argv[1]);
        recording = obs_output_create("mp4_output","capture recording",config,nullptr);
        obs_data_release(config); CHECK(recording);
        obs_output_set_video_encoder(recording,encoder);
        obs_set_output_source(0,source);
        CHECK(obs_output_start(recording));
        Sleep(2000);
    }
    obs_source_dec_showing(source); Sleep(250);
    obs_source_inc_showing(source);
    Sleep(1000);
    props=obs_source_properties(source);
    CHECK(strstr(obs_property_description(obs_properties_get(props,"obspt_capture_status")),"active"));
    obs_properties_destroy(props);
    HWND fixture = FindWindowW(L"CBZCaptureFixture", L"CBZ capture fixture");
    CHECK(fixture);
    // Surface resets are not failures and must not exhaust the retry budget.
    for (int i = 0; i < 3; ++i) {
        CHECK(PostMessageW(fixture, WM_APP + 34, 0, 0));
        Sleep(250);
        bool active = false;
        for (int attempt = 0; attempt < 80; ++attempt) {
            props = obs_source_properties(source);
            active = strstr(obs_property_description(obs_properties_get(props, "obspt_capture_status")), "active") != nullptr;
            obs_properties_destroy(props);
            if (active) break;
            Sleep(100);
        }
        CHECK(active);
    }
    bool disconnect = argc >= 3 && strcmp(argv[2], "disconnect") == 0;
    if (disconnect) {
        CHECK(PostMessageW(fixture, WM_APP + 33, 0, 0));
        Sleep(5500);
    } else {
        obs_data_set_bool(settings,"obspt_force_legacy_capture",true);
        obs_source_update(source,settings);
        Sleep(3500);
    }
    props=obs_source_properties(source);
    status=obs_properties_get(props,"obspt_capture_status");
    CHECK(status);
    CHECK(strstr(obs_property_description(status), disconnect ? "no cooperative endpoint" : "forced"));
    printf("FALLBACK_STATUS: %s\n",obs_property_description(status));
    obs_properties_destroy(props);
    if (!disconnect) {
        obs_data_set_bool(settings, "obspt_force_legacy_capture", false);
        obs_source_update(source, settings);
        bool restored = false;
        for (int i = 0; i < 100; ++i) {
            Sleep(100);
            props = obs_source_properties(source);
            restored = strstr(obs_property_description(obs_properties_get(props, "obspt_capture_status")), "active") != nullptr;
            obs_properties_destroy(props);
            if (restored) break;
        }
        CHECK(restored);
    }
    if (recording) {
        CHECK(obs_output_active(recording));
        CHECK(obs_output_get_total_frames(recording)>0);
        printf("RECORDED_FRAMES: %d\n",obs_output_get_total_frames(recording));
        obs_output_stop(recording);
        for(int i=0;i<100 && obs_output_active(recording);i++) Sleep(50);
        CHECK(!obs_output_active(recording));
        obs_set_output_source(0,nullptr);
        obs_output_release(recording); obs_encoder_release(encoder);
    }
    obs_source_dec_showing(source); obs_source_release(source); obs_data_release(settings);
    obs_shutdown();
    puts("PASS game_capture module: discovery/render/cursor/visibility/reconnect/forced fallback");
}
