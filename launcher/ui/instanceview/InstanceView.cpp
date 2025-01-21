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

#include "InstanceView.h"

#include <QAccessible>
#include <QApplication>
#include <QCache>
#include <QDrag>
#include <QFont>
#include <QListView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QtMath>

#include "VisualGroup.h"
#include "ui/themes/ThemeManager.h"

#include <Application.h>
#include <InstanceList.h>

template <typename T>
bool listsIntersect(const QList<T>& l1, const QList<T> t2)
{
    for (auto& item : l1) {
        if (t2.contains(item)) {
            return true;
        }
    }
    return false;
}

InstanceView::InstanceView(QWidget* parent) : QAbstractItemView(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setAcceptDrops(true);
    setAutoScroll(true);
    setPaintCat(APPLICATION->settings()->get("TheCat").toBool());
}

InstanceView::~InstanceView()
{
    qDeleteAll(m_groups);
    m_groups.clear();
}

void InstanceView::setModel(QAbstractItemModel* model)
{
    QAbstractItemView::setModel(model);
    connect(model, &QAbstractItemModel::modelReset, this, &InstanceView::modelReset);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &InstanceView::rowsRemoved);
}

void InstanceView::dataChanged([[maybe_unused]] const QModelIndex& topLeft,
                               [[maybe_unused]] const QModelIndex& bottomRight,
                               [[maybe_unused]] const QVector<int>& roles)
{
    scheduleDelayedItemsLayout();
}
void InstanceView::rowsInserted([[maybe_unused]] const QModelIndex& parent, [[maybe_unused]] int start, [[maybe_unused]] int end)
{
    scheduleDelayedItemsLayout();
}

void InstanceView::rowsAboutToBeRemoved([[maybe_unused]] const QModelIndex& parent, [[maybe_unused]] int start, [[maybe_unused]] int end)
{
    scheduleDelayedItemsLayout();
}

void InstanceView::modelReset()
{
    scheduleDelayedItemsLayout();
}

void InstanceView::rowsRemoved()
{
    scheduleDelayedItemsLayout();
}

void InstanceView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QAbstractItemView::currentChanged(current, previous);
    // TODO: for accessibility support, implement+register a factory, steal QAccessibleTable from Qt and return an instance of it for
    // InstanceView.
#ifndef QT_NO_ACCESSIBILITY
    if (QAccessible::isActive() && current.isValid()) {
        QAccessibleEvent event(this, QAccessible::Focus);
        event.setChild(current.row());
        QAccessible::updateAccessibility(&event);
    }
#endif /* !QT_NO_ACCESSIBILITY */
}

class LocaleString : public QString {
   public:
    LocaleString(const char* s) : QString(s) {}
    LocaleString(const QString& s) : QString(s) {}
};

inline bool operator<(const LocaleString& lhs, const LocaleString& rhs)
{
    return (QString::localeAwareCompare(lhs, rhs) < 0);
}

void InstanceView::updateScrollbar()
{
    int previousScroll = verticalScrollBar()->value();
    if (m_groups.isEmpty()) {
        verticalScrollBar()->setRange(0, 0);
    } else {
        int totalHeight = 0;
        // top margin
        totalHeight += m_categoryMargin;
        int itemScroll = 0;
        for (auto category : m_groups) {
            category->m_verticalPosition = totalHeight;
            totalHeight += category->totalHeight() + m_categoryMargin;
            if (!itemScroll && category->totalHeight() != 0) {
                itemScroll = category->contentHeight() / category->numRows();
            }
        }
        // do not divide by zero
        if (itemScroll == 0)
            itemScroll = 64;

        totalHeight += m_bottomMargin;
        verticalScrollBar()->setSingleStep(itemScroll);
        const int rowsPerPage = qMax(viewport()->height() / itemScroll, 1);
        verticalScrollBar()->setPageStep(rowsPerPage * itemScroll);

        verticalScrollBar()->setRange(0, totalHeight - height());
    }

    verticalScrollBar()->setValue(qMin(previousScroll, verticalScrollBar()->maximum()));
}

