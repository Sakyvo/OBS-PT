#pragma once

/* Recording directories are user-owned Profile values. Bootstrap may
 * initialise one that was never set, but must never replace an existing
 * choice - not on first run, not on a repair, and not on a future
 * BootstrapVersion bump. The three directories are independent and are never
 * synchronised with each other. */

/* True only when the value is absent, empty, or still the relative
 * placeholder that ships inside the packaged Profile. Any other value is
 * preserved byte-for-byte: no normalisation, no trailing-separator cleanup,
 * no existence check, and no fallback when the drive is offline. */
bool OBSPTRecordPathNeedsDefault(const char *current);

/* The packaged Profile ships this relative placeholder, which bootstrap
 * expands to an absolute <Install Root>/recordings. */
extern const char *const kOBSPTPackagedRecordPathPlaceholder;

/* Which of the three independent recording directories bootstrap must
 * initialise. A directory is judged only against its own current value; an
 * empty Simple Output path never drags the Advanced or Custom FFmpeg paths
 * with it, and vice versa. */
struct OBSPTRecordPathPlan {
	bool writeSimpleOutput = false;    /* SimpleOutput/FilePath */
	bool writeAdvancedRecording = false; /* AdvOut/RecFilePath */
	bool writeCustomFFmpeg = false;    /* AdvOut/FFFilePath */
};

OBSPTRecordPathPlan OBSPTPlanRecordPaths(const char *simpleOutput,
					 const char *advancedRecording,
					 const char *customFFmpeg);
