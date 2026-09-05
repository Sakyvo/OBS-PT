#include "obspt-audio-routing-model.hpp"

#include <QJsonArray>

namespace {

QString KindToString(OBSPTAudioSourceKind kind)
{
	switch (kind) {
	case OBSPTAudioSourceKind::ApplicationAudio:
		return QStringLiteral("application_audio");
	case OBSPTAudioSourceKind::AudioInput:
		return QStringLiteral("audio_input");
	case OBSPTAudioSourceKind::DesktopAudio:
		return QStringLiteral("desktop_audio");
	case OBSPTAudioSourceKind::Other:
		return QStringLiteral("other");
	}

	return QStringLiteral("other");
}

OBSPTAudioSourceKind KindFromString(const QString &value)
{
	if (value == QStringLiteral("application_audio"))
		return OBSPTAudioSourceKind::ApplicationAudio;
	if (value == QStringLiteral("audio_input"))
		return OBSPTAudioSourceKind::AudioInput;
	if (value == QStringLiteral("desktop_audio"))
		return OBSPTAudioSourceKind::DesktopAudio;
	return OBSPTAudioSourceKind::Other;
}

bool IsValidEditorTrack(int track)
{
	return track >= OBSPTAudioRoutingModel::FirstEditorTrack &&
	       track <= OBSPTAudioRoutingModel::LastEditorTrack;
}

} // namespace

OBSPTAudioRoutingModel::OBSPTAudioRoutingModel()
	: trackNames{QStringLiteral("Mix"), QStringLiteral("Game"),
		     QStringLiteral("Music"), QStringLiteral("Microphone"),
		     QStringLiteral("Voice Chat"), QStringLiteral("Aux")}
{
}

QString OBSPTAudioRoutingModel::RepairDefaultSourceName(
	const QString &sourceId, const QString &sourceName,
	const QString &defaultName)
{
	static const QString defaultDesktopId =
		QStringLiteral("obspt-default-desktop-audio");
	static const QString knownMojibake = QString::fromUtf8(
		"\xE5\xA6\x97\xE5\xB2\x84\xE6\xBD\xB0"
		"\xE9\x97\x8A\xE6\x8A\xBD\xEE\x95\xB6");
	return sourceId == defaultDesktopId && sourceName == knownMojibake
		       ? defaultName
		       : sourceName;
}

QString OBSPTAudioRoutingModel::TrackName(int track) const
{
	if (track < MixTrack || track > LastEditorTrack)
		return {};
	return trackNames[track - 1];
}

bool OBSPTAudioRoutingModel::SetTrackName(int track, const QString &name)
{
	if (TopologyLocked())
		return false;
	const QString trimmed = name.trimmed();
	if (!IsValidEditorTrack(track) || trimmed.isEmpty())
		return false;
	for (const QChar character : trimmed) {
		if (character.category() == QChar::Other_Control)
			return false;
	}

	trackNames[track - 1] = trimmed;
	return true;
}

int OBSPTAudioRoutingModel::DefaultEditorTrack(
	OBSPTAudioSourceKind kind) const
{
	if (kind == OBSPTAudioSourceKind::DesktopAudio)
		return 0;
	if (kind == OBSPTAudioSourceKind::ApplicationAudio)
		return 2;
	if (kind == OBSPTAudioSourceKind::AudioInput)
		return 4;

	for (int track = FirstEditorTrack; track <= LastEditorTrack; ++track) {
		bool occupied = false;
		for (const OBSPTAudioRoute &route : sources) {
			if (route.editorTrack == track) {
				occupied = true;
				break;
			}
		}
		if (!occupied)
			return track;
	}

	return LastEditorTrack;
}

