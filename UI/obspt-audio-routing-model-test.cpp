#include "obspt-audio-routing-model.hpp"

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

void TestKnownDefaultDesktopAudioNameRepair()
{
	const QString corrupted = QString::fromUtf8(
		"\xE5\xA6\x97\xE5\xB2\x84\xE6\xBD\xB0"
		"\xE9\x97\x8A\xE6\x8A\xBD\xEE\x95\xB6");
	const QString repaired = OBSPTAudioRoutingModel::RepairDefaultSourceName(
		QStringLiteral("obspt-default-desktop-audio"), corrupted,
		QStringLiteral("Desktop Audio"));
	Check(repaired == QStringLiteral("Desktop Audio"),
	      "known default Desktop Audio mojibake is repaired");
	Check(OBSPTAudioRoutingModel::RepairDefaultSourceName(
		      QStringLiteral("user-source"), corrupted,
		      QStringLiteral("Desktop Audio")) == corrupted,
	      "non-default source identity is not repaired");
	Check(OBSPTAudioRoutingModel::RepairDefaultSourceName(
		      QStringLiteral("obspt-default-desktop-audio"),
		      QStringLiteral("My Speakers"),
		      QStringLiteral("Desktop Audio")) ==
		      QStringLiteral("My Speakers"),
	      "custom default-source name is preserved");
}

void TestCompatibilityMixAndGameTracer()
{
	OBSPTAudioRoutingModel routing;

	Check(routing.TrackName(1) == QStringLiteral("Mix"),
	      "track 1 has the fixed Mix name");
	Check(!routing.SetTrackName(1, QStringLiteral("Master")),
	      "track 1 cannot be renamed");
	Check(!routing.AssignSource(QStringLiteral("app-1"), 1),
	      "track 1 cannot be selected as an Editor Track");

	Check(routing.AddSource(QStringLiteral("app-1"),
				QStringLiteral("Minecraft"),
				OBSPTAudioSourceKind::ApplicationAudio),
	      "application audio source can be added");
	const OBSPTAudioRoute *route =
		routing.FindSource(QStringLiteral("app-1"));
	Check(route && route->editorTrack == 2,
	      "application audio defaults to Track 2 Game");

	Check(routing.AssignSource(QStringLiteral("app-1"), 3),
	      "source can move to another Editor Track");
	Check(routing.AssignSource(QStringLiteral("app-1"), 2),
	      "source can move back to Game");
	route = routing.FindSource(QStringLiteral("app-1"));
	Check(route && route->editorTrack == 2,
	      "one source retains exactly one Editor Track assignment");

	const QJsonObject saved = routing.ToJson();
	const OBSPTAudioRoutingModel restored =
		OBSPTAudioRoutingModel::FromJson(saved);
	const OBSPTAudioRoute *restoredRoute =
		restored.FindSource(QStringLiteral("app-1"));
	Check(restoredRoute && restoredRoute->editorTrack == 2,
	      "Game assignment survives Scene Collection round trip");
	Check(restored.ToJson() == saved,
	      "Scene Collection routing serialization is stable");

	const QVector<OBSPTAudioOutputTrack> topology =
		restored.DeriveLocalTopology(QStringLiteral("hybrid_mp4"));
	Check(topology.size() == 2,
	      "Hybrid MP4 derives exactly Mix and one non-empty Editor Track");
	Check(topology.size() > 0 && topology[0].number == 1 &&
		      topology[0].name == QStringLiteral("Mix") &&
		      topology[0].defaultPlayback,
	      "Mix is first and is the default playback track");
	Check(topology.size() > 1 && topology[1].number == 2 &&
		      topology[1].name == QStringLiteral("Game") &&
		      !topology[1].defaultPlayback,
	      "Game is the second non-default track");
}

void TestDesktopAudioStaysInCompatibilityMix()
{
	OBSPTAudioRoutingModel routing;
	Check(routing.AddSource(QStringLiteral("desktop-1"),
				QStringLiteral("Desktop Audio"),
				OBSPTAudioSourceKind::DesktopAudio),
	      "desktop audio source can be added");

	const OBSPTAudioRoute *route =
		routing.FindSource(QStringLiteral("desktop-1"));
	Check(route && route->editorTrack == 0,
	      "desktop audio has no Editor Track assignment");
	Check(!routing.AssignSource(QStringLiteral("desktop-1"), 2),
	      "desktop audio cannot be assigned to an Editor Track");

	const QVector<OBSPTAudioOutputTrack> topology =
		routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"));
	Check(topology.size() == 1 && topology[0].number == 1,
	      "desktop-only topology contains Compatibility Mix only");
}

