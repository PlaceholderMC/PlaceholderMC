// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2025 Trial97 <alexandru.tripon97@gmail.com>
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
 */

#include "ui/themes/CatPainter.h"
#include <QPixmap>
#include "Application.h"

CatPainter::CatPainter(const QString& path, QObject* parent) : QObject(parent)
{
    // Attempt to load as a movie
    m_movie = new QMovie(path, QByteArray(), this);
    if (m_movie->isValid()) {
        // Start the animation if it's a valid movie file
        connect(m_movie, &QMovie::frameChanged, this, &CatPainter::updateFrame);
        m_movie->start();
    } else {
        // Otherwise, load it as a static image
        delete m_movie;
        m_movie = nullptr;

        m_image = QPixmap(path);
    }
}

CatPainter::~CatPainter()
{
    if (m_movie)
        delete m_movie;
}

void CatPainter::paint(QPainter* painter, const QRect& viewport)
{
    QPixmap frame = m_image;
    if (m_movie && m_movie->isValid()) {
        frame = m_movie->currentPixmap();
    }

    auto fit = APPLICATION->settings()->get("CatFit").toString();
    painter->setOpacity(APPLICATION->settings()->get("CatOpacity").toFloat() / 100);
    int widWidth = viewport.width();
    int widHeight = viewport.height();
    auto aspectMode = Qt::IgnoreAspectRatio;
    if (fit == "fill") {
        aspectMode = Qt::KeepAspectRatio;
    } else if (fit == "fit") {
        aspectMode = Qt::KeepAspectRatio;
        if (frame.width() < widWidth)
            widWidth = frame.width();
        if (frame.height() < widHeight)
            widHeight = frame.height();
    }
    auto pixmap = frame.scaled(widWidth, widHeight, aspectMode, Qt::SmoothTransformation);
    QRect rectOfPixmap = pixmap.rect();
    rectOfPixmap.moveBottomRight(viewport.bottomRight());
    painter->drawPixmap(rectOfPixmap.topLeft(), pixmap);
    painter->setOpacity(1.0);
};