bool OBSPTAudioRoutingModel::AddSource(const QString &sourceId,
				      const QString &sourceName,
				      OBSPTAudioSourceKind kind,
				      int initialEditorTrack)
{
	if (sourceId.isEmpty() || FindSource(sourceId))
		return false;
	if (initialEditorTrack != 0 &&
	    (kind == OBSPTAudioSourceKind::DesktopAudio ||
	     !IsValidEditorTrack(initialEditorTrack)))
		return false;

	OBSPTAudioRoute route;
	route.sourceId = sourceId;
	route.sourceName = sourceName;
	route.kind = kind;
	route.editorTrack = initialEditorTrack != 0
				    ? initialEditorTrack
				    : DefaultEditorTrack(kind);
	sources.push_back(route);
	return true;
}

bool OBSPTAudioRoutingModel::AssignSource(const QString &sourceId,
					 int editorTrack)
{
	if (TopologyLocked())
		return false;
	if (!IsValidEditorTrack(editorTrack))
		return false;

	for (OBSPTAudioRoute &route : sources) {
		if (route.sourceId != sourceId)
			continue;
		if (route.kind == OBSPTAudioSourceKind::DesktopAudio)
			return false;

		route.editorTrack = editorTrack;
		return true;
	}

	return false;
}

OBSPTAudioRouteChangeResult
OBSPTAudioRoutingModel::MoveSourceToEditorTrack(
	const QString &sourceId, const OBSPTAudioTrackTarget &target)
{
	const bool mutedTarget = target.editorTrack == 0;
	if (TopologyLocked() ||
	    (!mutedTarget && !IsValidEditorTrack(target.editorTrack)))
		return OBSPTAudioRouteChangeResult::Rejected;

	int sourceIndex = -1;
	for (int i = 0; i < sources.size(); ++i) {
		if (sources[i].sourceId == sourceId) {
			sourceIndex = i;
			break;
		}
	}
	if (sourceIndex < 0 ||
	    (!mutedTarget &&
	     sources[sourceIndex].kind == OBSPTAudioSourceKind::DesktopAudio))
		return OBSPTAudioRouteChangeResult::Rejected;

	const OBSPTAudioRoute &current = sources[sourceIndex];
	if (mutedTarget) {
		QVector<QString> order;
		for (const OBSPTAudioRoute *route : MutedSources()) {
			if (route->sourceId != sourceId)
				order.push_back(route->sourceId);
		}
		const int destinationIndex =
			target.placement == OBSPTAudioTrackPlacement::Append
				? order.size()
				: target.index;
		if (destinationIndex < 0 || destinationIndex > order.size())
			return OBSPTAudioRouteChangeResult::Rejected;

		if (current.muted) {
			const QVector<const OBSPTAudioRoute *> before =
				MutedSources();
			int currentIndex = -1;
			for (int i = 0; i < before.size(); ++i) {
				if (before[i]->sourceId == sourceId) {
					currentIndex = i;
					break;
				}
			}
			if (currentIndex == destinationIndex)
				return OBSPTAudioRouteChangeResult::Unchanged;
		}

		order.insert(destinationIndex, sourceId);
		sources[sourceIndex].muted = true;
		mutedOrder = order;
		return OBSPTAudioRouteChangeResult::Changed;
	}

	if (!current.muted && current.editorTrack == target.editorTrack &&
	    target.placement == OBSPTAudioTrackPlacement::Append)
		return OBSPTAudioRouteChangeResult::Unchanged;
	if (current.muted && current.editorTrack == target.editorTrack &&
	    target.placement == OBSPTAudioTrackPlacement::Append) {
		sources[sourceIndex].muted = false;
		mutedOrder.removeAll(sourceId);
		return OBSPTAudioRouteChangeResult::Changed;
	}

	QVector<int> destinationPositions;
	for (int i = 0; i < sources.size(); ++i) {
		if (i != sourceIndex && !sources[i].muted &&
		    sources[i].editorTrack == target.editorTrack)
			destinationPositions.push_back(i);
	}
	if (target.placement == OBSPTAudioTrackPlacement::AtIndex &&
	    (target.index < 0 || target.index > destinationPositions.size()))
		return OBSPTAudioRouteChangeResult::Rejected;
	if (!current.muted && current.editorTrack == target.editorTrack &&
	    target.placement == OBSPTAudioTrackPlacement::AtIndex) {
		const QVector<const OBSPTAudioRoute *> ordered =
			SourcesForTrack(target.editorTrack);
		for (int i = 0; i < ordered.size(); ++i) {
			if (ordered[i]->sourceId == sourceId && i == target.index)
				return OBSPTAudioRouteChangeResult::Unchanged;
		}
	}

	OBSPTAudioRoute moved = sources.takeAt(sourceIndex);
	moved.editorTrack = target.editorTrack;
	moved.muted = false;
	mutedOrder.removeAll(sourceId);

	destinationPositions.clear();
	for (int i = 0; i < sources.size(); ++i) {
		if (!sources[i].muted &&
		    sources[i].editorTrack == target.editorTrack)
			destinationPositions.push_back(i);
	}

	const int destinationIndex =
		target.placement == OBSPTAudioTrackPlacement::Append
			? destinationPositions.size()
			: target.index;
	if (destinationPositions.isEmpty())
		sources.push_back(moved);
	else if (destinationIndex >= destinationPositions.size())
		sources.insert(destinationPositions.back() + 1, moved);
	else
		sources.insert(destinationPositions[destinationIndex], moved);

	return OBSPTAudioRouteChangeResult::Changed;
}

