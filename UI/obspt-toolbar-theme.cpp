#include "obspt-toolbar-theme.hpp"

#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QSizePolicy>
#include <QStyle>
#include <QToolBar>
#include <QWidget>

const char *const kOBSPTAudioModeMultiThemeId = "audioModeMultiIconSmall";
const char *const kOBSPTAudioModeSingleThemeId = "audioModeSingleIconSmall";

namespace {

void Repolish(QWidget *widget)
{
	if (!widget)
		return;
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
	widget->update();
}

QAction *CreateToolAction(QToolBar *toolbar, const QString &text,
			  const char *themeId, const char *objectName)
{
	QAction *action = new QAction(text, toolbar);
	action->setToolTip(text);
	action->setObjectName(QString::fromLatin1(objectName));
	if (themeId && *themeId)
		action->setProperty("themeID", themeId);
	return action;
}

} // namespace

void OBSPTApplyToolbarActionTheme(QToolBar *toolbar)
{
	if (!toolbar)
		return;

	for (QAction *action : toolbar->actions()) {
		QWidget *widget = toolbar->widgetForAction(action);
		if (!widget)
			continue;

		for (const QByteArray &name : action->dynamicPropertyNames())
			widget->setProperty(name, action->property(name));

		if (!action->objectName().isEmpty())
			widget->setObjectName(action->objectName());
		if (!action->text().isEmpty())
			widget->setAccessibleName(action->text());

		/* The dock is built after the theme is loaded, so the button
		 * has already been polished once without themeID. Repolish to
		 * let the theme's qproperty-icon rules take effect. */
		Repolish(widget);
	}
}

OBSPTRoutingToolbarActions
OBSPTBuildRoutingToolbar(QToolBar *commands, QToolBar *placeholder,
			 const OBSPTRoutingToolbarLabels &labels)
{
	OBSPTRoutingToolbarActions actions;
	if (!commands || !placeholder)
		return actions;

	auto configure = [](QToolBar *bar, const QString &name) {
		bar->setObjectName(name);
		bar->setIconSize(QSize(16, 16));
		bar->setFloatable(false);
		bar->setMovable(false);
	};
	configure(commands, QStringLiteral("audioRoutingToolbar"));
	configure(placeholder,
		  QStringLiteral("audioRoutingPlaceholderToolbar"));

	actions.add = CreateToolAction(commands, labels.add, "addIconSmall",
				       "OBSBasic.audioRoutingAdd");
	actions.remove = CreateToolAction(commands, labels.remove,
					  "removeIconSmall",
					  "OBSBasic.audioRoutingRemove");
	actions.properties = CreateToolAction(
		commands, labels.properties, "propertiesIconSmall",
		"OBSBasic.audioRoutingProperties");
	actions.moveUp = CreateToolAction(commands, labels.moveUp,
					  "upArrowIconSmall",
					  "OBSBasic.audioRoutingMoveUp");
	actions.moveDown = CreateToolAction(commands, labels.moveDown,
					    "downArrowIconSmall",
					    "OBSBasic.audioRoutingMoveDown");

	commands->addAction(actions.add);
	commands->addAction(actions.remove);
	commands->addAction(actions.properties);
	commands->addSeparator();
	commands->addAction(actions.moveUp);
	commands->addAction(actions.moveDown);

	/* Per ADR-0003 no toolbar command is ever disabled, including this
	 * one: availability is decided when the command runs, not by widget
	 * state. The theme supplies its icon through the theme id, exactly
	 * like the ordinary commands; the caller swaps the id when the mode
	 * changes. */
	actions.multiTrack = CreateToolAction(
		placeholder, labels.multiTrack,
		kOBSPTAudioModeMultiThemeId,
		"OBSBasic.audioRoutingModeEntry");
	placeholder->addAction(actions.multiTrack);

	OBSPTApplyToolbarActionTheme(commands);
	OBSPTApplyToolbarActionTheme(placeholder);
	return actions;
}
