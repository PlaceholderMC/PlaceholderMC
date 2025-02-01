// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2024 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ui/dialogs/skins/draw/BoxGeometry.h"

#include <QOpenGLTexture>
namespace opengl {
class Scene {
   public:
    Scene(const QImage& skin, bool slim, const QImage& cape);
    virtual ~Scene();

    void draw(QOpenGLShaderProgram* program);
    void setSkin(const QImage& skin);
    void setCape(const QImage& cape);
    void setMode(bool slim);
    void setCapeVisible(bool visible);

   private:
    QVector<BoxGeometry*> m_staticComponents;
    QVector<BoxGeometry*> m_normalArms;
    QVector<BoxGeometry*> m_slimArms;
    BoxGeometry* m_cape = nullptr;
    QOpenGLTexture* m_skinTexture = nullptr;
    QOpenGLTexture* m_capeTexture = nullptr;
    bool m_slim = false;
    bool m_capeVisible = false;
};
}  // namespace opengl