bool OBSPTAudioRoutingModel::MoveSource(const QString &sourceId,
				       int indexInTrack)
{
	if (TopologyLocked())
		return false;
	int sourceIndex = -1;
	int track = 0;
	for (int i = 0; i < sources.size(); ++i) {
		if (sources[i].sourceId == sourceId) {
			sourceIndex = i;
			track = sources[i].editorTrack;
			break;
		}
	}
	if (sourceIndex < 0 || !IsValidEditorTrack(track))
		return false;

	const int count = SourcesForTrack(track).size();
	if (indexInTrack < 0 || indexInTrack >= count)
		return false;

	OBSPTAudioRoute moved = sources.takeAt(sourceIndex);
	QVector<int> positions;
	for (int i = 0; i < sources.size(); ++i) {
		if (sources[i].editorTrack == track)
			positions.push_back(i);
	}

	if (positions.isEmpty())
		sources.push_back(moved);
	else if (indexInTrack >= positions.size())
		sources.insert(positions.back() + 1, moved);
	else
		sources.insert(positions[indexInTrack], moved);
	return true;
}

bool OBSPTAudioRoutingModel::DuplicateSource(const QString &sourceId,
					    const QString &newSourceId,
					    const QString &newSourceName)
{
	if (newSourceId.isEmpty() || FindSource(newSourceId))
		return false;

	for (int i = 0; i < sources.size(); ++i) {
		if (sources[i].sourceId != sourceId)
			continue;
		OBSPTAudioRoute duplicate = sources[i];
		duplicate.sourceId = newSourceId;
		duplicate.sourceName = newSourceName;
		sources.insert(i + 1, duplicate);
		return true;
	}

	return false;
}

bool OBSPTAudioRoutingModel::SetMuted(const QString &sourceId, bool muted)
{
	for (OBSPTAudioRoute &route : sources) {
		if (route.sourceId != sourceId)
			continue;
		if (route.muted == muted)
			return true;
		route.muted = muted;
		mutedOrder.removeAll(sourceId);
		if (muted)
			mutedOrder.prepend(sourceId);
		return true;
	}

	return false;
}

bool OBSPTAudioRoutingModel::SetMonitorOnly(const QString &sourceId,
					    bool monitorOnly)
{
	for (OBSPTAudioRoute &route : sources) {
		if (route.sourceId != sourceId)
			continue;
		if (TopologyLocked() && route.monitorOnly != monitorOnly)
			return false;
		route.monitorOnly = monitorOnly;
		return true;
	}

	return false;
}