void TestEditorTracksAndSourceIdentity()
{
	OBSPTAudioRoutingModel routing;
	const QStringList defaults{QStringLiteral("Game"),
				   QStringLiteral("Music"),
				   QStringLiteral("Microphone"),
				   QStringLiteral("Voice Chat"),
				   QStringLiteral("Aux")};
	for (int i = 0; i < defaults.size(); ++i)
		Check(routing.TrackName(i + 2) == defaults[i],
		      "Editor Track has its fixed default role");

	Check(routing.SetTrackName(2, QStringLiteral("  Gameplay  ")),
	      "custom track name is accepted");
	Check(routing.TrackName(2) == QStringLiteral("Gameplay"),
	      "custom track name trims outer whitespace");
	Check(!routing.SetTrackName(2, QStringLiteral("   ")),
	      "empty custom track name is rejected");
	Check(!routing.SetTrackName(2, QStringLiteral("Bad\nName")),
	      "control characters in custom track name are rejected");
	Check(routing.SetTrackName(3, QStringLiteral("Gameplay")),
	      "duplicate custom track names are allowed");

	Check(routing.AddSource(QStringLiteral("game"),
				QStringLiteral("Game App"),
				OBSPTAudioSourceKind::ApplicationAudio),
	      "game application source is added");
	Check(routing.AddSource(QStringLiteral("mic"),
				QStringLiteral("Microphone"),
				OBSPTAudioSourceKind::AudioInput),
	      "audio input source is added");
	Check(routing.AddSource(QStringLiteral("media"),
				QStringLiteral("Media"),
				OBSPTAudioSourceKind::Other),
	      "unknown audio source is added");
	Check(routing.FindSource(QStringLiteral("game"))->editorTrack == 2,
	      "application audio defaults to Game");
	Check(routing.FindSource(QStringLiteral("mic"))->editorTrack == 4,
	      "audio input defaults to Microphone");
	Check(routing.FindSource(QStringLiteral("media"))->editorTrack == 3,
	      "unknown audio uses the first empty Editor Track");

	Check(routing.AddSource(QStringLiteral("game-2"),
				QStringLiteral("Second Game"),
				OBSPTAudioSourceKind::ApplicationAudio),
	      "a second application source can share Game");
	Check(routing.MoveSource(QStringLiteral("game-2"), 0),
	      "source can move within its Editor Track");
	const QVector<const OBSPTAudioRoute *> gameSources =
		routing.SourcesForTrack(2);
	Check(gameSources.size() == 2 &&
		      gameSources[0]->sourceId == QStringLiteral("game-2") &&
		      gameSources[1]->sourceId == QStringLiteral("game"),
	      "in-track source order is explicit and stable");

	Check(routing.DuplicateSource(QStringLiteral("game"),
				      QStringLiteral("game-copy"),
				      QStringLiteral("Game Copy")),
	      "source duplication creates a new identity");
	Check(routing.FindSource(QStringLiteral("game-copy"))->editorTrack == 2,
	      "duplicated source inherits its Editor Track");
	Check(routing.RemoveSource(QStringLiteral("game-copy")),
	      "duplicated identity can be removed");
	Check(!routing.FindSource(QStringLiteral("game-copy")),
	      "deleted identity no longer exists");

	const QJsonObject saved = routing.ToJson();
	const OBSPTAudioRoutingModel restored =
		OBSPTAudioRoutingModel::FromJson(saved);
	Check(restored.ToJson() == saved,
	      "names, assignments, and source order round-trip together");
	Check(restored.SourcesForTrack(2)[0]->sourceId ==
		      QStringLiteral("game-2"),
	      "in-track order survives Scene Collection reload");

	OBSPTAudioRoutingModel otherCollection;
	Check(otherCollection.TrackName(2) == QStringLiteral("Game"),
	      "another Scene Collection keeps independent track names");
	Check(!otherCollection.FindSource(QStringLiteral("game")),
	      "another Scene Collection keeps independent identities");

	OBSPTAudioRoutingModel creationRouting;
	creationRouting.BeginTopologySession();
	Check(creationRouting.AddSource(
		      QStringLiteral("selected-music"),
		      QStringLiteral("Selected Music"),
		      OBSPTAudioSourceKind::ApplicationAudio, 3),
	      "an explicitly selected creation track is accepted while output topology is locked");
	Check(creationRouting.FindSource(QStringLiteral("selected-music"))
		      ->editorTrack == 3,
	      "new application audio uses the selected creation track");
	Check(!creationRouting.FrozenRoutingModel().FindSource(
		      QStringLiteral("selected-music")),
	      "a newly routed source remains pending until the next output session");
	Check(!creationRouting.AddSource(
		      QStringLiteral("invalid-mix"), QStringLiteral("Invalid Mix"),
		      OBSPTAudioSourceKind::ApplicationAudio, 1),
	      "Compatibility Mix cannot be selected as a creation track");
}

void TestMutedAndMonitorOnlyRouting()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("game-a"), QStringLiteral("Game A"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("game-b"), QStringLiteral("Game B"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("music"), QStringLiteral("Music"),
			  OBSPTAudioSourceKind::Other);

	Check(routing.SetMuted(QStringLiteral("game-a"), true),
	      "source can enter Muted Sources Group");
	Check(routing.SetMuted(QStringLiteral("game-b"), true),
	      "second source can enter Muted Sources Group");
	const QVector<const OBSPTAudioRoute *> muted = routing.MutedSources();
	Check(muted.size() == 2 &&
		      muted[0]->sourceId == QStringLiteral("game-b") &&
		      muted[1]->sourceId == QStringLiteral("game-a"),
	      "newly muted source is placed at the top");
	Check(routing.MoveMutedSource(QStringLiteral("game-a"), 0),
	      "muted group can be reordered explicitly");
	Check(routing.MutedSources()[0]->sourceId == QStringLiteral("game-a"),
	      "manual muted order is retained");
	Check(routing.SourcesForTrack(2).isEmpty(),
	      "muted sources are absent from their visible Editor Track");

	Check(routing.SetMuted(QStringLiteral("game-a"), false),
	      "source can leave Muted Sources Group");
	Check(routing.FindSource(QStringLiteral("game-a"))->editorTrack == 2,
	      "unmuted source returns to its prior Editor Track");
	Check(routing.SourcesForTrack(2)[0]->sourceId ==
		      QStringLiteral("game-a"),
	      "unmuted source returns to its prior in-track position");

	Check(routing.SetMonitorOnly(QStringLiteral("game-a"), true),
	      "Monitor Only can be enabled independently");
	Check(routing.FindSource(QStringLiteral("game-a"))->monitorOnly,
	      "Monitor Only state is stored on the route");
	Check(routing.SourcesForTrack(2).size() == 1,
	      "Monitor Only source remains visible in its Editor Track");
	const QVector<OBSPTAudioOutputTrack> topology =
		routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"));
	Check(topology.size() == 2 && topology[0].number == 1 &&
		      topology[1].number == 3,
	      "muted and Monitor Only sources do not create an effective track");

	Check(routing.SetMuted(QStringLiteral("game-a"), true),
	      "Monitor Only source can also be muted");
	Check(routing.SetMonitorOnly(QStringLiteral("game-a"), false),
	      "monitoring state can change while muted");
	Check(routing.SetMuted(QStringLiteral("game-a"), false),
	      "muted source can be restored after monitoring change");
	Check(!routing.FindSource(QStringLiteral("game-a"))->monitorOnly,
	      "monitoring change made while muted is restored");

	const QJsonObject saved = routing.ToJson();
	Check(OBSPTAudioRoutingModel::FromJson(saved).ToJson() == saved,
	      "mute, monitor, restore, and muted order persist together");
}

