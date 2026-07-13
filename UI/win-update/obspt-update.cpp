#include "obspt-update.hpp"

#include "obs-app.hpp"
#include "qt-wrappers.hpp"
#include "update-window.hpp"
#include "window-basic-main.hpp"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>

#include <util/base.h>
#include <util/config-file.h>
#include <util/curl/curl-helper.h>

#include <limits>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace {

constexpr const char *LATEST_RELEASE_URL =
	"https://api.github.com/repos/Sakyvo/OBS-PT/releases/latest";
constexpr int UPDATE_TIMEOUT_MS = 30000;
constexpr int MAX_RELEASE_RESPONSE_SIZE = 2 * 1024 * 1024;

void SaveGlobalConfig()
{
	config_t *config = GetGlobalConfig();
	if (config && config_save_safe(config, "tmp", nullptr) != CONFIG_SUCCESS)
		blog(LOG_WARNING, "[OBS-PT Updater] Failed to save Global Config");
}

} // namespace

// The packaged libcurl uses Windows Schannel; Qt 5.15 HTTPS would require
// separately deployed OpenSSL runtime libraries.
struct OBSUpdateCurlState {
	enum class Kind { None, Check, Download };

	OBSUpdateManager *owner = nullptr;
	CURLM *multi = nullptr;
	CURL *easy = nullptr;
	curl_slist *headers = nullptr;
	QTimer *timer = nullptr;
	Kind kind = Kind::None;
	bool addedToMulti = false;
	QByteArray requestUrl;
	QByteArray userAgent;
	char errorBuffer[CURL_ERROR_SIZE] = {};

	explicit OBSUpdateCurlState(OBSUpdateManager *owner_) : owner(owner_)
	{
		multi = curl_multi_init();
		timer = new QTimer;
		timer->setInterval(10);
		QObject::connect(timer, &QTimer::timeout, owner,
				 [this]() { Poll(); });
	}

	~OBSUpdateCurlState()
	{
		Abort();
		delete timer;
		timer = nullptr;
		if (multi)
			curl_multi_cleanup(multi);
	}

	template<typename T>
	bool SetOption(CURLoption option, T value, QString &detail)
	{
		const CURLcode result = curl_easy_setopt(easy, option, value);
		if (result == CURLE_OK)
			return true;
		detail = QStringLiteral("curl_easy_setopt(%1) failed: %2")
				 .arg(static_cast<int>(option))
				 .arg(QString::fromLatin1(curl_easy_strerror(result)));
		return false;
	}

	bool AddHeader(const QByteArray &header, QString &detail)
	{
		curl_slist *next = curl_slist_append(headers, header.constData());
		if (next) {
			headers = next;
			return true;
		}
		detail = QStringLiteral("Could not allocate HTTP headers");
		return false;
	}

	bool Begin(const QByteArray &url, Kind nextKind, QString &detail)
	{
		Abort();
		if (!multi) {
			detail = QStringLiteral("curl_multi_init failed");
			return false;
		}

		easy = curl_easy_init();
		if (!easy) {
			detail = QStringLiteral("curl_easy_init failed");
			return false;
		}

		kind = nextKind;
		requestUrl = url;
		errorBuffer[0] = '\0';
		userAgent =
			QStringLiteral("OBS-PT/%1")
				.arg(QString::fromLatin1(OBS_VERSION))
				.toUtf8();

		const bool configured =
			SetOption(CURLOPT_URL, requestUrl.constData(), detail) &&
			SetOption(CURLOPT_USERAGENT, userAgent.constData(), detail) &&
			SetOption(CURLOPT_ACCEPT_ENCODING, "", detail) &&
			SetOption(CURLOPT_ERRORBUFFER, errorBuffer, detail) &&
			SetOption(CURLOPT_NOSIGNAL, 1L, detail) &&
			SetOption(CURLOPT_CONNECTTIMEOUT_MS,
				  static_cast<long>(UPDATE_TIMEOUT_MS), detail) &&
			SetOption(CURLOPT_LOW_SPEED_LIMIT, 1L, detail) &&
			SetOption(CURLOPT_LOW_SPEED_TIME,
				  static_cast<long>(UPDATE_TIMEOUT_MS / 1000),
				  detail) &&
			SetOption(CURLOPT_SSL_VERIFYPEER, 1L, detail) &&
			SetOption(CURLOPT_SSL_VERIFYHOST, 2L, detail) &&
			SetOption(CURLOPT_PROTOCOLS,
				  static_cast<long>(CURLPROTO_HTTPS), detail) &&
			SetOption(CURLOPT_TCP_KEEPALIVE, 1L, detail);
		if (!configured) {
			Abort();
			return false;
		}
		curl_obs_set_revoke_setting(easy);
		return true;
	}

