#include "obspt-update-model.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include <cmath>
#include <limits>

namespace {

constexpr const char *RELEASE_PATH_PREFIX =
	"/Sakyvo/OBS-PT/releases/download/";
constexpr double MAX_EXACT_JSON_INTEGER = 9007199254740991.0;

OBSUpdateParseResult Failure(OBSUpdateParseError error, const QString &detail)
{
	OBSUpdateParseResult result;
	result.error = error;
	result.detail = detail;
	return result;
}

bool IsHttpsHost(const QUrl &url, const QString &host)
{
	return url.isValid() && url.scheme().compare("https", Qt::CaseInsensitive) ==
					     0 &&
	       url.host().compare(host, Qt::CaseInsensitive) == 0 &&
	       url.userInfo().isEmpty() &&
	       (url.port(-1) == -1 || url.port(-1) == 443);
}

} // namespace

bool ParseOBSStableVersion(const QString &text, QVersionNumber &version,
			   QString *normalized)
{
	static const QRegularExpression versionPattern(
		QStringLiteral("^v?(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\."
			       "(0|[1-9][0-9]*)$"));

	const QRegularExpressionMatch match = versionPattern.match(text);
	if (!match.hasMatch())
		return false;

	QVector<int> segments;
	segments.reserve(3);
	for (int i = 1; i <= 3; ++i) {
		bool ok = false;
		const qulonglong value = match.captured(i).toULongLong(&ok);
		if (!ok || value > static_cast<qulonglong>(
					 std::numeric_limits<int>::max()))
			return false;
		segments.push_back(static_cast<int>(value));
	}

	version = QVersionNumber(segments);
	if (normalized)
		*normalized = version.toString();
	return true;
}

bool IsAllowedGitHubDownloadUrl(const QUrl &url, const QString &releaseTag,
				const QString &assetName)
{
	if (!IsHttpsHost(url, QStringLiteral("github.com")))
		return false;
	if (!url.query().isEmpty() || !url.fragment().isEmpty())
		return false;
	const QString expectedPath = QString::fromLatin1(RELEASE_PATH_PREFIX) +
				     releaseTag + QLatin1Char('/') + assetName;
	return url.path() == expectedPath;
}

bool IsAllowedGitHubRedirectUrl(const QUrl &url)
{
	if (!url.isValid() ||
	    url.scheme().compare("https", Qt::CaseInsensitive) != 0 ||
	    !url.userInfo().isEmpty() ||
	    (url.port(-1) != -1 && url.port(-1) != 443))
		return false;

	const QString host = url.host().toLower();
	return host == QStringLiteral("github.com") ||
	       host == QStringLiteral("objects.githubusercontent.com") ||
	       host == QStringLiteral("release-assets.githubusercontent.com");
}

