#include "obspt-toolbar-theme.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QToolBar>
#include <QToolButton>

#include <iostream>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char *what)
{
	if (condition)
		return;
	++failures;
	std::cerr << "FAIL: " << what << "\n";
}

OBSPTRoutingToolbarLabels Labels()
{
	OBSPTRoutingToolbarLabels labels;
	labels.add = QStringLiteral("Add");
	labels.remove = QStringLiteral("Remove");
	labels.properties = QStringLiteral("Properties");
	labels.moveUp = QStringLiteral("Move Up");
	labels.moveDown = QStringLiteral("Move Down");
	labels.multiTrack = QStringLiteral("Audio Mode");
	return labels;
}

bool LoadTheme(const QString &name)
{
	const QString path = QStringLiteral(OBSPT_THEME_DIR) +
			     QStringLiteral("/") + name + QStringLiteral(".qss");
	if (!QFile::exists(path))
		return false;
	qApp->setStyleSheet(QStringLiteral("file:///") + path);
	return true;
}

QString ReadTheme(const QString &name)
{
	QFile file(QStringLiteral(OBSPT_THEME_DIR) + QStringLiteral("/") +
		   name + QStringLiteral(".qss"));
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return QString();
	return QString::fromUtf8(file.readAll());
}

/* Command order must match the native sources toolbar: Add, Remove,
 * Properties, separator, Move Up, Move Down, then the right-aligned
 * audio mode entry. */
void TestCommandOrder()
{
	QToolBar commands;
	QToolBar placeholder;
	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(&commands, &placeholder, Labels());

	const QList<QAction *> laid = commands.actions();
	Check(laid.size() == 6, "command toolbar lays out 6 entries");
	if (laid.size() != 6)
		return;

	Check(laid[0] == actions.add, "slot 0 is Add");
	Check(laid[1] == actions.remove, "slot 1 is Remove");
	Check(laid[2] == actions.properties, "slot 2 is Properties");
	Check(laid[3]->isSeparator(),
	      "slot 3 is the native separator between Properties and Move Up");
	Check(laid[4] == actions.moveUp, "slot 4 is Move Up");
	Check(laid[5] == actions.moveDown, "slot 5 is Move Down");

	const QList<QAction *> right = placeholder.actions();
	Check(right.size() == 1 && right[0] == actions.multiTrack,
	      "the audio mode entry is the sole right-aligned command");
}

/* Mean luminance of an icon's opaque pixels. The Dark theme strokes its
 * toolbar glyphs in #d2d2d2 (~210). */
double IconLuminance(const QIcon &icon)
{
	const QImage image =
		icon.pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off).toImage();
	double sum = 0.0;
	int counted = 0;
	for (int y = 0; y < image.height(); ++y) {
		for (int x = 0; x < image.width(); ++x) {
			const QColor pixel = image.pixelColor(x, y);
			if (pixel.alpha() < 32)
				continue;
			sum += 0.299 * pixel.red() + 0.587 * pixel.green() +
			       0.114 * pixel.blue();
			++counted;
		}
	}
	return counted ? sum / counted : -1.0;
}

/* Every ordinary command takes its icon from the active theme, exactly like
 * the native sources toolbar, instead of baking a resource icon into the
 * QAction. */
void TestThemeSuppliesIcons()
{
	if (!LoadTheme(QStringLiteral("Dark"))) {
		Check(false, "Dark theme is readable from the source tree");
		return;
	}

	QToolBar toolbar;
	QToolBar placeholderBar;
	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(&toolbar, &placeholderBar, Labels());

	const std::vector<std::pair<QAction *, const char *>> commands = {
		{actions.add, "Add"},
		{actions.remove, "Remove"},
		{actions.properties, "Properties"},
		{actions.moveUp, "Move Up"},
		{actions.moveDown, "Move Down"},
	};

	for (const auto &command : commands) {
		auto *button = qobject_cast<QToolButton *>(
			toolbar.widgetForAction(command.first));
		if (!button) {
			Check(false, "command has a generated tool button");
			continue;
		}

		const QIcon icon = button->icon();
		if (icon.isNull()) {
			std::cerr << "  (" << command.second << ")\n";
			Check(false, "theme supplied an icon for the command");
			continue;
		}

		const double luminance = IconLuminance(icon);
		if (luminance < 100.0) {
			std::cerr << "  (" << command.second << " luminance "
				  << luminance << ")\n";
			Check(false,
			      "icon is theme-tinted, not near-black");
		}
	}
}

