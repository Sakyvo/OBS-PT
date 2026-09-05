#include "obspt-audio-routing-dock.hpp"

#include "obspt-audio-routing-model.hpp"
#include "obspt-audio-source-creation-model.hpp"
#include "obspt-audio-routing-table.hpp"
#include "obspt-routing-menu-style.hpp"
#include "obspt-toolbar-theme.hpp"
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSignalBlocker>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
namespace {

constexpr int SourceIdRole = OBSPTAudioRoutingTable::SourceIdRole;
constexpr int TrackRole = OBSPTAudioRoutingTable::TrackRole;
constexpr int TrackAnchorRole = OBSPTAudioRoutingTable::TrackAnchorRole;
constexpr int GroupStartRole = OBSPTAudioRoutingTable::GroupStartRole;
constexpr int GroupEndRole = OBSPTAudioRoutingTable::GroupEndRole;


QString SourceState(const OBSBasic *main, const OBSPTAudioRoute &route)
{
	QStringList states;
	if (route.muted)
		states.push_back(QTStr("AudioRouting.State.Muted"));
	if (route.monitorOnly)
		states.push_back(QTStr("AudioRouting.State.MonitorOnly"));
	if (main->AudioSourcePendingNextSession(route.sourceId))
		states.push_back(QTStr("AudioRouting.State.Pending"));
	if (main->AudioRoutingLocked())
		states.push_back(QTStr("AudioRouting.State.Locked"));

	OBSSource source = main->GetAudioRoutingSource(route.sourceId);
	states.push_back(source && obs_source_audio_active(source)
				 ? QTStr("AudioRouting.State.Active")
				 : QTStr("AudioRouting.State.Inactive"));
	return states.join(QStringLiteral(", "));
}

bool MixIsActive(const OBSPTAudioRoutingModel &routing)
{
	for (const OBSPTAudioRoute &route : routing.Sources()) {
		if (!route.muted && !route.monitorOnly)
			return true;
	}
	return false;
}

QColor TrackColor(const QPalette &palette, bool active, bool muted)
{
	if (muted)
		return QColor(QStringLiteral("#8b0000"));
	if (active)
		return QColor(QStringLiteral("#218739"));
	return palette.color(QPalette::Disabled, QPalette::Text);
}

} // namespace

