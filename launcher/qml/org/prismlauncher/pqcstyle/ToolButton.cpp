// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 Rachel Powers <508861+Ryex@users.noreply.github.com>
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 Rachel Powers <508861+Ryex@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *  This file incorporates work covered by the following copyright and
 *  permission notice:
 *
 *    Copyright © 2017 Marco Martin <mart@kde.org>
 *    Copyright © 2017 David Edmundson <davidedmundson@kde.org>
 *    Copyright © 2019 David Redondo <david@david-redondo.de>
 *    Copyright © 2023 ivan tkachenko <me@ratijas.tk>
 *
 *    Licensed under LGPL-3.0-only OR GPL-2.0-only OR
 *    GPL-3.0-only OR LicenseRef-KFQF-Accepted-GPL OR
 *    LicenseRef-Qt-Commercial
 *
 *          https://community.kde.org/Policies/Licensing_Policy
 */

#include "ToolButton.h"
#include "PQuickStyleItem.h"

#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionToolButton>

PStyleToolButton::PStyleToolButton(QQuickItem* parent) : PQuickStyleItem(parent)
{
    m_type = QStringLiteral("toolbutton");
}

void PStyleToolButton::doInitStyleOption()
{
    if (!m_styleoption) {
        m_styleoption = new QStyleOptionToolButton();
    }

    QStyleOptionToolButton* opt = qstyleoption_cast<QStyleOptionToolButton*>(m_styleoption);
    opt->subControls = QStyle::SC_ToolButton;

    if (m_flat) {
        opt->state |= QStyle::State_AutoRaise;
    }

    opt->activeSubControls = QStyle::SC_ToolButton;
    opt->text = text();

    if (m_iconDirty || opt->icon.isNull()) {
        opt->icon = iconFromIconProperty();
        m_iconDirty = false;
    }

    auto iconSize = QSize(m_properties[QStringLiteral("iconWidth")].toInt(), m_properties[QStringLiteral("iconHeight")].toInt());
    if (iconSize.isEmpty()) {
        const auto metric = m_flat ? QStyle::PM_ToolBarIconSize : QStyle::PM_ButtonIconSize;
        int e = PQuickStyleItem::style()->pixelMetric(metric, m_styleoption, nullptr);
        if (iconSize.width() <= 0) {
            iconSize.setWidth(e);
        }
        if (iconSize.height() <= 0) {
            iconSize.setHeight(e);
        }
    }
    opt->iconSize = iconSize;

    if (m_properties.value(QStringLiteral("menu")).toBool()) {
        opt->features |= QStyleOptionToolButton::HasMenu;
    }

    const int toolButtonStyle = m_properties.value(QStringLiteral("toolButtonStyle")).toInt();

    switch (toolButtonStyle) {
        case Qt::ToolButtonIconOnly:
        case Qt::ToolButtonTextOnly:
        case Qt::ToolButtonTextBesideIcon:
        case Qt::ToolButtonTextUnderIcon:
        case Qt::ToolButtonFollowStyle:
            opt->toolButtonStyle = (Qt::ToolButtonStyle)toolButtonStyle;
            break;
        default:
            opt->toolButtonStyle = Qt::ToolButtonFollowStyle;
    }

    const QFont font = m_font.isCopyOf(qApp->font()) ? qApp->font("QToolButton") : m_font;
    opt->font = font;
    opt->fontMetrics = QFontMetrics(font);
}

QSize PStyleToolButton::getContentSize(int, int)
{
    QStyleOptionToolButton* btn = qstyleoption_cast<QStyleOptionToolButton*>(m_styleoption);
    int w = 0;
    int h = 0;
    if (btn->toolButtonStyle != Qt::ToolButtonTextOnly) {
        QSize icon = btn->iconSize;
        if (btn->toolButtonStyle == Qt::ToolButtonIconOnly || !btn->icon.isNull()) {
            w = icon.width();
            h = icon.height();
        }
    }
    if (btn->toolButtonStyle != Qt::ToolButtonIconOnly) {
        QSize textSize = btn->fontMetrics.size(Qt::TextShowMnemonic, btn->text);
        textSize.setWidth(textSize.width() + btn->fontMetrics.horizontalAdvance(QLatin1Char(' ')) * 2);
        if (btn->toolButtonStyle == Qt::ToolButtonTextUnderIcon) {
            h += 4 + textSize.height();
            if (textSize.width() > w) {
                w = textSize.width();
            }
        } else if (btn->toolButtonStyle == Qt::ToolButtonTextBesideIcon) {
            w += 4 + textSize.width();
            if (textSize.height() > h) {
                h = textSize.height();
            }
        } else {  // TextOnly
            w = textSize.width();
            h = textSize.height();
        }
    }
    btn->rect.setSize(QSize(w, h));
    return PQuickStyleItem::style()->sizeFromContents(QStyle::CT_ToolButton, m_styleoption, QSize(w, h));
}

qreal PStyleToolButton::baselineOffset() const
{
    QRect r;
    if (const auto option = qstyleoption_cast<const QStyleOptionComplex*>(m_styleoption)) {
        r = PQuickStyleItem::style()->subControlRect(QStyle::CC_ToolButton, option, QStyle::SC_ToolButton);
    }
    return baselineOffsetFromRect(r);
}
void PStyleToolButton::doPaint(QPainter* painter)
{
    PQuickStyleItem::style()->drawComplexControl(QStyle::CC_ToolButton, qstyleoption_cast<QStyleOptionComplex*>(m_styleoption), painter);
}
