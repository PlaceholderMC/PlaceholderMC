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
#include <QPainter>

#include "FileSystem.h"
#include "Json.h"

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
    QImage preview(36, 36, QImage::Format_ARGB32);
    preview.fill(Qt::transparent);
    QPainter paint(&preview);

    // head
    paint.drawImage(4, 2, texture.copy(8, 8, 8, 8));
    paint.drawImage(4, 2, texture.copy(40, 8, 8, 8));
    // torso
    paint.drawImage(4, 10, texture.copy(20, 20, 8, 12));
    paint.drawImage(4, 10, texture.copy(20, 36, 8, 12));
    // right leg
    paint.drawImage(4, 22, texture.copy(4, 20, 4, 12));
    paint.drawImage(4, 22, texture.copy(4, 36, 4, 12));
    // left leg
    paint.drawImage(8, 22, texture.copy(4, 52, 4, 12));
    paint.drawImage(8, 22, texture.copy(20, 52, 4, 12));

    auto armWidth = slim ? 3 : 4;
    auto armPosX = slim ? 1 : 0;
    // right arm
    paint.drawImage(armPosX, 10, texture.copy(44, 20, armWidth, 12));
    paint.drawImage(armPosX, 10, texture.copy(44, 36, armWidth, 12));
    // left arm
    paint.drawImage(12, 10, texture.copy(36, 52, armWidth, 12));
    paint.drawImage(12, 10, texture.copy(52, 52, armWidth, 12));

    // back
    // head
    paint.drawImage(24, 2, texture.copy(24, 8, 8, 8));
    paint.drawImage(24, 2, texture.copy(56, 8, 8, 8));
    // torso
    paint.drawImage(24, 10, texture.copy(32, 20, 8, 12));
    paint.drawImage(24, 10, texture.copy(32, 36, 8, 12));
    // right leg
    paint.drawImage(24, 22, texture.copy(12, 20, 4, 12));
    paint.drawImage(24, 22, texture.copy(12, 36, 4, 12));
    // left leg
    paint.drawImage(28, 22, texture.copy(12, 52, 4, 12));
    paint.drawImage(28, 22, texture.copy(28, 52, 4, 12));

    // right arm
    paint.drawImage(armPosX + 20, 10, texture.copy(48 + armWidth, 20, armWidth, 12));
    paint.drawImage(armPosX + 20, 10, texture.copy(48 + armWidth, 36, armWidth, 12));
    // left arm
    paint.drawImage(32, 10, texture.copy(40 + armWidth, 52, armWidth, 12));
    paint.drawImage(32, 10, texture.copy(56 + armWidth, 52, armWidth, 12));

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
void SkinModel::setModel(Model model)
{
    m_model = model;
    m_preview = generatePreviews(m_texture, m_model == Model::SLIM);
}