	bool Start(QString &detail)
	{
		if (headers &&
		    !SetOption(CURLOPT_HTTPHEADER, headers, detail)) {
			Abort();
			return false;
		}

		const CURLMcode result = curl_multi_add_handle(multi, easy);
		if (result != CURLM_OK) {
			detail = QStringLiteral("curl_multi_add_handle failed: %1")
					 .arg(QString::fromLatin1(
						 curl_multi_strerror(result)));
			Abort();
			return false;
		}
		addedToMulti = true;
		timer->start();
		return true;
	}

	bool StartCheck(QString &detail)
	{
		if (!Begin(QByteArray(LATEST_RELEASE_URL), Kind::Check, detail))
			return false;

		const bool configured =
			AddHeader(QByteArray("Accept: application/vnd.github+json"),
				  detail) &&
			AddHeader(QByteArray(
					  "X-GitHub-Api-Version: 2022-11-28"),
				  detail) &&
			SetOption(CURLOPT_TIMEOUT_MS,
				  static_cast<long>(UPDATE_TIMEOUT_MS), detail) &&
			SetOption(CURLOPT_FOLLOWLOCATION, 0L, detail) &&
			SetOption(CURLOPT_WRITEFUNCTION, &CheckWrite, detail) &&
			SetOption(CURLOPT_WRITEDATA, this, detail);
		if (!configured) {
			Abort();
			return false;
		}
		return Start(detail);
	}

	bool StartDownload(const QUrl &url, QString &detail)
	{
		if (!Begin(url.toEncoded(QUrl::FullyEncoded), Kind::Download,
			   detail))
			return false;

		const bool configured =
			AddHeader(QByteArray("Accept: application/octet-stream"),
				  detail) &&
			SetOption(CURLOPT_FOLLOWLOCATION, 1L, detail) &&
			SetOption(CURLOPT_MAXREDIRS, 5L, detail) &&
			SetOption(CURLOPT_REDIR_PROTOCOLS,
				  static_cast<long>(CURLPROTO_HTTPS), detail) &&
			SetOption(CURLOPT_WRITEFUNCTION, &DownloadWrite, detail) &&
			SetOption(CURLOPT_WRITEDATA, this, detail) &&
			SetOption(CURLOPT_HEADERFUNCTION, &DownloadHeader, detail) &&
			SetOption(CURLOPT_HEADERDATA, this, detail) &&
			SetOption(CURLOPT_NOPROGRESS, 0L, detail) &&
			SetOption(CURLOPT_XFERINFOFUNCTION, &DownloadProgress,
				  detail) &&
			SetOption(CURLOPT_XFERINFODATA, this, detail);
		if (!configured) {
			Abort();
			return false;
		}
		return Start(detail);
	}

	void CleanupEasy()
	{
		if (timer)
			timer->stop();
		if (easy && addedToMulti)
			curl_multi_remove_handle(multi, easy);
		addedToMulti = false;
		if (easy)
			curl_easy_cleanup(easy);
		easy = nullptr;
		if (headers)
			curl_slist_free_all(headers);
		headers = nullptr;
		kind = Kind::None;
		requestUrl.clear();
		userAgent.clear();
	}

	void Abort()
	{
		CleanupEasy();
	}

	QString FailureDetail(CURLcode result, long status) const
	{
		const QString error = QString::fromUtf8(
			errorBuffer[0] ? errorBuffer : curl_easy_strerror(result));
		if (status > 0)
			return QStringLiteral("HTTP %1: %2").arg(status).arg(error);
		return error;
	}