void TestDesktopAudioAuthority()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desktop-a"),
			  QStringLiteral("Desktop A"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("desktop-b"),
			  QStringLiteral("Desktop B"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("game"), QStringLiteral("Game"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("mic"), QStringLiteral("Mic"),
			  OBSPTAudioSourceKind::AudioInput);

	QSet<QString> initialized{QStringLiteral("desktop-a")};
	QVector<OBSPTAudioOutputTrack> topology =
		routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					    initialized);
	Check(topology[0].sourceIds.contains(QStringLiteral("desktop-a")) &&
		      !topology[0].sourceIds.contains(QStringLiteral("desktop-b")) &&
		      !topology[0].sourceIds.contains(QStringLiteral("game")) &&
		      topology[0].sourceIds.contains(QStringLiteral("mic")),
	      "initialized Desktop authority de-duplicates application audio");

	routing.SetMuted(QStringLiteral("desktop-a"), true);
	topology = routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					       initialized);
	Check(topology[0].sourceIds.contains(QStringLiteral("game")) &&
		      topology[0].sourceIds.contains(QStringLiteral("mic")) &&
		      !topology[0].sourceIds.contains(QStringLiteral("desktop-a")),
	      "explicitly muting all authority sources enables application fallback");

	routing.SetMuted(QStringLiteral("desktop-a"), false);
	topology = routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					       initialized);
	Check(!topology[0].sourceIds.contains(QStringLiteral("game")),
	      "unmuting authority restores application de-duplication");

	topology = routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					       QSet<QString>{});
	Check(topology[0].sourceIds.contains(QStringLiteral("game")) &&
		      topology[0].sourceIds.contains(QStringLiteral("mic")) &&
		      !topology[0].sourceIds.contains(QStringLiteral("desktop-a")),
	      "unavailable Desktop devices at session start use application fallback");
}

void TestPrivateAudioMix()
{
	OBSPTAudioRoutingModel routing;
	const QVector<OBSPTAudioOutputTrack> withoutPrivateAudio =
		routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					     QSet<QString>{}, false);
	Check(withoutPrivateAudio.isEmpty(),
	      "a source-less local session remains video-only without private audio");

	const QVector<OBSPTAudioOutputTrack> withPrivateAudio =
		routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					     QSet<QString>{}, true);
	Check(withPrivateAudio.size() == 1 &&
		      withPrivateAudio[0].number == OBSPTAudioRoutingModel::MixTrack &&
		      withPrivateAudio[0].sourceIds.isEmpty(),
	      "private audio keeps an unassigned Compatibility Mix without an Editor Track");
}

void TestFrozenRecordingTopology()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("game"), QStringLiteral("Game"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.BeginTopologySession();
	routing.BeginTopologySession();
	Check(routing.TopologyLocked(),
	      "topology is locked while a recording session is active");
	Check(!routing.AssignSource(QStringLiteral("game"), 3),
	      "route movement is blocked by topology lock");
	Check(!routing.SetTrackName(2, QStringLiteral("Gameplay")),
	      "track rename is blocked by topology lock");
	Check(!routing.SetMonitorOnly(QStringLiteral("game"), true),
	      "eligibility-changing monitoring is blocked by topology lock");
	Check(routing.SetMuted(QStringLiteral("game"), true),
	      "source mute remains available during a session");
	Check(routing.AddSource(QStringLiteral("late"), QStringLiteral("Late"),
				OBSPTAudioSourceKind::ApplicationAudio),
	      "new source can be created for the next session");

	const OBSPTAudioRoutingModel frozen = routing.FrozenRoutingModel();
	const QVector<OBSPTAudioOutputTrack> topology =
		frozen.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					  QSet<QString>{});
	Check(topology.size() == 2 && topology[1].number == 2 &&
		      topology[1].sourceIds.contains(QStringLiteral("game")) &&
		      !topology[1].sourceIds.contains(QStringLiteral("late")),
	      "frozen topology retains initial eligibility and excludes late source");

	routing.EndTopologySession();
	Check(routing.TopologyLocked(),
	      "one remaining topology-bearing session keeps routing locked");
	routing.EndTopologySession();
	Check(!routing.TopologyLocked(),
	      "routing unlocks after every topology-bearing session stops");
	Check(routing.AssignSource(QStringLiteral("game"), 3),
	      "route movement resumes after topology unlock");
}

