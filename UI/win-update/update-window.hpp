#pragma once

#include <QDialog>
#include <QUrl>
#include <memory>

#include "ui_OBSUpdate.h"

class OBSUpdate : public QDialog {
	Q_OBJECT

public:
	enum ReturnVal { Ignore, No = Ignore, Update, Yes = Update, Skip };

	OBSUpdate(QWidget *parent, const QString &text, const QUrl &releaseUrl);
	OBSUpdate(QWidget *parent, bool manualUpdate, const QString &text);

public slots:
	void on_yes_clicked();
	void on_no_clicked();
	void on_skip_clicked();
	virtual void accept() override;
	virtual void reject() override;

private:
	std::unique_ptr<Ui_OBSUpdate> ui;
};