	void Deliver(CURLcode result, long status, const QString &detail,
		     const QUrl &effectiveUrl)
	{
		const Kind completedKind = kind;
		CleanupEasy();
		if (completedKind == Kind::Check)
			owner->CheckFinished(static_cast<int>(result), status, detail);
		else if (completedKind == Kind::Download)
			owner->DownloadFinished(static_cast<int>(result), status,
						detail, effectiveUrl);
	}

	void Poll()
	{
		if (!easy || !addedToMulti)
			return;

		int running = 0;
		CURLMcode multiResult;
		do {
			multiResult = curl_multi_perform(multi, &running);
		} while (multiResult == CURLM_CALL_MULTI_PERFORM);

		if (multiResult != CURLM_OK) {
			Deliver(CURLE_FAILED_INIT, 0,
				QStringLiteral("curl_multi_perform failed: %1")
					.arg(QString::fromLatin1(
						curl_multi_strerror(multiResult))),
				QUrl());
			return;
		}

		int remaining = 0;
		while (CURLMsg *message = curl_multi_info_read(multi, &remaining)) {
			if (message->msg != CURLMSG_DONE ||
			    message->easy_handle != easy)
				continue;

			long status = 0;
			char *effectiveUrl = nullptr;
			curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &status);
			curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL,
					  &effectiveUrl);
			const QString detail = FailureDetail(message->data.result,
							    status);
			Deliver(message->data.result, status, detail,
				effectiveUrl
					? QUrl(QString::fromUtf8(effectiveUrl))
					: QUrl());
			return;
		}
	}

	static bool TransferSize(size_t size, size_t count, size_t &total)
	{
		if (size &&
		    count > (std::numeric_limits<size_t>::max)() / size)
			return false;
		total = size * count;
		return true;
	}

	static size_t CheckWrite(char *data, size_t size, size_t count,
				 void *opaque)
	{
		auto *state = static_cast<OBSUpdateCurlState *>(opaque);
		size_t total = 0;
		if (!TransferSize(size, count, total) ||
		    total > static_cast<size_t>(MAX_RELEASE_RESPONSE_SIZE -
						state->owner->responseData.size())) {
			state->owner->responseTooLarge = true;
			return 0;
		}
		state->owner->responseData.append(data, static_cast<int>(total));
		return total;
	}

	static size_t DownloadWrite(char *data, size_t size, size_t count,
				    void *opaque)
	{
		auto *state = static_cast<OBSUpdateCurlState *>(opaque);
		OBSUpdateManager *owner = state->owner;
		size_t total = 0;
		if (!TransferSize(size, count, total) || !owner->downloadFile ||
		    total >
			    static_cast<size_t>((std::numeric_limits<int>::max)())) {
			owner->downloadWriteFailed = true;
			return 0;
		}

		const qint64 chunkSize = static_cast<qint64>(total);
		if (owner->bytesWritten > owner->pendingRelease.installer.size ||
		    chunkSize >
			    owner->pendingRelease.installer.size - owner->bytesWritten) {
			owner->downloadSizeExceeded = true;
			return 0;
		}

		const qint64 written = owner->downloadFile->write(data, chunkSize);
		if (written != chunkSize) {
			owner->downloadWriteFailed = true;
			return 0;
		}
		owner->downloadHash->addData(data, static_cast<int>(total));
		owner->bytesWritten += written;
		return total;
	}

	static size_t DownloadHeader(char *data, size_t size, size_t count,
				     void *opaque)
	{
		auto *state = static_cast<OBSUpdateCurlState *>(opaque);
		size_t total = 0;
		if (!TransferSize(size, count, total) ||
		    total >
			    static_cast<size_t>((std::numeric_limits<int>::max)()))
			return 0;

		const QByteArray line(data, static_cast<int>(total));
		if (line.left(9).compare("Location:", Qt::CaseInsensitive) != 0)
			return total;

		const QUrl redirect(
			QString::fromUtf8(line.mid(9).trimmed()));
		if (redirect.isRelative() ||
		    !IsAllowedGitHubRedirectUrl(redirect)) {
			state->owner->downloadRedirectRejected = true;
			return 0;
		}
		return total;
	}

	static int DownloadProgress(void *opaque, curl_off_t total,
				    curl_off_t received, curl_off_t, curl_off_t)
	{
		auto *state = static_cast<OBSUpdateCurlState *>(opaque);
		state->owner->DownloadProgress(static_cast<qint64>(received),
					       static_cast<qint64>(total));
		return state->owner->downloadCanceled ? 1 : 0;
	}
};