OBSPTAudioRoutingDock::OBSPTAudioRoutingDock(OBSBasic *main_, QWidget *parent)
	: QWidget(parent), main(main_)
{
	layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	table = new OBSPTAudioRoutingTable(this);
	table->setObjectName(QStringLiteral("OBSBasic.audioRoutingTable"));
	table->setColumnCount(3);
	table->setHeaderLabels({QTStr("AudioRouting.Track"),
				QTStr("AudioRouting.Name"),
				QTStr("AudioRouting.Source")});
	table->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	table->header()->setSectionResizeMode(1, QHeaderView::Interactive);
	table->header()->setSectionResizeMode(2, QHeaderView::Stretch);
	table->header()->resizeSection(1, 112);
	table->setRootIsDecorated(false);
	table->setIndentation(0);
	table->setUniformRowHeights(true);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setSelectionBehavior(QAbstractItemView::SelectItems);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setContextMenuPolicy(Qt::CustomContextMenu);
	table->setAccessibleName(QTStr("AudioRouting.Accessible.Tree"));
	layout->addWidget(table, 1);

	toolbar = new QToolBar(this);
	placeholderToolbar = new QToolBar(this);

	OBSPTRoutingToolbarLabels labels;
	labels.add = QTStr("AudioRouting.Menu.Add");
	labels.remove = QTStr("AudioRouting.Menu.Remove");
	labels.properties = QTStr("AudioRouting.Menu.Inspect");
	labels.moveUp = QTStr("AudioRouting.Menu.MoveUp");
	labels.moveDown = QTStr("AudioRouting.Menu.MoveDown");
	labels.multiTrack = QTStr("AudioRouting.AudioMode.MultiTrack");

	OBSPTRoutingToolbarActions actions =
		OBSPTBuildRoutingToolbar(toolbar, placeholderToolbar, labels);
	addAction = actions.add;
	removeAction = actions.remove;
	propertiesAction = actions.properties;
	moveUpAction = actions.moveUp;
	moveDownAction = actions.moveDown;
	multiTrackAction = actions.multiTrack;

	/* The native sources toolbar keeps its spacer outside the toolbar, so
	 * mirror that: commands on the left, placeholder pushed right. */
	QWidget *commandRow = new QWidget(this);
	QHBoxLayout *commandLayout = new QHBoxLayout(commandRow);
	commandLayout->setContentsMargins(0, 0, 0, 0);
	commandLayout->setSpacing(0);
	commandLayout->addWidget(toolbar);
	commandLayout->addStretch(1);
	commandLayout->addWidget(placeholderToolbar);
	layout->addWidget(commandRow);

	OBSPTAudioRoutingTable *routingTable =
		static_cast<OBSPTAudioRoutingTable *>(table);
	routingTable->dropHandler =
		[this](const OBSPTAudioRoutingDropRequest &request) {
			HandleDrop(request);
		};
	routingTable->menuHandler = [this](const QPoint &position) {
		if (main->GetAudioRoutingModel().Mode() ==
		    OBSPTAudioMode::SingleTrack)
			return;
		ShowSourceMenu(position);
	};
	routingTable->deleteHandler = [this] { RemoveSelectedSource(); };
	routingTable->reorderHandler = [this](int direction) {
		MoveSelectedSource(direction);
	};
	routingTable->renameHandler = [this] { RenameSelectedCell(); };

	connect(addAction, &QAction::triggered, this, [this] {
		if (RefuseWhenSingleTrack())
			return;
		QWidget *button = toolbar->widgetForAction(addAction);
		main->AddAudioSourcePopupMenu(
			button->mapToGlobal(QPoint(0, button->height())));
	});
	connect(removeAction, &QAction::triggered, this,
		&OBSPTAudioRoutingDock::RemoveSelectedSource);
	connect(propertiesAction, &QAction::triggered, this,
		&OBSPTAudioRoutingDock::OpenSelectedProperties);
	connect(moveUpAction, &QAction::triggered, this,
		[this] { MoveSelectedSource(-1); });
	connect(moveDownAction, &QAction::triggered, this,
		[this] { MoveSelectedSource(1); });
	connect(multiTrackAction, &QAction::triggered, this,
		&OBSPTAudioRoutingDock::ToggleAudioMode);
	connect(table, &QTreeWidget::itemSelectionChanged, this, [this] {
		QTreeWidgetItem *item = SelectedSourceItem();
		selectedSourceId = item ? item->data(0, SourceIdRole).toString()
					: QString();
	});
	connect(table, &QTreeWidget::itemDoubleClicked, this,
		[this](QTreeWidgetItem *item, int column) {
			if (column == 1 &&
			    item->data(0, TrackAnchorRole).toBool()) {
				const int track = item->data(0, TrackRole).toInt();
				if (track >=
				    OBSPTAudioRoutingModel::FirstEditorTrack &&
				    track <=
					    OBSPTAudioRoutingModel::LastEditorTrack)
					table->editItem(item, 1);
				return;
			}
			if (column == 2 &&
			    !item->data(0, SourceIdRole).toString().isEmpty())
				OpenSelectedProperties();
		});
	connect(table, &QTreeWidget::itemActivated, this,
		[this](QTreeWidgetItem *item) {
			if (table->currentColumn() == 2 &&
			    !item->data(0, SourceIdRole).toString().isEmpty())
				table->setCurrentItem(item, 2);
		});
	connect(table, &QTreeWidget::customContextMenuRequested, this,
		&OBSPTAudioRoutingDock::ShowSourceMenu);
	connect(table, &QTreeWidget::itemChanged, this,
		[this](QTreeWidgetItem *item, int column) {
			if (refreshing)
				return;
			if (column == 1 &&
			    item->data(0, TrackAnchorRole).toBool()) {
				const int track = item->data(0, TrackRole).toInt();
				if (!main->SetAudioTrackName(track, item->text(1)))
					Refresh();
			} else if (column == 2) {
				const QString sourceId =
					item->data(0, SourceIdRole).toString();
				if (!sourceId.isEmpty() &&
				    !main->RenameAudioRoutingSource(
					    sourceId, item->text(2), this))
					Refresh();
			}
		});

	Refresh();
}

