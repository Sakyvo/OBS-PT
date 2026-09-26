#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Current declared-repair generation. Single source of truth for both the
 * bootstrap and the decision model - a second copy silently drifts and makes
 * a repair that "bumped the version" never actually run.
 *
 * v4 (issue 059): enc-amf -> texture-amf migration.
 * v5 (issue 066): recording default moves to HEVC; streaming keeps H.264.
 * v6 (issue 073): recording keyframe interval 0; streaming CBR 4500. */
#define OBSPT_BOOTSTRAP_VERSION 6

/* Installer-driven default replacement (issue 075, ADR 0009).
 *
 * A fourth configuration-write semantics, alongside Preset Setting,
 * Force-Override Setting and Declared Repair: the installer records the
 * previously installed product version's relationship to this build, and
 * the application performs one silent replacement on the next launch.
 *
 * The marker is deliberately NOT BootstrapVersion - that axis carries the
 * v5 collateral of forcing PotPvP and FirstRun, which this replacement
 * must not do. Missing marker reads as 0, so a tree the installer never
 * saw (or one installed by a build predating the marker) still migrates
 * exactly once.
 *
 * 0 = migration pending; >= 1 = already applied. */
#define OBSPT_INSTALLER_MIGRATION_VERSION 1

/* Declared-repair decision model.
 *
 * Pure decision logic extracted from the bootstrap so the migration table
 * can be unit-tested without the Qt/obs runtime. */

/* The monitor-video repair (resolution write-back) belongs to the v3
 * semantics. A later repair must not re-run it, so it is gated on the tree
 * coming from a version older than 3. */
bool obspt_bootstrap_needs_video_repair(int previous_bootstrap_version);

/* The encoder repair re-probes the encoder and rewrites the Profile's
 * encoder selection. Returns true when the repair applies to this tree
 * (i.e. the stored bootstrap version is below the current one, or first-run
 * has not completed).
 *
 * v4 (issue 059): completes the enc-amf -> texture-amf migration.
 * v5 (issue 066): moves the recording default to HEVC; streaming keeps
 * H.264. This rewrites a v4 tree's recording encoder even when the user
 * picked H.264 by hand.
 * v6 (issue 073): recording keyframe interval 0, streaming CBR 4500. This
 * rewrites a v5 tree's recording AND streaming encoder JSON - including a
 * user's manually tuned recording keyframe interval or streaming bitrate. */
bool obspt_bootstrap_needs_encoder_repair(int previous_bootstrap_version,
					  bool first_run_completed);

/* Legacy AMD encoder id migration: the enc-amf plugin is retired, so a
 * stored `amd_amf_h264` selection can no longer start. The repair rewrites
 * it to the texture-amf id. Other ids are re-probed rather than mapped. */
bool obspt_amf_id_is_legacy(const char *encoder_id);

/* Recording defaults to a HEVC hardware encoder where one is registered;
 * streaming deliberately stays H.264. Returns the n-th HEVC encoder id in
 * preference order, or NULL when the list is exhausted. */
const char *obspt_recording_hevc_preference(int index);

/* Recording keyframe interval, in seconds, for every encoder template.
 * 0 means "let the encoder pick" (NVENC's own default), which is what
 * OBS-PT ships: a 480 fps frame-mixing workflow does not want an
 * artificial 2-second IDR cadence. */
int obspt_recording_keyint_sec(void);

/* Streaming target bitrate, in Kbps, for the shipped CBR default. */
int obspt_streaming_bitrate_kbps(void);

/* True when the installer-driven default replacement has not yet run on
 * this tree. `stored_version` is [OBSPT] InstallerMigrationVersion with a
 * missing key read as 0. A version at or above the current one (including
 * a downgrade marker written by a newer installer) never migrates. */
bool obspt_installer_migration_needed(int stored_version);

#ifdef __cplusplus
}
#endif
