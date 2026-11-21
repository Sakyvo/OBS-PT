#pragma once
#include <stdint.h>

#define CBZ_CAPTURE_VERSION 1
#define CBZ_CAPTURE_MAGIC 0x315a4243u
#define CBZ_CAPTURE_SLOTS 3
#define CBZ_CAPTURE_FORMAT 87 /* DXGI_FORMAT_B8G8R8A8_UNORM */
#define CBZ_CAPTURE_OPEN (0x8000 + 0x431)
#define CBZ_CAPTURE_PULSE (0x8000 + 0x432)
#define CBZ_CAPTURE_CLOSE (0x8000 + 0x433)

enum cbz_capture_status {
    CBZ_CAPTURE_WAITING, CBZ_CAPTURE_READY, CBZ_CAPTURE_CLOSED,
    CBZ_CAPTURE_VERSION_ERROR, CBZ_CAPTURE_IDENTITY_ERROR,
    CBZ_CAPTURE_ADAPTER_ERROR, CBZ_CAPTURE_FORMAT_ERROR,
    CBZ_CAPTURE_BUSY, CBZ_CAPTURE_CADENCE_ERROR,
    CBZ_CAPTURE_GPU_ERROR, CBZ_CAPTURE_SURFACE_RESET
};

/* Fixed-width, naturally aligned Windows x64 wire layout. No process pointers. */
struct cbz_capture_packet {
    uint32_t magic, version, bytes, producer_pid;
    uint64_t producer_started;
    uint32_t consumer_pid, format;
    uint64_t consumer_started, adapter_luid, interval_ns, session_id;
    uint32_t status, generation, width, height;
    uint64_t handles[CBZ_CAPTURE_SLOTS];
    uint64_t sequence[CBZ_CAPTURE_SLOTS];
    uint64_t produced, skipped, overwritten;
    uint64_t consumer_sequence;
};
