#pragma once

#include <QProxyStyle>
#include <QString>

class QMenu;

class OBSPTRoutingMenuStyle : public QProxyStyle {
public:
	OBSPTRoutingMenuStyle(QStyle *ownedBase, const QString &dangerText);

	void drawControl(ControlElement element, const QStyleOption *option,
			 QPainter *painter,
			 const QWidget *widget = nullptr) const override;

private:
	QString dangerText;
};

void InstallOBSPTRoutingMenuStyle(QMenu *menu, const QString &dangerText);
