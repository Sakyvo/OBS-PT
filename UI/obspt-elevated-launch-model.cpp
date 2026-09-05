#include "obspt-elevated-launch-model.hpp"

OBSPTElevatedLaunchAction DecideOBSPTElevatedLaunch(bool enabled,
						    bool alreadyElevated)
{
	return enabled && !alreadyElevated
		       ? OBSPTElevatedLaunchAction::RequestElevation
		       : OBSPTElevatedLaunchAction::ContinueCurrentProcess;
}

bool ShouldExitAfterElevationAttempt(OBSPTElevationAttemptResult result)
{
	return result == OBSPTElevationAttemptResult::StartedElevatedProcess;
}

std::wstring QuoteOBSPTWindowsArgument(const std::wstring &argument)
{
	if (!argument.empty() &&
	    argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
		return argument;

	std::wstring quoted = L"\"";
	size_t backslashes = 0;
	for (wchar_t ch : argument) {
		if (ch == L'\\') {
			++backslashes;
			continue;
		}

		if (ch == L'\"') {
			quoted.append(backslashes * 2 + 1, L'\\');
			quoted.push_back(ch);
		} else {
			quoted.append(backslashes, L'\\');
			quoted.push_back(ch);
		}
		backslashes = 0;
	}
	quoted.append(backslashes * 2, L'\\');
	quoted.push_back(L'\"');
	return quoted;
}

OBSPTElevatedLaunchRequest
BuildOBSPTElevatedLaunchRequest(const std::wstring &executable,
				const std::vector<std::wstring> &arguments,
				const std::wstring &workingDirectory)
{
	OBSPTElevatedLaunchRequest request;
	request.executable = executable;
	request.workingDirectory = workingDirectory;
	for (const std::wstring &argument : arguments) {
		if (!request.arguments.empty())
			request.arguments.push_back(L' ');
		request.arguments += QuoteOBSPTWindowsArgument(argument);
	}
	return request;
}

bool ExecuteOBSPTElevatedLaunch(const OBSPTElevatedLaunchRequest &request,
				OBSPTElevationExecutor executor, void *context)
{
	if (!executor)
		return false;
	return ShouldExitAfterElevationAttempt(
		executor(request, context)
			? OBSPTElevationAttemptResult::StartedElevatedProcess
			: OBSPTElevationAttemptResult::RefusedOrFailed);
}
