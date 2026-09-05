#include "obspt-routing-menu-style.hpp"

#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QStyleOptionMenuItem>

#include <iostream>

class RecordingStyle : public QProxyStyle {
public:
	mutable QColor text;
	mutable QColor highlightedText;

	void drawControl(ControlElement element, const QStyleOption *option,
			 QPainter *, const QWidget *) const override
	{
		if (element != CE_MenuItem)
			return;
		text = option->palette.color(QPalette::Text);
		highlightedText =
			option->palette.color(QPalette::HighlightedText);
	}
};

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	QStyle *sharedStyle = QApplication::style();
	int triggered = 0;
	{
		QMenu menu;
		InstallOBSPTRoutingMenuStyle(&menu, QStringLiteral("Remove"));
		QAction *remove = menu.addAction(QStringLiteral("Remove"));
		QObject::connect(remove, &QAction::triggered,
				 [&triggered] { ++triggered; });
		remove->trigger();
		menu.ensurePolished();
	}

	QMenuBar menuBar;
	menuBar.addMenu(QStringLiteral("File"));
	menuBar.ensurePolished();
	QPixmap rendered(menuBar.sizeHint());
	menuBar.render(&rendered);
	QApplication::processEvents();

	auto *recordingStyle = new RecordingStyle;
	OBSPTRoutingMenuStyle dangerStyle(recordingStyle,
					   QStringLiteral("Remove"));
	QStyleOptionMenuItem option;
	option.text = QStringLiteral("Remove");
	dangerStyle.drawControl(QStyle::CE_MenuItem, &option, nullptr, nullptr);
	auto isRed = [](const QColor &color) {
		return color.red() > color.green() + 40 &&
		       color.red() > color.blue() + 40;
	};

	const bool passed = triggered == 1 && QApplication::style() == sharedStyle &&
			    !rendered.isNull() &&
			    isRed(recordingStyle->text) &&
			    isRed(recordingStyle->highlightedText);
	if (passed)
		std::cout << "All OBS-PT routing menu style tests passed\n";
	else
		std::cerr << "FAIL: temporary routing menu damaged shared Qt style\n";
	return passed ? 0 : 1;
}