OBSUpdateManager::OBSUpdateManager(OBSBasic *main_) : QObject(main_), main(main_)
{
	curl = new OBSUpdateCurlState(this);
	downloadHash = new QCryptographicHash(QCryptographicHash::Sha256);
}

OBSUpdateManager::~OBSUpdateManager()
{
	delete curl;
	curl = nullptr;
	CleanupDownload(true);
	delete downloadHash;
}

bool OBSUpdateManager::IsBusy() const
{
	return state != State::Idle;
}

void OBSUpdateManager::SetState(State nextState)
{
	const bool wasBusy = IsBusy();
	state = nextState;
	if (wasBusy != IsBusy())
		emit BusyChanged(IsBusy());
}

void OBSUpdateManager::Finish()
{
	SetState(State::Idle);
}

void OBSUpdateManager::Check(bool manual)
{
	if (IsBusy())
		return;

	manualCheck = manual;
	responseData.clear();
	responseTooLarge = false;
	SetState(State::Checking);
	StartCheckRequest();
}

void OBSUpdateManager::StartCheckRequest()
{
	QString detail;
	if (!curl->StartCheck(detail))
		HandleCheckFailure(QTStr("Updater.Error.Network"), detail);
}

void OBSUpdateManager::CheckFinished(int curlResult, long status,
				     const QString &detail)
{
	if (responseTooLarge) {
		HandleCheckFailure(QTStr("Updater.Error.ResponseTooLarge"),
				   QStringLiteral("Release response exceeded %1 bytes")
					   .arg(MAX_RELEASE_RESPONSE_SIZE));
		return;
	}
	if (curlResult != CURLE_OK || status != 200) {
		HandleCheckFailure(QTStr("Updater.Error.Network"), detail);
		return;
	}

	const OBSUpdateParseResult parsed = ParseOBSUpdateRelease(responseData);
	if (!parsed) {
		HandleCheckFailure(ParseErrorMessage(parsed.error), parsed.detail);
		return;
	}

	QVersionNumber currentVersion;
	QString normalizedCurrent;
	if (!ParseOBSStableVersion(QString::fromLatin1(OBS_VERSION), currentVersion,
				   &normalizedCurrent)) {
		HandleCheckFailure(QTStr("Updater.Error.CurrentVersion"),
				   QStringLiteral("Invalid OBS_VERSION: %1")
					   .arg(QString::fromLatin1(OBS_VERSION)));
		return;
	}

	ResetAutomaticFailureNotification();
	if (QVersionNumber::compare(parsed.release.version, currentVersion) <= 0) {
		if (manualCheck) {
			OBSMessageBox::information(
				main, QTStr("Updater.NoUpdatesAvailable.Title"),
				QTStr("Updater.NoUpdatesAvailable.Text"));
		}
		Finish();
		return;
	}

	config_t *config = GetGlobalConfig();
	const char *skipped = config_get_string(config, "OBSPTUpdater",
						"SkippedVersion");
	if (!manualCheck && skipped &&
	    parsed.release.versionText == QString::fromUtf8(skipped)) {
		Finish();
		return;
	}

	OBSUpdate dialog(main, parsed.release.notesMarkdown,
			 parsed.release.releaseUrl);
	const int result = dialog.exec();
	if (result == OBSUpdate::Skip) {
		config_set_string(config, "OBSPTUpdater", "SkippedVersion",
				  parsed.release.versionText.toUtf8().constData());
		SaveGlobalConfig();
		Finish();
		return;
	}
	if (result != OBSUpdate::Update) {
		Finish();
		return;
	}

	StartDownload(parsed.release);
}

