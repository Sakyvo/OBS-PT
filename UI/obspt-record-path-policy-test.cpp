#include "obspt-record-path-policy.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const std::string &what)
{
	if (condition)
		return;
	++failures;
	std::cerr << "FAIL: " << what << "\n";
}

void ExpectPreserved(const char *value, const std::string &why)
{
	Check(!OBSPTRecordPathNeedsDefault(value),
	      "preserved: " + why + " [" + std::string(value) + "]");
}

void ExpectInitialised(const char *value, const std::string &why)
{
	Check(OBSPTRecordPathNeedsDefault(value),
	      "initialised: " + why);
}

/* An overwrite install restores the old active Profile's directories before
 * first launch. Bootstrap must leave every one of them exactly as found. */
void TestExistingChoicesArePreserved()
{
	ExpectPreserved("D:/OBS-PT/recordings", "an ordinary absolute path");
	ExpectPreserved("C:\\Users\\ASUS\\Videos",
			"a backslash path is not normalised");
	ExpectPreserved("Z:/offline/rec",
			"a path on a missing or offline drive");
	ExpectPreserved("D:/OBS-PT/recordings/",
			"a trailing separator is not cleaned up");
	ExpectPreserved("D:/My Recordings/PotPvP",
			"a path containing spaces");
	ExpectPreserved("//nas/share/obs", "a UNC network path");
	ExpectPreserved("E:/recordings",
			"an absolute path that merely ends in 'recordings'");
	ExpectPreserved("recordings/potpvp",
			"a relative path that is not the bare placeholder");
}

/* Only a genuinely unset value may be initialised, and each of the three
 * directories is judged on its own. */
void TestOnlyMissingValuesAreInitialised()
{
	ExpectInitialised(nullptr, "an absent key");
	ExpectInitialised("", "an empty value");
	ExpectInitialised(kOBSPTPackagedRecordPathPlaceholder,
			  "the packaged relative placeholder");
}

/* The three directories are independent. Restoring one must never touch the
 * others, and a partially-filled Profile initialises only its empty slots. */
void TestThreeDirectoriesStayIndependent()
{
	struct Case {
		const char *why;
		const char *simple;
		const char *advanced;
		const char *ffmpeg;
		bool wantSimple;
		bool wantAdvanced;
		bool wantFFmpeg;
	};

	const std::vector<Case> cases = {
		{"three distinct restored paths are all kept",
		 "D:/simple", "E:/advanced", "F:/ffmpeg", false, false, false},
		{"an empty Simple Output path does not drag the others",
		 "", "E:/advanced", "F:/ffmpeg", true, false, false},
		{"an empty Advanced path does not drag the others",
		 "D:/simple", "", "F:/ffmpeg", false, true, false},
		{"an empty Custom FFmpeg path does not drag the others",
		 "D:/simple", "E:/advanced", "", false, false, true},
		{"a fresh install initialises all three",
		 "recordings", "recordings", "recordings", true, true, true},
		{"absent keys initialise all three",
		 nullptr, nullptr, nullptr, true, true, true},
		{"one offline drive is still a user choice",
		 "Z:/offline", "", nullptr, false, true, true},
		{"the same path in all three slots is not deduplicated",
		 "D:/rec", "D:/rec", "D:/rec", false, false, false},
	};

	for (const Case &c : cases) {
		const OBSPTRecordPathPlan plan = OBSPTPlanRecordPaths(
			c.simple, c.advanced, c.ffmpeg);
		Check(plan.writeSimpleOutput == c.wantSimple,
		      std::string("Simple Output: ") + c.why);
		Check(plan.writeAdvancedRecording == c.wantAdvanced,
		      std::string("Advanced recording: ") + c.why);
		Check(plan.writeCustomFFmpeg == c.wantFFmpeg,
		      std::string("Custom FFmpeg: ") + c.why);
	}
}

/* A repair or a BootstrapVersion bump runs the same decision, so re-running
 * it must never start overwriting what the first run preserved. */
void TestRepeatedBootstrapIsStable()
{
	const OBSPTRecordPathPlan first =
		OBSPTPlanRecordPaths("D:/simple", "", "F:/ffmpeg");
	const OBSPTRecordPathPlan again =
		OBSPTPlanRecordPaths("D:/simple", "K:/expanded/recordings",
				     "F:/ffmpeg");
	Check(first.writeAdvancedRecording,
	      "the first run initialises the empty Advanced path");
	Check(!again.writeSimpleOutput && !again.writeAdvancedRecording &&
		      !again.writeCustomFFmpeg,
	      "a later bootstrap preserves every path it already settled");
}

} // namespace

int main()
{
	TestExistingChoicesArePreserved();
	TestOnlyMissingValuesAreInitialised();
	TestThreeDirectoriesStayIndependent();
	TestRepeatedBootstrapIsStable();

	if (failures == 0) {
		std::cout << "All OBS-PT record path policy tests passed\n";
		return 0;
	}
	std::cerr << failures << " record path assertion(s) failed\n";
	return 1;
}
