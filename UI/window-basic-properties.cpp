/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "obs-app.hpp"
#include "window-basic-properties.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "display-helpers.hpp"
#include "properties-view.hpp"

#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWindow>
#include <QMessageBox>
#include <obs-data.h>
#include <obs.h>
#include <qpointer.h>
#include <util/c99defs.h>

#include <cmath>

using namespace std;

static void CreateTransitionScene(OBSSource scene, const char *text,
				  uint32_t color);

OBSBasicProperties::OBSBasicProperties(QWidget *parent, OBSSource source_,
				       bool provisionalCreation_,
				       bool allowSourceRename_)
	: QDialog(parent),
	  preview(new OBSQTDisplay(this)),
	  main(qobject_cast<OBSBasic *>(parent)),
	  acceptClicked(false),
	  provisionalCreation(provisionalCreation_),
	  allowSourceRename(allowSourceRename_),
	  source(source_),
	  removedSignal(obs_source_get_signal_handler(source), "remove",
			OBSBasicProperties::SourceRemoved, this),
	  renamedSignal(obs_source_get_signal_handler(source), "rename",
			OBSBasicProperties::SourceRenamed, this),
	  oldSettings(obs_data_create()),
	  buttonBox(new QDialogButtonBox(this))
{
	oldVolume = obs_source_get_volume(source);
	oldBalance = obs_source_get_balance_value(source);
	oldSyncOffset = obs_source_get_sync_offset(source);
	oldSourceFlags = obs_source_get_flags(source);
	oldMonitoringType = obs_source_get_monitoring_type(source);
	int cx = (int)config_get_int(App()->GlobalConfig(), "PropertiesWindow",
				     "cx");
	int cy = (int)config_get_int(App()->GlobalConfig(), "PropertiesWindow",
				     "cy");

	enum obs_source_type type = obs_source_get_type(source);

	buttonBox->setObjectName(QStringLiteral("buttonBox"));
	buttonBox->setStandardButtons(QDialogButtonBox::Ok |
				      QDialogButtonBox::Cancel |
				      QDialogButtonBox::RestoreDefaults);

	buttonBox->button(QDialogButtonBox::Ok)->setText(QTStr("OK"));
	buttonBox->button(QDialogButtonBox::Cancel)->setText(QTStr("Cancel"));
	buttonBox->button(QDialogButtonBox::RestoreDefaults)
		->setText(QTStr("Defaults"));

	if (cx > 400 && cy > 400)
		resize(cx, cy);
	else
		resize(720, 580);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	QMetaObject::connectSlotsByName(this);

	/* The OBSData constructor increments the reference once */
	obs_data_release(oldSettings);

	OBSDataAutoRelease nd_settings = obs_source_get_settings(source);
	obs_data_apply(oldSettings, nd_settings);

	view = new OBSPropertiesView(
		nd_settings.Get(), source,
		(PropertiesReloadCallback)obs_source_properties,
		(PropertiesUpdateCallback) nullptr, // No special handling required for undo/redo
		(PropertiesVisualUpdateCb)obs_source_update);
	view->setMinimumHeight(150);

	preview->setMinimumSize(20, 150);
	preview->setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	// Create a QSplitter to keep a unified workflow here.
	windowSplitter = new QSplitter(Qt::Orientation::Vertical, this);
	windowSplitter->addWidget(preview);
	windowSplitter->addWidget(view);
	windowSplitter->setChildrenCollapsible(false);
	//windowSplitter->setSizes(QList<int>({ 16777216, 150 }));
	windowSplitter->setStretchFactor(0, 3);
	windowSplitter->setStretchFactor(1, 1);

	QWidget *capturePage = new QWidget(this);
	QVBoxLayout *captureLayout = new QVBoxLayout(capturePage);
	captureLayout->setContentsMargins(0, 0, 0, 0);
	captureLayout->addWidget(windowSplitter);

	tabs = new QTabWidget(this);
	tabs->addTab(capturePage, QTStr("Basic.PropertiesWindow.Capture"));
	if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0)
		BuildAudioPage();

	setLayout(new QVBoxLayout(this));
	if (allowSourceRename) {
		QFormLayout *nameLayout = new QFormLayout;
		nameLayout->setContentsMargins(0, 0, 0, 0);
		sourceNameEdit = new QLineEdit(QT_UTF8(obs_source_get_name(source)),
					       this);
		sourceNameEdit->setObjectName(QStringLiteral("sourceNameEdit"));
		nameLayout->addRow(QTStr("AudioRouting.Name"), sourceNameEdit);
		layout()->addItem(nameLayout);
	}
	layout()->addWidget(tabs);

	if (type == OBS_SOURCE_TYPE_TRANSITION) {
		connect(view, SIGNAL(PropertiesRefreshed()), this,
			SLOT(AddPreviewButton()));
	}

	layout()->addWidget(buttonBox);
	layout()->setAlignment(buttonBox, Qt::AlignBottom);

	view->show();
	installEventFilter(CreateShortcutFilter());

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name)));

	obs_source_inc_showing(source);

	updatePropertiesSignal.Connect(obs_source_get_signal_handler(source),
				       "update_properties",
				       OBSBasicProperties::UpdateProperties,
				       this);

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(preview->GetDisplay(),
					      OBSBasicProperties::DrawPreview,
					      this);
	};
	auto addTransitionDrawCallback = [this]() {
		obs_display_add_draw_callback(
			preview->GetDisplay(),
			OBSBasicProperties::DrawTransitionPreview, this);
	};
	uint32_t caps = obs_source_get_output_flags(source);
	bool drawable_type = type == OBS_SOURCE_TYPE_INPUT ||
			     type == OBS_SOURCE_TYPE_SCENE;
	bool drawable_preview = (caps & OBS_SOURCE_VIDEO) != 0;

	if (drawable_preview && drawable_type) {
		preview->show();
		connect(preview.data(), &OBSQTDisplay::DisplayCreated,
			addDrawCallback);

	} else if (type == OBS_SOURCE_TYPE_TRANSITION) {
		sourceA =
			obs_source_create_private("scene", "sourceA", nullptr);
		sourceB =
			obs_source_create_private("scene", "sourceB", nullptr);

		uint32_t colorA = 0xFFB26F52;
		uint32_t colorB = 0xFF6FB252;

		CreateTransitionScene(sourceA.Get(), "A", colorA);
		CreateTransitionScene(sourceB.Get(), "B", colorB);

		/**
		 * The cloned source is made from scratch, rather than using
		 * obs_source_duplicate, as the stinger transition would not
		 * play correctly otherwise.
		 */

		OBSDataAutoRelease settings = obs_source_get_settings(source);

		sourceClone = obs_source_create_private(
			obs_source_get_id(source), "clone", settings);

		obs_source_inc_active(sourceClone);
		obs_transition_set(sourceClone, sourceA);

		auto updateCallback = [=]() {
			OBSDataAutoRelease settings =
				obs_source_get_settings(source);
			obs_source_update(sourceClone, settings);

			obs_transition_clear(sourceClone);
			obs_transition_set(sourceClone, sourceA);
			obs_transition_force_stop(sourceClone);

			direction = true;
		};

		connect(view, &OBSPropertiesView::Changed, updateCallback);

		preview->show();
		connect(preview.data(), &OBSQTDisplay::DisplayCreated,
			addTransitionDrawCallback);

	} else {
		preview->hide();
	}
}