QString OBSUpdateManager::ParseErrorMessage(OBSUpdateParseError error) const
{
	switch (error) {
	case OBSUpdateParseError::InvalidJson:
		return QTStr("Updater.Error.InvalidResponse");
	case OBSUpdateParseError::InvalidVersion:
		return QTStr("Updater.Error.InvalidVersion");
	case OBSUpdateParseError::UnstableRelease:
		return QTStr("Updater.Error.UnstableRelease");
	case OBSUpdateParseError::InvalidDigest:
		return QTStr("Updater.Error.InvalidDigest");
	case OBSUpdateParseError::MissingInstaller:
	case OBSUpdateParseError::DuplicateInstaller:
	case OBSUpdateParseError::InvalidInstaller:
	case OBSUpdateParseError::InvalidReleaseUrl:
		return QTStr("Updater.Error.InvalidInstaller");
	case OBSUpdateParseError::InvalidRelease:
	case OBSUpdateParseError::None:
	default:
		return QTStr("Updater.Error.InvalidResponse");
	}
}

void OBSUpdateManager::HandleCheckFailure(const QString &message,
					 const QString &detail)
{
	blog(LOG_WARNING, "[OBS-PT Updater] Update check failed: %s",
	     QT_TO_UTF8(detail));

	bool showDialog = manualCheck;
	config_t *config = GetGlobalConfig();
	if (!manualCheck &&
	    !config_get_bool(config, "OBSPTUpdater", "AutoFailureNotified")) {
		config_set_bool(config, "OBSPTUpdater", "AutoFailureNotified", true);
		SaveGlobalConfig();
		showDialog = true;
	}

	if (showDialog)
		OBSMessageBox::warning(main, QTStr("Updater.CheckFailed.Title"),
				       message);
	Finish();
}

void OBSUpdateManager::ResetAutomaticFailureNotification()
{
	config_t *config = GetGlobalConfig();
	if (!config_get_bool(config, "OBSPTUpdater", "AutoFailureNotified"))
		return;
	config_set_bool(config, "OBSPTUpdater", "AutoFailureNotified", false);
	SaveGlobalConfig();
}

void OBSUpdateManager::StartDownload(const OBSUpdateRelease &release)
{
	pendingRelease = release;
	partialPath.clear();
	finalPath.clear();
	const QString desktop = QStandardPaths::writableLocation(
		QStandardPaths::DesktopLocation);
	if (desktop.isEmpty() || !QFileInfo(desktop).isDir()) {
		ShowDownloadFailure(QTStr("Updater.Error.Desktop"), desktop);
		return;
	}

	finalPath = QDir(desktop).filePath(release.installer.name);
	partialPath = finalPath + QStringLiteral(".part");
	if (QFile::exists(partialPath) && !QFile::remove(partialPath)) {
		ShowDownloadFailure(QTStr("Updater.Error.PartialFile"), partialPath);
		return;
	}

	downloadFile = new QFile(partialPath, this);
	if (!downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		ShowDownloadFailure(QTStr("Updater.Error.Write"),
				    downloadFile->errorString());
		return;
	}

	downloadHash->reset();
	bytesWritten = 0;
	downloadCanceled = false;
	downloadWriteFailed = false;
	downloadSizeExceeded = false;
	downloadRedirectRejected = false;
	SetState(State::Downloading);

	progress = new QProgressDialog(
		QTStr("Updater.Download.Progress").arg(release.installer.name),
		QTStr("Cancel"), 0, 100, main);
	progress->setWindowTitle(QTStr("Updater.Download.Title"));
	progress->setWindowModality(Qt::WindowModal);
	progress->setMinimumDuration(0);
	progress->setAutoClose(false);
	progress->setAutoReset(false);
	progress->setValue(0);
	connect(progress, &QProgressDialog::canceled, this,
		&OBSUpdateManager::CancelDownload);
	progress->show();

	QString detail;
	if (!curl->StartDownload(release.installer.downloadUrl, detail))
		ShowDownloadFailure(QTStr("Updater.Error.DownloadNetwork"),
				    detail);
}

