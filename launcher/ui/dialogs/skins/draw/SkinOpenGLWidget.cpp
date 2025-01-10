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

#include "ui/dialogs/skins/draw/SkinOpenGLWidget.h"

#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QVector2D>
#include <QVector3D>

#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/SkinManageDialog.h"
#include "ui/dialogs/skins/draw/BoxGeometry.h"
#include "ui/dialogs/skins/draw/Scene.h"

SkinOpenGLWidget::~SkinOpenGLWidget()
{
    // Make sure the context is current when deleting the texture
    // and the buffers.
    makeCurrent();
    delete m_scene;
    delete m_background;
    m_backgroundTexture->destroy();
    delete m_backgroundTexture;
    doneCurrent();
}

void SkinOpenGLWidget::mousePressEvent(QMouseEvent* e)
{
    // Save mouse press position
    m_mousePosition = QVector2D(e->pos());
    m_isMousePressed = true;
}
void SkinOpenGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isMousePressed) {
        int dx = event->x() - m_mousePosition.x();
        int dy = event->y() - m_mousePosition.y();

        // Update rotation angles based on mouse movement
        m_rotationX += dy;
        m_rotationY += dx;

        m_mousePosition = QVector2D(event->pos());
        update();  // Trigger a repaint
    }
}
void SkinOpenGLWidget::mouseReleaseEvent([[maybe_unused]] QMouseEvent* e)
{
    m_isMousePressed = false;
}

void SkinOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0, 0, 1, 1);

    initShaders();

    generateBackgroundTexture(32, 32, 1);

    QImage skin, cape;
    bool slim = false;
    if (auto p = dynamic_cast<SkinManageDialog*>(parent()); p) {
        if (auto s = p->getSelectedSkin()) {
            skin = s->getTexture();
            slim = s->getModel() == SkinModel::SLIM;
            cape = p->capes().value(s->getCapeId(), {});
        }
    }

    m_scene = new opengl::Scene(skin, slim, cape);
    m_background = opengl::BoxGeometry::Plane();
    glEnable(GL_TEXTURE_2D);
}

void SkinOpenGLWidget::initShaders()
{
    // Compile vertex shader
    if (!m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/vshader.glsl"))
        close();

    // Compile fragment shader
    if (!m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fshader.glsl"))
        close();

    // Link shader pipeline
    if (!m_program.link())
        close();

    // Bind shader pipeline for use
    if (!m_program.bind())
        close();
}

void SkinOpenGLWidget::resizeGL(int w, int h)
{
    // Calculate aspect ratio
    qreal aspect = qreal(w) / qreal(h ? h : 1);

    const qreal zNear = .1, zFar = 1000., fov = 45;

    // Reset projection
    m_projection.setToIdentity();

    // Set perspective projection
    m_projection.perspective(fov, aspect, zNear, zFar);
}

void SkinOpenGLWidget::paintGL()
{
    // Clear color and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth buffer
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Enable back face culling
    glEnable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_program.bind();

    renderBackground();
    // Calculate model view transformation
    QMatrix4x4 matrix;
    matrix.translate(0.0, 6.0, -50.);
    matrix.rotate(m_rotationX, 1.0f, 0.0f, 0.0f);
    matrix.rotate(m_rotationY, 0.0f, 1.0f, 0.0f);

    // Set modelview-projection matrix
    m_program.setUniformValue("mvp_matrix", m_projection * matrix);

    m_scene->draw(&m_program);
    m_program.release();
}

void SkinOpenGLWidget::updateScene(SkinModel* skin)
{
    if (skin && m_scene) {
        m_scene->setMode(skin->getModel() == SkinModel::SLIM);
        m_scene->setSkin(skin->getTexture());
        update();
    }
}
void SkinOpenGLWidget::updateCape(const QImage& cape)
{
    if (m_scene) {
        m_scene->setCapeVisible(!cape.isNull());
        m_scene->setCape(cape);
        update();
    }
}

QImage generateChessboardImage(int width, int height, int tileSize)
{
    QImage image(width, height, QImage::Format_RGB888);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool isWhite = ((x / tileSize) % 2) == ((y / tileSize) % 2);
            unsigned char color = isWhite ? 100 : 50;

            image.setPixelColor(x, y, QColor(color, color, color));
        }
    }
    return image;
}

void SkinOpenGLWidget::generateBackgroundTexture(int width, int height, int tileSize)
{
    m_backgroundTexture = new QOpenGLTexture(generateChessboardImage(width, height, tileSize));
    m_backgroundTexture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_backgroundTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
}

void SkinOpenGLWidget::renderBackground()
{
    glDepthMask(GL_FALSE);  // Disable depth buffer writing
    m_backgroundTexture->bind();
    QMatrix4x4 matrix;
    m_program.setUniformValue("mvp_matrix", matrix);
    m_program.setUniformValue("texture", 0);
    m_background->draw(&m_program);
    m_backgroundTexture->release();
    glDepthMask(GL_TRUE);  // Re-enable depth buffer writing
}
