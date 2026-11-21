#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <util/platform.h>
#include <cstdio>
#include <cwchar>
#include <sddl.h>
#include <vector>
#include <string>
#include "client-capture.h"
#include "client-capture-protocol.h"
using Microsoft::WRL::ComPtr;

struct client_capture {
    HANDLE process = nullptr, map = nullptr;
    HWND endpoint = nullptr;
    cbz_capture_packet *packet = nullptr;
    gs_texture_t *slots[CBZ_CAPTURE_SLOTS] = {}, *frame = nullptr;
    uint64_t session_id = 0, consumer_started = 0, last_pulse = 0, last_frame = 0, sequence = 0;
    uint32_t generation = 0;
    bool session_open = false, frame_valid = false;
    const char *reason = "ClientCapture.Waiting";
};

static uint64_t started(HANDLE process)
{
    FILETIME c, e, k, u;
    if (!GetProcessTimes(process, &c, &e, &k, &u)) return 0;
    return (uint64_t(c.dwHighDateTime) << 32) | c.dwLowDateTime;
}

static const char *status_reason(uint32_t status)
{
    switch (status) {
    case CBZ_CAPTURE_VERSION_ERROR: return "ClientCapture.Version";
    case CBZ_CAPTURE_IDENTITY_ERROR: return "ClientCapture.Identity";
    case CBZ_CAPTURE_ADAPTER_ERROR: return "ClientCapture.Adapter";
    case CBZ_CAPTURE_FORMAT_ERROR: return "ClientCapture.Format";
    case CBZ_CAPTURE_BUSY: return "ClientCapture.Busy";
    case CBZ_CAPTURE_CADENCE_ERROR: return "ClientCapture.Cadence";
    case CBZ_CAPTURE_GPU_ERROR: return "ClientCapture.GPU";
    default: return "ClientCapture.SessionLost";
    }
}

static void clear_textures(client_capture *c)
{
    for (auto &slot : c->slots) { gs_texture_destroy(slot); slot = nullptr; }
    gs_texture_destroy(c->frame); c->frame = nullptr;
    c->frame_valid = false;
}

void client_capture_close(client_capture *c)
{
    if (!c) return;
    if (c->endpoint && c->session_open)
        PostMessageW(c->endpoint, CBZ_CAPTURE_CLOSE, GetCurrentProcessId(), LPARAM(c->session_id));
    clear_textures(c);
    if (c->packet) UnmapViewOfFile(c->packet);
    if (c->map) CloseHandle(c->map);
    if (c->process) CloseHandle(c->process);
    delete c;
}

static HANDLE create_channel(const wchar_t *name)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return nullptr;
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    std::vector<unsigned char> user(bytes);
    bool found = GetTokenInformation(token, TokenUser, user.data(), bytes, &bytes) != FALSE;
    CloseHandle(token);
    if (!found) return nullptr;
    wchar_t *sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(user.data())->User.Sid, &sid)) return nullptr;
    // Same user only. Medium integrity allows a non-elevated CBZ to answer elevated OBS.
    std::wstring acl = L"D:P(A;;GA;;;";
    acl += sid; acl += L")S:(ML;;NW;;;ME)";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return nullptr;
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), descriptor, FALSE};
    HANDLE result = CreateFileMappingW(INVALID_HANDLE_VALUE, &attributes, PAGE_READWRITE, 0, sizeof(cbz_capture_packet), name);
    DWORD error = GetLastError();
    LocalFree(descriptor);
    SetLastError(error);
    return result;
}

