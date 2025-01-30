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
 */

#include <QTest>
#include <QTimer>
#include "minecraft/mod/tasks/LocalDataPackParseTask.h"

#include <FileSystem.h>

#include <minecraft/mod/ResourcePack.h>
#include <minecraft/mod/tasks/LocalResourcePackParseTask.h>

class ResourcePackParseTest : public QObject {
    Q_OBJECT

   private slots:
    void test_parseZIP()
    {
        QString source = QFINDTESTDATA("testdata/ResourcePackParse");

        QString zip_rp = FS::PathCombine(source, "test_resource_pack_idk.zip");
        ResourcePack pack{ QFileInfo(zip_rp) };

        bool valid = DataPackUtils::processZIP(&pack, DataPackUtils::ProcessingLevel::BasicInfoOnly);

        QVERIFY(pack.packFormat() == 3);
        QVERIFY(pack.description() ==
                "um dois, feijão com arroz, três quatro, feijão no prato, cinco seis, café inglês, sete oito, comer biscoito, nove dez "
                "comer pastéis!!");
        QVERIFY(valid == true);
    }

    void test_parseFolder()
    {
        QString source = QFINDTESTDATA("testdata/ResourcePackParse");

        QString folder_rp = FS::PathCombine(source, "test_folder");
        ResourcePack pack{ QFileInfo(folder_rp) };

        bool valid = DataPackUtils::processFolder(&pack, DataPackUtils::ProcessingLevel::BasicInfoOnly);

        QVERIFY(pack.packFormat() == 1);
        QVERIFY(pack.description() == "Some resource pack maybe");
        QVERIFY(valid == true);
    }

    void test_parseFolder2()
    {
        QString source = QFINDTESTDATA("testdata/ResourcePackParse");

        QString folder_rp = FS::PathCombine(source, "another_test_folder");
        ResourcePack pack{ QFileInfo(folder_rp) };

        bool valid = DataPackUtils::process(&pack, DataPackUtils::ProcessingLevel::BasicInfoOnly);

        QVERIFY(pack.packFormat() == 6);
        QVERIFY(pack.description() == "o quartel pegou fogo, policia deu sinal, acode acode acode a bandeira nacional");
        QVERIFY(valid == false);  // no assets dir
    }
};

QTEST_GUILESS_MAIN(ResourcePackParseTest)

#include "ResourcePackParse_test.moc"