void OBSPTAudioRoutingDock::Refresh()
{
	refreshing = true;
	BuildTable();
	refreshing = false;
}

int OBSPTAudioRoutingDock::SelectedCreationTrack() const
{
	QTreeWidgetItem *item = table->currentItem();
	if (!item)
		return 0;
	const int track = item->data(0, TrackRole).toInt();
	const bool trackAnchor = item->data(0, TrackAnchorRole).toBool();
	const bool sourceCellSelected =
		!item->data(0, SourceIdRole).toString().isEmpty() &&
		table->currentColumn() == 2;
	return ResolveSelectedEditorTrack(track, trackAnchor,
				  sourceCellSelected);
}

void OBSPTAudioRoutingDock::UpdateAudioModeEntry()
{
	if (!multiTrackAction)
		return;
	const bool singleTrack = main->GetAudioRoutingModel().Mode() ==
				 OBSPTAudioMode::SingleTrack;
	const QString text =
		singleTrack ? QTStr("AudioRouting.AudioMode.SingleTrack")
			    : QTStr("AudioRouting.AudioMode.MultiTrack");
	multiTrackAction->setText(text);
	multiTrackAction->setToolTip(text);

	/* The theme owns both glyphs; swapping the id and repolishing is what
	 * makes the icon follow the mode. */
	multiTrackAction->setProperty("themeID",
				      singleTrack
					      ? kOBSPTAudioModeSingleThemeId
					      : kOBSPTAudioModeMultiThemeId);
	OBSPTApplyToolbarActionTheme(placeholderToolbar);
}

/* ADR-0003 distinguishes two reasons a command does nothing. Ordinary context
 * unavailability stays silent, because the user can work it out. A command
 * locked out by single-track mode cannot be worked out, so it points at the
 * mode control. */
bool OBSPTAudioRoutingDock::RefuseWhenSingleTrack()
{
	if (main->GetAudioRoutingModel().Mode() != OBSPTAudioMode::SingleTrack)
		return false;
	OBSMessageBox::information(
		this, QTStr("AudioRouting.AudioMode.Locked.Title"),
		QTStr("AudioRouting.AudioMode.Locked.Text"));
	return true;
}

void OBSPTAudioRoutingDock::ToggleAudioMode()
{
	const bool singleTrack = main->GetAudioRoutingModel().Mode() ==
				 OBSPTAudioMode::SingleTrack;
	main->SetAudioMode(singleTrack ? OBSPTAudioMode::MultiTrack
				       : OBSPTAudioMode::SingleTrack);
}

