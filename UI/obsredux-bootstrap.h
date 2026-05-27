#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OBSREDUX_BOOTSTRAP_VERSION 2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct config_data config_t;

typedef enum {
	WRITE_PROBE_OK = 0,
	WRITE_PROBE_SYSTEM_PROTECTED,
	WRITE_PROBE_READ_ONLY_VOLUME,
	WRITE_PROBE_ANTIVIRUS_BLOCKED,
	WRITE_PROBE_UNKNOWN,
} write_probe_result_t;

write_probe_result_t probe_install_root_writable(const char *install_root);

typedef struct {
	const char *encoder_id;
	bool is_software_fallback;
} encoder_probe_result_t;

/* MUST be called AFTER obs_load_all_modules() has finished, otherwise
   encoder plugins are not yet registered and the probe will always fall
   through to obs_x264. */
encoder_probe_result_t probe_record_encoder(void);

typedef struct {
	int64_t (*get_current_time_sec)(void);
	int (*list_files)(const char *dir, const char *glob, char ***out,
			  size_t *count);
	int (*get_mtime_sec)(const char *path, int64_t *out);
	int (*unlink_file)(const char *path);
} sweep_deps_t;

int sweep_retention(const char *dir, int retention_days, const char *glob,
		    const sweep_deps_t *deps);

bool is_first_run(void);
bool validate_obsredux_preset_files(char *failed_path,
				    size_t failed_path_size);
void prepare_obsredux_global_config(config_t *global_config);
int apply_encoder_to_profile(const char *profile_name,
			      const char *encoder_id);
void apply_record_path_to_config(config_t *cfg, const char *abs_path);
int apply_record_path_to_profile(const char *profile_name,
				  const char *abs_path);
void mark_first_run_completed(void);

#ifdef __cplusplus
}

void ShowWritePermissionFailureDialog(write_probe_result_t reason);
void ShowPresetIntegrityFailureDialog(const char *failed_path);
void ShowFirstRunRecommendationsDialog(bool is_software_encoder);
bool run_obsredux_early_bootstrap(config_t *global_config);
void run_first_run_bootstrap_if_needed(const char *active_profile_name,
				       config_t *active_config);
#endif