void InstanceView::updateGeometries()
{
    geometryCache.clear();

    QMap<LocaleString, VisualGroup*> cats;

    for (int i = 0; i < model()->rowCount(); ++i) {
        const QString groupName = model()->index(i, 0).data(InstanceViewRoles::GroupRole).toString();
        if (!cats.contains(groupName)) {
            VisualGroup* old = this->category(groupName);
            if (old) {
                auto cat = new VisualGroup(old);
                cats.insert(groupName, cat);
                cat->update();
            } else {
                auto cat = new VisualGroup(groupName, this);
                if (fVisibility) {
                    cat->collapsed = fVisibility(groupName);
                }
                cats.insert(groupName, cat);
                cat->update();
            }
        }
    }

    qDeleteAll(m_groups);
    m_groups = cats.values();
    updateScrollbar();
    viewport()->update();
}

bool InstanceView::isIndexHidden(const QModelIndex& index) const
{
    VisualGroup* cat = category(index);
    if (cat) {
        return cat->collapsed;
    } else {
        return false;
    }
}

VisualGroup* InstanceView::category(const QModelIndex& index) const
{
    return category(index.data(InstanceViewRoles::GroupRole).toString());
}

VisualGroup* InstanceView::category(const QString& cat) const
{
    for (auto group : m_groups) {
        if (group->text == cat) {
            return group;
        }
    }
    return nullptr;
}

VisualGroup* InstanceView::categoryAt(const QPoint& pos, VisualGroup::HitResults& result) const
{
    for (auto group : m_groups) {
        result = group->hitScan(pos);
        if (result != VisualGroup::NoHit) {
            return group;
        }
    }
    result = VisualGroup::NoHit;
    return nullptr;
}

QString InstanceView::groupNameAt(const QPoint& point)
{
    executeDelayedItemsLayout();

    VisualGroup::HitResults hitResult;
    auto group = categoryAt(point + offset(), hitResult);
    if (group && (hitResult & (VisualGroup::HeaderHit | VisualGroup::BodyHit))) {
        return group->text;
    }
    return QString();
}

int InstanceView::calculateItemsPerRow() const
{
    return qFloor((qreal)(contentWidth()) / (qreal)(itemWidth() + m_spacing));
}

int InstanceView::contentWidth() const
{
    return width() - m_leftMargin - m_rightMargin;
}

int InstanceView::itemWidth() const
{
    return m_itemWidth;
}

void InstanceView::mousePressEvent(QMouseEvent* event)
{
    executeDelayedItemsLayout();

    QPoint visualPos = event->pos();
    QPoint geometryPos = event->pos() + offset();

    QPersistentModelIndex index = indexAt(visualPos);

    m_pressedIndex = index;
    m_pressedAlreadySelected = selectionModel()->isSelected(m_pressedIndex);
    m_pressedPosition = geometryPos;

    if (event->button() == Qt::LeftButton) {
        VisualGroup::HitResults hitResult;
        m_pressedCategory = categoryAt(geometryPos, hitResult);
        if (m_pressedCategory && hitResult & VisualGroup::CheckboxHit) {
            setState(m_pressedCategory->collapsed ? ExpandingState : CollapsingState);
            event->accept();
            return;
        }
    }

    if (index.isValid() && (index.flags() & Qt::ItemIsEnabled)) {
        if (index != currentIndex()) {
            // FIXME: better!
            m_currentCursorColumn = -1;
        }
        // we disable scrollTo for mouse press so the item doesn't change position
        // when the user is interacting with it (ie. clicking on it)
        bool autoScroll = hasAutoScroll();
        setAutoScroll(false);
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);

        setAutoScroll(autoScroll);
        QRect rect(visualPos, visualPos);
        setSelection(rect, QItemSelectionModel::ClearAndSelect);

        // signal handlers may change the model
        emit pressed(index);
    } else {
        // Forces a finalize() even if mouse is pressed, but not on a item
        selectionModel()->select(QModelIndex(), QItemSelectionModel::Select);
    }
}

