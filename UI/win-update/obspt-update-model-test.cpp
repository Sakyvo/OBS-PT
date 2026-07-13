#include "obspt-update-model.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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

QJsonObject ValidRelease(const QString &tag = QStringLiteral("1.1.0"))
{
	QString normalized;
	QVersionNumber version;
	ParseOBSStableVersion(tag, version, &normalized);
	const QString name =
		QStringLiteral("OBS-PT-%1-Installer.exe").arg(normalized);

	QJsonObject asset;
	asset.insert(QStringLiteral("name"), name);
	asset.insert(QStringLiteral("size"), 12345);
	asset.insert(QStringLiteral("browser_download_url"),
		     QStringLiteral("https://github.com/Sakyvo/OBS-PT/releases/"
				    "download/%1/%2")
			     .arg(tag, name));
	asset.insert(QStringLiteral("digest"),
		     QStringLiteral("sha256:") + QString(64, QLatin1Char('a')));

	QJsonObject release;
	release.insert(QStringLiteral("tag_name"), tag);
	release.insert(QStringLiteral("draft"), false);
	release.insert(QStringLiteral("prerelease"), false);
	release.insert(QStringLiteral("body"), QStringLiteral("# Notes"));
	release.insert(QStringLiteral("html_url"),
		       QStringLiteral("https://github.com/Sakyvo/OBS-PT/releases/"
				      "tag/%1")
			       .arg(tag));
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	return release;
}

OBSUpdateParseResult Parse(const QJsonObject &object)
{
	return ParseOBSUpdateRelease(QJsonDocument(object).toJson());
}

void TestVersions()
{
	QVersionNumber version;
	QString normalized;
	Check(ParseOBSStableVersion(QStringLiteral("1.2.3"), version,
				    &normalized),
	      "plain stable version parses");
	Check(normalized == QStringLiteral("1.2.3"),
	      "plain version normalizes");
	Check(ParseOBSStableVersion(QStringLiteral("v10.20.30"), version,
				    &normalized),
	      "leading-v version parses");
	Check(normalized == QStringLiteral("10.20.30"),
	      "leading-v version normalizes");
	Check(!ParseOBSStableVersion(QStringLiteral("1.2"), version),
	      "two-component version is rejected");
	Check(!ParseOBSStableVersion(QStringLiteral("1.2.3-beta"), version),
	      "prerelease suffix is rejected");
	Check(!ParseOBSStableVersion(QStringLiteral("01.2.3"), version),
	      "leading zero is rejected");
	Check(!ParseOBSStableVersion(QStringLiteral(" 1.2.3"), version),
	      "version whitespace is rejected");

	QVersionNumber current;
	QVersionNumber older;
	QVersionNumber newer;
	ParseOBSStableVersion(QStringLiteral("1.0.0"), current);
	ParseOBSStableVersion(QStringLiteral("0.9.9"), older);
	ParseOBSStableVersion(QStringLiteral("1.0.1"), newer);
	Check(QVersionNumber::compare(current, current) == 0,
	      "semantic version comparison detects current release");
	Check(QVersionNumber::compare(older, current) < 0,
	      "semantic version comparison detects older release");
	Check(QVersionNumber::compare(newer, current) > 0,
	      "semantic version comparison detects newer release");
}

void TestValidRelease()
{
	const OBSUpdateParseResult result = Parse(ValidRelease());
	Check(static_cast<bool>(result), "valid release parses");
	Check(result.release.versionText == QStringLiteral("1.1.0"),
	      "valid release version is normalized");
	Check(result.release.installer.name ==
		      QStringLiteral("OBS-PT-1.1.0-Installer.exe"),
	      "valid installer is selected");
	Check(result.release.installer.sha256.size() == 32,
	      "valid digest decodes to 32 bytes");

	const OBSUpdateParseResult leadingV = Parse(ValidRelease("v1.2.0"));
	Check(static_cast<bool>(leadingV), "leading-v release parses");
	Check(leadingV.release.installer.name ==
		      QStringLiteral("OBS-PT-1.2.0-Installer.exe"),
	      "leading-v release uses normalized installer name");
}

