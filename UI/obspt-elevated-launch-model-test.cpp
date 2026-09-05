#include "obspt-elevated-launch-model.hpp"

#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (condition)
		return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}

void TestDecision()
{
	Check(DecideOBSPTElevatedLaunch(false, false) ==
		      OBSPTElevatedLaunchAction::ContinueCurrentProcess,
	      "disabled preference continues without UAC");
	Check(DecideOBSPTElevatedLaunch(true, false) ==
		      OBSPTElevatedLaunchAction::RequestElevation,
	      "enabled non-elevated launch requests UAC");
	Check(DecideOBSPTElevatedLaunch(true, true) ==
		      OBSPTElevatedLaunchAction::ContinueCurrentProcess,
	      "already elevated launch cannot loop");
	Check(ShouldExitAfterElevationAttempt(
		      OBSPTElevationAttemptResult::StartedElevatedProcess),
	      "accepted UAC exits the original process");
	Check(!ShouldExitAfterElevationAttempt(
		      OBSPTElevationAttemptResult::RefusedOrFailed),
	      "refused UAC continues the original process");
}

void TestRelaunchContext()
{
	const std::vector<std::wstring> arguments = {
		L"--profile",      L"Pot PvP", L"--collection",
		L"Quoted \"Set\"", L"",        L"C:\\capture path\\"};
	const auto request = BuildOBSPTElevatedLaunchRequest(
		L"K:\\OBS-PT\\bin\\64bit\\OBS-PT.exe", arguments,
		L"K:\\OBS-PT\\bin\\64bit");

	Check(request.executable == L"K:\\OBS-PT\\bin\\64bit\\OBS-PT.exe",
	      "executable is preserved");
	Check(request.workingDirectory == L"K:\\OBS-PT\\bin\\64bit",
	      "working directory is preserved");
	Check(request.arguments ==
		      L"--profile \"Pot PvP\" --collection \"Quoted \\\"Set\\\"\" "
		      L"\"\" \"C:\\capture path\\\\\"",
	      "argument order and Windows quoting are preserved");
}

struct ExecutorState {
	int calls = 0;
	bool result = false;
	OBSPTElevatedLaunchRequest request;
};

bool FakeExecutor(const OBSPTElevatedLaunchRequest &request, void *context)
{
	auto *state = static_cast<ExecutorState *>(context);
	++state->calls;
	state->request = request;
	return state->result;
}

void TestElevationExecution()
{
	const auto request = BuildOBSPTElevatedLaunchRequest(
		L"K:\\OBS-PT\\OBS-PT.exe", {L"--profile", L"Pot PvP"},
		L"K:\\OBS-PT");
	ExecutorState state;

	Check(!ExecuteOBSPTElevatedLaunch(request, FakeExecutor, &state),
	      "refused or failed elevation continues the original process");
	Check(state.calls == 1, "refused elevation is attempted exactly once");
	Check(state.request.arguments == L"--profile \"Pot PvP\"",
	      "executor receives the complete relaunch request");

	state.calls = 0;
	state.result = true;
	Check(ExecuteOBSPTElevatedLaunch(request, FakeExecutor, &state),
	      "accepted elevation exits the original process");
	Check(state.calls == 1, "accepted elevation is attempted exactly once");
	Check(!ExecuteOBSPTElevatedLaunch(request, nullptr),
	      "missing executor continues the original process");
}

} // namespace

int main()
{
	TestDecision();
	TestRelaunchContext();
	TestElevationExecution();
	if (failures == 0)
		std::cout << "All OBS-PT elevated launch model tests passed\n";
	return failures == 0 ? 0 : 1;
}