bool OBSPTAudioRoutingModel::MoveMutedSource(const QString &sourceId,
					     int index)
{
	if (index < 0 || index >= mutedOrder.size())
		return false;
	const int current = mutedOrder.indexOf(sourceId);
	if (current < 0)
		return false;
	mutedOrder.move(current, index);
	return true;
}

bool OBSPTAudioRoutingModel::UpdateSourceName(const QString &sourceId,
					      const QString &sourceName)
{
	for (OBSPTAudioRoute &route : sources) {
		if (route.sourceId != sourceId)
			continue;
		route.sourceName = sourceName;
		return true;
	}

	return false;
}

bool OBSPTAudioRoutingModel::RemoveSource(const QString &sourceId)
{
	for (int i = 0; i < sources.size(); ++i) {
		if (sources[i].sourceId != sourceId)
			continue;
		sources.removeAt(i);
		mutedOrder.removeAll(sourceId);
		return true;
	}

	return false;
}

const OBSPTAudioRoute *
OBSPTAudioRoutingModel::FindSource(const QString &sourceId) const
{
	for (const OBSPTAudioRoute &route : sources) {
		if (route.sourceId == sourceId)
			return &route;
	}

	return nullptr;
}

QVector<const OBSPTAudioRoute *>
OBSPTAudioRoutingModel::SourcesForTrack(int track) const
{
	QVector<const OBSPTAudioRoute *> result;
	for (const OBSPTAudioRoute &route : sources) {
		if (route.editorTrack == track && !route.muted)
			result.push_back(&route);
	}
	return result;
}

OBSPTMixerBadge
OBSPTAudioRoutingModel::MixerBadge(const OBSPTAudioRoute &route) const
{
	if (mode == OBSPTAudioMode::SingleTrack)
		return route.kind == OBSPTAudioSourceKind::DesktopAudio
			       ? OBSPTMixerBadge::Mix
			       : OBSPTMixerBadge::Hidden;
	if (route.muted)
		return OBSPTMixerBadge::Muted;
	return route.kind == OBSPTAudioSourceKind::DesktopAudio
		       ? OBSPTMixerBadge::Mix
		       : OBSPTMixerBadge::EditorTrack;
}

QVector<const OBSPTAudioRoute *> OBSPTAudioRoutingModel::MutedSources() const
{
	QVector<const OBSPTAudioRoute *> result;
	for (const QString &sourceId : mutedOrder) {
		const OBSPTAudioRoute *route = FindSource(sourceId);
		if (route && route->muted)
			result.push_back(route);
	}
	for (const OBSPTAudioRoute &route : sources) {
		if (route.muted && !mutedOrder.contains(route.sourceId))
			result.push_back(&route);
	}
	return result;
}

OBSPTAudioMoveAvailability
OBSPTAudioRoutingModel::MoveAvailability(const QString &sourceId) const
{
	const OBSPTAudioRoute *route = FindSource(sourceId);
	if (!route)
		return {};
	if (route->kind == OBSPTAudioSourceKind::DesktopAudio && !route->muted)
		return {};
	const QVector<const OBSPTAudioRoute *> ordered =
		route->muted ? MutedSources()
			     : SourcesForTrack(route->editorTrack);
	for (int index = 0; index < ordered.size(); ++index) {
		if (ordered[index]->sourceId != sourceId)
			continue;
		return {index > 0, index + 1 < ordered.size()};
	}
	return {};
}

QVector<OBSPTAudioOutputTrack> OBSPTAudioRoutingModel::DeriveLocalTopology(
	const QString &format) const
{
	QSet<QString> initializedDesktopSources;
	for (const OBSPTAudioRoute &route : sources) {
		if (route.kind == OBSPTAudioSourceKind::DesktopAudio)
			initializedDesktopSources.insert(route.sourceId);
	}
	return DeriveLocalTopology(format, initializedDesktopSources);
}

