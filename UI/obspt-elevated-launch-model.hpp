#pragma once

#include <string>
#include <vector>

enum class OBSPTElevatedLaunchAction {
	ContinueCurrentProcess,
	RequestElevation,
};

enum class OBSPTElevationAttemptResult {
	StartedElevatedProcess,
	RefusedOrFailed,
};

struct OBSPTElevatedLaunchRequest {
	std::wstring executable;
	std::wstring arguments;
	std::wstring workingDirectory;
};

using OBSPTElevationExecutor = bool (*)(
	const OBSPTElevatedLaunchRequest &request, void *context);

OBSPTElevatedLaunchAction DecideOBSPTElevatedLaunch(bool enabled,
						    bool alreadyElevated);
bool ShouldExitAfterElevationAttempt(OBSPTElevationAttemptResult result);
std::wstring QuoteOBSPTWindowsArgument(const std::wstring &argument);
OBSPTElevatedLaunchRequest
BuildOBSPTElevatedLaunchRequest(const std::wstring &executable,
				const std::vector<std::wstring> &arguments,
				const std::wstring &workingDirectory);
bool ExecuteOBSPTElevatedLaunch(const OBSPTElevatedLaunchRequest &request,
				OBSPTElevationExecutor executor,
				void *context = nullptr);