OBSBasicProperties::~OBSBasicProperties()
{
	if (sourceClone) {
		obs_source_dec_active(sourceClone);
	}
	obs_source_dec_showing(source);
	main->SaveProject();
	main->UpdateContextBarDeferred(true);
}

void OBSBasicProperties::AddPreviewButton()
{
	QPushButton *playButton =
		new QPushButton(QTStr("PreviewTransition"), this);
	VScrollArea *area = view;
	area->widget()->layout()->addWidget(playButton);

	playButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	auto play = [=]() {
		OBSSource start;
		OBSSource end;

		if (direction) {
			start = sourceA;
			end = sourceB;
		} else {
			start = sourceB;
			end = sourceA;
		}

		obs_transition_set(sourceClone, start);
		obs_transition_start(sourceClone, OBS_TRANSITION_MODE_AUTO,
				     main->GetTransitionDuration(), end);
		direction = !direction;

		start = nullptr;
		end = nullptr;
	};

	connect(playButton, &QPushButton::clicked, play);
}

static obs_source_t *CreateLabel(const char *name, size_t h)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();

	std::string text;
	text += " ";
	text += name;
	text += " ";

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", min(int(h), 300));

	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);

#ifdef _WIN32
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	obs_source_t *txtSource =
		obs_source_create_private(text_source_id, name, settings);

	return txtSource;
}

