#include "window-basic-welcome.hpp"
#include "obs-app.hpp"
#include "platform.hpp"

#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <string>

OBSWelcome::OBSWelcome(QWidget *parent, bool softwareEncoder)
	: QDialog(parent), ui(new Ui::OBSWelcome)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui->setupUi(this);

	setWindowTitle(QTStr("OBSPT.Welcome.Title"));
	ui->title->setText(QTStr("OBSPT.Welcome.Title"));
	ui->title->setProperty("themeID", "aboutName");
	ui->title->setContentsMargins(0, 0, 0, 12);

	QString titleFamily;
	std::string fontPath;
	if (GetDataFilePath("fonts/minecraft.ttf", fontPath)) {
		int id = QFontDatabase::addApplicationFont(
			QString::fromStdString(fontPath));
		if (id != -1) {
			QStringList fams =
				QFontDatabase::applicationFontFamilies(id);
			if (!fams.isEmpty())
				titleFamily = fams.at(0);
		}
	}
	if (!titleFamily.isEmpty())
		ui->title->setStyleSheet(
			QString("font-family:'%1'; font-size:40px;")
				.arg(titleFamily));

	QLabel *pages[] = {ui->page1, ui->page2, ui->page3, ui->page4,
			   ui->page5};
	const char *keys[] = {"OBSPT.Welcome.Page1", "OBSPT.Welcome.Page2",
			      "OBSPT.Welcome.Page3", "OBSPT.Welcome.Page4",
			      "OBSPT.Welcome.Page5"};
	for (int i = 0; i < 5; i++) {
		QString text = QTStr(keys[i]);
		if (i == 1 && softwareEncoder)
			text += "<br><br>" +
				QTStr("OBSPT.FirstRun.SoftwareEncoderWarning");
		pages[i]->setText(text);
		pages[i]->setTextInteractionFlags(Qt::TextBrowserInteraction);
		pages[i]->setOpenExternalLinks(true);
	}

	ui->prevBtn->setText(QTStr("OBSPT.Welcome.Prev"));
	ui->nextBtn->setText(QTStr("OBSPT.Welcome.Next"));

	int btnW = qMax(ui->prevBtn->sizeHint().width(),
			ui->nextBtn->sizeHint().width());
	ui->prevBtn->setFixedWidth(btnW);
	ui->nextBtn->setFixedWidth(btnW);

	connect(ui->prevBtn, &QPushButton::clicked, this, &OBSWelcome::prev);
	connect(ui->nextBtn, &QPushButton::clicked, this, &OBSWelcome::next);

	ui->pages->setCurrentIndex(0);
	updateNav();
}

void OBSWelcome::updateNav()
{
	int i = ui->pages->currentIndex();
	int last = ui->pages->count() - 1;

	ui->prevBtn->setVisible(i > 0);

	if (i >= last) {
		ui->nextBtn->setText(QTStr("OBSPT.Welcome.End"));
		ui->nextBtn->setEnabled(false);
	} else {
		ui->nextBtn->setText(QTStr("OBSPT.Welcome.Next"));
		ui->nextBtn->setEnabled(true);
	}
}

void OBSWelcome::prev()
{
	int i = ui->pages->currentIndex();
	if (i > 0)
		ui->pages->setCurrentIndex(i - 1);
	updateNav();
}

void OBSWelcome::next()
{
	int i = ui->pages->currentIndex();
	if (i < ui->pages->count() - 1)
		ui->pages->setCurrentIndex(i + 1);
	updateNav();
}
