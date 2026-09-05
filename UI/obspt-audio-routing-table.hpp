#pragma once

#include <QTreeWidget>

#include <functional>

class QDropEvent;
class QKeyEvent;

struct OBSPTAudioRoutingDropRequest {
	QString sourceId;
	int sourceTrack = 0;
	QString targetSourceId;
	int targetTrack = 0;
	int indicator = 0;
};

class OBSPTAudioRoutingTable : public QTreeWidget {
public:
	static constexpr int SourceIdRole = Qt::UserRole;
	static constexpr int TrackRole = Qt::UserRole + 1;
	static constexpr int TrackAnchorRole = Qt::UserRole + 2;
	static constexpr int GroupStartRole = Qt::UserRole + 3;
	static constexpr int GroupEndRole = Qt::UserRole + 4;
	using DropHandler =
		std::function<void(const OBSPTAudioRoutingDropRequest &)>;
	using MenuHandler = std::function<void(const QPoint &)>;
	using DeleteHandler = std::function<void()>;
	using ReorderHandler = std::function<void(int)>;
	using RenameHandler = std::function<void()>;

	explicit OBSPTAudioRoutingTable(QWidget *parent = nullptr);

	/* Rect the active style will outline when a cell has focus. The
	 * Visual Composition Panel outlines the whole cell, so this must span
	 * the cell rather than hug the text. */
	QRect FocusOutlineRect(const QModelIndex &index) const;

	DropHandler dropHandler;
	MenuHandler menuHandler;
	DeleteHandler deleteHandler;
	ReorderHandler reorderHandler;
	RenameHandler renameHandler;

protected:
	void startDrag(Qt::DropActions supportedActions) override;
	void keyPressEvent(QKeyEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	virtual void RunNativeDrag(Qt::DropActions supportedActions);

private:
	QTreeWidgetItem *draggedItem = nullptr;
	bool nativeDragActive = false;
	bool hasPendingDrop = false;
	OBSPTAudioRoutingDropRequest pendingDrop;
	void DispatchDrop(const OBSPTAudioRoutingDropRequest &request);
	bool CurrentCellIsSource() const;
};