static void CreateTransitionScene(OBSSource scene, const char *text,
				  uint32_t color)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_int(settings, "width", obs_source_get_width(scene));
	obs_data_set_int(settings, "height", obs_source_get_height(scene));
	obs_data_set_int(settings, "color", color);

	OBSSourceAutoRelease colorBG = obs_source_create_private(
		"color_source", "background", settings);

	obs_scene_add(obs_scene_from_source(scene), colorBG);

	OBSSourceAutoRelease label =
		CreateLabel(text, obs_source_get_height(scene));
	obs_sceneitem_t *item =
		obs_scene_add(obs_scene_from_source(scene), label);

	vec2 size;
	vec2_set(&size, obs_source_get_width(scene),
#ifdef _WIN32
		 obs_source_get_height(scene));
#else
		 obs_source_get_height(scene) * 0.8);
#endif

	obs_sceneitem_set_bounds(item, &size);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
}

void OBSBasicProperties::SourceRemoved(void *data, calldata_t *params)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data),
				  "close");

	UNUSED_PARAMETER(params);
}

void OBSBasicProperties::SourceRenamed(void *data, calldata_t *params)
{
	const char *name = calldata_string(params, "new_name");
	QString title = QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name));
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	QMetaObject::invokeMethod(window,
				  "setWindowTitle", Q_ARG(QString, title));
	if (window->sourceNameEdit)
		QMetaObject::invokeMethod(window->sourceNameEdit, "setText",
				  Q_ARG(QString, QT_UTF8(name)));
}

void OBSBasicProperties::UpdateProperties(void *data, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicProperties *>(data)->view,
				  "ReloadProperties");
}

void OBSBasicProperties::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = buttonBox->buttonRole(button);

	if (val == QDialogButtonBox::AcceptRole) {
		if (!CommitSourceName())
			return;

		std::string scene_name =
			obs_source_get_name(main->GetCurrentSceneSource());

		auto undo_redo = [scene_name](const std::string &data) {
			OBSDataAutoRelease settings =
				obs_data_create_from_json(data.c_str());
			OBSSourceAutoRelease source = obs_get_source_by_name(
				obs_data_get_string(settings, "undo_sname"));
			obs_source_reset_settings(source, settings);

			obs_source_update_properties(source);

			OBSSourceAutoRelease scene_source =
				obs_get_source_by_name(scene_name.c_str());

			OBSBasic::Get()->SetCurrentScene(scene_source.Get(),
							 true);
		};

		OBSDataAutoRelease new_settings = obs_data_create();
		OBSDataAutoRelease curr_settings =
			obs_source_get_settings(source);
		obs_data_apply(new_settings, curr_settings);
		obs_data_set_string(new_settings, "undo_sname",
				    obs_source_get_name(source));
		obs_data_set_string(oldSettings, "undo_sname",
				    obs_source_get_name(source));

		std::string undo_data(obs_data_get_json(oldSettings));
		std::string redo_data(obs_data_get_json(new_settings));

		if (!provisionalCreation && undo_data.compare(redo_data) != 0)
			main->undo_s.add_action(
				QTStr("Undo.Properties")
					.arg(obs_source_get_name(source)),
				undo_redo, undo_redo, undo_data, redo_data);

		acceptClicked = true;
		close();

		if (view->DeferUpdate())
			view->UpdateSettings();

	} else if (val == QDialogButtonBox::RejectRole) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_clear(settings);

		if (view->DeferUpdate())
			obs_data_apply(settings, oldSettings);
		else
			obs_source_update(source, oldSettings);
		RestoreAudioSettings();

		close();

	} else if (val == QDialogButtonBox::ResetRole) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		obs_data_clear(settings);

		if (!view->DeferUpdate())
			obs_source_update(source, nullptr);

		view->ReloadProperties();
	}
}

void OBSBasicProperties::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicProperties::DrawTransitionPreview(void *data, uint32_t cx,
					       uint32_t cy)
{
	OBSBasicProperties *window = static_cast<OBSBasicProperties *>(data);

	if (!window->sourceClone)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->sourceClone), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->sourceClone), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(window->sourceClone);

	gs_projection_pop();
	gs_viewport_pop();
}

void OBSBasicProperties::Cleanup()
{
	config_set_int(App()->GlobalConfig(), "PropertiesWindow", "cx",
		       width());
	config_set_int(App()->GlobalConfig(), "PropertiesWindow", "cy",
		       height());

	obs_display_remove_draw_callback(preview->GetDisplay(),
					 OBSBasicProperties::DrawPreview, this);
	obs_display_remove_draw_callback(
		preview->GetDisplay(),
		OBSBasicProperties::DrawTransitionPreview, this);
}