void OBSPTAudioRoutingDock::BuildTable()
{
	UpdateAudioModeEntry();
	const QSignalBlocker blocker(table);
	table->clear();
	const OBSPTAudioRoutingModel &routing = main->GetAudioRoutingModel();

	auto appendGroup = [this, &routing](
			   const QString &trackText, int track,
			   const QString &name,
			   const QVector<const OBSPTAudioRoute *> &sources,
			   bool active, bool mutedGroup) {
		const int rowCount = std::max(1, sources.size());
		for (int index = 0; index < rowCount; ++index) {
			QTreeWidgetItem *item = new QTreeWidgetItem(table);
			const QString trackLabel =
				trackText + QStringLiteral(" ") + name;
			item->setData(0, TrackRole, track);
			item->setData(0, TrackAnchorRole, index == 0);
			item->setData(0, GroupStartRole, index == 0);
			item->setData(0, GroupEndRole, index == rowCount - 1);
			item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable |
				       Qt::ItemIsDropEnabled);
			if (index == 0) {
				item->setText(0, trackText);
				item->setText(1, name);
				item->setForeground(
					0, TrackColor(table->palette(), active,
						      mutedGroup));
				QFont trackFont = item->font(0);
				trackFont.setBold(true);
				item->setFont(0, trackFont);
				if (track >=
					    OBSPTAudioRoutingModel::FirstEditorTrack &&
				    track <=
					    OBSPTAudioRoutingModel::LastEditorTrack)
					item->setFlags(item->flags() |
						       Qt::ItemIsEditable);
			}

			if (index >= sources.size()) {
				item->setData(0, Qt::AccessibleTextRole, trackLabel);
				continue;
			}
			const OBSPTAudioRoute &route = *sources[index];
			const QString state = SourceState(main, route);
			const QString accessible =
				QTStr("AudioRouting.Accessible.Source")
					.arg(route.sourceName)
					.arg(trackLabel)
					.arg(state);
			item->setText(2, route.sourceName);
			item->setData(0, SourceIdRole, route.sourceId);
			item->setFlags(item->flags() | Qt::ItemIsDragEnabled |
				       Qt::ItemIsEditable);
			item->setToolTip(2, state);
			item->setData(0, Qt::AccessibleTextRole, accessible);
			item->setData(2, Qt::AccessibleTextRole, accessible);
			item->setData(2, Qt::AccessibleDescriptionRole, state);
		}
	};

	const bool singleTrack =
		routing.Mode() == OBSPTAudioMode::SingleTrack;

	QVector<const OBSPTAudioRoute *> mixSources;
	for (const OBSPTAudioRoute &route : routing.Sources()) {
		if (singleTrack) {
			/* Everything mixes into the one track. Application
			 * Audio Sources are left out because the Mix
			 * de-duplication rules keep them from doubling
			 * system playback. */
			if (route.kind !=
			    OBSPTAudioSourceKind::ApplicationAudio)
				mixSources.push_back(&route);
		} else if (route.kind == OBSPTAudioSourceKind::DesktopAudio &&
			   !route.muted) {
			mixSources.push_back(&route);
		}
	}
	appendGroup(QString::number(OBSPTAudioRoutingModel::MixTrack),
		    OBSPTAudioRoutingModel::MixTrack,
		    routing.TrackName(OBSPTAudioRoutingModel::MixTrack),
		    mixSources,
		    singleTrack ? !routing.SingleTrackMuted()
				: MixIsActive(routing),
		    false);

	/* Single-track mode has no Editor Track and no Muted Sources Group,
	 * so neither is shown at all - not dimmed, absent. */
	if (!singleTrack) {
		for (int track = OBSPTAudioRoutingModel::FirstEditorTrack;
		     track <= OBSPTAudioRoutingModel::LastEditorTrack;
		     ++track) {
			appendGroup(QString::number(track), track,
				    routing.TrackName(track),
				    routing.SourcesForTrack(track),
				    routing.HasEligibleEditorTrack(track),
				    false);
		}

		appendGroup(QStringLiteral("M"), 0,
			    QTStr("AudioRouting.MutedGroup"),
			    routing.MutedSources(),
			    !routing.MutedSources().isEmpty(), true);
	}

	if (!selectedSourceId.isEmpty()) {
		for (int index = 0; index < table->topLevelItemCount(); ++index) {
			QTreeWidgetItem *item = table->topLevelItem(index);
			if (item->data(0, SourceIdRole).toString() ==
			    selectedSourceId) {
				table->setCurrentItem(item, 2);
				break;
			}
		}
	}
}

QTreeWidgetItem *OBSPTAudioRoutingDock::SelectedSourceItem() const
{
	QTreeWidgetItem *item = table->currentItem();
	return item && table->currentColumn() == 2 &&
		       !item->data(0, SourceIdRole).toString().isEmpty()
		       ? item
		       : nullptr;
}


void OBSPTAudioRoutingDock::OpenSelectedProperties()
{
	QTreeWidgetItem *item = SelectedSourceItem();
	if (!item)
		return;
	OBSSource source = main->GetAudioRoutingSource(
		item->data(0, SourceIdRole).toString());
	if (source)
		main->OpenProperties(source, true);
}