OBSUpdateParseResult ParseOBSUpdateRelease(const QByteArray &json)
{
	QJsonParseError jsonError;
	const QJsonDocument document = QJsonDocument::fromJson(json, &jsonError);
	if (jsonError.error != QJsonParseError::NoError || !document.isObject())
		return Failure(OBSUpdateParseError::InvalidJson,
			       jsonError.errorString());

	const QJsonObject root = document.object();
	if (!root.value(QStringLiteral("draft")).isBool() ||
	    !root.value(QStringLiteral("prerelease")).isBool() ||
	    !root.value(QStringLiteral("tag_name")).isString() ||
	    !root.value(QStringLiteral("html_url")).isString() ||
	    !root.value(QStringLiteral("assets")).isArray()) {
		return Failure(OBSUpdateParseError::InvalidRelease,
			       QStringLiteral("Required release fields are missing"));
	}

	if (root.value(QStringLiteral("draft")).toBool() ||
	    root.value(QStringLiteral("prerelease")).toBool()) {
		return Failure(OBSUpdateParseError::UnstableRelease,
			       QStringLiteral("Release is a draft or prerelease"));
	}

	OBSUpdateParseResult result;
	const QString tag = root.value(QStringLiteral("tag_name")).toString();
	if (!ParseOBSStableVersion(tag, result.release.version,
				   &result.release.versionText)) {
		return Failure(OBSUpdateParseError::InvalidVersion,
			       QStringLiteral("Invalid release tag: %1").arg(tag));
	}

	result.release.releaseUrl =
		QUrl(root.value(QStringLiteral("html_url")).toString());
	if (!IsHttpsHost(result.release.releaseUrl,
			 QStringLiteral("github.com")) ||
	    !result.release.releaseUrl.query().isEmpty() ||
	    !result.release.releaseUrl.fragment().isEmpty() ||
	    result.release.releaseUrl.path() !=
		    QStringLiteral("/Sakyvo/OBS-PT/releases/tag/") + tag) {
		return Failure(OBSUpdateParseError::InvalidReleaseUrl,
			       QStringLiteral("Invalid release URL"));
	}

	if (root.value(QStringLiteral("body")).isString())
		result.release.notesMarkdown =
			root.value(QStringLiteral("body")).toString();

	const QString expectedName =
		QStringLiteral("OBS-PT-%1-Installer.exe")
			.arg(result.release.versionText);
	QJsonObject installer;
	int installerCount = 0;
	const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
	for (const QJsonValue &value : assets) {
		if (!value.isObject())
			continue;
		const QJsonObject asset = value.toObject();
		const QJsonValue nameValue = asset.value(QStringLiteral("name"));
		if (!nameValue.isString())
			continue;
		const QString name = nameValue.toString();
		const bool installerLike =
			name.startsWith(QStringLiteral("OBS-PT-")) &&
			name.endsWith(QStringLiteral("-Installer.exe"));
		if (!installerLike)
			continue;
		if (name != expectedName) {
			return Failure(
				OBSUpdateParseError::InvalidInstaller,
				QStringLiteral("Version-mismatched installer asset: %1")
					.arg(name));
		}
		installer = asset;
		++installerCount;
	}

	if (installerCount == 0)
		return Failure(OBSUpdateParseError::MissingInstaller,
			       QStringLiteral("Expected asset not found: %1")
				       .arg(expectedName));
	if (installerCount != 1)
		return Failure(OBSUpdateParseError::DuplicateInstaller,
			       QStringLiteral("Duplicate installer assets: %1")
				       .arg(expectedName));

	const QJsonValue sizeValue = installer.value(QStringLiteral("size"));
	const double sizeDouble = sizeValue.toDouble(-1.0);
	if (!sizeValue.isDouble() || sizeDouble <= 0.0 ||
	    sizeDouble > MAX_EXACT_JSON_INTEGER ||
	    std::floor(sizeDouble) != sizeDouble) {
		return Failure(OBSUpdateParseError::InvalidInstaller,
			       QStringLiteral("Invalid installer size"));
	}

	result.release.installer.name = expectedName;
	result.release.installer.size = static_cast<qint64>(sizeDouble);
	result.release.installer.downloadUrl = QUrl(
		installer.value(QStringLiteral("browser_download_url")).toString());
	if (!installer.value(QStringLiteral("browser_download_url")).isString() ||
	    !IsAllowedGitHubDownloadUrl(result.release.installer.downloadUrl,
					tag, expectedName)) {
		return Failure(OBSUpdateParseError::InvalidInstaller,
			       QStringLiteral("Invalid installer download URL"));
	}

	if (!installer.value(QStringLiteral("digest")).isString()) {
		return Failure(OBSUpdateParseError::InvalidDigest,
			       QStringLiteral("Installer SHA-256 digest is missing"));
	}

	static const QRegularExpression digestPattern(
		QStringLiteral("^sha256:([0-9a-fA-F]{64})$"));
	const QString digest = installer.value(QStringLiteral("digest")).toString();
	const QRegularExpressionMatch digestMatch = digestPattern.match(digest);
	if (!digestMatch.hasMatch()) {
		return Failure(OBSUpdateParseError::InvalidDigest,
			       QStringLiteral("Installer SHA-256 digest is malformed"));
	}

	result.release.installer.sha256 =
		QByteArray::fromHex(digestMatch.captured(1).toLatin1());
	if (result.release.installer.sha256.size() != 32) {
		return Failure(OBSUpdateParseError::InvalidDigest,
			       QStringLiteral("Installer SHA-256 digest has wrong size"));
	}

	return result;
}
