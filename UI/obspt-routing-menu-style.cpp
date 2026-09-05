#include "obspt-routing-menu-style.hpp"

#include <QMenu>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleOptionMenuItem>

OBSPTRoutingMenuStyle::OBSPTRoutingMenuStyle(QStyle *ownedBase,
					     const QString &dangerText)
	: QProxyStyle(ownedBase), dangerText(dangerText)
{
}

void OBSPTRoutingMenuStyle::drawControl(ControlElement element,
					const QStyleOption *option,
					QPainter *painter,
					const QWidget *widget) const
{
	if (element == CE_MenuItem) {
		const auto *menuOption =
			static_cast<const QStyleOptionMenuItem *>(option);
		if (menuOption->text.section(QLatin1Char('\t'), 0, 0)
			    .remove(QLatin1Char('&')) == dangerText) {
			QStyleOptionMenuItem danger = *menuOption;
			const QColor red(QStringLiteral("#c42b1c"));
			danger.palette.setColor(QPalette::Text, red);
			danger.palette.setColor(QPalette::ButtonText, red);
			danger.palette.setColor(QPalette::HighlightedText, red);
			danger.palette.setColor(QPalette::WindowText, red);
			QProxyStyle::drawControl(element, &danger, painter,
					 widget);
			return;
		}
	}
	QProxyStyle::drawControl(element, option, painter, widget);
}

void InstallOBSPTRoutingMenuStyle(QMenu *menu, const QString &dangerText)
{
	if (!menu)
		return;
	QStyle *ownedBase = QStyleFactory::create(menu->style()->objectName());
	if (!ownedBase)
		ownedBase = QStyleFactory::create(QStringLiteral("windows"));
	auto *style = new OBSPTRoutingMenuStyle(ownedBase, dangerText);
	style->setParent(menu);
	menu->setStyle(style);
}
