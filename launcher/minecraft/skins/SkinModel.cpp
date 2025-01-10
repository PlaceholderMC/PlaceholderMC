// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "SkinModel.h"
#include <QFileInfo>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObjectFormat>
#include <QPainter>

#include "FileSystem.h"
#include "Json.h"
#include "ui/dialogs/skins/draw/Scene.h"

QImage improveSkin(const QImage& skin)
{
    if (skin.size() == QSize(64, 32)) {  // old format
        QImage newSkin = QImage(QSize(64, 64), skin.format());
        newSkin.fill(Qt::transparent);
        QPainter p(&newSkin);
        p.drawImage(QPoint(0, 0), skin.copy(QRect(0, 0, 64, 32)));  // copy head

        auto leg = skin.copy(QRect(0, 16, 16, 16));
        p.drawImage(QPoint(16, 48), leg);  // copy leg

        auto arm = skin.copy(QRect(40, 16, 16, 16));
        p.drawImage(QPoint(32, 48), arm);  // copy arm
        return newSkin;
    }
    return skin;
}
QImage getSkin(const QString path)
{
    return improveSkin(QImage(path));
}

QImage generatePreviews(QImage texture, bool slim)
{
    QImage preview;

    // Set up OpenGL context and offscreen surface
    QOpenGLContext context;
    context.setFormat(QSurfaceFormat::defaultFormat());
    if (!context.create()) {
        qWarning() << "Failed to create OpenGL context";
        return preview;
    }

    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();

    // Make the OpenGL context current
    context.makeCurrent(&surface);

    QOpenGLFunctions* gl = context.functions();
    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

    QOpenGLShaderProgram program;
    // Compile vertex shader
    if (!program.addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/vshader.glsl")) {
        qWarning() << "Failed to create vertex";
        return preview;
    }
    // Compile fragment shader
    if (!program.addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/fshader.glsl")) {
        qWarning() << "Failed to create fragment";
        return preview;
    }
    // Link shader pipeline
    if (!program.link()) {
        qWarning() << "Failed to create link";
        return preview;
    }
    // Bind shader pipeline for use
    if (!program.bind()) {
        qWarning() << "Failed to create bind";
        return preview;
    }
    auto scene = new opengl::Scene(texture, slim, {});

    const qreal zNear = .1, zFar = 1000., fov = 45, aspect = 1;

    // Reset projection
    QMatrix4x4 m_projection;
    m_projection.setToIdentity();

    // Set perspective projection
    m_projection.perspective(fov, aspect, zNear, zFar);

    // Calculate model view transformation
    QMatrix4x4 matrix;
    matrix.translate(0.0, 6.0, -50.);

    // Create a framebuffer object for rendering
    QOpenGLFramebufferObject fbo(64, 64, fboFormat);
    fbo.bind();

    // Clear the framebuffer
    gl->glViewport(0, 0, 64, 64);
    gl->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // Clear color and depth buffer
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth buffer
    gl->glEnable(GL_DEPTH_TEST);
    gl->glDepthFunc(GL_LESS);

    // Enable back face culling
    gl->glEnable(GL_CULL_FACE);

    gl->glEnable(GL_BLEND);
    gl->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set modelview-projection matrix
    program.setUniformValue("mvp_matrix", m_projection * matrix);

    // scene->setMode(skin->getModel() == SkinModel::SLIM);
    // scene->setSkin(skin->getTexture());

    scene->draw(&program);

    // Read the framebuffer into a QImage
    preview = fbo.toImage();

    fbo.release();
    delete scene;
    context.doneCurrent();
    return preview;
}

SkinModel::SkinModel(QString path) : m_path(path), m_texture(getSkin(path)), m_model(Model::CLASSIC)
{
    m_preview = generatePreviews(m_texture, false);
}

SkinModel::SkinModel(QDir skinDir, QJsonObject obj)
    : m_capeId(Json::ensureString(obj, "capeId")), m_model(Model::CLASSIC), m_url(Json::ensureString(obj, "url"))
{
    auto name = Json::ensureString(obj, "name");

    if (auto model = Json::ensureString(obj, "model"); model == "SLIM") {
        m_model = Model::SLIM;
    }
    m_path = skinDir.absoluteFilePath(name) + ".png";
    m_texture = QImage(getSkin(m_path));
    m_preview = generatePreviews(m_texture, m_model == Model::SLIM);
}

QString SkinModel::name() const
{
    return QFileInfo(m_path).completeBaseName();
}

bool SkinModel::rename(QString newName)
{
    auto info = QFileInfo(m_path);
    m_path = FS::PathCombine(info.absolutePath(), newName + ".png");
    return FS::move(info.absoluteFilePath(), m_path);
}

QJsonObject SkinModel::toJSON() const
{
    QJsonObject obj;
    obj["name"] = name();
    obj["capeId"] = m_capeId;
    obj["url"] = m_url;
    obj["model"] = getModelString();
    return obj;
}

QString SkinModel::getModelString() const
{
    switch (m_model) {
        case CLASSIC:
            return "CLASSIC";
        case SLIM:
            return "SLIM";
    }
    return {};
}

bool SkinModel::isValid() const
{
    return !m_texture.isNull() && (m_texture.size().height() == 32 || m_texture.size().height() == 64) && m_texture.size().width() == 64;
}
void SkinModel::refresh()
{
    m_texture = getSkin(m_path);
    m_preview = generatePreviews(m_texture, m_model == Model::SLIM);
}