static bool MonitoringFeedbackRisk(OBSBasic *main)
{
	QString monitoringDevice = QT_UTF8(config_get_string(
		main->Config(), "Audio", "MonitoringDeviceId"));
	if (monitoringDevice.isEmpty())
		monitoringDevice = QStringLiteral("default");

	for (int channel = 1; channel <= 2; ++channel) {
		OBSSourceAutoRelease source = obs_get_output_source(channel);
		if (!source)
			continue;
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		const QString device =
			QT_UTF8(obs_data_get_string(settings, "device_id"));
		if (!device.isEmpty() && device == monitoringDevice)
			return true;
	}
	return false;
}

void OBSBasicProperties::BuildAudioPage()
{
	audioPage = new QWidget(this);
	QFormLayout *audioLayout = new QFormLayout(audioPage);
	audioLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	audioLayout->setContentsMargins(12, 12, 12, 12);
	audioLayout->setVerticalSpacing(10);

	const QString sourceName = QT_UTF8(obs_source_get_name(source));
	QDoubleSpinBox *volume = new QDoubleSpinBox(audioPage);
	volume->setRange(-96.1, 26.0);
	volume->setDecimals(1);
	volume->setSingleStep(0.1);
	volume->setSuffix(QStringLiteral(" dB"));
	volume->setSpecialValueText(QStringLiteral("-inf dB"));
	volume->setValue(obs_mul_to_db(obs_source_get_volume(source)));
	volume->setAccessibleName(
		QTStr("AudioRouting.Accessible.Volume").arg(sourceName));
	connect(volume, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this, [this](double value) {
			obs_source_set_volume(
				source,
				value <= -96.0 ? 0.0f : obs_db_to_mul(value));
		});
	audioLayout->addRow(QTStr("Basic.AdvAudio.Volume"), volume);

	QCheckBox *mono = new QCheckBox(audioPage);
	mono->setChecked((obs_source_get_flags(source) &
			  OBS_SOURCE_FLAG_FORCE_MONO) != 0);
	mono->setAccessibleName(
		QTStr("AudioRouting.Accessible.Mono").arg(sourceName));
	connect(mono, &QCheckBox::toggled, this, [this](bool enabled) {
		uint32_t flags = obs_source_get_flags(source);
		obs_source_set_flags(source,
			enabled ? flags | OBS_SOURCE_FLAG_FORCE_MONO
				: flags & ~OBS_SOURCE_FLAG_FORCE_MONO);
	});
	audioLayout->addRow(QTStr("Basic.AdvAudio.Mono"), mono);

	QSlider *balance = new QSlider(Qt::Horizontal, audioPage);
	balance->setRange(0, 100);
	balance->setValue((int)std::round(
		obs_source_get_balance_value(source) * 100.0f));
	balance->setAccessibleName(
		QTStr("AudioRouting.Accessible.Balance").arg(sourceName));
	connect(balance, &QSlider::valueChanged, this, [this](int value) {
		obs_source_set_balance_value(source, value / 100.0f);
	});
	audioLayout->addRow(QTStr("Basic.AdvAudio.Balance"), balance);

	QSpinBox *sync = new QSpinBox(audioPage);
	sync->setRange(-950, 20000);
	sync->setSuffix(QStringLiteral(" ms"));
	sync->setValue((int)(obs_source_get_sync_offset(source) / 1000000));
	sync->setAccessibleName(
		QTStr("AudioRouting.Accessible.Sync").arg(sourceName));
	connect(sync, QOverload<int>::of(&QSpinBox::valueChanged), this,
		[this](int value) {
			obs_source_set_sync_offset(source,
					   (int64_t)value * 1000000);
		});
	audioLayout->addRow(QTStr("Basic.AdvAudio.SyncOffset"), sync);

	if (obs_audio_monitoring_available()) {
		QComboBox *monitoring = new QComboBox(audioPage);
		monitoring->addItem(QTStr("Basic.AdvAudio.Monitoring.None"),
				    (int)OBS_MONITORING_TYPE_NONE);
		monitoring->addItem(
			QTStr("Basic.AdvAudio.Monitoring.MonitorOnly"),
			(int)OBS_MONITORING_TYPE_MONITOR_ONLY);
		monitoring->addItem(QTStr("Basic.AdvAudio.Monitoring.Both"),
				    (int)OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
		monitoring->setCurrentIndex(monitoring->findData(
			(int)obs_source_get_monitoring_type(source)));
		monitoring->setAccessibleName(
			QTStr("AudioRouting.Accessible.Monitoring")
				.arg(sourceName));

		QLabel *warning = new QLabel(
			QTStr("AudioRouting.MonitoringWarning"), audioPage);
		warning->setWordWrap(true);
		warning->setProperty("warning", true);
		warning->setAccessibleName(
			QTStr("AudioRouting.Accessible.Warning"));
		warning->setVisible(
			obs_source_get_monitoring_type(source) ==
					OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT &&
			MonitoringFeedbackRisk(main));

		connect(monitoring,
			QOverload<int>::of(&QComboBox::currentIndexChanged), this,
			[this, monitoring, warning](int index) {
				const obs_monitoring_type requested =
					(obs_monitoring_type)monitoring
						->itemData(index)
						.toInt();
				obs_source_set_monitoring_type(source, requested);
				main->AudioSourceMonitoringChanged(
					source,
					requested ==
						OBS_MONITORING_TYPE_MONITOR_ONLY);
				const obs_monitoring_type actual =
					obs_source_get_monitoring_type(source);
				if (actual != requested) {
					const QSignalBlocker blocker(monitoring);
					monitoring->setCurrentIndex(
						monitoring->findData((int)actual));
				}
				warning->setVisible(
					actual ==
						OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT &&
					MonitoringFeedbackRisk(main));
			});
		audioLayout->addRow(QTStr("Basic.AdvAudio.Monitoring"),
				    monitoring);
		audioLayout->addRow(warning);
	}

	tabs->addTab(audioPage, QTStr("Basic.PropertiesWindow.Audio"));
}

bool OBSBasicProperties::AudioSettingsChanged() const
{
	return obs_source_get_volume(source) != oldVolume ||
	       obs_source_get_balance_value(source) != oldBalance ||
	       obs_source_get_sync_offset(source) != oldSyncOffset ||
	       obs_source_get_flags(source) != oldSourceFlags ||
	       obs_source_get_monitoring_type(source) != oldMonitoringType;
}

bool OBSBasicProperties::CommitSourceName()
{
	if (!sourceNameEdit)
		return true;
	return main->RenameAudioRoutingSource(
		main->GetAudioRoutingSourceIdentity(source), sourceNameEdit->text(),
		this);
}

void OBSBasicProperties::RestoreAudioSettings()
{
	obs_source_set_volume(source, oldVolume);
	obs_source_set_balance_value(source, oldBalance);
	obs_source_set_sync_offset(source, oldSyncOffset);
	obs_source_set_flags(source, oldSourceFlags);
	obs_source_set_monitoring_type(source, oldMonitoringType);
	main->AudioSourceMonitoringChanged(
		source, oldMonitoringType == OBS_MONITORING_TYPE_MONITOR_ONLY);
}

void OBSBasicProperties::reject()
{
	if (!provisionalCreation && !acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			return;
		}
	}

	Cleanup();
	done(0);
}

