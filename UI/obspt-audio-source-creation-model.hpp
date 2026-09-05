#pragma once

#include "obspt-audio-routing-model.hpp"

#include <QSet>

struct OBSPTAudioSourceCreationPolicy {
	bool showExisting = true;
	bool routeRequired = false;
	bool fixedMix = false;
	int initialEditorTrack = 0;
};

int ResolveSelectedEditorTrack(int track, bool trackAnchor,
			       bool sourceCellSelected);

OBSPTAudioSourceCreationPolicy ResolveAudioSourceCreationPolicy(
	bool collectionAudio, OBSPTAudioSourceKind kind, int selectedEditorTrack,
	const QSet<int> &occupiedEditorTracks);