OBSPTCustomContainerMode
OBSPTAudioRoutingModel::CustomContainerMode(const QString &format)
{
	const QString normalized = format.trimmed().toLower();
	if (normalized == QStringLiteral("matroska") ||
	    normalized == QStringLiteral("mkv") ||
	    normalized == QStringLiteral("mp4") ||
	    normalized == QStringLiteral("mov") ||
	    normalized == QStringLiteral("mpegts") ||
	    normalized == QStringLiteral("mpeg-ts") ||
	    normalized == QStringLiteral("ts"))
		return OBSPTCustomContainerMode::MultiTrack;

	if (normalized == QStringLiteral("flv") ||
	    normalized == QStringLiteral("hls") ||
	    normalized == QStringLiteral("m3u8"))
		return OBSPTCustomContainerMode::MixOnly;

	return OBSPTCustomContainerMode::Unsupported;
}

QVector<OBSPTAudioOutputTrack>
OBSPTAudioRoutingModel::DeriveStreamingTopology(int vodEditorTrack) const
{
	QSet<QString> initializedDesktopSources;
	for (const OBSPTAudioRoute &route : sources) {
		if (route.kind == OBSPTAudioSourceKind::DesktopAudio)
			initializedDesktopSources.insert(route.sourceId);
	}
	return DeriveStreamingTopology(vodEditorTrack,
				       initializedDesktopSources);
}

QVector<OBSPTAudioOutputTrack>
OBSPTAudioRoutingModel::DeriveStreamingTopology(
	int vodEditorTrack,
	const QSet<QString> &initializedDesktopSources,
	bool hasPrivateAudio) const
{
	QVector<OBSPTAudioOutputTrack> topology =
		DeriveLocalTopology(QStringLiteral("flv"),
				    initializedDesktopSources, hasPrivateAudio);
	if (topology.isEmpty()) {
		OBSPTAudioOutputTrack mix;
		mix.number = MixTrack;
		mix.name = TrackName(MixTrack);
		mix.defaultPlayback = true;
		topology.push_back(mix);
	}

	if (!HasEligibleEditorTrack(vodEditorTrack))
		return topology;

	OBSPTAudioOutputTrack vod;
	vod.number = vodEditorTrack;
	vod.name = TrackName(vodEditorTrack);
	for (const OBSPTAudioRoute &route : sources) {
		if (route.editorTrack == vodEditorTrack && !route.muted &&
		    !route.monitorOnly)
			vod.sourceIds.push_back(route.sourceId);
	}
	topology.push_back(vod);
	return topology;
}

bool OBSPTAudioRoutingModel::HasEligibleEditorTrack(int editorTrack) const
{
	/* Single-track mode has no Editor Track at all, so nothing is
	 * eligible - including the VOD track. */
	if (mode == OBSPTAudioMode::SingleTrack)
		return false;
	if (editorTrack < FirstEditorTrack || editorTrack > LastEditorTrack)
		return false;
	for (const OBSPTAudioRoute &route : sources) {
		if (route.editorTrack == editorTrack && !route.muted &&
		    !route.monitorOnly)
			return true;
	}
	return false;
}

int OBSPTAudioRoutingModel::PruneSourcesNotIn(const QSet<QString> &reachableIds)
{
	QVector<QString> doomed;
	for (const OBSPTAudioRoute &route : sources) {
		if (!reachableIds.contains(route.sourceId))
			doomed.push_back(route.sourceId);
	}
	for (const QString &sourceId : doomed)
		RemoveSource(sourceId);
	return doomed.size();
}

bool OBSPTAudioRoutingModel::SourceContributesPictureOnly(const QString &typeId)
{
	return typeId == QStringLiteral("dshow_input");
}

bool OBSPTAudioRoutingModel::ServiceSupportsVodTrack(
	const QString &serviceName)
{
	return serviceName.trimmed().compare(QStringLiteral("Twitch"),
					 Qt::CaseInsensitive) == 0;
}

