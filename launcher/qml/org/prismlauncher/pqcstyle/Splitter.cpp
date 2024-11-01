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

#include "Splitter.h"
#include "PQuickStyleItem.h"

#include <QApplication>
#include <QSplitter>
#include <QStyle>

PStyleSplitter::PStyleSplitter(QQuickItem* parent) : PQuickStyleItem(parent)
{
    m_type = QStringLiteral("splitter");
}

QSize PStyleSplitter::getContentSize(int, int)
{
    const auto hw = PQuickStyleItem::style()->pixelMetric(QStyle::PM_SplitterWidth);
    return PQuickStyleItem::style()->sizeFromContents(QStyle::CT_Splitter, m_styleoption, QSize(hw, hw));
}

void PStyleSplitter::doPaint(QPainter* painter)
{
    PQuickStyleItem::style()->drawControl(QStyle::CE_Splitter, m_styleoption, painter);
}