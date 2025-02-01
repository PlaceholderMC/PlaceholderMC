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

#include "BoxGeometry.h"

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector>

struct VertexData {
    QVector4D position;
    QVector2D texCoord;
    VertexData(const QVector4D& pos, const QVector2D& tex) : position(pos), texCoord(tex) {}
};

// For cube we would need only 8 vertices but we have to
// duplicate vertex for each face because texture coordinate
// is different.
static const QVector<QVector4D> vertices = {
    // Vertex data for face 0
    QVector4D(-0.5f, -0.5f, 0.5f, 1.0f),  // v0
    QVector4D(0.5f, -0.5f, 0.5f, 1.0f),   // v1
    QVector4D(-0.5f, 0.5f, 0.5f, 1.0f),   // v2
    QVector4D(0.5f, 0.5f, 0.5f, 1.0f),    // v3
    // Vertex data for face 1
    QVector4D(0.5f, -0.5f, 0.5f, 1.0f),   // v4
    QVector4D(0.5f, -0.5f, -0.5f, 1.0f),  // v5
    QVector4D(0.5f, 0.5f, 0.5f, 1.0f),    // v6
    QVector4D(0.5f, 0.5f, -0.5f, 1.0f),   // v7

    // Vertex data for face 2
    QVector4D(0.5f, -0.5f, -0.5f, 1.0f),   // v8
    QVector4D(-0.5f, -0.5f, -0.5f, 1.0f),  // v9
    QVector4D(0.5f, 0.5f, -0.5f, 1.0f),    // v10
    QVector4D(-0.5f, 0.5f, -0.5f, 1.0f),   // v11

    // Vertex data for face 3
    QVector4D(-0.5f, -0.5f, -0.5f, 1.0f),  // v12
    QVector4D(-0.5f, -0.5f, 0.5f, 1.0f),   // v13
    QVector4D(-0.5f, 0.5f, -0.5f, 1.0f),   // v14
    QVector4D(-0.5f, 0.5f, 0.5f, 1.0f),    // v15

    // Vertex data for face 4
    QVector4D(-0.5f, -0.5f, -0.5f, 1.0f),  // v16
    QVector4D(0.5f, -0.5f, -0.5f, 1.0f),   // v17
    QVector4D(-0.5f, -0.5f, 0.5f, 1.0f),   // v18
    QVector4D(0.5f, -0.5f, 0.5f, 1.0f),    // v19

    // Vertex data for face 5
    QVector4D(-0.5f, 0.5f, 0.5f, 1.0f),   // v20
    QVector4D(0.5f, 0.5f, 0.5f, 1.0f),    // v21
    QVector4D(-0.5f, 0.5f, -0.5f, 1.0f),  // v22
    QVector4D(0.5f, 0.5f, -0.5f, 1.0f),   // v23
};

// Indices for drawing cube faces using triangle strips.
// Triangle strips can be connected by duplicating indices
// between the strips. If connecting strips have opposite
// vertex order then last index of the first strip and first
// index of the second strip needs to be duplicated. If
// connecting strips have same vertex order then only last
// index of the first strip needs to be duplicated.
static const QVector<GLushort> indices = {
    0,  1,  2,  3,  3,       // Face 0 - triangle strip ( v0,  v1,  v2,  v3)
    4,  4,  5,  6,  7,  7,   // Face 1 - triangle strip ( v4,  v5,  v6,  v7)
    8,  8,  9,  10, 11, 11,  // Face 2 - triangle strip ( v8,  v9, v10, v11)
    12, 12, 13, 14, 15, 15,  // Face 3 - triangle strip (v12, v13, v14, v15)
    16, 16, 17, 18, 19, 19,  // Face 4 - triangle strip (v16, v17, v18, v19)
    20, 20, 21, 22, 23       // Face 5 - triangle strip (v20, v21, v22, v23)
};

static const QVector<VertexData> planeVertices = {
    { QVector4D(-1.0f, -1.0f, -0.5f, 1.0f), QVector2D(0.0f, 0.0f) },  // Bottom-left
    { QVector4D(1.0f, -1.0f, -0.5f, 1.0f), QVector2D(1.0f, 0.0f) },   // Bottom-right
    { QVector4D(-1.0f, 1.0f, -0.5f, 1.0f), QVector2D(0.0f, 1.0f) },   // Top-left
    { QVector4D(1.0f, 1.0f, -0.5f, 1.0f), QVector2D(1.0f, 1.0f) },    // Top-right
};
static const QVector<GLushort> planeIndices = {
    0, 1, 2, 3, 3  // Face 0 - triangle strip ( v0,  v1,  v2,  v3)
};

QVector<QVector4D> transformVectors(const QMatrix4x4& matrix, const QVector<QVector4D>& vectors)
{
    QVector<QVector4D> transformedVectors;
    transformedVectors.reserve(vectors.size());

    for (const QVector4D& vec : vectors) {
        if (!matrix.isIdentity()) {
            transformedVectors.append(matrix * vec);
        } else {
            transformedVectors.append(vec);
        }
    }

    return transformedVectors;
}

