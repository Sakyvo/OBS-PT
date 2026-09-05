#include "executable-audio-session-model.hpp"

#include <cstdlib>
#include <iostream>

static void Require(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	using obspt::AudioSessionProcess;
	using obspt::SelectExecutableSessions;

	const std::vector<AudioSessionProcess> sessions = {
		{101, 0, "C:/Apps/Vivaldi.EXE", true, true},
		{102, 0, "C:/Apps/vivaldi-helper.exe", true, true},
		{103, 0, "C:/Elsewhere/vivaldi.exe", false, true},
		{104, 0, "", true, false},
	};

	const auto result = SelectExecutableSessions("vivaldi.exe", sessions);
	Require(result.processIds == std::vector<uint32_t>{101},
		"select active sessions by exact case-insensitive executable basename");
	Require(result.hadUnreadableIdentity,
		"report unreadable session identity without guessing");

	const std::vector<AudioSessionProcess> processTrees = {
		{200, 0, "C:/Apps/browser.exe", true, true},
		{201, 200, "C:/Apps/browser.exe", true, true},
		{300, 0, "D:/Portable/browser.exe", true, true},
		{300, 0, "D:/Portable/browser.exe", true, true},
	};
	const auto roots = SelectExecutableSessions("BROWSER.EXE", processTrees);
	Require(roots.processIds == std::vector<uint32_t>({200, 300}),
		"retain unrelated roots once and remove matching descendants");

	// Java Detection leniency (issue 041): the javaw/java alias
	// group matches bidirectionally only in lenient mode.
	const std::vector<AudioSessionProcess> javaSessions = {
		{401, 0, "C:/Program Files/Java/javaw.exe", true, true},
		{402, 0, "C:/Program Files/Java/java.exe", true, true},
		{403, 0, "C:/Elsewhere/notch.exe", true, true},
	};

	const auto strictJavaw =
		SelectExecutableSessions("javaw.exe", javaSessions, false);
	Require(strictJavaw.processIds == std::vector<uint32_t>({401}),
		"strict: java.exe does not match a javaw.exe target");
	const auto strictJava =
		SelectExecutableSessions("java.exe", javaSessions, false);
	Require(strictJava.processIds == std::vector<uint32_t>({402}),
		"strict: javaw.exe does not match a java.exe target");

	const auto lenientJavaw =
		SelectExecutableSessions("javaw.exe", javaSessions, true);
	Require(lenientJavaw.processIds == std::vector<uint32_t>({401, 402}),
		"lenient: java.exe also matches a javaw.exe target");
	const auto lenientJava =
		SelectExecutableSessions("java.exe", javaSessions, true);
	Require(lenientJava.processIds == std::vector<uint32_t>({401, 402}),
		"lenient: javaw.exe also matches a java.exe target");
	const auto lenientOther =
		SelectExecutableSessions("notch.exe", javaSessions, true);
	Require(lenientOther.processIds == std::vector<uint32_t>({403}),
		"leniency leaves non-alias targets untouched");

	const auto changes = obspt::ReconcileExecutableSessions(
		std::vector<uint32_t>{200, 400}, roots.processIds);
	Require(changes.added == std::vector<uint32_t>{300},
		"add only newly desired roots");
	Require(changes.removed == std::vector<uint32_t>{400},
		"remove only roots no longer desired");
	Require(changes.retained == std::vector<uint32_t>{200},
		"retain unchanged clients without restart");

	const auto repeated = obspt::ReconcileExecutableSessions(
		roots.processIds, roots.processIds);
	Require(repeated.added.empty() && repeated.removed.empty() &&
			repeated.retained == roots.processIds,
		"repeated notifications are idempotent");

	const auto emptied = obspt::ReconcileExecutableSessions(
		roots.processIds, std::vector<uint32_t>{});
	Require(emptied.added.empty() && emptied.retained.empty() &&
			emptied.removed == roots.processIds,
		"empty transition releases every active root");

	const auto processTarget = obspt::NormalizeExecutableTargetSettings(
		true, "Old Title:Class:WindowsTerminal.exe", "vivaldi.exe");
	Require(processTarget.window.empty() &&
			processTarget.processExecutable == "vivaldi.exe",
		"process mode saves only the executable target");
	const auto windowTarget = obspt::NormalizeExecutableTargetSettings(
		false, "Minecraft:LWJGL:javaw.exe", "stale.exe");
	Require(windowTarget.window == "Minecraft:LWJGL:javaw.exe" &&
			windowTarget.processExecutable.empty(),
		"window mode saves only the window target");

	using obspt::DetermineExecutableCaptureStatus;
	using obspt::ExecutableCaptureStatus;
	Require(DetermineExecutableCaptureStatus(0, false) ==
			ExecutableCaptureStatus::Pending,
		"zero roots without permission failure is waiting");
	Require(DetermineExecutableCaptureStatus(1, false) ==
			ExecutableCaptureStatus::Active,
		"one established root is capturing even while silent");
	Require(DetermineExecutableCaptureStatus(0, true) ==
			ExecutableCaptureStatus::Permission,
		"permission failure without roots is distinct");
	Require(DetermineExecutableCaptureStatus(2, true) ==
			ExecutableCaptureStatus::ActiveWithPermission,
		"readable roots continue while another identity is inaccessible");

	std::cout << "Executable audio session model tests passed\n";
	return 0;
}
