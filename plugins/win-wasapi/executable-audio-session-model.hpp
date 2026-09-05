#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obspt {

struct AudioSessionProcess {
	uint32_t processId = 0;
	uint32_t parentProcessId = 0;
	std::string executablePath;
	bool active = false;
	bool identityReadable = false;
};

struct ExecutableSessionChanges {
	std::vector<uint32_t> added;
	std::vector<uint32_t> removed;
	std::vector<uint32_t> retained;
};

struct ExecutableSessionSelection {
	std::vector<uint32_t> processIds;
	bool hadUnreadableIdentity = false;
};

struct ExecutableTargetSettings {
	std::string window;
	std::string processExecutable;
};

enum class ExecutableCaptureStatus {
	Pending,
	Active,
	Permission,
	ActiveWithPermission,
};

ExecutableSessionSelection SelectExecutableSessions(
	const std::string &targetExecutable,
	const std::vector<AudioSessionProcess> &sessions,
	bool javaLenient = false);

ExecutableSessionChanges ReconcileExecutableSessions(
	const std::vector<uint32_t> &activeProcessIds,
	const std::vector<uint32_t> &desiredProcessIds);

ExecutableTargetSettings NormalizeExecutableTargetSettings(
	bool processMode, const std::string &window,
	const std::string &processExecutable);

ExecutableCaptureStatus DetermineExecutableCaptureStatus(
	uint32_t establishedRootCount, bool permissionFailure);

} // namespace obspt