client_capture *client_capture_open(uint32_t pid, uint64_t interval, const char **reason)
{
    *reason = "ClientCapture.Identity";
    auto *c = new client_capture;
    c->process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_DUP_HANDLE | SYNCHRONIZE, FALSE, pid);
    auto fail = [&]() -> client_capture * { client_capture_close(c); return nullptr; };
    if (!c->process) return fail();
    uint64_t birth = started(c->process);
    if (!birth) return fail();
    wchar_t name[192];
    swprintf_s(name, L"CBZCapture.v1.%lu.%llu", DWORD(pid), birth);
    c->endpoint = FindWindowExW(HWND_MESSAGE, nullptr, L"CBZCapture.v1", name);
    if (!c->endpoint) { *reason = "ClientCapture.NoEndpoint"; return fail(); }
    DWORD owner = 0;
    GetWindowThreadProcessId(c->endpoint, &owner);
    wchar_t executable[32768]; DWORD size = _countof(executable);
    if (owner != pid || !QueryFullProcessImageNameW(c->process, 0, executable, &size)) return fail();
    const wchar_t *base = wcsrchr(executable, L'\\'); base = base ? base + 1 : executable;
    if (_wcsicmp(base, L"java.exe") && _wcsicmp(base, L"javaw.exe")) return fail();
    *reason = "ClientCapture.Adapter";
    if (gs_get_device_type() != GS_DEVICE_DIRECT3D_11) return fail();
    auto *device = static_cast<ID3D11Device *>(gs_get_device_obj());
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; DXGI_ADAPTER_DESC desc;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgi))) || FAILED(dxgi->GetAdapter(&adapter)) ||
        FAILED(adapter->GetDesc(&desc)) || desc.VendorId != 0x10de) return fail();
    c->consumer_started = started(GetCurrentProcess());
    static volatile LONG64 counter = 0;
    LARGE_INTEGER clock;
    QueryPerformanceCounter(&clock);
    c->session_id = uint64_t(clock.QuadPart) + uint64_t(InterlockedIncrement64(&counter));
    swprintf_s(name, L"Local\\CBZCapture.v1.%lu.%llu.%lu.%llu.%llu", DWORD(pid), birth,
        GetCurrentProcessId(), c->consumer_started, c->session_id);
    *reason = "ClientCapture.Handshake";
    c->map = create_channel(name);
    if (!c->map) return fail();
    if (GetLastError() == ERROR_ALREADY_EXISTS) { *reason = "ClientCapture.Busy"; return fail(); }
    c->packet = static_cast<cbz_capture_packet *>(MapViewOfFile(c->map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(cbz_capture_packet)));
    if (!c->packet) return fail();
    auto &p = *c->packet;
    p = {};
    p.magic = CBZ_CAPTURE_MAGIC; p.version = CBZ_CAPTURE_VERSION; p.bytes = sizeof(p);
    p.producer_pid = pid; p.producer_started = birth;
    p.consumer_pid = GetCurrentProcessId(); p.consumer_started = c->consumer_started;
    p.session_id = c->session_id;
    p.format = CBZ_CAPTURE_FORMAT; p.interval_ns = interval;
    p.adapter_luid = (uint64_t(uint32_t(desc.AdapterLuid.HighPart)) << 32) | desc.AdapterLuid.LowPart;
    DWORD_PTR result = CBZ_CAPTURE_CLOSED;
    if (!SendMessageTimeoutW(c->endpoint, CBZ_CAPTURE_OPEN, GetCurrentProcessId(), LPARAM(c->session_id),
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result)) {
        // The producer may have accepted after the sender timed out.
        PostMessageW(c->endpoint, CBZ_CAPTURE_CLOSE, GetCurrentProcessId(), LPARAM(c->session_id));
        return fail();
    }
    if (result != CBZ_CAPTURE_WAITING) { *reason = status_reason(uint32_t(result)); return fail(); }
    c->session_open = true;
    c->last_pulse = c->last_frame = os_gettime_ns();
    *reason = "ClientCapture.Waiting";
    return c;
}

