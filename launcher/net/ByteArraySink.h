// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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

#include "Sink.h"

namespace Net {

/*
 * Sink object for downloads that uses an external QByteArray it doesn't own as a target.
 */
class ByteArraySink : public Sink {
   public:
    ByteArraySink(std::shared_ptr<QByteArray> output) : m_output(output) {};

    virtual ~ByteArraySink() = default;

   public:
    auto init(QNetworkRequest& request) -> Task::State override
    {
        if (m_output)
            m_output->clear();
        else
            qWarning() << "ByteArraySink did not initialize the buffer because it's not addressable";
        if (initAllValidators(request))
            return Task::State::Running;
        m_fail_reason = "Failed to initialize validators";
        return Task::State::Failed;
    };

    auto write(QByteArray& data) -> Task::State override
    {
        if (m_output)
            m_output->append(data);
        else
            qWarning() << "ByteArraySink did not write the buffer because it's not addressable";
        if (writeAllValidators(data))
            return Task::State::Running;
        m_fail_reason = "Failed to write validators";
        return Task::State::Failed;
    }

    auto abort() -> Task::State override
    {
        failAllValidators();
        m_fail_reason = "Aborted";
        return Task::State::Failed;
    }

    auto finalize(QNetworkReply& reply) -> Task::State override
    {
        if (finalizeAllValidators(reply))
            return Task::State::Succeeded;
        m_fail_reason = "Failed to finalize validators";
        return Task::State::Failed;
    }

    auto hasLocalData() -> bool override { return false; }

   protected:
    std::shared_ptr<QByteArray> m_output;
};
}  // namespace Net
