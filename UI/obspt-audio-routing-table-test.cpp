#include "obspt-audio-routing-table.hpp"

#include <QApplication>
#include <QDropEvent>
#include <QHeaderView>
#include <QImage>
#include <QMimeData>
#include <QPixmap>

#include <iostream>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (condition)
		return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}

class TestRoutingTable : public OBSPTAudioRoutingTable {
public:
	using OBSPTAudioRoutingTable::dropEvent;
	using OBSPTAudioRoutingTable::keyPressEvent;
	using OBSPTAudioRoutingTable::startDrag;
	std::function<void()> nativeDrag;

protected:
	void RunNativeDrag(Qt::DropActions) override
	{
		if (nativeDrag)
			nativeDrag();
	}
};

void TestKeyboardReorderCommands()
{
	TestRoutingTable table;
	table.setColumnCount(3);
	auto *source = new QTreeWidgetItem(&table, {QStringLiteral("source")});
	source->setData(0, OBSPTAudioRoutingTable::SourceIdRole,
			QStringLiteral("source-id"));
	table.setCurrentItem(source, 2);
	int direction = 0;
	int calls = 0;
	table.reorderHandler = [&](int requested) {
		direction = requested;
		++calls;
	};

	QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::ControlModifier);
	table.keyPressEvent(&up);
	Check(up.isAccepted() && calls == 1 && direction == -1,
	      "Ctrl+Up invokes the shared move-up command");

	QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::ControlModifier);
	table.keyPressEvent(&down);
	Check(down.isAccepted() && calls == 2 && direction == 1,
	      "Ctrl+Down invokes the shared move-down command");
}

void TestF2UsesCurrentCellRenameCommand()
{
	TestRoutingTable table;
	table.setColumnCount(3);
	auto *track = new QTreeWidgetItem(&table);
	track->setData(0, OBSPTAudioRoutingTable::TrackRole, 2);
	auto *source = new QTreeWidgetItem(&table);
	source->setData(0, OBSPTAudioRoutingTable::SourceIdRole,
			QStringLiteral("source-id"));

	int calls = 0;
	table.renameHandler = [&] { ++calls; };
	QKeyEvent f2(QEvent::KeyPress, Qt::Key_F2, Qt::NoModifier);

	table.setCurrentItem(track, 0);
	table.keyPressEvent(&f2);
	Check(calls == 0, "F2 does not rename the track-number cell");

	table.setCurrentItem(track, 1);
	table.keyPressEvent(&f2);
	Check(calls == 1, "F2 renames the selected track-name cell");

	table.setCurrentItem(source, 2);
	table.keyPressEvent(&f2);
	Check(calls == 2, "F2 renames the selected source cell");
}

void TestOnlySourceCellStartsDrag()
{
	TestRoutingTable table;
	table.setColumnCount(3);
	auto *source = new QTreeWidgetItem(&table);
	source->setData(0, OBSPTAudioRoutingTable::SourceIdRole,
			QStringLiteral("source-id"));
	int nativeDrags = 0;
	table.nativeDrag = [&] { ++nativeDrags; };

	table.setCurrentItem(source, 0);
	table.startDrag(Qt::MoveAction);
	table.setCurrentItem(source, 1);
	table.startDrag(Qt::MoveAction);
	Check(nativeDrags == 0,
	      "track and name cells cannot initiate source dragging");

	table.setCurrentItem(source, 2);
	table.startDrag(Qt::MoveAction);
	Check(nativeDrags == 1, "source cell can initiate source dragging");
}