client_capture_result client_capture_tick(client_capture *c, uint64_t interval, gs_texture_t **frame)
{
    *frame = c->frame_valid ? c->frame : nullptr;
    uint64_t now = os_gettime_ns();
    if (WaitForSingleObject(c->process, 0) != WAIT_TIMEOUT || !IsWindow(c->endpoint)) {
        c->reason = "ClientCapture.SessionLost"; return CLIENT_CAPTURE_LOST;
    }
    if (now - c->last_pulse >= 500000000ull) {
        InterlockedExchange64(reinterpret_cast<volatile LONG64 *>(&c->packet->interval_ns), interval);
        if (!PostMessageW(c->endpoint, CBZ_CAPTURE_PULSE, GetCurrentProcessId(), LPARAM(c->session_id))) {
            c->reason = "ClientCapture.SessionLost"; return CLIENT_CAPTURE_LOST;
        }
        c->last_pulse = now;
    }
    auto &p = *c->packet;
    uint32_t status = uint32_t(InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&p.status), 0, 0));
    if (status == CBZ_CAPTURE_SURFACE_RESET) {
        c->reason = "ClientCapture.Waiting";
        return CLIENT_CAPTURE_RESTART;
    }
    if (status != CBZ_CAPTURE_WAITING && status != CBZ_CAPTURE_READY) {
        c->reason = status_reason(status); return CLIENT_CAPTURE_LOST;
    }
    if (now - c->last_frame > 5000000000ull) { c->reason = "ClientCapture.Timeout"; return CLIENT_CAPTURE_LOST; }
    if (status != CBZ_CAPTURE_READY) return CLIENT_CAPTURE_WAITING;
    const uint32_t generation = p.generation, width = p.width, height = p.height;
    if (p.magic != CBZ_CAPTURE_MAGIC || p.version != CBZ_CAPTURE_VERSION || p.bytes != sizeof(p) ||
        p.format != CBZ_CAPTURE_FORMAT || !width || !height || width > 16384 || height > 16384) {
        c->reason = "ClientCapture.Format"; return CLIENT_CAPTURE_LOST;
    }
    if (c->generation != generation) {
        clear_textures(c); *frame = nullptr;
        for (unsigned i = 0; i < CBZ_CAPTURE_SLOTS; i++) {
            HANDLE handle = nullptr;
            if (!DuplicateHandle(c->process, HANDLE(uintptr_t(p.handles[i])), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                if (p.generation != generation || p.status != CBZ_CAPTURE_READY) return CLIENT_CAPTURE_WAITING;
                c->reason = "ClientCapture.Texture"; return CLIENT_CAPTURE_LOST;
            }
            c->slots[i] = gs_texture_open_nt_shared(uint32_t(uintptr_t(handle)));
            CloseHandle(handle);
            if (!c->slots[i] || gs_texture_get_width(c->slots[i]) != width || gs_texture_get_height(c->slots[i]) != height) {
                c->reason = "ClientCapture.Texture"; return CLIENT_CAPTURE_LOST;
            }
        }
        c->frame = gs_texture_create(width, height, GS_BGRA, 1, nullptr, 0);
        if (!c->frame) { c->reason = "ClientCapture.Texture"; return CLIENT_CAPTURE_LOST; }
        if (p.generation != generation || p.status != CBZ_CAPTURE_READY) {
            clear_textures(c);
            return CLIENT_CAPTURE_WAITING;
        }
        c->generation = generation;
    }
    int newest = -1;
    uint64_t sequence = c->sequence;
    for (unsigned i = 0; i < CBZ_CAPTURE_SLOTS; ++i) {
        uint64_t value = uint64_t(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64 *>(&p.sequence[i]), 0, 0));
        if (value > sequence) { sequence = value; newest = int(i); }
    }
    if (newest < 0) return CLIENT_CAPTURE_WAITING;
    int acquired = gs_texture_acquire_sync(c->slots[newest], 0, 0);
    if (acquired == ETIMEDOUT) return CLIENT_CAPTURE_WAITING;
    if (acquired != 0) { c->reason = "ClientCapture.Texture"; return CLIENT_CAPTURE_LOST; }
    if (p.generation != generation || p.status != CBZ_CAPTURE_READY) {
        gs_texture_release_sync(c->slots[newest], 0); return CLIENT_CAPTURE_WAITING;
    }
    sequence = p.sequence[newest];
    gs_copy_texture(c->frame, c->slots[newest]);
    if (gs_texture_release_sync(c->slots[newest], 0) != 0) { c->reason = "ClientCapture.Texture"; return CLIENT_CAPTURE_LOST; }
    c->sequence = sequence;
    InterlockedExchange64(reinterpret_cast<volatile LONG64 *>(&p.consumer_sequence), sequence);
    c->last_frame = now; c->frame_valid = true; c->reason = "ClientCapture.Active";
    *frame = c->frame;
    return CLIENT_CAPTURE_FRAME;
}

const char *client_capture_reason(const client_capture *c) { return c->reason; }
void client_capture_counters(const client_capture *c, uint64_t *produced, uint64_t *skipped, uint64_t *overwritten)
{
    *produced = c->packet->produced;
    *skipped = c->packet->skipped;
    *overwritten = c->packet->overwritten;
}