/* ADR-0003: toolbar commands never enter the disabled state. They stay
 * enabled permanently and decide whether to act when clicked, exactly like
 * the native Visual Composition Panel toolbar. */
void TestCommandsNeverDisabled()
{
	QToolBar toolbar;
	QToolBar placeholderBar;
	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(&toolbar, &placeholderBar, Labels());

	const std::vector<std::pair<QAction *, const char *>> all = {
		{actions.add, "Add"},
		{actions.remove, "Remove"},
		{actions.properties, "Properties"},
		{actions.moveUp, "Move Up"},
		{actions.moveDown, "Move Down"},
		{actions.multiTrack, "Audio Mode"},
	};

	for (const auto &entry : all) {
		if (!entry.first->isEnabled()) {
			std::cerr << "  (" << entry.second << ")\n";
			Check(false, "command is built enabled");
		}
	}

	for (const auto &entry : all) {
		QToolBar *bar = entry.first == actions.multiTrack
					? &placeholderBar
					: &toolbar;
		auto *button = qobject_cast<QToolButton *>(
			bar->widgetForAction(entry.first));
		if (!button) {
			Check(false, "command has a generated tool button");
			continue;
		}
		if (!button->isEnabled()) {
			std::cerr << "  (" << entry.second << ")\n";
			Check(false, "tool button is enabled");
		}
	}
}

/* The workaround stack built for the disabled state is gone: no pointer
 * tracking, no marker property, and no theme rules referencing it. Keeping
 * any of it alive would silently reintroduce the behaviour ADR-0003 rejects. */
void TestNoDisabledStateMachinery()
{
	QToolBar toolbar;
	QToolBar placeholderBar;
	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(&toolbar, &placeholderBar, Labels());

	Check(!toolbar.property("obsptHoverTracked").isValid(),
	      "the toolbar installs no pointer tracking");
	Check(!placeholderBar.property("obsptHoverTracked").isValid(),
	      "the placeholder toolbar installs no pointer tracking");

	for (QAction *action : {actions.add, actions.remove, actions.properties,
				actions.moveUp, actions.moveDown}) {
		auto *button = qobject_cast<QToolButton *>(
			toolbar.widgetForAction(action));
		if (!button)
			continue;
		if (button->property("obsptHovered").isValid()) {
			std::cerr << "  ("
				  << action->objectName().toLatin1().constData()
				  << ")\n";
			Check(false, "no button carries the hover marker");
		}
	}

	for (const QString &theme : {QStringLiteral("Dark"),
				     QStringLiteral("Acri"),
				     QStringLiteral("Rachni"),
				     QStringLiteral("System")}) {
		const QString sheet = ReadTheme(theme);
		if (sheet.isEmpty()) {
			Check(false, "bundled theme is readable");
			continue;
		}
		if (sheet.contains(QStringLiteral("obsptHovered"))) {
			std::cerr << "  (" << theme.toLatin1().constData()
				  << ")\n";
			Check(false,
			      "theme carries no disabled-hover marker rule");
		}
	}
}

/* Slots must not shift or vanish. The native toolbar keeps its icons through
 * the theme, so a button that loses its icon collapses to a text label and
 * pushes its neighbours into the overflow area. */
void TestSlotsStayStable()
{
	if (!LoadTheme(QStringLiteral("Dark"))) {
		Check(false, "Dark theme is readable from the source tree");
		return;
	}

	QToolBar toolbar;
	QToolBar placeholderBar;
	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(&toolbar, &placeholderBar, Labels());
	toolbar.resize(320, 30);
	toolbar.show();
	QApplication::processEvents();

	struct Slot {
		QToolButton *button;
		QRect geometry;
	};
	std::vector<Slot> captured;
	for (QAction *a : {actions.add, actions.remove, actions.properties,
			   actions.moveUp, actions.moveDown}) {
		auto *b = qobject_cast<QToolButton *>(
			toolbar.widgetForAction(a));
		if (!b) {
			Check(false, "every command has a tool button");
			return;
		}
		captured.push_back({b, b->geometry()});
	}

	/* Selection changes must not move anything. Rebuilding the table is
	 * the only thing that happens now that availability no longer touches
	 * widget state. */
	QApplication::processEvents();

	for (const Slot &slot : captured) {
		if (!slot.button->isVisible()) {
			std::cerr << "  ("
				  << slot.button->objectName().toLatin1().constData()
				  << " became invisible)\n";
			Check(false, "a command keeps its slot");
		}
		if (slot.button->geometry() != slot.geometry) {
			std::cerr << "  ("
				  << slot.button->objectName().toLatin1().constData()
				  << " moved)\n";
			Check(false, "a command does not reflow");
		}
		if (slot.button->icon().isNull()) {
			std::cerr << "  ("
				  << slot.button->objectName().toLatin1().constData()
				  << " lost its icon)\n";
			Check(false, "a command keeps its themed icon");
		}
	}
}