void OBSBasicProperties::closeEvent(QCloseEvent *event)
{
	if (!provisionalCreation && !acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			event->ignore();
			return;
		}
	}

	QDialog::closeEvent(event);
	if (!event->isAccepted())
		return;

	Cleanup();
}

void OBSBasicProperties::Init()
{
	show();
}

int OBSBasicProperties::CheckSettings()
{
	OBSDataAutoRelease currentSettings = obs_source_get_settings(source);
	const char *oldSettingsJson = obs_data_get_json(oldSettings);
	const char *currentSettingsJson = obs_data_get_json(currentSettings);

	return strcmp(currentSettingsJson, oldSettingsJson) ||
	       AudioSettingsChanged() ||
	       (sourceNameEdit &&
		sourceNameEdit->text().trimmed() !=
			QT_UTF8(obs_source_get_name(source)));
}

bool OBSBasicProperties::ConfirmQuit()
{
	QMessageBox::StandardButton button;

	button = OBSMessageBox::question(
		this, QTStr("Basic.PropertiesWindow.ConfirmTitle"),
		QTStr("Basic.PropertiesWindow.Confirm"),
		QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

	switch (button) {
	case QMessageBox::Save:
		if (!CommitSourceName())
			return false;
		acceptClicked = true;
		if (view->DeferUpdate())
			view->UpdateSettings();
		// Do nothing because the settings are already updated
		break;
	case QMessageBox::Discard:
		obs_source_update(source, oldSettings);
		RestoreAudioSettings();
		break;
	case QMessageBox::Cancel:
		return false;
		break;
	default:
		/* If somehow the dialog fails to show, just default to
		 * saving the settings. */
		break;
	}
	return true;
}