void InstanceView::mouseMoveEvent(QMouseEvent* event)
{
    executeDelayedItemsLayout();

    QPoint topLeft;
    QPoint visualPos = event->pos();
    QPoint geometryPos = event->pos() + offset();

    if (state() == ExpandingState || state() == CollapsingState) {
        return;
    }

    if (state() == DraggingState) {
        topLeft = m_pressedPosition - offset();
        if ((topLeft - event->pos()).manhattanLength() > QApplication::startDragDistance()) {
            m_pressedIndex = QModelIndex();
            startDrag(model()->supportedDragActions());
            setState(NoState);
            stopAutoScroll();
        }
        return;
    }

    if (selectionMode() != SingleSelection) {
        topLeft = m_pressedPosition - offset();
    } else {
        topLeft = geometryPos;
    }

    if (m_pressedIndex.isValid() && (state() != DragSelectingState) && (event->buttons() != Qt::NoButton) && !selectedIndexes().isEmpty()) {
        setState(DraggingState);
        return;
    }

    if ((event->buttons() & Qt::LeftButton) && selectionModel()) {
        setState(DragSelectingState);

        setSelection(QRect(visualPos, visualPos), QItemSelectionModel::ClearAndSelect);
        QModelIndex index = indexAt(visualPos);

        // set at the end because it might scroll the view
        if (index.isValid() && (index != selectionModel()->currentIndex()) && (index.flags() & Qt::ItemIsEnabled)) {
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        }
    }
}

void InstanceView::mouseReleaseEvent(QMouseEvent* event)
{
    executeDelayedItemsLayout();

    QPoint visualPos = event->pos();
    QPoint geometryPos = event->pos() + offset();
    QPersistentModelIndex index = indexAt(visualPos);

    VisualGroup::HitResults hitResult;

    if (event->button() == Qt::LeftButton && m_pressedCategory != nullptr && m_pressedCategory == categoryAt(geometryPos, hitResult)) {
        if (state() == ExpandingState) {
            m_pressedCategory->collapsed = false;
            emit groupStateChanged(m_pressedCategory->text, false);

            updateGeometries();
            viewport()->update();
            event->accept();
            m_pressedCategory = nullptr;
            setState(NoState);
            return;
        } else if (state() == CollapsingState) {
            m_pressedCategory->collapsed = true;
            emit groupStateChanged(m_pressedCategory->text, true);

            updateGeometries();
            viewport()->update();
            event->accept();
            m_pressedCategory = nullptr;
            setState(NoState);
            return;
        }
    }

    m_ctrlDragSelectionFlag = QItemSelectionModel::NoUpdate;

    setState(NoState);

    if (index == m_pressedIndex && index.isValid()) {
        if (event->button() == Qt::LeftButton) {
            emit clicked(index);
        }
        QStyleOptionViewItem option;
        initViewItemOption(&option);
        if (m_pressedAlreadySelected) {
            option.state |= QStyle::State_Selected;
        }
        if ((model()->flags(index) & Qt::ItemIsEnabled) &&
            style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, &option, this)) {
            emit activated(index);
        }
    }
}

void InstanceView::mouseDoubleClickEvent(QMouseEvent* event)
{
    executeDelayedItemsLayout();

    QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || !(index.flags() & Qt::ItemIsEnabled) || (m_pressedIndex != index)) {
        QMouseEvent me(QEvent::MouseButtonPress, event->localPos(), event->windowPos(), event->screenPos(), event->button(),
                       event->buttons(), event->modifiers());
        mousePressEvent(&me);
        return;
    }
    // signal handlers may change the model
    QPersistentModelIndex persistent = index;
    emit doubleClicked(persistent);

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    if ((model()->flags(index) & Qt::ItemIsEnabled) && !style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, &option, this)) {
        emit activated(index);
    }
}

void InstanceView::setPaintCat(bool visible)
{
    m_catVisible = visible;
    if (visible)
        m_catPixmap.load(APPLICATION->themeManager()->getCatPack());
    else
        m_catPixmap = QPixmap();
}