void TestReleaseFailures()
{
	Check(ParseOBSUpdateRelease("not-json").error ==
		      OBSUpdateParseError::InvalidJson,
	      "malformed JSON is rejected");

	QJsonObject release = ValidRelease();
	release.remove(QStringLiteral("draft"));
	Check(Parse(release).error == OBSUpdateParseError::InvalidRelease,
	      "release missing a required field is rejected");

	release = ValidRelease();
	release.insert(QStringLiteral("draft"), true);
	Check(Parse(release).error == OBSUpdateParseError::UnstableRelease,
	      "draft release is rejected");

	release = ValidRelease();
	release.insert(QStringLiteral("prerelease"), true);
	Check(Parse(release).error == OBSUpdateParseError::UnstableRelease,
	      "prerelease is rejected");

	release = ValidRelease();
	release.insert(QStringLiteral("tag_name"), QStringLiteral("1.1-beta"));
	Check(Parse(release).error == OBSUpdateParseError::InvalidVersion,
	      "invalid release tag is rejected");

	release = ValidRelease();
	release.insert(QStringLiteral("assets"), QJsonArray{});
	Check(Parse(release).error == OBSUpdateParseError::MissingInstaller,
	      "missing installer is rejected");

	release = ValidRelease();
	QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
	assets.append(assets.first());
	release.insert(QStringLiteral("assets"), assets);
	Check(Parse(release).error == OBSUpdateParseError::DuplicateInstaller,
	      "duplicate installer is rejected");

	release = ValidRelease();
	assets = release.value(QStringLiteral("assets")).toArray();
	QJsonObject asset = assets.first().toObject();
	asset.insert(QStringLiteral("size"), 0);
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidInstaller,
	      "zero-size installer is rejected");

	release = ValidRelease();
	asset = release.value(QStringLiteral("assets")).toArray().first().toObject();
	asset.insert(QStringLiteral("size"), 1.5);
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidInstaller,
	      "fractional installer size is rejected");

	release = ValidRelease();
	asset = release.value(QStringLiteral("assets")).toArray().first().toObject();
	asset.insert(QStringLiteral("browser_download_url"),
		     QStringLiteral("https://example.com/installer.exe"));
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidInstaller,
	      "non-GitHub installer URL is rejected");

	release = ValidRelease();
	asset = release.value(QStringLiteral("assets")).toArray().first().toObject();
	asset.insert(QStringLiteral("browser_download_url"),
		     QStringLiteral("https://github.com/Sakyvo/OBS-PT/releases/"
				    "download/9.9.9/OBS-PT-1.1.0-Installer.exe"));
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidInstaller,
	      "download URL with a mismatched tag is rejected");

	release = ValidRelease();
	asset = release.value(QStringLiteral("assets")).toArray().first().toObject();
	asset.insert(QStringLiteral("name"),
		     QStringLiteral("OBS-PT-9.9.9-Installer.exe"));
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidInstaller,
	      "version-mismatched installer name is rejected");

	release = ValidRelease();
	release.insert(QStringLiteral("html_url"),
		       QStringLiteral("https://github.com/Sakyvo/OBS-PT/releases/"
				      "tag/9.9.9"));
	Check(Parse(release).error == OBSUpdateParseError::InvalidReleaseUrl,
	      "release URL with a mismatched tag is rejected");

	release = ValidRelease();
	asset = release.value(QStringLiteral("assets")).toArray().first().toObject();
	asset.remove(QStringLiteral("digest"));
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidDigest,
	      "missing digest is rejected");

	release = ValidRelease();
	asset = release.value(QStringLiteral("assets")).toArray().first().toObject();
	asset.insert(QStringLiteral("digest"), QStringLiteral("sha256:1234"));
	release.insert(QStringLiteral("assets"), QJsonArray{asset});
	Check(Parse(release).error == OBSUpdateParseError::InvalidDigest,
	      "malformed digest is rejected");

	Check(IsAllowedGitHubRedirectUrl(QUrl(QStringLiteral(
		      "https://release-assets.githubusercontent.com/file?token=x"))),
	      "GitHub release-asset redirect is allowed");
	Check(!IsAllowedGitHubRedirectUrl(
		      QUrl(QStringLiteral("http://release-assets.githubusercontent.com/file"))),
	      "redirect downgrade is rejected");
	Check(!IsAllowedGitHubRedirectUrl(
		      QUrl(QStringLiteral("https://example.com/file"))),
	      "non-GitHub redirect is rejected");
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	TestVersions();
	TestValidRelease();
	TestReleaseFailures();
	if (failures == 0)
		std::cout << "All OBS-PT update model tests passed\n";
	return failures == 0 ? 0 : 1;
}
