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

#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QSplitter>
#include "qt-display.hpp"
#include <obs.hpp>

class OBSPropertiesView;
class OBSBasic;
class QTabWidget;
class QWidget;
class QLineEdit;

class OBSBasicProperties : public QDialog {
	Q_OBJECT

private:
	QPointer<OBSQTDisplay> preview;

	OBSBasic *main;
	bool acceptClicked;
	bool provisionalCreation = false;
	bool allowSourceRename = false;

	OBSSource source;
	OBSSignal removedSignal;
	OBSSignal renamedSignal;
	OBSSignal updatePropertiesSignal;
	OBSData oldSettings;
	OBSPropertiesView *view;
	QDialogButtonBox *buttonBox;
	QSplitter *windowSplitter;
	QTabWidget *tabs = nullptr;
	QWidget *audioPage = nullptr;
	QLineEdit *sourceNameEdit = nullptr;
	float oldVolume = 1.0f;
	float oldBalance = 0.5f;
	int64_t oldSyncOffset = 0;
	uint32_t oldSourceFlags = 0;
	obs_monitoring_type oldMonitoringType = OBS_MONITORING_TYPE_NONE;

	OBSSourceAutoRelease sourceA;
	OBSSourceAutoRelease sourceB;
	OBSSourceAutoRelease sourceClone;
	bool direction = true;

	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void UpdateProperties(void *data, calldata_t *params);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void DrawTransitionPreview(void *data, uint32_t cx, uint32_t cy);
	void UpdateCallback(void *obj, obs_data_t *settings);
	bool ConfirmQuit();
	int CheckSettings();
	void Cleanup();
	void BuildAudioPage();
	bool AudioSettingsChanged() const;
	void RestoreAudioSettings();
	bool CommitSourceName();

private slots:
	void on_buttonBox_clicked(QAbstractButton *button);
	void AddPreviewButton();

public:
	OBSBasicProperties(QWidget *parent, OBSSource source_,
			   bool provisionalCreation_ = false,
			   bool allowSourceRename_ = false);
	~OBSBasicProperties();

	void Init();
	bool WasAccepted() const { return acceptClicked; }

protected:
	virtual void closeEvent(QCloseEvent *event) override;
	virtual void reject() override;
};