QVector<OBSPTAudioOutputTrack> OBSPTAudioRoutingModel::DeriveLocalTopology(
	const QString &format,
	const QSet<QString> &initializedDesktopSources,
	bool hasPrivateAudio, bool includeMixTrack) const
{
	QVector<OBSPTAudioOutputTrack> topology;
	if (sources.isEmpty() && !hasPrivateAudio)
		return topology;

	/* The two modes are independent: a source muted for multi-track
	 * editing does not silence the single track, which has its own mute
	 * on the Mix itself. */
	const bool single = mode == OBSPTAudioMode::SingleTrack;
	if (single && singleTrackMuted)
		return topology;

	OBSPTAudioOutputTrack mix;
	mix.number = MixTrack;
	mix.name = TrackName(MixTrack);
	mix.defaultPlayback = true;
	bool desktopAuthority = false;
	for (const OBSPTAudioRoute &route : sources) {
		if (route.kind == OBSPTAudioSourceKind::DesktopAudio &&
		    initializedDesktopSources.contains(route.sourceId) &&
		    (single || (!route.muted && !route.monitorOnly))) {
			desktopAuthority = true;
			break;
		}
	}
	for (const OBSPTAudioRoute &route : sources) {
		if (!single && (route.muted || route.monitorOnly))
			continue;
		if (route.kind == OBSPTAudioSourceKind::DesktopAudio) {
			if (initializedDesktopSources.contains(route.sourceId))
				mix.sourceIds.push_back(route.sourceId);
		} else if (route.kind !=
				   OBSPTAudioSourceKind::ApplicationAudio ||
			   !desktopAuthority) {
			mix.sourceIds.push_back(route.sourceId);
		}
	}
	const bool mixOnlyContainer =
		format.compare(QStringLiteral("flv"), Qt::CaseInsensitive) ==
				0 ||
		format.compare(QStringLiteral("m3u8"), Qt::CaseInsensitive) ==
				0 ||
		format.compare(QStringLiteral("hls"), Qt::CaseInsensitive) == 0;

	/* The Profile's Mix Track Behavior option only gates containers
	 * that carry several tracks: single-track mode and single-track
	 * containers always keep the Mix. Desktop Audio lives on the Mix
	 * by domain rule, so an omitted Mix is not recorded at all. */
	const bool includeMix =
		includeMixTrack || single || mixOnlyContainer;
	if (includeMix) {
		if (mix.sourceIds.isEmpty() && !hasPrivateAudio)
			return topology;
		topology.push_back(mix);

		if (single || mixOnlyContainer)
			return topology;
	}

	for (int track = FirstEditorTrack; track <= LastEditorTrack; ++track) {
		OBSPTAudioOutputTrack outputTrack;
		outputTrack.number = track;
		outputTrack.name = TrackName(track);

		for (const OBSPTAudioRoute &route : sources) {
			if (route.editorTrack == track && !route.muted &&
			    !route.monitorOnly)
				outputTrack.sourceIds.push_back(route.sourceId);
		}

		if (!outputTrack.sourceIds.isEmpty())
			topology.push_back(outputTrack);
	}

	/* Without the Mix the file starts at the first non-empty Editor
	 * Track; flag it so exactly one track stays marked as the
	 * default playback stream. */
	if (!includeMix && !topology.isEmpty())
		topology.first().defaultPlayback = true;

	return topology;
}

