#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <graphics/graphics.h>

#ifdef __cplusplus
extern "C" {
#endif

struct client_capture;
enum client_capture_result { CLIENT_CAPTURE_WAITING, CLIENT_CAPTURE_FRAME, CLIENT_CAPTURE_RESTART, CLIENT_CAPTURE_LOST };
/* Called inside the OBS graphics context. No injection and no pixel readback. */
struct client_capture *client_capture_open(uint32_t pid, uint64_t interval_ns, const char **reason);
enum client_capture_result client_capture_tick(struct client_capture *, uint64_t interval_ns, gs_texture_t **frame);
void client_capture_close(struct client_capture *);
const char *client_capture_reason(const struct client_capture *);
void client_capture_counters(const struct client_capture *, uint64_t *produced, uint64_t *skipped, uint64_t *overwritten);
#ifdef __cplusplus
}
#endif