void TestStandardContainerTopology()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("game"), QStringLiteral("Game"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("music"), QStringLiteral("Music"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("music"), 3);

	const QStringList multiTrackFormats{
		QStringLiteral("hybrid_mp4"), QStringLiteral("mp4"),
		QStringLiteral("mov"), QStringLiteral("mkv"),
		QStringLiteral("ts")};
	for (const QString &format : multiTrackFormats) {
		const QVector<OBSPTAudioOutputTrack> topology =
			routing.DeriveLocalTopology(format, QSet<QString>{});
		Check(topology.size() == 3 && topology[0].number == 1 &&
			      topology[1].number == 2 && topology[2].number == 3,
		      "multi-track standard container emits Mix and Editor Tracks");
	}
	for (const QString &format :
	     {QStringLiteral("flv"), QStringLiteral("m3u8")}) {
		const QVector<OBSPTAudioOutputTrack> topology =
			routing.DeriveLocalTopology(format, QSet<QString>{});
		Check(topology.size() == 1 && topology[0].number == 1,
		      "single-track standard container emits Mix only");
	}

	routing.SetMuted(QStringLiteral("game"), true);
	routing.SetMuted(QStringLiteral("music"), true);
	Check(routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					 QSet<QString>{})
		      .isEmpty(),
	      "all-muted local topology is video-only");
}

void TestCustomContainerCapabilities()
{
	Check(OBSPTAudioRoutingModel::CustomContainerMode(
		QStringLiteral("matroska")) ==
		      OBSPTCustomContainerMode::MultiTrack,
	      "known custom multi-track container is accepted");
	Check(OBSPTAudioRoutingModel::CustomContainerMode(
		QStringLiteral("mp4")) == OBSPTCustomContainerMode::MultiTrack,
	      "custom MP4 is multi-track capable");
	Check(OBSPTAudioRoutingModel::CustomContainerMode(
		QStringLiteral("flv")) == OBSPTCustomContainerMode::MixOnly,
	      "known custom single-track container uses Mix only");
	Check(OBSPTAudioRoutingModel::CustomContainerMode(
		QStringLiteral("mystery-container")) ==
		      OBSPTCustomContainerMode::Unsupported,
	      "unknown custom container is rejected instead of guessing");
}

