#pragma once

#include <QWidget>

class OBSBasic;
class QAction;
class QPoint;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
struct OBSPTAudioRoutingDropRequest;

class OBSPTAudioRoutingDock : public QWidget {
public:
	explicit OBSPTAudioRoutingDock(OBSBasic *main, QWidget *parent = nullptr);

	void Refresh();
	int SelectedCreationTrack() const;

private:
	OBSBasic *main;
	QVBoxLayout *layout = nullptr;
	QTreeWidget *table = nullptr;
	QToolBar *toolbar = nullptr;
	QToolBar *placeholderToolbar = nullptr;
	QAction *addAction = nullptr;
	QAction *removeAction = nullptr;
	QAction *propertiesAction = nullptr;
	QAction *moveUpAction = nullptr;
	QAction *moveDownAction = nullptr;
	QAction *multiTrackAction = nullptr;
	QString selectedSourceId;
	bool refreshing = false;

	void BuildTable();
	void UpdateAudioModeEntry();
	void ToggleAudioMode();
	bool RefuseWhenSingleTrack();
	void OpenSelectedProperties();
	void RemoveSelectedSource();
	void MoveSelectedSource(int direction);
	void RenameSelectedCell();
	void RenameSource(const QString &sourceId);
	void ShowSourceMenu(const QPoint &position);
	void HandleDrop(const OBSPTAudioRoutingDropRequest &request);
	QTreeWidgetItem *SelectedSourceItem() const;
};
