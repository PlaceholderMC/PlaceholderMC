// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 flowln <flowlnlnln@gmail.com>
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

#include "Validator.h"
#include "tasks/Task.h"

namespace Net {
class Sink {
   public:
    Sink() = default;
    virtual ~Sink() = default;

   public:
    virtual auto init(QNetworkRequest& request) -> Task::State = 0;
    virtual auto write(QByteArray& data) -> Task::State = 0;
    virtual auto abort() -> Task::State = 0;
    virtual auto finalize(QNetworkReply& reply) -> Task::State = 0;

    virtual auto hasLocalData() -> bool = 0;

    QString failReason() const { return m_fail_reason; }

    void addValidator(Validator* validator)
    {
        if (validator) {
            validators.push_back(std::shared_ptr<Validator>(validator));
        }
    }

   protected:
    bool initAllValidators(QNetworkRequest& request)
    {
        for (auto& validator : validators) {
            if (!validator->init(request))
                return false;
        }
        return true;
    }
    bool finalizeAllValidators(QNetworkReply& reply)
    {
        for (auto& validator : validators) {
            if (!validator->validate(reply))
                return false;
        }
        return true;
    }
    bool failAllValidators()
    {
        bool success = true;
        for (auto& validator : validators) {
            success &= validator->abort();
        }
        return success;
    }
    bool writeAllValidators(QByteArray& data)
    {
        for (auto& validator : validators) {
            if (!validator->write(data))
                return false;
        }
        return true;
    }

   protected:
    std::vector<std::shared_ptr<Validator>> validators;
    QString m_fail_reason;
};
}  // namespace Net
