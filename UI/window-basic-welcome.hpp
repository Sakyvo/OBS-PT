#pragma once

#include <memory>
#include <QDialog>

#include "ui_OBSWelcome.h"

class OBSWelcome : public QDialog {
	Q_OBJECT

public:
	explicit OBSWelcome(QWidget *parent = nullptr,
			    bool softwareEncoder = false);

	std::unique_ptr<Ui::OBSWelcome> ui;

private:
	void updateNav();

private slots:
	void prev();
	void next();
};