void TestStreamingAndVodTopology()
{
	Check(OBSPTAudioRoutingModel::ServiceSupportsVodTrack(
		      QStringLiteral("Twitch")),
	      "Twitch exposes VOD Editor Track routing");
	Check(!OBSPTAudioRoutingModel::ServiceSupportsVodTrack(
		       QStringLiteral("Bilibili")),
	      "Bilibili remains Compatibility Mix-only");
	Check(!OBSPTAudioRoutingModel::ServiceSupportsVodTrack(
		       QStringLiteral("YouTube - RTMPS")),
	      "ordinary services do not expose unsupported VOD routing");

	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("game"), QStringLiteral("Game"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("music"), QStringLiteral("Music"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("music"), 3);

	QVector<OBSPTAudioOutputTrack> topology =
		routing.DeriveStreamingTopology();
	Check(topology.size() == 1 && topology[0].number == 1,
	      "ordinary streaming always emits Compatibility Mix only");

	topology = routing.DeriveStreamingTopology(3);
	Check(topology.size() == 2 && topology[0].number == 1 &&
		      topology[1].number == 3 &&
		      topology[1].name == QStringLiteral("Music"),
	      "VOD streaming adds exactly the selected non-empty Editor Track");
	Check(routing.HasEligibleEditorTrack(3),
	      "selected VOD Editor Track reports eligible content");

	routing.SetMuted(QStringLiteral("music"), true);
	Check(!routing.HasEligibleEditorTrack(3),
	      "muted-only VOD Editor Track is ineligible");
	Check(routing.DeriveStreamingTopology(3).size() == 1,
	      "empty selected VOD Editor Track never falls back to another track");

	routing.SetMuted(QStringLiteral("game"), true);
	topology = routing.DeriveStreamingTopology();
	Check(topology.size() == 1 && topology[0].number == 1 &&
		      topology[0].sourceIds.isEmpty(),
	      "all-muted live output retains a silent Compatibility Mix stream");
}

void TestMoveAvailabilityFollowsRealPosition()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("first"), QStringLiteral("First"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	auto availability = routing.MoveAvailability(QStringLiteral("first"));
	Check(!availability.up && !availability.down,
	      "one-source track disables both reorder commands");

	routing.AddSource(QStringLiteral("middle"), QStringLiteral("Middle"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	routing.AddSource(QStringLiteral("last"), QStringLiteral("Last"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	availability = routing.MoveAvailability(QStringLiteral("first"));
	Check(!availability.up && availability.down,
	      "first source only moves down");
	availability = routing.MoveAvailability(QStringLiteral("middle"));
	Check(availability.up && availability.down,
	      "middle source moves in both directions");
	availability = routing.MoveAvailability(QStringLiteral("last"));
	Check(availability.up && !availability.down,
	      "last source only moves up");

	routing.SetMuted(QStringLiteral("first"), true);
	availability = routing.MoveAvailability(QStringLiteral("first"));
	Check(!availability.up && !availability.down,
	      "one-source muted group disables both reorder commands");
	routing.SetMuted(QStringLiteral("middle"), true);
	availability = routing.MoveAvailability(QStringLiteral("middle"));
	Check(!availability.up && availability.down,
	      "newly muted first row only moves down");
	availability = routing.MoveAvailability(QStringLiteral("first"));
	Check(availability.up && !availability.down,
	      "muted last row only moves up");

	routing.AddSource(QStringLiteral("desktop"), QStringLiteral("Desktop"),
			  OBSPTAudioSourceKind::DesktopAudio);
	availability = routing.MoveAvailability(QStringLiteral("desktop"));
	Check(!availability.up && !availability.down,
	      "active Desktop Audio cannot be reordered in fixed Mix");
	routing.SetMuted(QStringLiteral("desktop"), true);
	availability = routing.MoveAvailability(QStringLiteral("desktop"));
	Check(!availability.up && availability.down,
	      "muted Desktop Audio can be reordered in the muted group");
}

void TestAtomicContextMenuMoveAppendsOrDoesNothing()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("game"), QStringLiteral("Game"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	routing.AddSource(QStringLiteral("music"), QStringLiteral("Music"),
			  OBSPTAudioSourceKind::ApplicationAudio, 3);

	const QJsonObject unchanged = routing.ToJson();
	const OBSPTAudioRouteChangeResult sameTrack =
		routing.MoveSourceToEditorTrack(
			QStringLiteral("game"),
			{2, OBSPTAudioTrackPlacement::Append, 0});
	Check(sameTrack == OBSPTAudioRouteChangeResult::Unchanged,
	      "moving an active source to its current track is unchanged");
	Check(routing.ToJson() == unchanged,
	      "same-track context move has no model side effect");

	const OBSPTAudioRouteChangeResult moved =
		routing.MoveSourceToEditorTrack(
			QStringLiteral("game"),
			{3, OBSPTAudioTrackPlacement::Append, 0});
	const QVector<const OBSPTAudioRoute *> music =
		routing.SourcesForTrack(3);
	Check(moved == OBSPTAudioRouteChangeResult::Changed,
	      "cross-track context move reports a committed change");
	Check(music.size() == 2 &&
		      music[0]->sourceId == QStringLiteral("music") &&
		      music[1]->sourceId == QStringLiteral("game"),
	      "cross-track context move appends to the destination");
}

void TestAtomicContextMenuMoveRestoresOrRejectsWithoutPartialState()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("first"), QStringLiteral("First"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	routing.AddSource(QStringLiteral("middle"), QStringLiteral("Middle"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	routing.AddSource(QStringLiteral("last"), QStringLiteral("Last"),
			  OBSPTAudioSourceKind::ApplicationAudio, 2);
	routing.AddSource(QStringLiteral("music"), QStringLiteral("Music"),
			  OBSPTAudioSourceKind::ApplicationAudio, 3);
	routing.AddSource(QStringLiteral("desktop"), QStringLiteral("Desktop"),
			  OBSPTAudioSourceKind::DesktopAudio);

	routing.SetMonitorOnly(QStringLiteral("middle"), true);
	routing.SetMuted(QStringLiteral("middle"), true);
	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {2, OBSPTAudioTrackPlacement::Append, 0}) ==
		      OBSPTAudioRouteChangeResult::Changed,
	      "choosing a muted source's prior track restores it");
	const QVector<const OBSPTAudioRoute *> restored =
		routing.SourcesForTrack(2);
	const OBSPTAudioRoute *middle =
		routing.FindSource(QStringLiteral("middle"));
	Check(restored.size() == 3 &&
		      restored[0]->sourceId == QStringLiteral("first") &&
		      restored[1]->sourceId == QStringLiteral("middle") &&
		      restored[2]->sourceId == QStringLiteral("last"),
	      "restoring a muted source preserves its prior position");
	Check(middle && !middle->muted && middle->monitorOnly,
	      "restoring a muted source preserves monitoring state");

	routing.SetMuted(QStringLiteral("middle"), true);
	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {3, OBSPTAudioTrackPlacement::Append, 0}) ==
		      OBSPTAudioRouteChangeResult::Changed,
	      "moving a muted source to another track commits");
	const QVector<const OBSPTAudioRoute *> music =
		routing.SourcesForTrack(3);
	middle = routing.FindSource(QStringLiteral("middle"));
	Check(music.size() == 2 &&
		      music[0]->sourceId == QStringLiteral("music") &&
		      music[1]->sourceId == QStringLiteral("middle") && middle &&
		      !middle->muted && middle->monitorOnly,
	      "muted cross-track move appends, unmutes, and preserves monitoring");

	const QJsonObject beforeDesktop = routing.ToJson();
	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("desktop"),
		      {2, OBSPTAudioTrackPlacement::Append, 0}) ==
		      OBSPTAudioRouteChangeResult::Rejected &&
		      routing.ToJson() == beforeDesktop,
	      "Desktop Audio reassignment is rejected without partial state");

	routing.BeginTopologySession();
	const QJsonObject beforeLocked = routing.ToJson();
	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {4, OBSPTAudioTrackPlacement::Append, 0}) ==
		      OBSPTAudioRouteChangeResult::Rejected &&
		      routing.ToJson() == beforeLocked,
	      "topology lock rejects the complete transition");
	routing.EndTopologySession();
}

