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

#pragma once

#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QString>

class CatPainter : public QObject {
    Q_OBJECT
   public:
    CatPainter(const QString& path, QObject* parent = nullptr);
    virtual ~CatPainter();
    void paint(QPainter*, const QRect&);

   signals:
    void updateFrame();

   private:
    QMovie* m_movie = nullptr;
    QPixmap m_image;
};