void OBSUpdateManager::DownloadProgress(qint64 bytesReceived, qint64)
{
	if (!progress || pendingRelease.installer.size <= 0)
		return;
	const qint64 bounded = qBound<qint64>(
		0, bytesReceived, pendingRelease.installer.size);
	progress->setValue(static_cast<int>(
		static_cast<double>(bounded) * 100.0 /
		pendingRelease.installer.size));
}

void OBSUpdateManager::DownloadFinished(int result, long status,
					const QString &detail,
					const QUrl &effectiveUrl)
{
	blog(LOG_INFO,
	     "[OBS-PT Updater] Download finished: curl=%d HTTP=%ld bytes=%lld",
	     result, status, static_cast<long long>(bytesWritten));
	if (downloadFile && downloadFile->isOpen()) {
		if (!downloadWriteFailed && !downloadFile->flush())
			downloadWriteFailed = true;
		downloadFile->close();
	}
	if (progress) {
		disconnect(progress, &QProgressDialog::canceled, this,
			   &OBSUpdateManager::CancelDownload);
		progress->hide();
		progress->deleteLater();
		progress = nullptr;
	}

	if (downloadCanceled) {
		CleanupDownload(true);
		Finish();
		return;
	}
	if (downloadRedirectRejected ||
	    !IsAllowedGitHubRedirectUrl(effectiveUrl)) {
		ShowDownloadFailure(QTStr("Updater.Error.UnsafeRedirect"),
				    effectiveUrl.toString());
		return;
	}
	if (downloadSizeExceeded) {
		ShowDownloadFailure(
			QTStr("Updater.Error.SizeMismatch"),
			QStringLiteral("Download exceeded the declared %1 bytes")
				.arg(pendingRelease.installer.size));
		return;
	}
	if (downloadWriteFailed) {
		const QString detail = downloadFile ? downloadFile->errorString()
						    : partialPath;
		ShowDownloadFailure(QTStr("Updater.Error.Write"), detail);
		return;
	}
	if (result != CURLE_OK || status != 200) {
		ShowDownloadFailure(QTStr("Updater.Error.DownloadNetwork"),
				    detail);
		return;
	}
	if (bytesWritten != pendingRelease.installer.size) {
		ShowDownloadFailure(
			QTStr("Updater.Error.SizeMismatch"),
			QStringLiteral("Expected %1 bytes, wrote %2")
				.arg(pendingRelease.installer.size)
				.arg(bytesWritten));
		return;
	}
	if (downloadHash->result() != pendingRelease.installer.sha256) {
		ShowDownloadFailure(QTStr("Updater.Error.HashMismatch"),
				    QStringLiteral("SHA-256 mismatch"));
		return;
	}

	QString promoteDetail;
	if (!PromoteDownload(promoteDetail)) {
		ShowDownloadFailure(QTStr("Updater.Error.Promote"), promoteDetail);
		return;
	}

	CleanupDownload(false);
	blog(LOG_INFO, "[OBS-PT Updater] Validated installer saved to: %s",
	     QT_TO_UTF8(finalPath));
	SetState(State::DownloadReady);
	ShowDownloadComplete();
}

void OBSUpdateManager::CancelDownload()
{
	if (state != State::Downloading)
		return;
	blog(LOG_INFO, "[OBS-PT Updater] Download canceled by user");
	downloadCanceled = true;
	// setValue() may process modal events from inside libcurl's callback.
	// Let the progress callback abort before cleaning up the easy handle.
}

void OBSUpdateManager::CleanupDownload(bool removePartial)
{
	if (downloadFile) {
		if (downloadFile->isOpen())
			downloadFile->close();
		downloadFile->deleteLater();
		downloadFile = nullptr;
	}
	if (progress) {
		disconnect(progress, &QProgressDialog::canceled, this,
			   &OBSUpdateManager::CancelDownload);
		progress->hide();
		progress->deleteLater();
		progress = nullptr;
	}
	if (removePartial && !partialPath.isEmpty() && QFile::exists(partialPath) &&
	    !QFile::remove(partialPath)) {
		blog(LOG_WARNING,
		     "[OBS-PT Updater] Failed to remove partial download: %s",
		     QT_TO_UTF8(partialPath));
	}
	partialPath.clear();
}