void TestAtomicDragMoveUsesExactDestinationAndMutedGroup()
{
	OBSPTAudioRoutingModel routing;
	for (const QString &id : {QStringLiteral("first"),
				  QStringLiteral("middle"),
				  QStringLiteral("last")})
		routing.AddSource(id, id,
				  OBSPTAudioSourceKind::ApplicationAudio, 2);
	for (const QString &id :
	     {QStringLiteral("music-a"), QStringLiteral("music-b")})
		routing.AddSource(id, id,
				  OBSPTAudioSourceKind::ApplicationAudio, 3);

	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {2, OBSPTAudioTrackPlacement::AtIndex, 0}) ==
		      OBSPTAudioRouteChangeResult::Changed,
	      "same-track drag commits one reorder");
	auto game = routing.SourcesForTrack(2);
	Check(game.size() == 3 &&
		      game[0]->sourceId == QStringLiteral("middle") &&
		      game[1]->sourceId == QStringLiteral("first") &&
		      game[2]->sourceId == QStringLiteral("last"),
	      "same-track drag inserts at the first position");

	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {3, OBSPTAudioTrackPlacement::AtIndex, 1}) ==
		      OBSPTAudioRouteChangeResult::Changed,
	      "cross-track drag commits one transition");
	auto music = routing.SourcesForTrack(3);
	Check(music.size() == 3 &&
		      music[0]->sourceId == QStringLiteral("music-a") &&
		      music[1]->sourceId == QStringLiteral("middle") &&
		      music[2]->sourceId == QStringLiteral("music-b"),
	      "cross-track drag inserts at the exact destination");

	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {0, OBSPTAudioTrackPlacement::AtIndex, 0}) ==
		      OBSPTAudioRouteChangeResult::Changed,
	      "dragging into the muted group commits");
	const OBSPTAudioRoute *middle =
		routing.FindSource(QStringLiteral("middle"));
	Check(middle && middle->muted && middle->editorTrack == 3,
	      "muting preserves the source's assigned editor track");

	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {2, OBSPTAudioTrackPlacement::AtIndex, 1}) ==
		      OBSPTAudioRouteChangeResult::Changed,
	      "dragging out of muted atomically reassigns and unmutes");
	game = routing.SourcesForTrack(2);
	middle = routing.FindSource(QStringLiteral("middle"));
	Check(game.size() == 3 &&
		      game[0]->sourceId == QStringLiteral("first") &&
		      game[1]->sourceId == QStringLiteral("middle") &&
		      game[2]->sourceId == QStringLiteral("last") && middle &&
		      !middle->muted,
	      "muted source is inserted at the exact active destination");

	routing.BeginTopologySession();
	const QJsonObject locked = routing.ToJson();
	Check(routing.MoveSourceToEditorTrack(
		      QStringLiteral("middle"),
		      {0, OBSPTAudioTrackPlacement::AtIndex, 0}) ==
		      OBSPTAudioRouteChangeResult::Rejected &&
		      routing.ToJson() == locked,
	      "locked drag leaves the complete model unchanged");
	routing.EndTopologySession();
}


/* Single-track Audio Mode is defined as "Compatibility Mix only, no Editor
 * Track". Every input mixes into that one track, and the recording carries a
 * single audio stream. */
void TestSingleTrackAudioModeEmitsMixOnly()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desktop"), QStringLiteral("Desktop"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("game"), QStringLiteral("Game"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("mic"), QStringLiteral("Mic"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("mic"), 4);
	const QSet<QString> initialized{QStringLiteral("desktop")};

	Check(routing.Mode() == OBSPTAudioMode::MultiTrack,
	      "a fresh routing configuration defaults to multi-track");

	const QVector<OBSPTAudioOutputTrack> multi = routing.DeriveLocalTopology(
		QStringLiteral("hybrid_mp4"), initialized);
	Check(multi.size() > 1, "multi-track mode still emits Editor Tracks");

	routing.SetMode(OBSPTAudioMode::SingleTrack);
	Check(routing.Mode() == OBSPTAudioMode::SingleTrack,
	      "the mode switch takes effect");

	const QVector<OBSPTAudioOutputTrack> single = routing.DeriveLocalTopology(
		QStringLiteral("hybrid_mp4"), initialized);
	Check(single.size() == 1 && single[0].number == 1,
	      "single-track mode emits the Compatibility Mix alone");
	Check(single.size() == 1 &&
		      single[0].sourceIds.contains(QStringLiteral("mic")),
	      "a microphone on an Editor Track still reaches the single track");
	Check(single.size() == 1 &&
		      single[0].sourceIds.contains(QStringLiteral("desktop")),
	      "system playback reaches the single track");

	const QVector<OBSPTAudioOutputTrack> streaming =
		routing.DeriveStreamingTopology(0, initialized);
	Check(streaming.size() == 1 && streaming[0].number == 1,
	      "single-track mode streams the Compatibility Mix alone");

	/* Switching back must restore the multi-track layout untouched. */
	routing.SetMode(OBSPTAudioMode::MultiTrack);
	const QVector<OBSPTAudioOutputTrack> restored =
		routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					    initialized);
	auto shape = [](const QVector<OBSPTAudioOutputTrack> &topology) {
		QStringList out;
		for (const OBSPTAudioOutputTrack &track : topology)
			out << QStringLiteral("%1:%2").arg(track.number).arg(
				static_cast<QStringList>(
					QStringList(track.sourceIds.toList()))
					.join(QLatin1Char(',')));
		return out.join(QLatin1Char('|'));
	};
	Check(shape(restored) == shape(multi),
	      "returning to multi-track restores the original topology");
}

/* The two modes hold separate settings: a source the user muted for
 * multi-track editing does not silence the single track, and muting the
 * single track does not disturb the multi-track layout. */
void TestAudioModesKeepSeparateSettings()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desktop"), QStringLiteral("Desktop"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("mic"), QStringLiteral("Mic"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("mic"), 4);
	const QSet<QString> initialized{QStringLiteral("desktop")};

	routing.SetMuted(QStringLiteral("desktop"), true);
	routing.SetMuted(QStringLiteral("mic"), true);

	routing.SetMode(OBSPTAudioMode::SingleTrack);
	const QVector<OBSPTAudioOutputTrack> single = routing.DeriveLocalTopology(
		QStringLiteral("hybrid_mp4"), initialized);
	Check(single.size() == 1 &&
		      single[0].sourceIds.contains(QStringLiteral("desktop")) &&
		      single[0].sourceIds.contains(QStringLiteral("mic")),
	      "multi-track mute choices do not silence the single track");

	routing.SetSingleTrackMuted(true);
	Check(routing.SingleTrackMuted(), "the single track can be muted");
	Check(routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					  initialized)
		      .isEmpty(),
	      "muting the single track produces no audio stream");

	routing.SetSingleTrackMuted(false);
	routing.SetMode(OBSPTAudioMode::MultiTrack);
	Check(routing.DeriveLocalTopology(QStringLiteral("hybrid_mp4"),
					  initialized)
		      .isEmpty(),
	      "the multi-track mute choices survived the round trip");
}