QJsonObject OBSPTAudioRoutingModel::ToJson() const
{
	QJsonObject object;
	object.insert(QStringLiteral("version"), CurrentVersion);
	object.insert(QStringLiteral("audioMode"),
		      mode == OBSPTAudioMode::SingleTrack
			      ? QStringLiteral("single")
			      : QStringLiteral("multi"));
	object.insert(QStringLiteral("singleTrackMuted"), singleTrackMuted);

	QJsonArray tracks;
	for (int track = FirstEditorTrack; track <= LastEditorTrack; ++track) {
		QJsonObject item;
		item.insert(QStringLiteral("number"), track);
		item.insert(QStringLiteral("name"), TrackName(track));
		tracks.push_back(item);
	}
	object.insert(QStringLiteral("tracks"), tracks);

	QJsonArray sourceArray;
	for (const OBSPTAudioRoute &route : sources) {
		QJsonObject item;
		item.insert(QStringLiteral("id"), route.sourceId);
		item.insert(QStringLiteral("name"), route.sourceName);
		item.insert(QStringLiteral("kind"), KindToString(route.kind));
		item.insert(QStringLiteral("editor_track"), route.editorTrack);
		item.insert(QStringLiteral("muted"), route.muted);
		item.insert(QStringLiteral("monitor_only"), route.monitorOnly);
		sourceArray.push_back(item);
	}
	object.insert(QStringLiteral("sources"), sourceArray);
	QJsonArray mutedArray;
	for (const QString &sourceId : mutedOrder)
		mutedArray.push_back(sourceId);
	object.insert(QStringLiteral("muted_order"), mutedArray);

	return object;
}

OBSPTAudioRoutingModel
OBSPTAudioRoutingModel::FromJson(const QJsonObject &object)
{
	OBSPTAudioRoutingModel model;

	/* Absent in configurations written before the mode existed, which
	 * must keep behaving as multi-track. */
	if (object.value(QStringLiteral("audioMode")).toString() ==
	    QStringLiteral("single"))
		model.SetMode(OBSPTAudioMode::SingleTrack);
	model.SetSingleTrackMuted(
		object.value(QStringLiteral("singleTrackMuted")).toBool());

	const QJsonArray tracks = object.value(QStringLiteral("tracks")).toArray();
	for (const QJsonValue &value : tracks) {
		const QJsonObject item = value.toObject();
		const int track = item.value(QStringLiteral("number")).toInt();
		model.SetTrackName(track,
				   item.value(QStringLiteral("name")).toString());
	}

	const QJsonArray sourceArray =
		object.value(QStringLiteral("sources")).toArray();
	for (const QJsonValue &value : sourceArray) {
		const QJsonObject item = value.toObject();
		const QString sourceId = item.value(QStringLiteral("id")).toString();
		const int editorTrack =
			item.value(QStringLiteral("editor_track")).toInt();

		if (!model.AddSource(
			    sourceId, item.value(QStringLiteral("name")).toString(),
			    KindFromString(
				    item.value(QStringLiteral("kind")).toString())))
			continue;
		model.AssignSource(sourceId, editorTrack);
		for (OBSPTAudioRoute &route : model.sources) {
			if (route.sourceId == sourceId) {
				route.muted =
					item.value(QStringLiteral("muted")).toBool();
				route.monitorOnly = item
					.value(QStringLiteral("monitor_only"))
					.toBool();
				if (route.muted)
					model.mutedOrder.push_back(sourceId);
				break;
			}
		}
	}
	const QJsonArray mutedOrder =
		object.value(QStringLiteral("muted_order")).toArray();
	if (!mutedOrder.isEmpty()) {
		model.mutedOrder.clear();
		for (const QJsonValue &value : mutedOrder)
			if (model.FindSource(value.toString()) &&
			    model.FindSource(value.toString())->muted)
				model.mutedOrder.push_back(value.toString());
	}

	return model;
}

void OBSPTAudioRoutingModel::BeginTopologySession()
{
	if (topologySessionRefs++ == 0)
		frozenRouting = ToJson();
}

void OBSPTAudioRoutingModel::EndTopologySession()
{
	if (topologySessionRefs <= 0)
		return;
	if (--topologySessionRefs == 0)
		frozenRouting = QJsonObject();
}

OBSPTAudioRoutingModel OBSPTAudioRoutingModel::FrozenRoutingModel() const
{
	return TopologyLocked() ? FromJson(frozenRouting) : FromJson(ToJson());
}
