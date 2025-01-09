#pragma once

#include <QColor>
#include <QPainter>
#include <QSize>

// https://stackoverflow.com/questions/21400254/how-to-draw-a-qr-code-with-qt-in-native-c-c
void paintQR(QPainter& painter, const QSize sz, const QString& data, QColor fg);
