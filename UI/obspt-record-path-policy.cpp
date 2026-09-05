#include "obspt-record-path-policy.hpp"

#include <cstring>

const char *const kOBSPTPackagedRecordPathPlaceholder = "recordings";

bool OBSPTRecordPathNeedsDefault(const char *current)
{
	if (!current || !*current)
		return true;

	/* Compared byte-for-byte on purpose: only the bare packaged
	 * placeholder counts. "E:/recordings" and "recordings/potpvp" are
	 * user choices and must survive untouched. */
	return strcmp(current, kOBSPTPackagedRecordPathPlaceholder) == 0;
}

OBSPTRecordPathPlan OBSPTPlanRecordPaths(const char *simpleOutput,
					 const char *advancedRecording,
					 const char *customFFmpeg)
{
	OBSPTRecordPathPlan plan;
	plan.writeSimpleOutput = OBSPTRecordPathNeedsDefault(simpleOutput);
	plan.writeAdvancedRecording =
		OBSPTRecordPathNeedsDefault(advancedRecording);
	plan.writeCustomFFmpeg = OBSPTRecordPathNeedsDefault(customFFmpeg);
	return plan;
}
