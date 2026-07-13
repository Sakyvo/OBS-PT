#pragma once

#include "obspt-update-model.hpp"

#include <QByteArray>
#include <QObject>
#include <QPointer>

class QFile;
class QProgressDialog;
class QCryptographicHash;
class OBSBasic;
struct OBSUpdateCurlState;

class OBSUpdateManager : public QObject {
	Q_OBJECT

public:
	explicit OBSUpdateManager(OBSBasic *main);
	~OBSUpdateManager() override;

	void Check(bool manual);
	bool IsBusy() const;

signals:
	void BusyChanged(bool busy);

private:
	friend struct OBSUpdateCurlState;

	enum class State { Idle, Checking, Downloading, DownloadReady };

	void SetState(State nextState);
	void Finish();
	void StartCheckRequest();
	void CheckFinished(int result, long status, const QString &detail);
	void HandleCheckFailure(const QString &message, const QString &detail);
	void ResetAutomaticFailureNotification();
	QString ParseErrorMessage(OBSUpdateParseError error) const;

	void StartDownload(const OBSUpdateRelease &release);
	void DownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void DownloadFinished(int result, long status, const QString &detail,
			      const QUrl &effectiveUrl);
	void CancelDownload();
	void CleanupDownload(bool removePartial);
	void ShowDownloadFailure(const QString &message, const QString &detail);
	bool PromoteDownload(QString &detail);
	void ShowDownloadComplete();
	bool LaunchInstaller(QString &detail);

	OBSBasic *main = nullptr;
	OBSUpdateCurlState *curl = nullptr;
	QFile *downloadFile = nullptr;
	QPointer<QProgressDialog> progress;
	QCryptographicHash *downloadHash = nullptr;
	State state = State::Idle;
	bool manualCheck = false;
	bool responseTooLarge = false;
	bool downloadCanceled = false;
	bool downloadWriteFailed = false;
	bool downloadSizeExceeded = false;
	bool downloadRedirectRejected = false;
	qint64 bytesWritten = 0;
	QByteArray responseData;
	OBSUpdateRelease pendingRelease;
	QString partialPath;
	QString finalPath;
};
