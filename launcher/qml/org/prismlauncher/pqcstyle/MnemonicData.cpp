// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2024 Rachel Powers <508861+Ryex@users.noreply.github.com>
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 Rachel Powers <508861+Ryex@users.noreply.github.com>
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
 *  This file incorporates work covered by the following copyright and
 *  permission notice:
 *
 *    Copyright © 2017 Marco Martin <mart@kde.org>
 *
 *    Licensed under LGPL-2.0-or-later 
 *          
 *          https://community.kde.org/Policies/Licensing_Policy
 */

/*
 * Modified minimally from https://invent.kde.org/frameworks/kirigami/-/blob/master/src/mnemonicattached.cpp
 * under GPL-3.0
 */

#include "MnemonicData.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QWindow>

QHash<QKeySequence, MnemonicData*> MnemonicData::s_sequenceToObject = QHash<QKeySequence, MnemonicData*>();

// If pos points to alphanumeric X in "...(X)...", which is preceded or
// followed only by non-alphanumerics, then "(X)" gets removed.
static QString removeReducedCJKAccMark(const QString& label, int pos)
{
    /* clang-format off */
    if (pos > 0 && pos + 1 < label.length()
        && label[pos - 1] == QLatin1Char('(')
        && label[pos + 1] == QLatin1Char(')')
        && label[pos].isLetterOrNumber()) { /* clang-format on */
        // Check if at start or end, ignoring non-alphanumerics.
        int len = label.length();
        int p1 = pos - 2;
        while (p1 >= 0 && !label[p1].isLetterOrNumber()) {
            --p1;
        }
        ++p1;
        int p2 = pos + 2;
        while (p2 < len && !label[p2].isLetterOrNumber()) {
            ++p2;
        }
        --p2;

        const QStringView strView(label);
        if (p1 == 0) {
            return strView.left(pos - 1).toString() + strView.mid(p2 + 1).toString();
        } else if (p2 + 1 == len) {
            return strView.left(p1).toString() + strView.mid(pos + 2).toString();
        }
    }
    return label;
}

static QString removeAcceleratorMarker(const QString& label_)
{
    QString label = label_;

    int p = 0;
    bool accmarkRemoved = false;
    while (true) {
        p = label.indexOf(QLatin1Char('&'), p);
        if (p < 0 || p + 1 == label.length()) {
            break;
        }

        if (label.at(p + 1).isLetterOrNumber()) {
            // Valid accelerator.
            const QStringView sv(label);
            label = sv.left(p).toString() + sv.mid(p + 1).toString();

            // May have been an accelerator in CJK-style "(&X)"
            // at the start or end of text.
            label = removeReducedCJKAccMark(label, p);

            accmarkRemoved = true;
        } else if (label.at(p + 1) == QLatin1Char('&')) {
            // Escaped accelerator marker.
            const QStringView sv(label);
            label = sv.left(p).toString() + sv.mid(p + 1).toString();
        }

        ++p;
    }

    // If no marker was removed, and there are CJK characters in the label,
    // also try to remove reduced CJK marker -- something may have removed
    // ampersand beforehand.
    if (!accmarkRemoved) {
        bool hasCJK = false;
        for (const QChar c : std::as_const(label)) {
            if (c.unicode() >= 0x2e00) {  // rough, but should be sufficient
                hasCJK = true;
                break;
            }
        }
        if (hasCJK) {
            p = 0;
            while (true) {
                p = label.indexOf(QLatin1Char('('), p);
                if (p < 0) {
                    break;
                }
                label = removeReducedCJKAccMark(label, p + 1);
                ++p;
            }
        }
    }

    return label;
}

MnemonicData::MnemonicData(QObject* parent) : QObject(parent)
{
    connect(&MnemonicEventFilter::instance(), &MnemonicEventFilter::altPressed, this, &MnemonicData::onAltPressed);
    connect(&MnemonicEventFilter::instance(), &MnemonicEventFilter::altReleased, this, &MnemonicData::onAltReleased);
}

MnemonicData::~MnemonicData()
{
    s_sequenceToObject.remove(m_sequence);
}