void InstanceView::paintEvent([[maybe_unused]] QPaintEvent* event)
{
    executeDelayedItemsLayout();

    QPainter painter(this->viewport());

    if (m_catVisible) {
        painter.setOpacity(APPLICATION->settings()->get("CatOpacity").toFloat() / 100);
        int widWidth = this->viewport()->width();
        int widHeight = this->viewport()->height();
        if (m_catPixmap.width() < widWidth)
            widWidth = m_catPixmap.width();
        if (m_catPixmap.height() < widHeight)
            widHeight = m_catPixmap.height();
        auto pixmap = m_catPixmap.scaled(widWidth, widHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QRect rectOfPixmap = pixmap.rect();
        rectOfPixmap.moveBottomRight(this->viewport()->rect().bottomRight());
        painter.drawPixmap(rectOfPixmap.topLeft(), pixmap);
        painter.setOpacity(1.0);
    }

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.widget = this;

    if (model()->rowCount() == 0) {
        painter.save();
        QString emptyString = tr("Welcome!") + "\n" + tr("Click \"Add Instance\" to get started.");

        // calculate the rect for the overlay
        painter.setRenderHint(QPainter::Antialiasing, true);
        QFont font("sans", 20);
        font.setBold(true);

        QRect bounds = viewport()->geometry();
        bounds.moveTop(0);
        auto innerBounds = bounds;
        innerBounds.adjust(10, 10, -10, -10);

        QColor background = QApplication::palette().color(QPalette::WindowText);
        QColor foreground = QApplication::palette().color(QPalette::Base);
        foreground.setAlpha(190);
        painter.setFont(font);
        auto fontMetrics = painter.fontMetrics();
        auto textRect = fontMetrics.boundingRect(innerBounds, Qt::AlignHCenter | Qt::TextWordWrap, emptyString);
        textRect.moveCenter(bounds.center());

        auto wrapRect = textRect;
        wrapRect.adjust(-10, -10, 10, 10);

        // check if we are allowed to draw in our area
        if (!event->rect().intersects(wrapRect)) {
            return;
        }

        painter.setBrush(QBrush(background));
        painter.setPen(foreground);
        painter.drawRoundedRect(wrapRect, 5.0, 5.0);

        painter.setPen(foreground);
        painter.setFont(font);
        painter.drawText(textRect, Qt::AlignHCenter | Qt::TextWordWrap, emptyString);

        painter.restore();
        return;
    }

    int wpWidth = viewport()->width();
    option.rect.setWidth(wpWidth);
    for (int i = 0; i < m_groups.size(); ++i) {
        VisualGroup* category = m_groups.at(i);
        int y = category->verticalPosition();
        y -= verticalOffset();
        QRect backup = option.rect;
        int height = category->totalHeight();
        option.rect.setTop(y);
        option.rect.setHeight(height);
        option.rect.setLeft(m_leftMargin);
        option.rect.setRight(wpWidth - m_rightMargin);
        category->drawHeader(&painter, option);
        y += category->totalHeight() + m_categoryMargin;
        option.rect = backup;
    }

    for (int i = 0; i < model()->rowCount(); ++i) {
        const QModelIndex index = model()->index(i, 0);
        if (isIndexHidden(index)) {
            continue;
        }
        Qt::ItemFlags flags = index.flags();
        option.rect = visualRect(index);
        option.features |= QStyleOptionViewItem::WrapText;
        if (flags & Qt::ItemIsSelectable && selectionModel()->isSelected(index)) {
            option.state |= selectionModel()->isSelected(index) ? QStyle::State_Selected : QStyle::State_None;
        } else {
            option.state &= ~QStyle::State_Selected;
        }
        option.state |= (index == currentIndex()) ? QStyle::State_HasFocus : QStyle::State_None;
        if (!(flags & Qt::ItemIsEnabled)) {
            option.state &= ~QStyle::State_Enabled;
        }
        itemDelegate()->paint(&painter, option, index);
    }

    /*
     * Drop indicators for manual reordering...
     */
#if 0
    if (!m_lastDragPosition.isNull())
    {
        std::pair<VisualGroup *, VisualGroup::HitResults> pair = rowDropPos(m_lastDragPosition);
        VisualGroup *category = pair.first;
        VisualGroup::HitResults row = pair.second;
        if (category)
        {
            int internalRow = row - category->firstItemIndex;
            QLine line;
            if (internalRow >= category->numItems())
            {
                QRect toTheRightOfRect = visualRect(category->lastItem());
                line = QLine(toTheRightOfRect.topRight(), toTheRightOfRect.bottomRight());
            }
            else
            {
                QRect toTheLeftOfRect = visualRect(model()->index(row, 0));
                line = QLine(toTheLeftOfRect.topLeft(), toTheLeftOfRect.bottomLeft());
            }
            painter.save();
            painter.setPen(QPen(Qt::black, 3));
            painter.drawLine(line);
            painter.restore();
        }
    }
#endif
}

void InstanceView::resizeEvent([[maybe_unused]] QResizeEvent* event)
{
    int newItemsPerRow = calculateItemsPerRow();
    if (newItemsPerRow != m_currentItemsPerRow) {
        m_currentCursorColumn = -1;
        m_currentItemsPerRow = newItemsPerRow;
        updateGeometries();
    } else {
        updateScrollbar();
    }
}

void InstanceView::dragEnterEvent(QDragEnterEvent* event)
{
    executeDelayedItemsLayout();

    if (!isDragEventAccepted(event)) {
        return;
    }
    m_lastDragPosition = event->pos() + offset();
    viewport()->update();
    event->accept();
}

void InstanceView::dragMoveEvent(QDragMoveEvent* event)
{
    executeDelayedItemsLayout();

    if (!isDragEventAccepted(event)) {
        return;
    }
    m_lastDragPosition = event->pos() + offset();
    viewport()->update();
    event->accept();
}

void InstanceView::dragLeaveEvent([[maybe_unused]] QDragLeaveEvent* event)
{
    executeDelayedItemsLayout();

    m_lastDragPosition = QPoint();
    viewport()->update();
}

void InstanceView::dropEvent(QDropEvent* event)
{
    executeDelayedItemsLayout();

    m_lastDragPosition = QPoint();

    stopAutoScroll();
    setState(NoState);

    auto mimedata = event->mimeData();

    if (event->source() == this) {
        if (event->possibleActions() & Qt::MoveAction) {
            std::pair<VisualGroup*, VisualGroup::HitResults> dropPos = rowDropPos(event->pos());
            const VisualGroup* group = dropPos.first;
            auto hitResult = dropPos.second;

            if (hitResult == VisualGroup::HitResult::NoHit) {
                viewport()->update();
                return;
            }
            auto instanceId = QString::fromUtf8(mimedata->data("application/x-instanceid"));
            auto instanceList = APPLICATION->instances().get();
            instanceList->setInstanceGroup(instanceId, group->text);
            event->setDropAction(Qt::MoveAction);
            event->accept();

            updateGeometries();
            viewport()->update();
        }
        return;
    }

    // check if the action is supported
    if (!mimedata) {
        return;
    }

    // files dropped from outside?
    if (mimedata->hasUrls()) {
        auto urls = mimedata->urls();
        event->accept();
        emit droppedURLs(urls);
    }
}

void InstanceView::startDrag(Qt::DropActions supportedActions)
{
    executeDelayedItemsLayout();

    QModelIndexList indexes = selectionModel()->selectedIndexes();
    if (indexes.count() == 0)
        return;

    QMimeData* mimeData = model()->mimeData(indexes);
    if (!mimeData) {
        return;
    }
    QRect rect;
    QPixmap pixmap = renderToPixmap(indexes, &rect);
    QDrag* drag = new QDrag(this);
    drag->setPixmap(pixmap);
    drag->setMimeData(mimeData);
    drag->setHotSpot(m_pressedPosition - rect.topLeft());
    Qt::DropAction defaultDropAction = Qt::IgnoreAction;
    if (this->defaultDropAction() != Qt::IgnoreAction && (supportedActions & this->defaultDropAction())) {
        defaultDropAction = this->defaultDropAction();
    }
    /*auto action = */
    drag->exec(supportedActions, defaultDropAction);
}

QRect InstanceView::visualRect(const QModelIndex& index) const
{
    const_cast<InstanceView*>(this)->executeDelayedItemsLayout();

    return geometryRect(index).translated(-offset());
}

QRect InstanceView::geometryRect(const QModelIndex& index) const
{
    const_cast<InstanceView*>(this)->executeDelayedItemsLayout();

    if (!index.isValid() || isIndexHidden(index) || index.column() > 0) {
        return QRect();
    }

    int row = index.row();
    if (geometryCache.contains(row)) {
        return *geometryCache[row];
    }

    const VisualGroup* cat = category(index);
    QPair<int, int> pos = cat->positionOf(index);
    int x = pos.first;
    // int y = pos.second;

    QStyleOptionViewItem option;
    initViewItemOption(&option);

    QRect out;
    out.setTop(cat->verticalPosition() + cat->headerHeight() + 5 + cat->rowTopOf(index));
    out.setLeft(m_spacing + x * (itemWidth() + m_spacing));
    out.setSize(itemDelegate()->sizeHint(option, index));
    geometryCache.insert(row, new QRect(out));
    return out;
}

QModelIndex InstanceView::indexAt(const QPoint& point) const
{
    const_cast<InstanceView*>(this)->executeDelayedItemsLayout();

    for (int i = 0; i < model()->rowCount(); ++i) {
        QModelIndex index = model()->index(i, 0);
        if (visualRect(index).contains(point)) {
            return index;
        }
    }
    return QModelIndex();
}

void InstanceView::setSelection(const QRect& rect, const QItemSelectionModel::SelectionFlags commands)
{
    executeDelayedItemsLayout();

    for (int i = 0; i < model()->rowCount(); ++i) {
        QModelIndex index = model()->index(i, 0);
        QRect itemRect = visualRect(index);
        if (itemRect.intersects(rect)) {
            selectionModel()->select(index, commands);
            update(itemRect.translated(-offset()));
        }
    }
}

QPixmap InstanceView::renderToPixmap(const QModelIndexList& indices, QRect* r) const
{
    Q_ASSERT(r);
    auto paintPairs = draggablePaintPairs(indices, r);
    if (paintPairs.isEmpty()) {
        return QPixmap();
    }
    QPixmap pixmap(r->size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Selected;
    for (int j = 0; j < paintPairs.count(); ++j) {
        option.rect = paintPairs.at(j).first.translated(-r->topLeft());
        const QModelIndex& current = paintPairs.at(j).second;
        itemDelegate()->paint(&painter, option, current);
    }
    return pixmap;
}

QList<std::pair<QRect, QModelIndex>> InstanceView::draggablePaintPairs(const QModelIndexList& indices, QRect* r) const
{
    Q_ASSERT(r);
    QRect& rect = *r;
    QList<std::pair<QRect, QModelIndex>> ret;
    for (int i = 0; i < indices.count(); ++i) {
        const QModelIndex& index = indices.at(i);
        const QRect current = geometryRect(index);
        ret += std::make_pair(current, index);
        rect |= current;
    }
    return ret;
}

bool InstanceView::isDragEventAccepted([[maybe_unused]] QDropEvent* event)
{
    return true;
}

std::pair<VisualGroup*, VisualGroup::HitResults> InstanceView::rowDropPos(const QPoint& pos)
{
    VisualGroup::HitResults hitResult;
    auto group = categoryAt(pos + offset(), hitResult);
    return std::make_pair(group, hitResult);
}

QPoint InstanceView::offset() const
{
    return QPoint(horizontalOffset(), verticalOffset());
}

QRegion InstanceView::visualRegionForSelection(const QItemSelection& selection) const
{
    QRegion region;
    for (auto& range : selection) {
        int start_row = range.top();
        int end_row = range.bottom();
        for (int row = start_row; row <= end_row; ++row) {
            int start_column = range.left();
            int end_column = range.right();
            for (int column = start_column; column <= end_column; ++column) {
                QModelIndex index = model()->index(row, column, rootIndex());
                region += visualRect(index);  // OK
            }
        }
    }
    return region;
}

QModelIndex InstanceView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    auto current = currentIndex();
    if (!current.isValid()) {
        return current;
    }
    auto cat = category(current);
    int group_index = m_groups.indexOf(cat);
    if (group_index < 0)
        return current;

    QPair<int, int> pos = cat->positionOf(current);
    int column = pos.first;
    int row = pos.second;
    if (m_currentCursorColumn < 0) {
        m_currentCursorColumn = column;
    }
    // Handle different movement actions.
    switch (cursorAction) {
        case MoveUp: {
            if (row == 0) {
                int prevGroupIndex = group_index - 1;
                while (prevGroupIndex >= 0) {
                    auto prevGroup = m_groups[prevGroupIndex];
                    if (prevGroup->collapsed) {
                        prevGroupIndex--;
                        continue;
                    }
                    int newRow = prevGroup->numRows() - 1;
                    int newRowSize = prevGroup->rows[newRow].size();
                    int newColumn = m_currentCursorColumn;
                    if (m_currentCursorColumn >= newRowSize) {
                        newColumn = newRowSize - 1;
                    }
                    return prevGroup->rows[newRow][newColumn];
                }
            } else {
                int newRow = row - 1;
                int newRowSize = cat->rows[newRow].size();
                int newColumn = m_currentCursorColumn;
                if (m_currentCursorColumn >= newRowSize) {
                    newColumn = newRowSize - 1;
                }
                return cat->rows[newRow][newColumn];
            }
            return current;
        }
        case MoveDown: {
            if (row == cat->rows.size() - 1) {
                int nextGroupIndex = group_index + 1;
                while (nextGroupIndex < m_groups.size()) {
                    auto nextGroup = m_groups[nextGroupIndex];
                    if (nextGroup->collapsed) {
                        nextGroupIndex++;
                        continue;
                    }
                    int newRowSize = nextGroup->rows[0].size();
                    int newColumn = m_currentCursorColumn;
                    if (m_currentCursorColumn >= newRowSize) {
                        newColumn = newRowSize - 1;
                    }
                    return nextGroup->rows[0][newColumn];
                }
            } else {
                int newRow = row + 1;
                int newRowSize = cat->rows[newRow].size();
                int newColumn = m_currentCursorColumn;
                if (m_currentCursorColumn >= newRowSize) {
                    newColumn = newRowSize - 1;
                }
                return cat->rows[newRow][newColumn];
            }
            return current;
        }
        case MoveLeft: {
            if (column > 0) {
                m_currentCursorColumn = column - 1;
                return cat->rows[row][column - 1];
            } else if (row > 0) {
                row -= 1;
                int newRowSize = cat->rows[row].size();
                m_currentCursorColumn = newRowSize - 1;
                return cat->rows[row][m_currentCursorColumn];
            } else {
                int prevGroupIndex = group_index - 1;
                while (prevGroupIndex >= 0) {
                    auto prevGroup = m_groups[prevGroupIndex];
                    if (prevGroup->collapsed) {
                        prevGroupIndex--;
                        continue;
                    }
                    int lastRow = prevGroup->numRows() - 1;
                    int lastCol = prevGroup->rows[lastRow].size() - 1;
                    m_currentCursorColumn = lastCol;
                    return prevGroup->rows[lastRow][lastCol];
                }
            }
            return current;
        }
        case MoveRight: {
            if (column < cat->rows[row].size() - 1) {
                m_currentCursorColumn = column + 1;
                return cat->rows[row][column + 1];
            } else if (row < cat->rows.size() - 1) {
                row += 1;
                m_currentCursorColumn = 0;
                return cat->rows[row][m_currentCursorColumn];
            } else {
                int nextGroupIndex = group_index + 1;
                while (nextGroupIndex < m_groups.size()) {
                    auto nextGroup = m_groups[nextGroupIndex];
                    if (nextGroup->collapsed) {
                        nextGroupIndex++;
                        continue;
                    }
                    m_currentCursorColumn = 0;
                    return nextGroup->rows[0][0];
                }
            }
            return current;
        }
        case MoveHome: {
            m_currentCursorColumn = 0;
            return cat->rows[row][0];
        }
        case MoveEnd: {
            auto last = cat->rows[row].size() - 1;
            m_currentCursorColumn = last;
            return cat->rows[row][last];
        }
        default:
            // For unsupported cursor actions, return the current index.
            break;
    }
    return current;
}

int InstanceView::horizontalOffset() const
{
    return horizontalScrollBar()->value();
}

int InstanceView::verticalOffset() const
{
    return verticalScrollBar()->value();
}

void InstanceView::scrollContentsBy(int dx, int dy)
{
    scrollDirtyRegion(dx, dy);
    viewport()->scroll(dx, dy);
}

void InstanceView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if (!index.isValid())
        return;

    const QRect rect = visualRect(index);
    if (hint == EnsureVisible && viewport()->rect().contains(rect)) {
        viewport()->update(rect);
        return;
    }

    verticalScrollBar()->setValue(verticalScrollToValue(index, rect, hint));
}

int InstanceView::verticalScrollToValue([[maybe_unused]] const QModelIndex& index, const QRect& rect, QListView::ScrollHint hint) const
{
    const QRect area = viewport()->rect();
    const bool above = (hint == QListView::EnsureVisible && rect.top() < area.top());
    const bool below = (hint == QListView::EnsureVisible && rect.bottom() > area.bottom());

    int verticalValue = verticalScrollBar()->value();
    QRect adjusted = rect.adjusted(-spacing(), -spacing(), spacing(), spacing());
    if (hint == QListView::PositionAtTop || above)
        verticalValue += adjusted.top();
    else if (hint == QListView::PositionAtBottom || below)
        verticalValue += qMin(adjusted.top(), adjusted.bottom() - area.height() + 1);
    else if (hint == QListView::PositionAtCenter)
        verticalValue += adjusted.top() - ((area.height() - adjusted.height()) / 2);
    return verticalValue;
}
