#include "window-basic-about.hpp"
#include "window-basic-main.hpp"
#include "window-basic-welcome.hpp"
#include "obs-app.hpp"
#include "qt-wrappers.hpp"
#include "remote-text.hpp"
#include <QTextCursor>
#include <util/util.hpp>
#include <util/platform.h>
#include <platform.hpp>
#include <json11.hpp>

using namespace json11;

static QString OBSPTPresentHeader()
{
	const QString locale = QString::fromUtf8(App()->GetLocale());
	const char *url = locale.startsWith("zh", Qt::CaseInsensitive)
			  ? "https://space.bilibili.com/1049515077"
			  : "https://github.com/Sakyvo";

	return QStringLiteral(
		       "<p style='margin:0;'>"
		       "<a href='%1' style='text-decoration:none;'>"
		       "<span style='color:#000080;font-size:18px;font-weight:600;'>Sakyvo</span> "
		       "<span style='color:#8B0000;font-size:18px;font-weight:600;'>Present</span>"
		       "</a></p>"
		       "<hr style='color:#666666;margin-top:12px;margin-bottom:20px;' />")
		.arg(QString::fromUtf8(url));
}

static void SetAuthorsText(QTextBrowser *browser, const QString &text)
{
	browser->setPlainText(text);
	QTextCursor cursor(browser->document());
	cursor.movePosition(QTextCursor::Start);
	cursor.insertHtml(OBSPTPresentHeader());
	cursor.insertBlock();
}

OBSAbout::OBSAbout(QWidget *parent) : QDialog(parent), ui(new Ui::OBSAbout)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->setupUi(this);

	QString bitness;
	QString ver;

	if (sizeof(void *) == 4)
		bitness = " (32 bit)";
	else if (sizeof(void *) == 8)
		bitness = " (64 bit)";

#ifdef HAVE_OBSCONFIG_H
	ver += OBS_VERSION;
#else
	ver += LIBOBS_API_MAJOR_VER + "." + LIBOBS_API_MINOR_VER + "." +
	       LIBOBS_API_PATCH_VER;
#endif

	ui->version->setText(ver + bitness);

	ui->contribute->setText(QTStr("About.Contribute"));
	ui->donate->setText(
		"&nbsp;&nbsp;<a href='https://github.com/Sakyvo/OBS-PT/issues'>" +
		QTStr("About.Donate") + "</a>");
	ui->donate->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui->donate->setOpenExternalLinks(true);

	ui->getInvolved->setText(
		"&nbsp;&nbsp;<a href='https://github.com/Sakyvo/OBS-PT/pulls'>" +
		QTStr("About.GetInvolved") + "</a>");
	ui->getInvolved->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui->getInvolved->setOpenExternalLinks(true);

	ui->about->setText("<a href='#'>" + QTStr("About") + "</a>");
	ui->authors->setText("<a href='#'>" + QTStr("About.Authors") + "</a>");
	ui->license->setText("<a href='#'>" + QTStr("About.License") + "</a>");

	ui->name->setProperty("themeID", "aboutName");
	ui->version->setProperty("themeID", "aboutVersion");
	ui->about->setProperty("themeID", "aboutHLayout");
	ui->authors->setProperty("themeID", "aboutHLayout");
	ui->license->setProperty("themeID", "aboutHLayout");
	ui->info->setProperty("themeID", "aboutInfo");

	connect(ui->about, &ClickableLabel::clicked, this, [this]() {
		OBSWelcome *w = new OBSWelcome(OBSBasic::Get());
		w->setAttribute(Qt::WA_DeleteOnClose, true);
		w->show();
		close();
	});
	connect(ui->authors, SIGNAL(clicked()), this, SLOT(ShowAuthors()));
	connect(ui->license, SIGNAL(clicked()), this, SLOT(ShowLicense()));

	QPointer<OBSAbout> about(this);

	OBSBasic *main = OBSBasic::Get();
	if (main->patronJson.empty() && !main->patronJsonThread) {
		RemoteTextThread *thread = new RemoteTextThread(
			"https://obsproject.com/patreon/about-box.json",
			"application/json");
		QObject::connect(thread, &RemoteTextThread::Result, main,
				 &OBSBasic::UpdatePatronJson);
		QObject::connect(
			thread,
			SIGNAL(Result(const QString &, const QString &)), this,
			SLOT(ShowAbout()));
		main->patronJsonThread.reset(thread);
		thread->start();
	} else {
		ShowAbout();
	}
}

void OBSAbout::ShowAbout()
{
	OBSBasic *main = OBSBasic::Get();

	QString text = OBSPTPresentHeader();
	if (main->patronJson.empty()) {
		ui->textBrowser->setHtml(text);
		return;
	}

	std::string error;
	Json json = Json::parse(main->patronJson, error);
	const Json::array &patrons = json.array_items();

	text += "<h1>Top Patreon contributors:</h1>";
	text += "<p style=\"font-size:16px;\">";
	bool first = true;
	bool top = true;

	for (const Json &patron : patrons) {
		std::string name = patron["name"].string_value();
		std::string link = patron["link"].string_value();
		int amount = patron["amount"].int_value();

		if (top && amount < 10000) {
			text += "</p>";
			top = false;
		} else if (!first) {
			text += "<br/>";
		}

		if (!link.empty()) {
			text += "<a href=\"";
			text += QT_UTF8(link.c_str()).toHtmlEscaped();
			text += "\">";
		}
		text += QT_UTF8(name.c_str()).toHtmlEscaped();
		if (!link.empty())
			text += "</a>";

		if (first)
			first = false;
	}

	ui->textBrowser->setHtml(text);
}

void OBSAbout::ShowAuthors()
{
	std::string path;
	QString error = "Error! File could not be read.\n\n \
		Go to: https://github.com/obsproject/obs-studio/blob/master/AUTHORS";

	if (!GetDataFilePath("authors/AUTHORS", path)) {
		SetAuthorsText(ui->textBrowser, error);
		return;
	}

	SetAuthorsText(ui->textBrowser, QString::fromStdString(path));

	BPtr<char> text = os_quick_read_utf8_file(path.c_str());

	if (!text || !*text) {
		SetAuthorsText(ui->textBrowser, error);
		return;
	}

	SetAuthorsText(ui->textBrowser, QT_UTF8(text));
}

void OBSAbout::ShowLicense()
{
	std::string path;
	QString error = "Error! File could not be read.\n\n \
		Go to: https://github.com/obsproject/obs-studio/blob/master/COPYING";

	if (!GetDataFilePath("license/gplv2.txt", path)) {
		ui->textBrowser->setPlainText(error);
		return;
	}

	BPtr<char> text = os_quick_read_utf8_file(path.c_str());

	if (!text || !*text) {
		ui->textBrowser->setPlainText(error);
		return;
	}

	ui->textBrowser->setPlainText(QT_UTF8(text));
}
