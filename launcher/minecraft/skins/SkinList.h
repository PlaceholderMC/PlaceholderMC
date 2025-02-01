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

#pragma once

#include <QAbstractListModel>
#include <QDir>
#include <QFileSystemWatcher>

#include "QObjectPtr.h"
#include "SkinModel.h"
#include "minecraft/auth/MinecraftAccount.h"

class SkinList : public QAbstractListModel {
    Q_OBJECT
   public:
    explicit SkinList(QObject* parent, QString path, MinecraftAccountPtr acct);
    virtual ~SkinList() { save(); };

    int getSkinIndex(const QString& key) const;

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& idx, const QVariant& value, int role) override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    virtual QStringList mimeTypes() const override;
    virtual Qt::DropActions supportedDropActions() const override;
    virtual bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool deleteSkin(const QString& key, bool trash);

    void installSkins(const QStringList& iconFiles);
    QString installSkin(const QString& file, const QString& name = {});

    const SkinModel* skin(const QString& key) const;
    SkinModel* skin(const QString& key);

    void startWatching();
    void stopWatching();

    QString getDir() const { return m_dir.absolutePath(); }
    void save();
    int getSelectedAccountSkin();

    void updateSkin(SkinModel* s);

   private:
    // hide copy constructor
    SkinList(const SkinList&) = delete;
    // hide assign op
    SkinList& operator=(const SkinList&) = delete;

   protected slots:
    void directoryChanged(const QString& path);
    void fileChanged(const QString& path);
    bool update();

   private:
    shared_qobject_ptr<QFileSystemWatcher> m_watcher;
    bool m_isWatching;
    QVector<SkinModel> m_skinList;
    QDir m_dir;
    MinecraftAccountPtr m_acct;
};