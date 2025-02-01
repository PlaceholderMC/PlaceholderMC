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

#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLWindow>
#include <QVector2D>
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/BoxGeometry.h"
#include "ui/dialogs/skins/draw/Scene.h"

class SkinProvider {
   public:
    virtual ~SkinProvider() = default;
    virtual SkinModel* getSelectedSkin() = 0;
    virtual QHash<QString, QImage> capes() = 0;
};
class SkinOpenGLWindow : public QOpenGLWindow, protected QOpenGLFunctions {
    Q_OBJECT

   public:
    SkinOpenGLWindow(SkinProvider* parent, QColor color);
    virtual ~SkinOpenGLWindow();

    void updateScene(SkinModel* skin);
    void updateCape(const QImage& cape);

   protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void initShaders();

    void generateBackgroundTexture(int width, int height, int tileSize);
    void renderBackground();

   private:
    QOpenGLShaderProgram* m_program;
    opengl::Scene* m_scene = nullptr;

    QMatrix4x4 m_projection;

    QVector2D m_mousePosition;

    bool m_isMousePressed = false;
    float m_distance = 48;
    float m_yaw = 90;   // Horizontal rotation angle
    float m_pitch = 0;  // Vertical rotation angle

    opengl::BoxGeometry* m_background = nullptr;
    QOpenGLTexture* m_backgroundTexture = nullptr;
    QColor m_baseColor;
    SkinProvider* m_parent = nullptr;
};
