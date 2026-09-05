#include "executable-audio-session-model.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <set>
#include <unordered_map>

namespace obspt {

static std::string ExecutableBaseName(const std::string &path)
{
	const size_t separator = path.find_last_of("/\\");
	return separator == std::string::npos ? path : path.substr(separator + 1);
}

static bool EqualExecutable(const std::string &left, const std::string &right)
{
	if (left.size() != right.size())
		return false;

	return std::equal(left.begin(), left.end(), right.begin(),
		[](unsigned char a, unsigned char b) {
			return std::tolower(a) == std::tolower(b);
		});
}

// Issue 041 Java Detection: in lenient mode the javaw/java alias group
// matches bidirectionally because some Minecraft launches use java.exe.
// Mirrored in libobs/util/windows/window-helpers.c; keep the pair in sync.
static const char *const kJavaAliasGroup[] = {"javaw.exe", "java.exe", nullptr};

static bool SameExecutableName(const std::string &value, const char *name)
{
	if (value.size() != strlen(name))
		return false;
	for (size_t i = 0; i < value.size(); ++i)
		if (std::tolower((unsigned char)value[i]) != name[i])
			return false;
	return true;
}

static bool JavaAliasMembers(const std::string &left, const std::string &right)
{
	bool leftIsAlias = false;
	bool rightIsAlias = false;
	for (int i = 0; kJavaAliasGroup[i]; ++i) {
		leftIsAlias = leftIsAlias || SameExecutableName(left, kJavaAliasGroup[i]);
		rightIsAlias = rightIsAlias || SameExecutableName(right, kJavaAliasGroup[i]);
	}
	return leftIsAlias && rightIsAlias;
}

static bool EqualExecutableLenient(const std::string &left,
				   const std::string &right, bool javaLenient)
{
	if (EqualExecutable(left, right))
		return true;
	return javaLenient && JavaAliasMembers(left, right);
}

ExecutableSessionSelection SelectExecutableSessions(
	const std::string &targetExecutable,
	const std::vector<AudioSessionProcess> &sessions, bool javaLenient)
{
	ExecutableSessionSelection result;
	std::unordered_map<uint32_t, uint32_t> matchingParents;
	for (const AudioSessionProcess &session : sessions) {
		if (!session.active)
			continue;
		if (!session.identityReadable) {
			result.hadUnreadableIdentity = true;
			continue;
		}
		if (EqualExecutableLenient(
			    ExecutableBaseName(session.executablePath),
			    targetExecutable, javaLenient))
			matchingParents[session.processId] = session.parentProcessId;
	}

	std::set<uint32_t> roots;
	for (const auto &entry : matchingParents) {
		uint32_t ancestor = entry.second;
		bool covered = false;
		std::set<uint32_t> visited;
		while (ancestor && visited.insert(ancestor).second) {
			const auto parent = matchingParents.find(ancestor);
			if (parent == matchingParents.end())
				break;
			covered = true;
			break;
		}
		if (!covered)
			roots.insert(entry.first);
	}
	result.processIds.assign(roots.begin(), roots.end());
	return result;
}

ExecutableSessionChanges ReconcileExecutableSessions(
	const std::vector<uint32_t> &activeProcessIds,
	const std::vector<uint32_t> &desiredProcessIds)
{
	const std::set<uint32_t> active(activeProcessIds.begin(),
					activeProcessIds.end());
	const std::set<uint32_t> desired(desiredProcessIds.begin(),
					 desiredProcessIds.end());
	ExecutableSessionChanges changes;
	std::set_difference(desired.begin(), desired.end(), active.begin(),
			    active.end(), std::back_inserter(changes.added));
	std::set_difference(active.begin(), active.end(), desired.begin(),
			    desired.end(), std::back_inserter(changes.removed));
	std::set_intersection(active.begin(), active.end(), desired.begin(),
			      desired.end(),
			      std::back_inserter(changes.retained));
	return changes;
}

ExecutableTargetSettings NormalizeExecutableTargetSettings(
	bool processMode, const std::string &window,
	const std::string &processExecutable)
{
	ExecutableTargetSettings settings;
	if (processMode)
		settings.processExecutable = processExecutable;
	else
		settings.window = window;
	return settings;
}

ExecutableCaptureStatus DetermineExecutableCaptureStatus(
	uint32_t establishedRootCount, bool permissionFailure)
{
	if (establishedRootCount > 0)
		return permissionFailure
			       ? ExecutableCaptureStatus::ActiveWithPermission
			       : ExecutableCaptureStatus::Active;
	return permissionFailure ? ExecutableCaptureStatus::Permission
				 : ExecutableCaptureStatus::Pending;
}

} // namespace obspt