/* The Audio Mode entry takes both glyphs from the theme like every other
 * command, and the icon follows the mode. The active track is drawn green,
 * which now marks the mode in effect rather than an unimplemented feature. */
void TestModeEntryIconFollowsMode()
{
	if (!LoadTheme(QStringLiteral("Dark"))) {
		Check(false, "Dark theme is readable from the source tree");
		return;
	}

	QToolBar toolbar;
	QToolBar placeholderBar;
	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(&toolbar, &placeholderBar, Labels());

	Check(actions.multiTrack->property("themeID").toString() ==
		      QString::fromLatin1(kOBSPTAudioModeMultiThemeId),
	      "the mode entry starts on the multi-track theme id");
	Check(actions.multiTrack->icon().isNull(),
	      "no icon is baked into the action; the theme supplies it");

	auto *button = qobject_cast<QToolButton *>(
		placeholderBar.widgetForAction(actions.multiTrack));
	if (!button) {
		Check(false, "the mode entry has a generated tool button");
		return;
	}

	auto greenest = [](const QIcon &icon) {
		const QImage image = icon.pixmap(QSize(16, 16), QIcon::Normal,
						 QIcon::Off).toImage();
		double strongest = -1.0;
		int hue = -1;
		for (int y = 0; y < image.height(); ++y) {
			for (int x = 0; x < image.width(); ++x) {
				const QColor pixel = image.pixelColor(x, y);
				if (pixel.alpha() < 96)
					continue;
				if (pixel.saturationF() > strongest) {
					strongest = pixel.saturationF();
					hue = pixel.hue();
				}
			}
		}
		return hue;
	};

	const QIcon multi = button->icon();
	if (multi.isNull()) {
		Check(false, "the theme supplied a multi-track icon");
		return;
	}
	const int multiHue = greenest(multi);
	const int wantHue = QColor(QStringLiteral("#2e9f58")).hue();
	if (qAbs(multiHue - wantHue) > 12) {
		std::cerr << "  (multi hue " << multiHue << " want " << wantHue
			  << ")\n";
		Check(false, "the multi-track icon carries the green centre line");
	}

	/* Switching the id must yield a different glyph. */
	actions.multiTrack->setProperty("themeID",
					kOBSPTAudioModeSingleThemeId);
	OBSPTApplyToolbarActionTheme(&placeholderBar);
	const QIcon single = button->icon();
	if (single.isNull()) {
		Check(false, "the theme supplied a single-track icon");
		return;
	}
	const int singleHue = greenest(single);
	if (qAbs(singleHue - wantHue) > 12) {
		std::cerr << "  (single hue " << singleHue << " want " << wantHue
			  << ")\n";
		Check(false, "the single-track icon is green");
	}

	const QImage a = multi.pixmap(QSize(16, 16)).toImage();
	const QImage b = single.pixmap(QSize(16, 16)).toImage();
	Check(a != b, "the two modes render different glyphs");
}

/* The tinted settings glyph used while the feature was a placeholder must be
 * gone: nothing may reference it, or the old green cog would come back. */
void TestPlaceholderGlyphRetired()
{
	for (const QString &theme : {QStringLiteral("Dark"),
				     QStringLiteral("Acri"),
				     QStringLiteral("Rachni"),
				     QStringLiteral("System")}) {
		const QString sheet = ReadTheme(theme);
		if (sheet.isEmpty()) {
			Check(false, "bundled theme is readable");
			continue;
		}
		if (!sheet.contains(QStringLiteral("audioModeMultiIconSmall")) ||
		    !sheet.contains(QStringLiteral("audioModeSingleIconSmall"))) {
			std::cerr << "  (" << theme.toLatin1().constData() << ")\n";
			Check(false, "theme declares both audio mode icons");
		}
	}
}

} // namespace

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);

	TestCommandOrder();
	TestThemeSuppliesIcons();
	TestCommandsNeverDisabled();
	TestNoDisabledStateMachinery();
	TestSlotsStayStable();
	TestModeEntryIconFollowsMode();
	TestPlaceholderGlyphRetired();

	if (failures == 0) {
		std::cout << "All OBS-PT audio routing toolbar tests passed\n";
		return 0;
	}
	std::cerr << failures << " audio routing toolbar assertion(s) failed\n";
	return 1;
}
