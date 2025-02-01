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
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVector3D>

namespace opengl {
class BoxGeometry : protected QOpenGLFunctions {
   public:
    BoxGeometry(QVector3D size, QVector3D position);
    BoxGeometry(QVector3D size, QVector3D position, QPoint uv, QVector3D textureDim, QSize textureSize = { 64, 64 });
    static BoxGeometry* Plane();
    virtual ~BoxGeometry();

    void draw(QOpenGLShaderProgram* program);

    void initGeometry(float u, float v, float width, float height, float depth, float textureWidth = 64, float textureHeight = 64);
    void rotate(float angle, const QVector3D& vector);

   private:
    QOpenGLBuffer m_vertexBuf;
    QOpenGLBuffer m_indexBuf;
    QVector3D m_size;
    QVector3D m_position;
    QMatrix4x4 m_matrix;
    GLsizei m_indecesCount;
};
}  // namespace opengl
