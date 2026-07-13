#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

enum class OBSUpdateParseError {
	None,
	InvalidJson,
	InvalidRelease,
	InvalidVersion,
	UnstableRelease,
	InvalidReleaseUrl,
	MissingInstaller,
	DuplicateInstaller,
	InvalidInstaller,
	InvalidDigest,
};

struct OBSUpdateAsset {
	QString name;
	QUrl downloadUrl;
	qint64 size = 0;
	QByteArray sha256;
};

struct OBSUpdateRelease {
	QVersionNumber version;
	QString versionText;
	QString notesMarkdown;
	QUrl releaseUrl;
	OBSUpdateAsset installer;
};

struct OBSUpdateParseResult {
	OBSUpdateRelease release;
	OBSUpdateParseError error = OBSUpdateParseError::None;
	QString detail;

	explicit operator bool() const
	{
		return error == OBSUpdateParseError::None;
	}
};

bool ParseOBSStableVersion(const QString &text, QVersionNumber &version,
			   QString *normalized = nullptr);
bool IsAllowedGitHubDownloadUrl(const QUrl &url, const QString &releaseTag,
				const QString &assetName);
bool IsAllowedGitHubRedirectUrl(const QUrl &url);
OBSUpdateParseResult ParseOBSUpdateRelease(const QByteArray &json);
