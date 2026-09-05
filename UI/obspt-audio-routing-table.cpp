#include "obspt-audio-routing-table.hpp"

#include <QDropEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>

namespace {

/* Draws the cell separators and group rules the routing table needs, and
 * makes the selection highlight span the whole cell rather than hugging the
 * text - matching the Visual Composition Panel. */
class RoutingTableDelegate : public QStyledItemDelegate {
public:
	explicit RoutingTableDelegate(QObject *parent)
		: QStyledItemDelegate(parent)
	{
	}

	void initStyleOption(QStyleOptionViewItem *option,
			     const QModelIndex &index) const override
	{
		QStyledItemDelegate::initStyleOption(option, index);
		option->showDecorationSelected = true;
	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
		   const QModelIndex &index) const override
	{
		QStyledItemDelegate::paint(painter, option, index);
		painter->save();
		QPen pen(option.palette.color(QPalette::Mid));
		pen.setWidth(1);
		painter->setPen(pen);
		if (index.column() < 2)
			painter->drawLine(option.rect.topRight(),
					  option.rect.bottomRight());
		if (index.data(OBSPTAudioRoutingTable::GroupStartRole).toBool())
			painter->drawLine(option.rect.topLeft(),
					  option.rect.topRight());
		if (index.data(OBSPTAudioRoutingTable::GroupEndRole).toBool())
			painter->drawLine(option.rect.bottomLeft(),
					  option.rect.bottomRight());
		painter->restore();
	}
};

} // namespace

OBSPTAudioRoutingTable::OBSPTAudioRoutingTable(QWidget *parent)
	: QTreeWidget(parent)
{
	setDragDropMode(QAbstractItemView::DragDrop);
	setDefaultDropAction(Qt::MoveAction);
	setDropIndicatorShown(true);
	setItemDelegate(new RoutingTableDelegate(this));
}

QRect OBSPTAudioRoutingTable::FocusOutlineRect(const QModelIndex &index) const
{
	QStyleOptionViewItem option;
	option.initFrom(this);
	option.rect = visualRect(index);
	option.state |= QStyle::State_HasFocus | QStyle::State_Selected;
	if (auto *delegate =
		    dynamic_cast<RoutingTableDelegate *>(itemDelegate(index)))
		delegate->initStyleOption(&option, index);
	return style()->subElementRect(QStyle::SE_ItemViewItemFocusRect,
				       &option, this);
}

void OBSPTAudioRoutingTable::startDrag(Qt::DropActions supportedActions)
{
	if (!CurrentCellIsSource())
		return;
	draggedItem = currentItem();
	nativeDragActive = true;
	RunNativeDrag(supportedActions);
	nativeDragActive = false;
	draggedItem = nullptr;
	if (hasPendingDrop) {
		const OBSPTAudioRoutingDropRequest request = pendingDrop;
		hasPendingDrop = false;
		DispatchDrop(request);
	}
}

bool OBSPTAudioRoutingTable::CurrentCellIsSource() const
{
	return currentItem() && currentColumn() == 2 &&
	       !currentItem()->data(0, SourceIdRole).toString().isEmpty();
}

void OBSPTAudioRoutingTable::RunNativeDrag(
	Qt::DropActions supportedActions)
{
	QTreeWidget::startDrag(supportedActions);
}

void OBSPTAudioRoutingTable::DispatchDrop(
	const OBSPTAudioRoutingDropRequest &request)
{
	if (dropHandler)
		dropHandler(request);
}

void OBSPTAudioRoutingTable::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_F2 && renameHandler && currentItem() &&
	    (currentColumn() == 1 || CurrentCellIsSource())) {
		renameHandler();
		event->accept();
		return;
	}
	const bool reorder = event->modifiers() == Qt::ControlModifier &&
		(event->key() == Qt::Key_Up || event->key() == Qt::Key_Down);
	if (reorder && reorderHandler && CurrentCellIsSource()) {
		reorderHandler(event->key() == Qt::Key_Up ? -1 : 1);
		event->accept();
		return;
	}
	const bool menuKey = event->key() == Qt::Key_Menu;
	const bool shiftF10 = event->key() == Qt::Key_F10 &&
			      event->modifiers().testFlag(Qt::ShiftModifier);
	if ((menuKey || shiftF10) && menuHandler && CurrentCellIsSource()) {
		QPoint position = visualItemRect(currentItem()).center();
		position.setX(columnViewportPosition(2) + columnWidth(2) / 2);
		menuHandler(position);
		event->accept();
		return;
	}
	if (event->key() == Qt::Key_Delete && deleteHandler &&
	    CurrentCellIsSource()) {
		deleteHandler();
		event->accept();
		return;
	}
	QTreeWidget::keyPressEvent(event);
}

void OBSPTAudioRoutingTable::dropEvent(QDropEvent *event)
{
	QTreeWidgetItem *source = draggedItem ? draggedItem : currentItem();
	QTreeWidgetItem *target = itemAt(event->pos());
	const DropIndicatorPosition position = dropIndicatorPosition();
	const int relativePosition =
		position == BelowItem ? 1 : position == AboveItem ? -1 : 0;
	if (dropHandler && source && target) {
		OBSPTAudioRoutingDropRequest request;
		request.sourceId =
			source->data(0, SourceIdRole).toString();
		request.sourceTrack = source->data(0, TrackRole).toInt();
		request.targetSourceId =
			target->data(0, SourceIdRole).toString();
		request.targetTrack = target->data(0, TrackRole).toInt();
		request.indicator = relativePosition;
		if (nativeDragActive) {
			pendingDrop = request;
			hasPendingDrop = true;
		} else {
			QTimer::singleShot(
				0, this,
				[this, request] { DispatchDrop(request); });
		}
		event->accept();
		return;
	}
	event->ignore();
}