QWindow* MnemonicData::window() const
{
    if (auto* parentItem = qobject_cast<QQuickItem*>(parent())) {
        if (auto* window = parentItem->window()) {
            if (auto* renderWindow = QQuickRenderControl::renderWindowFor(window)) {
                return renderWindow;
            }

            return window;
        }
    }

    return nullptr;
}

void MnemonicData::onAltPressed()
{
    if (m_active || m_richTextLabel.isEmpty()) {
        return;
    }

    auto* win = window();
    if (!win || !win->isActive()) {
        return;
    }

    m_actualRichTextLabel = m_richTextLabel;
    Q_EMIT richTextLabelChanged();
    m_active = true;
    Q_EMIT activeChanged();
}

void MnemonicData::onAltReleased()
{
    if (!m_active || m_richTextLabel.isEmpty()) {
        return;
    }

    // Disabling menmonics again is always fine, e.g. on window deactivation,
    // don't check for window is active here.

    m_actualRichTextLabel = removeAcceleratorMarker(m_label);
    Q_EMIT richTextLabelChanged();
    m_active = false;
    Q_EMIT activeChanged();
}

// Algorithm adapted from KAccelString
void MnemonicData::calculateWeights()
{
    m_weights.clear();

    int pos = 0;
    bool start_character = true;
    bool wanted_character = false;

    while (pos < m_label.length()) {
        QChar c = m_label[pos];

        // skip non typeable characters
        if (!c.isLetterOrNumber() && c != QLatin1Char('&')) {
            start_character = true;
            ++pos;
            continue;
        }

        int weight = 1;

        // add special weight to first character
        if (pos == 0) {
            weight += FIRST_CHARACTER_EXTRA_WEIGHT;
            // add weight to word beginnings
        } else if (start_character) {
            weight += WORD_BEGINNING_EXTRA_WEIGHT;
            start_character = false;
        }

        // add weight to characters that have an & beforehand
        if (wanted_character) {
            weight += WANTED_ACCEL_EXTRA_WEIGHT;
            wanted_character = false;
        }

        // add decreasing weight to left characters
        if (pos < 50) {
            weight += (50 - pos);
        }

        // try to preserve the wanted accelerators
        /* clang-format off */
        if (c == QLatin1Char('&')
            && (pos != m_label.length() - 1
                && m_label[pos + 1] != QLatin1Char('&')
                && m_label[pos + 1].isLetterOrNumber())) { /* clang-format on */
            wanted_character = true;
            ++pos;
            continue;
        }

        while (m_weights.contains(weight)) {
            ++weight;
        }

        if (c != QLatin1Char('&')) {
            m_weights[weight] = c;
        }

        ++pos;
    }

    // update our maximum weight
    if (m_weights.isEmpty()) {
        m_weight = m_baseWeight;
    } else {
        m_weight = m_baseWeight + (std::prev(m_weights.cend())).key();
    }
}

