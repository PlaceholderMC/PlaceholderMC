// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
// https://code.qt.io/cgit/qt/qtbase.git/tree/examples/opengl/cube/vshader.glsl
#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform mat4 mvp_matrix;
uniform mat4 model_matrix;

attribute vec4 a_position;
attribute vec2 a_texcoord;

varying vec2 v_texcoord;

void main()
{
    // Calculate vertex position in screen space
    gl_Position = mvp_matrix * model_matrix * a_position;

    // Pass texture coordinate to fragment shader
    // Value will be automatically interpolated to fragments inside polygon faces
    v_texcoord = a_texcoord;
}
