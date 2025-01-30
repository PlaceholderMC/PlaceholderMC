// SPDX-FileCopyrightText: 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Rachel Powers <508861+Ryex@users.noreply.github.com>
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

#include <QTest>
#include <QTimer>

#include <FileSystem.h>

#include <minecraft/mod/DataPack.h>
#include <minecraft/mod/tasks/LocalDataPackParseTask.h>

class DataPackParseTest : public QObject {
    Q_OBJECT

   private slots:
    void test_parseZIP()
    {
        QString source = QFINDTESTDATA("testdata/DataPackParse");

        QString zip_dp = FS::PathCombine(source, "test_data_pack_boogaloo.zip");
        DataPack pack{ QFileInfo(zip_dp) };

        bool valid = DataPackUtils::processZIP(&pack);

        QVERIFY(pack.packFormat() == 4);
        QVERIFY(pack.description() == "Some data pack 2 boobgaloo");
        QVERIFY(valid == true);
    }

    void test_parseFolder()
    {
        QString source = QFINDTESTDATA("testdata/DataPackParse");

        QString folder_dp = FS::PathCombine(source, "test_folder");
        DataPack pack{ QFileInfo(folder_dp) };

        bool valid = DataPackUtils::processFolder(&pack);

        QVERIFY(pack.packFormat() == 10);
        QVERIFY(pack.description() == "Some data pack, maybe");
        QVERIFY(valid == true);
    }

    void test_parseFolder2()
    {
        QString source = QFINDTESTDATA("testdata/DataPackParse");

        QString folder_dp = FS::PathCombine(source, "another_test_folder");
        DataPack pack{ QFileInfo(folder_dp) };

        bool valid = DataPackUtils::process(&pack);

        QVERIFY(pack.packFormat() == 6);
        QVERIFY(pack.description() == "Some data pack three, leaves on the tree");
        QVERIFY(valid == true);
    }
};

QTEST_GUILESS_MAIN(DataPackParseTest)

#include "DataPackParse_test.moc"