/* The mode travels with the Audio Routing Configuration, which lives in the
 * Scene Collection. A payload written before the field existed loads as
 * multi-track. */
void TestAudioModeRoundTripsThroughSerialisation()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desktop"), QStringLiteral("Desktop"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.SetMode(OBSPTAudioMode::SingleTrack);
	routing.SetSingleTrackMuted(true);

	const OBSPTAudioRoutingModel reloaded =
		OBSPTAudioRoutingModel::FromJson(routing.ToJson());
	Check(reloaded.Mode() == OBSPTAudioMode::SingleTrack,
	      "the mode survives a save and load cycle");
	Check(reloaded.SingleTrackMuted(),
	      "the single-track mute state survives a save and load cycle");

	QJsonObject legacy = routing.ToJson();
	legacy.remove(QStringLiteral("audioMode"));
	legacy.remove(QStringLiteral("singleTrackMuted"));
	const OBSPTAudioRoutingModel upgraded =
		OBSPTAudioRoutingModel::FromJson(legacy);
	Check(upgraded.Mode() == OBSPTAudioMode::MultiTrack,
	      "a configuration without the field loads as multi-track");
}


/* The Video Capture Device is the documented exception to the
 * capability-based panel boundary: its sound comes from an independent
 * physical device, so the picture source contributes picture only and the
 * microphone is added separately in the Audio and Routing Panel. Everything
 * else that carries picture and sound keeps its audio, because that audio is
 * part of the content. */
void TestOnlyVideoCaptureDeviceContributesPictureOnly()
{
	Check(OBSPTAudioRoutingModel::SourceContributesPictureOnly(
		      QStringLiteral("dshow_input")),
	      "the Video Capture Device contributes picture only");

	const QStringList keepsAudio{
		QStringLiteral("ffmpeg_source"),      /* media source */
		QStringLiteral("browser_source"),
		QStringLiteral("vlc_source"),
		QStringLiteral("game_capture"),
		QStringLiteral("monitor_capture"),
		QStringLiteral("window_capture"),
		QStringLiteral("wasapi_input_capture"),
		QStringLiteral("wasapi_output_capture"),
		QStringLiteral("wasapi_process_output_capture"),
		QStringLiteral("")};
	for (const QString &typeId : keepsAudio) {
		if (OBSPTAudioRoutingModel::SourceContributesPictureOnly(typeId)) {
			std::cerr << "  (" << typeId.toLatin1().constData()
				  << ")" << std::endl;
			Check(false,
			      "an unrelated source type keeps its audio");
		}
	}
}


/* A source removed from every scene must lose its routing entry, or the panel
 * keeps a row the user can neither route nor delete and the output topology
 * still counts it. The model cannot see scenes, so the caller supplies the set
 * of sources that are still reachable. */
void TestPruningDropsOnlyUnreachableSources()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desktop"), QStringLiteral("Desktop"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("mic"), QStringLiteral("Mic"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("mic"), 4);
	routing.AddSource(QStringLiteral("media"), QStringLiteral("Media"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("media"), 3);
	routing.AddSource(QStringLiteral("gone"), QStringLiteral("Deleted"),
			  OBSPTAudioSourceKind::Other);
	routing.AssignSource(QStringLiteral("gone"), 5);
	routing.SetMuted(QStringLiteral("mic"), true);

	const QSet<QString> reachable{QStringLiteral("desktop"),
				      QStringLiteral("mic"),
				      QStringLiteral("media")};
	const int dropped = routing.PruneSourcesNotIn(reachable);

	Check(dropped == 1, "exactly the unreachable entry is dropped");
	Check(!routing.FindSource(QStringLiteral("gone")),
	      "the removed source no longer has a routing entry");
	Check(routing.FindSource(QStringLiteral("desktop")) &&
		      routing.FindSource(QStringLiteral("mic")) &&
		      routing.FindSource(QStringLiteral("media")),
	      "reachable sources are untouched");

	const OBSPTAudioRoute *mic = routing.FindSource(QStringLiteral("mic"));
	Check(mic && mic->editorTrack == 4 && mic->muted,
	      "pruning does not disturb another source's track or mute state");

	const OBSPTAudioRoute *media =
		routing.FindSource(QStringLiteral("media"));
	Check(media && media->editorTrack == 3,
	      "pruning does not disturb another source's track assignment");

	Check(routing.PruneSourcesNotIn(reachable) == 0,
	      "a second pass with no change drops nothing");

	/* An empty reachable set means nothing is reachable - it must not be
	 * mistaken for "skip pruning". */
	Check(routing.PruneSourcesNotIn(QSet<QString>{}) == 3,
	      "an empty reachable set drops every remaining entry");
}

/* The mixer's track badge is a paint-time derivation over the preserved
 * routing state. Multi-track shows the true assignment (Muted wins over
 * kind); single-track hides every badge except the fixed Mix identity of
 * Desktop Audio, because preserved assignments and mute flags are ignored
 * everywhere in that mode. */
