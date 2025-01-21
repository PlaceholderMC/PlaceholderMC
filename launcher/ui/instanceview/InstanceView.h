// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#pragma once

#include <QCache>
#include <QLineEdit>
#include <QListView>
#include <QScrollBar>
#include <functional>
#include "VisualGroup.h"
#include "ui/themes/CatPainter.h"

struct InstanceViewRoles {
    enum { GroupRole = Qt::UserRole, ProgressValueRole, ProgressMaximumRole };
};

class InstanceView : public QAbstractItemView {
    Q_OBJECT

   public:
    InstanceView(QWidget* parent = 0);
    ~InstanceView();

    void setModel(QAbstractItemModel* model) override;

    using visibilityFunction = std::function<bool(const QString&)>;
    void setSourceOfGroupCollapseStatus(visibilityFunction f) { m_fVisibility = f; }

    /// return geometry rectangle occupied by the specified model item
    QRect geometryRect(const QModelIndex& index) const;
    /// return visual rectangle occupied by the specified model item
    virtual QRect visualRect(const QModelIndex& index) const override;
    /// get the model index at the specified visual point
    virtual QModelIndex indexAt(const QPoint& point) const override;
    QString groupNameAt(const QPoint& point);
    void setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags commands) override;

    virtual int horizontalOffset() const override;
    virtual int verticalOffset() const override;
    virtual void scrollContentsBy(int dx, int dy) override;
    virtual void scrollTo(const QModelIndex& index, ScrollHint hint = EnsureVisible) override;

    virtual QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;

    virtual QRegion visualRegionForSelection(const QItemSelection& selection) const override;

    int spacing() const { return m_spacing; };
    void setPaintCat(bool visible);

   public slots:
    virtual void updateGeometries() override;

   protected slots:
    virtual void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) override;
    virtual void rowsInserted(const QModelIndex& parent, int start, int end) override;
    virtual void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;
    void modelReset();
    void rowsRemoved();
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

   signals:
    void droppedURLs(QList<QUrl> urls);
    void groupStateChanged(QString group, bool collapsed);

   protected:
    bool isIndexHidden(const QModelIndex& index) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void startDrag(Qt::DropActions supportedActions) override;

    void updateScrollbar();

   private:
    friend struct VisualGroup;
    QList<VisualGroup*> m_groups;

    visibilityFunction m_fVisibility;

    // geometry
    int m_leftMargin = 5;
    int m_rightMargin = 5;
    int m_bottomMargin = 5;
    int m_categoryMargin = 5;
    int m_spacing = 5;
    int m_itemWidth = 100;
    int m_currentItemsPerRow = -1;
    int m_currentCursorColumn = -1;
    mutable QCache<int, QRect> m_geometryCache;
    CatPainter* m_cat;

    // point where the currently active mouse action started in geometry coordinates
    QPoint m_pressedPosition;
    QPersistentModelIndex m_pressedIndex;
    bool m_pressedAlreadySelected;
    VisualGroup* m_pressedCategory;
    QItemSelectionModel::SelectionFlag m_ctrlDragSelectionFlag;
    QPoint m_lastDragPosition;

    VisualGroup* category(const QModelIndex& index) const;
    VisualGroup* category(const QString& cat) const;
    VisualGroup* categoryAt(const QPoint& pos, VisualGroup::HitResults& result) const;

    int itemsPerRow() const { return m_currentItemsPerRow; };
    int contentWidth() const;

   private: /* methods */
    int itemWidth() const;
    int calculateItemsPerRow() const;
    int verticalScrollToValue(const QModelIndex& index, const QRect& rect, QListView::ScrollHint hint) const;
    QPixmap renderToPixmap(const QModelIndexList& indices, QRect* r) const;
    QList<std::pair<QRect, QModelIndex>> draggablePaintPairs(const QModelIndexList& indices, QRect* r) const;

    bool isDragEventAccepted(QDropEvent* event);

    std::pair<VisualGroup*, VisualGroup::HitResults> rowDropPos(const QPoint& pos);

    QPoint offset() const;
};