void MnemonicData::updateSequence()
{
    if (!m_sequence.isEmpty()) {
        s_sequenceToObject.remove(m_sequence);
        m_sequence = {};
    }

    calculateWeights();

    // Preserve strings like "One & Two" where & is not an accelerator escape
    const QString text = label().replace(QStringLiteral("& "), QStringLiteral("&& "));
    m_actualRichTextLabel = removeAcceleratorMarker(text);

    if (!m_enabled) {
        // was the label already completely plain text? try to limit signal emission
        if (m_mnemonicLabel != m_actualRichTextLabel) {
            m_mnemonicLabel = m_actualRichTextLabel;
            Q_EMIT mnemonicLabelChanged();
            Q_EMIT richTextLabelChanged();
        }
        return;
    }

    m_mnemonicLabel = text;
    m_mnemonicLabel.replace(QRegularExpression(QLatin1String("\\&([^\\&])")), QStringLiteral("\\1"));

    if (!m_weights.isEmpty()) {
        QMap<int, QChar>::const_iterator i = m_weights.constEnd();
        do {
            --i;
            QChar c = i.value();

            QKeySequence ks(QStringLiteral("Alt+") % c);
            MnemonicData* otherMa = s_sequenceToObject.value(ks);
            Q_ASSERT(otherMa != this);
            if (!otherMa || otherMa->m_weight < m_weight) {
                // the old shortcut is less valuable than the current: remove it
                if (otherMa) {
                    s_sequenceToObject.remove(otherMa->sequence());
                    otherMa->m_sequence = {};
                }

                s_sequenceToObject[ks] = this;
                m_sequence = ks;
                m_richTextLabel = text;
                m_richTextLabel.replace(QRegularExpression(QLatin1String("\\&([^\\&])")), QStringLiteral("\\1"));
                m_mnemonicLabel = text;
                const int mnemonicPos = m_mnemonicLabel.indexOf(c);

                if (mnemonicPos > -1 && (mnemonicPos == 0 || m_mnemonicLabel[mnemonicPos - 1] != QLatin1Char('&'))) {
                    m_mnemonicLabel.replace(mnemonicPos, 1, QStringLiteral("&") % c);
                }

                const int richTextPos = m_richTextLabel.indexOf(c);
                if (richTextPos > -1) {
                    m_richTextLabel.replace(richTextPos, 1, QLatin1String("<u>") % c % QLatin1String("</u>"));
                }

                // remap the sequence of the previous shortcut
                if (otherMa) {
                    otherMa->updateSequence();
                }

                break;
            }
        } while (i != m_weights.constBegin());
    }

    if (!m_sequence.isEmpty()) {
        Q_EMIT sequenceChanged();
    }

    Q_EMIT richTextLabelChanged();
    Q_EMIT mnemonicLabelChanged();
}

void MnemonicData::setLabel(const QString& text)
{
    if (m_label == text) {
        return;
    }

    m_label = text;
    updateSequence();
    Q_EMIT labelChanged();
}

QString MnemonicData::richTextLabel() const
{
    if (!m_actualRichTextLabel.isEmpty()) {
        return m_actualRichTextLabel;
    } else {
        return removeAcceleratorMarker(m_label);
    }
}

QString MnemonicData::mnemonicLabel() const
{
    return m_mnemonicLabel;
}

QString MnemonicData::label() const
{
    return m_label;
}

void MnemonicData::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    updateSequence();
    Q_EMIT enabledChanged();
}

bool MnemonicData::enabled() const
{
    return m_enabled;
}

void MnemonicData::setControlType(MnemonicData::ControlType controlType)
{
    if (m_controlType == controlType) {
        return;
    }

    m_controlType = controlType;

    switch (controlType) {
        case ActionElement:
            m_baseWeight = ACTION_ELEMENT_WEIGHT;
            break;
        case DialogButton:
            m_baseWeight = DIALOG_BUTTON_EXTRA_WEIGHT;
            break;
        case MenuItem:
            m_baseWeight = MENU_ITEM_WEIGHT;
            break;
        case FormLabel:
            m_baseWeight = FORM_LABEL_WEIGHT;
            break;
        default:
            m_baseWeight = SECONDARY_CONTROL_WEIGHT;
            break;
    }
    // update our maximum weight
    if (m_weights.isEmpty()) {
        m_weight = m_baseWeight;
    } else {
        m_weight = m_baseWeight + (std::prev(m_weights.constEnd())).key();
    }
    Q_EMIT controlTypeChanged();
}

MnemonicData::ControlType MnemonicData::controlType() const
{
    return m_controlType;
}

QKeySequence MnemonicData::sequence()
{
    return m_sequence;
}

bool MnemonicData::active() const
{
    return m_active;
}

MnemonicData* MnemonicData::qmlAttachedProperties(QObject* object)
{
    return new MnemonicData(object);
}

void MnemonicData::setActive(bool active)
{
    // We can't rely on previous value when it's true since it can be
    // caused by Alt key press and we need to remove the event filter
    // additionally. False should be ok as it's a default state.
    if (!m_active && m_active == active) {
        return;
    }

    m_active = active;

    if (m_active) {
        if (m_actualRichTextLabel != m_richTextLabel) {
            m_actualRichTextLabel = m_richTextLabel;
            Q_EMIT richTextLabelChanged();
        }

    } else {
        m_actualRichTextLabel = removeAcceleratorMarker(m_label);
        Q_EMIT richTextLabelChanged();
    }

    Q_EMIT activeChanged();
}