void TestMixerBadgeDerivation()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desk-1"),
			  QStringLiteral("Desktop Audio"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("app-1"),
			  QStringLiteral("Minecraft"),
			  OBSPTAudioSourceKind::ApplicationAudio);
	routing.AddSource(QStringLiteral("mic-1"),
			  QStringLiteral("Microphone"),
			  OBSPTAudioSourceKind::AudioInput);
	routing.SetMuted(QStringLiteral("mic-1"), true);

	const OBSPTAudioRoute *desk =
		routing.FindSource(QStringLiteral("desk-1"));
	const OBSPTAudioRoute *app =
		routing.FindSource(QStringLiteral("app-1"));
	const OBSPTAudioRoute *mic =
		routing.FindSource(QStringLiteral("mic-1"));
	Check(desk && app && mic, "badge test sources exist");

	Check(routing.MixerBadge(*desk) == OBSPTMixerBadge::Mix,
	      "multi-track: Desktop Audio carries the Mix badge");
	Check(routing.MixerBadge(*app) == OBSPTMixerBadge::EditorTrack,
	      "multi-track: ordinary source carries its Editor Track badge");
	Check(routing.MixerBadge(*mic) == OBSPTMixerBadge::Muted,
	      "multi-track: mute wins over the Editor Track badge");

	routing.SetMode(OBSPTAudioMode::SingleTrack);
	Check(routing.MixerBadge(*desk) == OBSPTMixerBadge::Mix,
	      "single-track: Desktop Audio keeps its fixed Mix badge");
	Check(routing.MixerBadge(*app) == OBSPTMixerBadge::Hidden,
	      "single-track: preserved Editor Track assignment is hidden");
	Check(routing.MixerBadge(*mic) == OBSPTMixerBadge::Hidden,
	      "single-track: preserved mute shows no badge either");
}

/* The Profile's Mix Track Behavior option gates whether the Compatibility
 * Mix is written into multi-track-capable local recordings. Desktop Audio
 * lives only on the Mix by domain rule, so omitting the Mix excludes it from
 * the file entirely; the first non-empty Editor Track becomes the default
 * stream. Single-track mode and single-track containers are exempt. */
void TestMixTrackBehaviorTopology()
{
	OBSPTAudioRoutingModel routing;
	routing.AddSource(QStringLiteral("desk-1"),
			  QStringLiteral("Desktop Audio"),
			  OBSPTAudioSourceKind::DesktopAudio);
	routing.AddSource(QStringLiteral("app-1"),
			  QStringLiteral("Minecraft"),
			  OBSPTAudioSourceKind::ApplicationAudio);

	QSet<QString> desktop;
	desktop.insert(QStringLiteral("desk-1"));

	QVector<OBSPTAudioOutputTrack> topology =
		routing.DeriveLocalTopology(QStringLiteral("mkv"), desktop,
					    false, true);
	Check(topology.size() == 2 && topology[0].number == 1 &&
		      topology[0].defaultPlayback,
	      "generate: Mix leads the recording topology as default stream");
	Check(topology[1].number == 2 && !topology[1].defaultPlayback,
	      "generate: Game follows the Mix");

	topology = routing.DeriveLocalTopology(QStringLiteral("mkv"), desktop,
					       false, false);
	Check(topology.size() == 1 && topology[0].number == 2,
	      "omit: no Mix stream, Game is the first stream");
	Check(topology[0].defaultPlayback,
	      "omit: the first Editor Track becomes the default stream");
	Check(topology[0].sourceIds ==
		      QVector<QString>{QStringLiteral("app-1")},
	      "omit: Game carries its sources");
	Check(!topology[0].sourceIds.contains(QStringLiteral("desk-1")),
	      "omit: Desktop Audio is in no track, exactly as the fixed-Mix "
	      "rule implies");

	OBSPTAudioRoutingModel desktopOnly;
	desktopOnly.AddSource(QStringLiteral("desk-1"),
			      QStringLiteral("Desktop Audio"),
			      OBSPTAudioSourceKind::DesktopAudio);
	topology = desktopOnly.DeriveLocalTopology(QStringLiteral("mkv"),
						   desktop, false, false);
	Check(topology.isEmpty(),
	      "omit: no Editor Track sources means no audio stream at all");

	routing.SetMode(OBSPTAudioMode::SingleTrack);
	topology = routing.DeriveLocalTopology(QStringLiteral("mkv"), desktop,
					       false, false);
	Check(topology.size() == 1 && topology[0].number == 1,
	      "single-track mode always keeps the Mix");
	routing.SetMode(OBSPTAudioMode::MultiTrack);

	topology = routing.DeriveLocalTopology(QStringLiteral("flv"), desktop,
					       false, false);
	Check(topology.size() == 1 && topology[0].number == 1,
	      "single-track containers always keep the Mix");
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	TestKnownDefaultDesktopAudioNameRepair();
	TestCompatibilityMixAndGameTracer();
	TestDesktopAudioStaysInCompatibilityMix();
	TestEditorTracksAndSourceIdentity();
	TestMutedAndMonitorOnlyRouting();
	TestDesktopAudioAuthority();
	TestPrivateAudioMix();
	TestFrozenRecordingTopology();
	TestStandardContainerTopology();
	TestCustomContainerCapabilities();
	TestStreamingAndVodTopology();
	TestMoveAvailabilityFollowsRealPosition();
	TestAtomicContextMenuMoveAppendsOrDoesNothing();
	TestAtomicContextMenuMoveRestoresOrRejectsWithoutPartialState();
	TestAtomicDragMoveUsesExactDestinationAndMutedGroup();
	TestSingleTrackAudioModeEmitsMixOnly();
	TestAudioModesKeepSeparateSettings();
	TestAudioModeRoundTripsThroughSerialisation();
	TestOnlyVideoCaptureDeviceContributesPictureOnly();
	TestPruningDropsOnlyUnreachableSources();
	TestMixerBadgeDerivation();
	TestMixTrackBehaviorTopology();

	if (failures == 0)
		std::cout << "All OBS-PT audio routing model tests passed\n";
	return failures == 0 ? 0 : 1;
}
