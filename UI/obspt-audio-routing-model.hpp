#pragma once

#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

enum class OBSPTAudioSourceKind {
	ApplicationAudio,
	AudioInput,
	DesktopAudio,
	Other,
};

enum class OBSPTCustomContainerMode {
	MultiTrack,
	MixOnly,
	Unsupported,
};

/* Top-level switch of the Audio Routing Configuration. SingleTrack means
 * "Compatibility Mix only, no Editor Track": every input mixes into one
 * track. The two modes hold separate settings and neither disturbs the
 * other. */
enum class OBSPTAudioMode { MultiTrack, SingleTrack };

struct OBSPTAudioRoute {
	QString sourceId;
	QString sourceName;
	OBSPTAudioSourceKind kind = OBSPTAudioSourceKind::Other;
	int editorTrack = 2;
	bool muted = false;
	bool monitorOnly = false;
};

struct OBSPTAudioOutputTrack {
	int number = 0;
	QString name;
	bool defaultPlayback = false;
	QVector<QString> sourceIds;
};

struct OBSPTAudioMoveAvailability {
	bool up = false;
	bool down = false;
};

/* Paint-time classification of a Mixer's per-source track badge. The badge
 * is derived, never stored: multi-track renders the true assignment with
 * Muted winning over kind, while single-track hides everything except the
 * fixed Mix identity of Desktop Audio, because preserved assignments and
 * mute flags carry no meaning in that mode. */
enum class OBSPTMixerBadge {
	Hidden,
	Mix,
	EditorTrack,
	Muted,
};

enum class OBSPTAudioRouteChangeResult {
	Rejected,
	Unchanged,
	Changed,
};

enum class OBSPTAudioTrackPlacement {
	Append,
	AtIndex,
};

struct OBSPTAudioTrackTarget {
	int editorTrack = 0;
	OBSPTAudioTrackPlacement placement = OBSPTAudioTrackPlacement::Append;
	int index = 0;
};

class OBSPTAudioRoutingModel {
public:
	static constexpr int CurrentVersion = 1;
	static constexpr int MixTrack = 1;
	static constexpr int FirstEditorTrack = 2;
	static constexpr int LastEditorTrack = 6;
	static constexpr const char *InitialTrackSetting =
		"obspt_initial_editor_track";

	OBSPTAudioRoutingModel();

	OBSPTAudioMode Mode() const { return mode; }
	void SetMode(OBSPTAudioMode newMode) { mode = newMode; }

	/* Muting in single-track mode applies to the Compatibility Mix
	 * itself. It is not a Muted Sources Group membership, which does not
	 * exist in that mode. */
	bool SingleTrackMuted() const { return singleTrackMuted; }
	void SetSingleTrackMuted(bool muted) { singleTrackMuted = muted; }

	QString TrackName(int track) const;
	bool SetTrackName(int track, const QString &name);

	bool AddSource(const QString &sourceId, const QString &sourceName,
		       OBSPTAudioSourceKind kind, int initialEditorTrack = 0);
	bool AssignSource(const QString &sourceId, int editorTrack);
	OBSPTAudioRouteChangeResult
	MoveSourceToEditorTrack(const QString &sourceId,
				const OBSPTAudioTrackTarget &target);
	bool MoveSource(const QString &sourceId, int indexInTrack);
	bool DuplicateSource(const QString &sourceId, const QString &newSourceId,
			     const QString &newSourceName);
	bool SetMuted(const QString &sourceId, bool muted);
	bool SetMonitorOnly(const QString &sourceId, bool monitorOnly);
	bool MoveMutedSource(const QString &sourceId, int index);
	bool UpdateSourceName(const QString &sourceId, const QString &sourceName);
	bool RemoveSource(const QString &sourceId);

	OBSPTMixerBadge MixerBadge(const OBSPTAudioRoute &route) const;

	/* Drop every entry whose source is no longer reachable. The model
	 * cannot see scenes, so the caller supplies the reachable set; an
	 * empty set means nothing is reachable, not "skip". Returns how many
	 * entries were dropped. */
	int PruneSourcesNotIn(const QSet<QString> &reachableIds);
	const OBSPTAudioRoute *FindSource(const QString &sourceId) const;
	QVector<const OBSPTAudioRoute *> SourcesForTrack(int track) const;
	QVector<const OBSPTAudioRoute *> MutedSources() const;
	OBSPTAudioMoveAvailability
	MoveAvailability(const QString &sourceId) const;
	const QVector<OBSPTAudioRoute> &Sources() const { return sources; }

	QVector<OBSPTAudioOutputTrack>
	DeriveLocalTopology(const QString &format) const;
	QVector<OBSPTAudioOutputTrack>
	DeriveLocalTopology(const QString &format,
			    const QSet<QString> &initializedDesktopSources,
			    bool hasPrivateAudio = false,
			    bool includeMixTrack = true) const;
	QVector<OBSPTAudioOutputTrack>
	DeriveStreamingTopology(int vodEditorTrack = 0) const;
	QVector<OBSPTAudioOutputTrack>
	DeriveStreamingTopology(
		int vodEditorTrack,
		const QSet<QString> &initializedDesktopSources,
		bool hasPrivateAudio = false) const;
	bool HasEligibleEditorTrack(int editorTrack) const;
	static OBSPTCustomContainerMode CustomContainerMode(const QString &format);
	static bool ServiceSupportsVodTrack(const QString &serviceName);

	/* The Video Capture Device is the one documented exception to the
	 * capability-based panel boundary: its sound comes from an
	 * independent physical device, so the picture source contributes
	 * picture only and the microphone is added separately. Sources whose
	 * audio is part of their content - media, browser, VLC - keep it. */
	static bool SourceContributesPictureOnly(const QString &typeId);
	static QString RepairDefaultSourceName(const QString &sourceId,
					       const QString &sourceName,
					       const QString &defaultName);

	QJsonObject ToJson() const;
	static OBSPTAudioRoutingModel FromJson(const QJsonObject &object);

	void BeginTopologySession();
	void EndTopologySession();
	bool TopologyLocked() const { return topologySessionRefs > 0; }
	OBSPTAudioRoutingModel FrozenRoutingModel() const;

private:
	QVector<QString> trackNames;
	QVector<OBSPTAudioRoute> sources;
	QVector<QString> mutedOrder;
	OBSPTAudioMode mode = OBSPTAudioMode::MultiTrack;
	bool singleTrackMuted = false;
	int topologySessionRefs = 0;
	QJsonObject frozenRouting;
	int DefaultEditorTrack(OBSPTAudioSourceKind kind) const;
};