void OBSPTAudioRoutingDock::RenameSelectedCell()
{
	QTreeWidgetItem *item = table->currentItem();
	if (!item)
		return;
	if (table->currentColumn() == 1) {
		const int track = item->data(0, TrackRole).toInt();
		if (item->data(0, TrackAnchorRole).toBool() &&
		    track >= OBSPTAudioRoutingModel::FirstEditorTrack &&
		    track <= OBSPTAudioRoutingModel::LastEditorTrack)
			table->editItem(item, 1);
		return;
	}
	if (table->currentColumn() == 2 &&
	    !item->data(0, SourceIdRole).toString().isEmpty())
		table->editItem(item, 2);
}

void OBSPTAudioRoutingDock::RenameSource(const QString &sourceId)
{
	for (int index = 0; index < table->topLevelItemCount(); ++index) {
		QTreeWidgetItem *item = table->topLevelItem(index);
		if (item->data(0, SourceIdRole).toString() != sourceId)
			continue;
		table->setCurrentItem(item, 2);
		table->editItem(item, 2);
		return;
	}
}

void OBSPTAudioRoutingDock::RemoveSelectedSource()
{
	if (RefuseWhenSingleTrack())
		return;
	QTreeWidgetItem *item = SelectedSourceItem();
	if (!item)
		return;
	/* Commands are never disabled (ADR-0003), so eligibility is decided
	 * here rather than by the button's enabled state. */
	const QString sourceId = item->data(0, SourceIdRole).toString();
	if (!main->CanRemoveAudioRoutingSource(sourceId))
		return;
	main->RemoveAudioRoutingSource(sourceId);
}

void OBSPTAudioRoutingDock::MoveSelectedSource(int direction)
{
	if (RefuseWhenSingleTrack())
		return;
	QTreeWidgetItem *item = SelectedSourceItem();
	if (!item)
		return;
	const QString sourceId = item->data(0, SourceIdRole).toString();
	const OBSPTAudioRoute *route =
		main->GetAudioRoutingModel().FindSource(sourceId);
	if (!route)
		return;
	/* Commands are never disabled (ADR-0003): refuse a direction the
	 * model says is unavailable instead of relying on the button. */
	const OBSPTAudioMoveAvailability movement =
		main->GetAudioRoutingModel().MoveAvailability(sourceId);
	if ((direction < 0 && !movement.up) || (direction > 0 && !movement.down))
		return;
	if (route->muted)
		main->MoveMutedAudioSource(sourceId, direction);
	else
		main->MoveAudioSource(sourceId, direction);
}

