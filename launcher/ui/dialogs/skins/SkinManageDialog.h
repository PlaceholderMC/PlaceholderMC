// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023-2024 Trial97 <alexandru.tripon97@gmail.com>
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

#include <QDialog>
#include <QItemSelection>
#include <QPixmap>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"

namespace Ui {
class SkinManageDialog;
}

class SkinManageDialog : public QDialog {
    Q_OBJECT
   public:
    explicit SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct);
    virtual ~SkinManageDialog();
    void resizeEvent(QResizeEvent* event) override;

    SkinModel* getSelectedSkin();
    QHash<QString, QImage> capes();

   public slots:
    void selectionChanged(QItemSelection, QItemSelection);
    void activated(QModelIndex);
    void delayed_scroll(QModelIndex);
    void on_openDirBtn_clicked();
    void on_fileBtn_clicked();
    void on_urlBtn_clicked();
    void on_userBtn_clicked();
    void accept() override;
    void on_capeCombo_currentIndexChanged(int index);
    void on_steveBtn_toggled(bool checked);
    void on_resetBtn_clicked();
    void show_context_menu(const QPoint& pos);
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void on_action_Rename_Skin_triggered(bool checked);
    void on_action_Delete_Skin_triggered(bool checked);

   private:
    void setupCapes();

    MinecraftAccountPtr m_acct;
    Ui::SkinManageDialog* m_ui;
    SkinList m_list;
    QString m_selectedSkinKey;
    QHash<QString, QImage> m_capes;
    QHash<QString, int> m_capesIdx;
};
