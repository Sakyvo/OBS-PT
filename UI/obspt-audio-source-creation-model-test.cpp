#include "obspt-audio-source-creation-model.hpp"

#include <QCoreApplication>

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

void TestVisualSourceReuseRemainsAvailable()
{
	const auto policy = ResolveAudioSourceCreationPolicy(
		false, OBSPTAudioSourceKind::Other, 3, {});
	Check(policy.showExisting,
	      "visual source creation retains Add Existing");
}

void TestApplicationAudioRequiresAnEditorTrack()
{
	const auto unselected = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::ApplicationAudio, 0, {});
	Check(!unselected.showExisting,
	      "collection audio creation hides Add Existing");
	Check(unselected.routeRequired,
	      "application audio requires an explicit route");
	Check(unselected.initialEditorTrack == 0,
	      "unselected application audio has no guessed route");

	const auto selected = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::ApplicationAudio, 3, {});
	Check(selected.initialEditorTrack == 3,
	      "selected Music track is proposed for application audio");

	for (const int invalid : {0, 1, 7}) {
		const auto policy = ResolveAudioSourceCreationPolicy(
			true, OBSPTAudioSourceKind::ApplicationAudio, invalid, {});
		Check(policy.initialEditorTrack == 0,
		      "Mix, Muted, and no selection cannot seed a route");
	}
}

void TestRoutingSelectionDistinguishesTrackAndSourceCells()
{
	Check(ResolveSelectedEditorTrack(3, true, false) == 3,
	      "an occupied track's name cell proposes that track");
	Check(ResolveSelectedEditorTrack(3, true, true) == 0,
	      "an occupied track's source cell does not propose a route");
	Check(ResolveSelectedEditorTrack(3, false, false) == 0,
	      "a non-anchor source row does not propose a route");
	Check(ResolveSelectedEditorTrack(1, true, false) == 0,
	      "Compatibility Mix does not propose a route");
	Check(ResolveSelectedEditorTrack(0, true, false) == 0,
	      "Muted Sources Group does not propose a route");
}

void TestSelectedEditorTrackOverridesTypeDefaults()
{
	const auto input = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::AudioInput, 5, {});
	Check(input.initialEditorTrack == 5,
	      "selected Editor Track overrides the microphone default");

	const auto other = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::Other, 2, {2, 3, 4, 5, 6});
	Check(other.initialEditorTrack == 2,
	      "selected Editor Track overrides the first-empty fallback");
}

void TestTypeDefaultsRemainDeterministic()
{
	const auto input = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::AudioInput, 0, {});
	Check(input.initialEditorTrack == 4,
	      "audio input defaults to Microphone");

	const auto other = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::Other, 0, {2, 3});
	Check(other.initialEditorTrack == 4,
	      "unknown audio uses the first empty Editor Track");

	const auto full = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::Other, 0, {2, 3, 4, 5, 6});
	Check(full.initialEditorTrack == 6,
	      "unknown audio falls back to Aux when every track is occupied");
}

void TestDesktopAudioRemainsMixOnly()
{
	const auto policy = ResolveAudioSourceCreationPolicy(
		true, OBSPTAudioSourceKind::DesktopAudio, 3, {});
	Check(!policy.showExisting,
	      "Desktop Audio creation hides Add Existing");
	Check(policy.fixedMix, "Desktop Audio is fixed to Mix");
	Check(policy.initialEditorTrack == 0,
	      "selected Editor Track cannot override Desktop Audio");
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	TestVisualSourceReuseRemainsAvailable();
	TestApplicationAudioRequiresAnEditorTrack();
	TestRoutingSelectionDistinguishesTrackAndSourceCells();
	TestSelectedEditorTrackOverridesTypeDefaults();
	TestTypeDefaultsRemainDeterministic();
	TestDesktopAudioRemainsMixOnly();
	if (failures == 0)
		std::cout << "All OBS-PT audio source creation model tests passed\n";
	return failures == 0 ? 0 : 1;
}