void OBSPTAudioRoutingDock::ShowSourceMenu(const QPoint &position)
{
	QTreeWidgetItem *item = table->itemAt(position);
	if (!item || table->columnAt(position.x()) != 2 ||
	    item->data(0, SourceIdRole).toString().isEmpty())
		return;
	table->setCurrentItem(item, 2);
	const QString sourceId = item->data(0, SourceIdRole).toString();
	const OBSPTAudioRoute *route =
		main->GetAudioRoutingModel().FindSource(sourceId);
	if (!route)
		return;
	const bool wasMuted = route->muted;

	QMenu menu(table);
	const QString removeText = QTStr("AudioRouting.Menu.Remove");
	InstallOBSPTRoutingMenuStyle(&menu, removeText);
	QMenu *moveMenu = menu.addMenu(QTStr("AudioRouting.Menu.MoveTo"));
	for (int track = OBSPTAudioRoutingModel::FirstEditorTrack;
	     track <= OBSPTAudioRoutingModel::LastEditorTrack; ++track) {
		QAction *action = moveMenu->addAction(
			QTStr("AudioRouting.TrackLabel")
				.arg(track)
				.arg(main->GetAudioRoutingModel().TrackName(track)));
		const bool currentTrack = route->editorTrack == track;
		action->setCheckable(true);
		action->setChecked(currentTrack);
		action->setEnabled(
			route->kind != OBSPTAudioSourceKind::DesktopAudio &&
			(!currentTrack || wasMuted));
		connect(action, &QAction::triggered, this,
			[this, sourceId, track] {
				main->MoveAudioSourceToTrack(
					sourceId,
					{track,
					 OBSPTAudioTrackPlacement::Append, 0});
			});
	}
	menu.addSeparator();
	QAction *up = menu.addAction(QTStr("AudioRouting.Menu.MoveUp"));
	QAction *down = menu.addAction(QTStr("AudioRouting.Menu.MoveDown"));
	const OBSPTAudioMoveAvailability movement =
		main->GetAudioRoutingModel().MoveAvailability(sourceId);
	up->setEnabled(movement.up);
	down->setEnabled(movement.down);
	connect(up, &QAction::triggered, this,
		[this] { MoveSelectedSource(-1); });
	connect(down, &QAction::triggered, this,
		[this] { MoveSelectedSource(1); });
	QAction *mute = menu.addAction(
		wasMuted ? QTStr("AudioRouting.Menu.Unmute")
			 : QTStr("AudioRouting.Menu.Mute"));
	connect(mute, &QAction::triggered, this,
		[this, sourceId, wasMuted] {
			main->SetAudioSourceMuted(sourceId, !wasMuted);
		});
	menu.addSeparator();
	QAction *rename = menu.addAction(QTStr("Rename"));
	connect(rename, &QAction::triggered, this, [this, sourceId] {
		QTimer::singleShot(0, this,
				   [this, sourceId] { RenameSource(sourceId); });
	});
	QAction *properties =
		menu.addAction(QTStr("AudioRouting.Menu.Inspect"));
	connect(properties, &QAction::triggered, this,
		&OBSPTAudioRoutingDock::OpenSelectedProperties);
	if (main->CanRemoveAudioRoutingSource(sourceId)) {
		QAction *remove = menu.addAction(removeText);
		connect(remove, &QAction::triggered, this,
			&OBSPTAudioRoutingDock::RemoveSelectedSource);
	}
	menu.exec(table->viewport()->mapToGlobal(position));
}

void OBSPTAudioRoutingDock::HandleDrop(
	const OBSPTAudioRoutingDropRequest &request)
{
	const QString sourceId = request.sourceId;
	if (sourceId.isEmpty())
		return;
	const int targetTrack = request.targetTrack;
	const QString targetSourceId = request.targetSourceId;
	const int indicator = request.indicator;

	if (targetTrack == OBSPTAudioRoutingModel::MixTrack) {
		QMessageBox::information(this, QTStr("AudioRouting.Title"),
					 QTStr("AudioRouting.MixFixed"));
		return;
	}
	if (targetSourceId == sourceId)
		return;

	if (targetTrack == 0) {
		const QVector<const OBSPTAudioRoute *> sources =
			main->GetAudioRoutingModel().MutedSources();
		int index = 0;
		int destinationCount = 0;
		for (int i = 0; i < sources.size(); ++i) {
			if (sources[i]->sourceId == sourceId)
				continue;
			if (sources[i]->sourceId == targetSourceId)
				index = destinationCount +
					(indicator > 0 ? 1 : 0);
			++destinationCount;
		}
		if (targetSourceId.isEmpty())
			index = destinationCount;
		main->MoveAudioSourceToTrack(
			sourceId,
			{0, OBSPTAudioTrackPlacement::AtIndex, index});
		return;
	}

	const OBSPTAudioRoute *route =
		main->GetAudioRoutingModel().FindSource(sourceId);
	if (!route || route->kind == OBSPTAudioSourceKind::DesktopAudio)
		return;
	const QVector<const OBSPTAudioRoute *> sources =
		main->GetAudioRoutingModel().SourcesForTrack(targetTrack);
	int index = 0;
	int destinationCount = 0;
	for (int i = 0; i < sources.size(); ++i) {
		if (sources[i]->sourceId == sourceId)
			continue;
		if (sources[i]->sourceId == targetSourceId)
			index = destinationCount + (indicator > 0 ? 1 : 0);
		++destinationCount;
	}
	if (targetSourceId.isEmpty())
		index = destinationCount;
	main->MoveAudioSourceToTrack(
		sourceId,
		{targetTrack, OBSPTAudioTrackPlacement::AtIndex, index});
}
