#include "obspt-audio-source-creation-model.hpp"

int ResolveSelectedEditorTrack(int track, bool trackAnchor,
			       bool sourceCellSelected)
{
	return trackAnchor && !sourceCellSelected &&
			       track >= OBSPTAudioRoutingModel::FirstEditorTrack &&
			       track <= OBSPTAudioRoutingModel::LastEditorTrack
		       ? track
		       : 0;
}

OBSPTAudioSourceCreationPolicy ResolveAudioSourceCreationPolicy(
	bool collectionAudio, OBSPTAudioSourceKind kind, int selectedEditorTrack,
	const QSet<int> &occupiedEditorTracks)
{
	OBSPTAudioSourceCreationPolicy policy;
	if (!collectionAudio)
		return policy;

	policy.showExisting = false;
	if (kind == OBSPTAudioSourceKind::DesktopAudio) {
		policy.fixedMix = true;
		return policy;
	}

	policy.routeRequired =
		kind == OBSPTAudioSourceKind::ApplicationAudio;
	if (selectedEditorTrack >=
		    OBSPTAudioRoutingModel::FirstEditorTrack &&
	    selectedEditorTrack <= OBSPTAudioRoutingModel::LastEditorTrack) {
		policy.initialEditorTrack = selectedEditorTrack;
		return policy;
	}

	if (policy.routeRequired)
		return policy;
	if (kind == OBSPTAudioSourceKind::AudioInput) {
		policy.initialEditorTrack = 4;
		return policy;
	}

	for (int track = OBSPTAudioRoutingModel::FirstEditorTrack;
	     track <= OBSPTAudioRoutingModel::LastEditorTrack; ++track) {
		if (!occupiedEditorTracks.contains(track)) {
			policy.initialEditorTrack = track;
			return policy;
		}
	}
	policy.initialEditorTrack = OBSPTAudioRoutingModel::LastEditorTrack;
	return policy;
}