// Function to calculate UV coordinates
// this is pure magic (if something is wrong with textures this is at fault)
QVector<QVector2D> getCubeUVs(float u, float v, float width, float height, float depth, float textureWidth, float textureHeight)
{
    auto toFaceVertices = [textureHeight, textureWidth](float x1, float y1, float x2, float y2) -> QVector<QVector2D> {
        return {
            QVector2D(x1 / textureWidth, 1.0 - y2 / textureHeight),
            QVector2D(x2 / textureWidth, 1.0 - y2 / textureHeight),
            QVector2D(x2 / textureWidth, 1.0 - y1 / textureHeight),
            QVector2D(x1 / textureWidth, 1.0 - y1 / textureHeight),
        };
    };

    auto top = toFaceVertices(u + depth, v, u + width + depth, v + depth);
    auto bottom = toFaceVertices(u + width + depth, v, u + width * 2 + depth, v + depth);
    auto left = toFaceVertices(u, v + depth, u + depth, v + depth + height);
    auto front = toFaceVertices(u + depth, v + depth, u + width + depth, v + depth + height);
    auto right = toFaceVertices(u + width + depth, v + depth, u + width + depth * 2, v + height + depth);
    auto back = toFaceVertices(u + width + depth * 2, v + depth, u + width * 2 + depth * 2, v + height + depth);

    auto uvRight = {
        right[0],
        right[1],
        right[3],
        right[2],
    };
    auto uvLeft = {
        left[0],
        left[1],
        left[3],
        left[2],
    };
    auto uvTop = {
        top[0],
        top[1],
        top[3],
        top[2],
    };
    auto uvBottom = {
        bottom[3],
        bottom[2],
        bottom[0],
        bottom[1],
    };
    auto uvFront = {
        front[0],
        front[1],
        front[3],
        front[2],
    };
    auto uvBack = {
        back[0],
        back[1],
        back[3],
        back[2],
    };
    // Create a new array to hold the modified UV data
    QVector<QVector2D> uvData;
    uvData.reserve(24);

    // Iterate over the arrays and copy the data to newUVData
    for (const auto& uvArray : { uvFront, uvRight, uvBack, uvLeft, uvBottom, uvTop }) {
        uvData.append(uvArray);
    }

    return uvData;
}

namespace opengl {
BoxGeometry::BoxGeometry(QVector3D size, QVector3D position) : m_indexBuf(QOpenGLBuffer::IndexBuffer), m_size(size), m_position(position)
{
    initializeOpenGLFunctions();

    // Generate 2 VBOs
    m_vertexBuf.create();
    m_indexBuf.create();
}

BoxGeometry::BoxGeometry(QVector3D size, QVector3D position, QPoint uv, QVector3D textureDim, QSize textureSize)
    : BoxGeometry(size, position)
{
    initGeometry(uv.x(), uv.y(), textureDim.x(), textureDim.y(), textureDim.z(), textureSize.width(), textureSize.height());
}

BoxGeometry::~BoxGeometry()
{
    m_vertexBuf.destroy();
    m_indexBuf.destroy();
}

void BoxGeometry::draw(QOpenGLShaderProgram* program)
{
    // Tell OpenGL which VBOs to use
    program->setUniformValue("model_matrix", m_matrix);
    m_vertexBuf.bind();
    m_indexBuf.bind();

    // Offset for position
    quintptr offset = 0;

    // Tell OpenGL programmable pipeline how to locate vertex position data
    int vertexLocation = program->attributeLocation("a_position");
    program->enableAttributeArray(vertexLocation);
    program->setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 4, sizeof(VertexData));

    // Offset for texture coordinate
    offset += sizeof(QVector4D);
    // Tell OpenGL programmable pipeline how to locate vertex texture coordinate data
    int texcoordLocation = program->attributeLocation("a_texcoord");
    program->enableAttributeArray(texcoordLocation);
    program->setAttributeBuffer(texcoordLocation, GL_FLOAT, offset, 2, sizeof(VertexData));

    // Draw cube geometry using indices from VBO 1
    glDrawElements(GL_TRIANGLE_STRIP, m_indecesCount, GL_UNSIGNED_SHORT, nullptr);
}

void BoxGeometry::initGeometry(float u, float v, float width, float height, float depth, float textureWidth, float textureHeight)
{
    auto textureCord = getCubeUVs(u, v, width, height, depth, textureWidth, textureHeight);

    // this should not be needed to be done on each render for most of the objects
    QMatrix4x4 transformation;
    transformation.translate(m_position);
    transformation.scale(m_size);
    auto positions = transformVectors(transformation, vertices);

    QVector<VertexData> verticesData;
    verticesData.reserve(positions.size());  // Reserve space for efficiency

    for (int i = 0; i < positions.size(); ++i) {
        verticesData.append(VertexData(positions[i], textureCord[i]));
    }

    // Transfer vertex data to VBO 0
    m_vertexBuf.bind();
    m_vertexBuf.allocate(verticesData.constData(), verticesData.size() * sizeof(VertexData));

    // Transfer index data to VBO 1
    m_indexBuf.bind();
    m_indexBuf.allocate(indices.constData(), indices.size() * sizeof(GLushort));
    m_indecesCount = indices.size();
}

void BoxGeometry::rotate(float angle, const QVector3D& vector)
{
    m_matrix.rotate(angle, vector);
}

BoxGeometry* BoxGeometry::Plane()
{
    auto b = new BoxGeometry(QVector3D(), QVector3D());

    // Transfer vertex data to VBO 0
    b->m_vertexBuf.bind();
    b->m_vertexBuf.allocate(planeVertices.constData(), planeVertices.size() * sizeof(VertexData));

    // Transfer index data to VBO 1
    b->m_indexBuf.bind();
    b->m_indexBuf.allocate(planeIndices.constData(), planeIndices.size() * sizeof(GLushort));
    b->m_indecesCount = planeIndices.size();

    return b;
}
}  // namespace opengl