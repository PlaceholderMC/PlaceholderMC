// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
// https://code.qt.io/cgit/qt/qtbase.git/tree/examples/opengl/cube/fshader.glsl
#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform sampler2D texture;

varying vec2 v_texcoord;

void main()
{
    // Set fragment color from texture
    vec4 texColor = texture2D(texture, v_texcoord);
    if (texColor.a < 0.1) discard; // Optional: Discard fully transparent pixels
    gl_FragColor = texColor;
}
