#pragma once

#include <QString>

class QAction;
class QColor;
class QIcon;
class QPixmap;
class QToolBar;

struct OBSPTRoutingToolbarActions {
	QAction *add = nullptr;
	QAction *remove = nullptr;
	QAction *properties = nullptr;
	QAction *moveUp = nullptr;
	QAction *moveDown = nullptr;
	QAction *multiTrack = nullptr;
};

struct OBSPTRoutingToolbarLabels {
	QString add;
	QString remove;
	QString properties;
	QString moveUp;
	QString moveDown;
	QString multiTrack;
};

/* Copy every QAction dynamic property onto the widget the toolbar generated
 * for it, then repolish so themeID selectors such as qproperty-icon apply.
 * Shared by the native scenes/sources toolbars and the Audio and Routing
 * toolbar. */
void OBSPTApplyToolbarActionTheme(QToolBar *toolbar);

/* Build the Audio and Routing toolbar with the native sources toolbar
 * contract: theme id only and no baked icon, so the active theme supplies
 * every icon. The native layout keeps its spacer outside the toolbar, so the
 * right-aligned placeholder lives in its own toolbar and the caller puts the
 * stretch between them. */
OBSPTRoutingToolbarActions
OBSPTBuildRoutingToolbar(QToolBar *commands, QToolBar *placeholder,
			 const OBSPTRoutingToolbarLabels &labels);

/* Theme ids for the Audio Mode entry. Its icon marks the mode in effect, with
 * the active track drawn green; the caller swaps the id when the mode
 * changes and repolishes so the theme reloads the glyph. */
extern const char *const kOBSPTAudioModeMultiThemeId;
extern const char *const kOBSPTAudioModeSingleThemeId;