void TestRebuildRunsAfterCompleteStartDragLifetime()
{
	TestRoutingTable table;
	table.setAttribute(Qt::WA_DontShowOnScreen);
	table.setColumnCount(3);
	table.resize(400, 300);
	table.show();

	auto *source = new QTreeWidgetItem(&table, {QStringLiteral("source")});
	auto *target = new QTreeWidgetItem(&table, {QStringLiteral("target")});
	source->setData(0, OBSPTAudioRoutingTable::SourceIdRole,
			QStringLiteral("source-id"));
	source->setData(0, OBSPTAudioRoutingTable::TrackRole, 2);
	target->setData(0, OBSPTAudioRoutingTable::SourceIdRole,
			QStringLiteral("target-id"));
	target->setData(0, OBSPTAudioRoutingTable::TrackRole, 3);
	table.setCurrentItem(source, 2);
	QApplication::processEvents();

	int calls = 0;
	bool nativeReturned = false;
	bool callbackAfterNativeReturn = false;
	bool receivedStableValues = false;
	table.dropHandler = [&](const OBSPTAudioRoutingDropRequest &request) {
		++calls;
		callbackAfterNativeReturn = nativeReturned;
		receivedStableValues =
			request.sourceId == QStringLiteral("source-id") &&
			request.sourceTrack == 2 &&
			request.targetSourceId == QStringLiteral("target-id") &&
			request.targetTrack == 3;
		table.clear();
		for (int track = 1; track <= 7; ++track)
			new QTreeWidgetItem(
				&table, {QStringLiteral("track-%1").arg(track)});
	};

	table.nativeDrag = [&] {
		QMimeData mime;
		QDropEvent event(table.visualItemRect(target).center(),
				 Qt::MoveAction, &mime, Qt::LeftButton,
				 Qt::NoModifier);
		table.dropEvent(&event);
		Check(event.isAccepted(), "custom drop accepts the event");
		Check(calls == 0,
		      "drop does not rebuild while native startDrag is active");
		nativeReturned = true;
	};
	table.startDrag(Qt::MoveAction);

	Check(calls == 1, "one drop invokes one routing mutation");
	Check(callbackAfterNativeReturn,
	      "routing mutation runs after native drag returns");
	Check(receivedStableValues,
	      "drop callback receives values instead of item pointers");
	Check(table.topLevelItemCount() == 7,
	      "Qt does not remove a rebuilt track row after the callback");
	Check(table.dragDropMode() == QAbstractItemView::DragDrop,
	      "routing table does not enable InternalMove");
}


/* The focus outline must span the whole cell, like the Visual Composition
 * Panel, instead of hugging the text. Qt's SE_ItemViewItemFocusRect collapses
 * onto the text rect unless the item reports showDecorationSelected. */
void TestFocusOutlineFillsCell()
{
	OBSPTAudioRoutingTable table;
	table.setColumnCount(3);
	table.setHeaderLabels({QStringLiteral("Track"), QStringLiteral("Name"),
			       QStringLiteral("Source")});
	table.setRootIsDecorated(false);
	table.setIndentation(0);
	table.setSelectionBehavior(QAbstractItemView::SelectItems);
	table.header()->resizeSection(2, 240);

	auto *item = new QTreeWidgetItem(&table);
	item->setText(2, QStringLiteral("aa"));

	table.resize(420, 120);
	table.show();
	QApplication::processEvents();
	table.setCurrentItem(item, 2);
	QApplication::processEvents();

	const QModelIndex index = table.model()->index(0, 2);
	const QRect cell = table.visualRect(index);
	if (cell.width() < 80) {
		Check(false, "the source column is laid out wide enough to test");
		return;
	}

	const QRect outline = table.FocusOutlineRect(index);
	if (outline.width() < cell.width() - 4) {
		std::cerr << "  (outline " << outline.width() << " of cell "
			  << cell.width() << ")" << std::endl;
		Check(false, "focus outline spans the whole cell");
	}
}

} // namespace

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication app(argc, argv);
	TestKeyboardReorderCommands();
	TestF2UsesCurrentCellRenameCommand();
	TestOnlySourceCellStartsDrag();
	TestRebuildRunsAfterCompleteStartDragLifetime();
	TestFocusOutlineFillsCell();
	if (failures == 0)
		std::cout << "All OBS-PT routing table tests passed\n";
	return failures == 0 ? 0 : 1;
}