void OBSUpdateManager::ShowDownloadFailure(const QString &message,
					   const QString &detail)
{
	blog(LOG_WARNING, "[OBS-PT Updater] Download failed: %s",
	     QT_TO_UTF8(detail));
	CleanupDownload(true);
	OBSMessageBox::warning(main, QTStr("Updater.DownloadFailed.Title"),
			       message);
	Finish();
}

bool OBSUpdateManager::PromoteDownload(QString &detail)
{
	const std::wstring source =
		QDir::toNativeSeparators(partialPath).toStdWString();
	const std::wstring destination =
		QDir::toNativeSeparators(finalPath).toStdWString();
	if (MoveFileExW(source.c_str(), destination.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return true;

	detail = QStringLiteral("MoveFileExW failed with error %1")
			 .arg(GetLastError());
	return false;
}

void OBSUpdateManager::ShowDownloadComplete()
{
	QMessageBox box(QMessageBox::Information,
			QTStr("Updater.DownloadComplete.Title"),
			QTStr("Updater.DownloadComplete.Text").arg(finalPath),
			QMessageBox::NoButton, main);
	QPushButton *installButton = box.addButton(
		QTStr("Updater.AutomaticInstall"), QMessageBox::AcceptRole);
	QPushButton *openButton = box.addButton(
		QTStr("Updater.OpenFileLocation"), QMessageBox::ActionRole);
	QPushButton *closeButton =
		box.addButton(QTStr("Close"), QMessageBox::RejectRole);
	box.setDefaultButton(closeButton);

	const bool outputsActive = main->Active();
	installButton->setEnabled(!outputsActive);
	if (outputsActive)
		box.setInformativeText(QTStr("Updater.OutputsActive.Text"));

	box.exec();
	if (box.clickedButton() == openButton) {
		QDesktopServices::openUrl(
			QUrl::fromLocalFile(QFileInfo(finalPath).absolutePath()));
		Finish();
		return;
	}
	if (box.clickedButton() != installButton) {
		Finish();
		return;
	}

	if (main->Active()) {
		OBSMessageBox::warning(main, QTStr("Updater.OutputsActive.Title"),
				       QTStr("Updater.OutputsActive.Text"));
		Finish();
		return;
	}

	QString detail;
	if (!LaunchInstaller(detail)) {
		blog(LOG_WARNING, "[OBS-PT Updater] Installer launch failed: %s",
		     QT_TO_UTF8(detail));
		OBSMessageBox::warning(main, QTStr("Updater.LaunchFailed.Title"),
				       QTStr("Updater.LaunchFailed.Text"));
		Finish();
		return;
	}

	Finish();
	QTimer::singleShot(0, main, [main = main]() {
		if (!main->close()) {
			OBSMessageBox::warning(
				main, QTStr("Updater.ShutdownFailed.Title"),
				QTStr("Updater.ShutdownFailed.Text"));
		}
	});
}

bool OBSUpdateManager::LaunchInstaller(QString &detail)
{
	char installRoot[1024];
	if (GetConfigPath(installRoot, sizeof(installRoot), "") <= 0) {
		detail = QStringLiteral("Could not resolve Install Root");
		return false;
	}

	const QString parameters =
		QStringLiteral("/D=") +
		QDir::toNativeSeparators(QString::fromUtf8(installRoot));
	const std::wstring installer =
		QDir::toNativeSeparators(finalPath).toStdWString();
	const std::wstring arguments = parameters.toStdWString();
	const std::wstring directory =
		QDir::toNativeSeparators(QFileInfo(finalPath).absolutePath())
			.toStdWString();

	SHELLEXECUTEINFOW executeInfo = {};
	executeInfo.cbSize = sizeof(executeInfo);
	executeInfo.lpVerb = L"open";
	executeInfo.lpFile = installer.c_str();
	executeInfo.lpParameters = arguments.c_str();
	executeInfo.lpDirectory = directory.c_str();
	executeInfo.nShow = SW_SHOWNORMAL;
	if (ShellExecuteExW(&executeInfo))
		return true;

	detail = QStringLiteral("ShellExecuteExW failed with error %1")
			 .arg(GetLastError());
	return false;
}
