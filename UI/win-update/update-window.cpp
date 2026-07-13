#include "update-window.hpp"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"

#include <QDesktopServices>
#include <QTextDocument>

#include <util/base.h>

OBSUpdate::OBSUpdate(QWidget *parent, const QString &text,
		     const QUrl &releaseUrl)
	: QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint |
				  Qt::WindowCloseButtonHint),
	  ui(new Ui_OBSUpdate)
{
	ui->setupUi(this);
	ui->text->setOpenLinks(false);
	ui->text->setOpenExternalLinks(false);
	ui->text->document()->setBaseUrl(releaseUrl);
	if (text.isEmpty()) {
		ui->text->setPlainText(QTStr("Updater.NoReleaseNotes"));
	} else {
		QTextDocument::MarkdownFeatures features(
			QTextDocument::MarkdownDialectGitHub);
		features |= QTextDocument::MarkdownNoHTML;
		ui->text->document()->setMarkdown(text, features);
	}
	connect(ui->text, &QTextBrowser::anchorClicked, this,
		[releaseUrl](const QUrl &link) {
			const QUrl resolved = releaseUrl.resolved(link);
			const QString scheme = resolved.scheme().toLower();
			if (scheme == QStringLiteral("http") ||
			    scheme == QStringLiteral("https")) {
				QDesktopServices::openUrl(resolved);
			} else {
				blog(LOG_WARNING,
				     "[OBS-PT Updater] Blocked release-note URL: %s",
				     QT_TO_UTF8(resolved.toString()));
			}
		});
}

OBSUpdate::OBSUpdate(QWidget *parent, bool, const QString &text)
	: OBSUpdate(parent, text, QUrl())
{
}

void OBSUpdate::on_yes_clicked()
{
	done(OBSUpdate::Update);
}

void OBSUpdate::on_no_clicked()
{
	done(OBSUpdate::Ignore);
}

void OBSUpdate::on_skip_clicked()
{
	done(OBSUpdate::Skip);
}

void OBSUpdate::accept()
{
	done(OBSUpdate::Update);
}

void OBSUpdate::reject()
{
	done(OBSUpdate::Ignore);
}
