#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include <tasks/Task.h>

#include <FileSystem.h>
#include <StringUtils.h>

// Snippet from https://github.com/gulrak/filesystem#using-it-as-single-file-header

#ifdef __APPLE__
#include <Availability.h>  // for deployment target to support pre-catalina targets without std::fs
#endif                     // __APPLE__

#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (defined(__cplusplus) && __cplusplus >= 201703L)) && defined(__has_include)
#if __has_include(<filesystem>) && (!defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500)
#define GHC_USE_STD_FS
#include <filesystem>
namespace fs = std::filesystem;
#endif  // MacOS min version check
#endif  // Other OSes version check

#ifndef GHC_USE_STD_FS
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#endif

#include <pathmatcher/RegexpMatcher.h>

class LinkTask : public Task {
    Q_OBJECT

    friend class FileSystemTest;

    LinkTask(QString src, QString dst)
    {
        m_lnk = new FS::create_link(src, dst, this);
        m_lnk->debug(true);
    }

    ~LinkTask() { delete m_lnk; }

    void matcher(IPathMatcher::Ptr filter) { m_lnk->matcher(filter); }

    void linkRecursively(bool recursive)
    {
        m_lnk->linkRecursively(recursive);
        m_linkRecursive = recursive;
    }

    void whitelist(bool b) { m_lnk->whitelist(b); }

    void setMaxDepth(int depth) { m_lnk->setMaxDepth(depth); }

   private:
    void executeTask() override
    {
        if (!(*m_lnk)()) {
#if defined Q_OS_WIN32
            if (!m_useHard) {
                qDebug() << "EXPECTED: Link failure, Windows requires permissions for symlinks";

                qDebug() << "atempting to run with privelage";
                connect(m_lnk, &FS::create_link::finishedPrivileged, this, [this](bool gotResults) {
                    if (gotResults) {
                        emitSucceeded();
                    } else {
                        qDebug() << "Privileged run exited without results!";
                        emitFailed();
                    }
                });
                m_lnk->runPrivileged();
            } else {
                qDebug() << "Link Failed!" << m_lnk->getOSError().value() << m_lnk->getOSError().message().c_str();
            }
#else
            qDebug() << "Link Failed!" << m_lnk->getOSError().value() << m_lnk->getOSError().message().c_str();
#endif
        } else {
            emitSucceeded();
        }
    }

    FS::create_link* m_lnk;
    [[maybe_unused]] bool m_useHard = false;
    bool m_linkRecursive = true;
};

class FileSystemTest : public QObject {
    Q_OBJECT

    const QString bothSlash = "/foo/";
    const QString trailingSlash = "foo/";
    const QString leadingSlash = "/foo";

   private slots:
    void test_pathCombine()
    {
        QCOMPARE(QString("/foo/foo"), FS::PathCombine(bothSlash, bothSlash));
        QCOMPARE(QString("foo/foo"), FS::PathCombine(trailingSlash, trailingSlash));
        QCOMPARE(QString("/foo/foo"), FS::PathCombine(leadingSlash, leadingSlash));

        QCOMPARE(QString("/foo/foo/foo"), FS::PathCombine(bothSlash, bothSlash, bothSlash));
        QCOMPARE(QString("foo/foo/foo"), FS::PathCombine(trailingSlash, trailingSlash, trailingSlash));
        QCOMPARE(QString("/foo/foo/foo"), FS::PathCombine(leadingSlash, leadingSlash, leadingSlash));
    }

    void test_PathCombine1_data()
    {
        QTest::addColumn<QString>("result");
        QTest::addColumn<QString>("path1");
        QTest::addColumn<QString>("path2");

        QTest::newRow("qt 1") << "/abc/def/ghi/jkl" << "/abc/def" << "ghi/jkl";
        QTest::newRow("qt 2") << "/abc/def/ghi/jkl" << "/abc/def/" << "ghi/jkl";
#if defined(Q_OS_WIN)
        QTest::newRow("win native, from C:") << "C:/abc" << "C:" << "abc";
        QTest::newRow("win native 1") << "C:/abc/def/ghi/jkl" << "C:\\abc\\def" << "ghi\\jkl";
        QTest::newRow("win native 2") << "C:/abc/def/ghi/jkl" << "C:\\abc\\def\\" << "ghi\\jkl";
#endif
    }

    void test_PathCombine1()
    {
        QFETCH(QString, result);
        QFETCH(QString, path1);
        QFETCH(QString, path2);

        QCOMPARE(FS::PathCombine(path1, path2), result);
    }

    void test_PathCombine2_data()
    {
        QTest::addColumn<QString>("result");
        QTest::addColumn<QString>("path1");
        QTest::addColumn<QString>("path2");
        QTest::addColumn<QString>("path3");

        QTest::newRow("qt 1") << "/abc/def/ghi/jkl" << "/abc" << "def" << "ghi/jkl";
        QTest::newRow("qt 2") << "/abc/def/ghi/jkl" << "/abc/" << "def" << "ghi/jkl";
        QTest::newRow("qt 3") << "/abc/def/ghi/jkl" << "/abc" << "def/" << "ghi/jkl";
        QTest::newRow("qt 4") << "/abc/def/ghi/jkl" << "/abc/" << "def/" << "ghi/jkl";
#if defined(Q_OS_WIN)
        QTest::newRow("win 1") << "C:/abc/def/ghi/jkl" << "C:\\abc" << "def" << "ghi\\jkl";
        QTest::newRow("win 2") << "C:/abc/def/ghi/jkl" << "C:\\abc\\" << "def" << "ghi\\jkl";
        QTest::newRow("win 3") << "C:/abc/def/ghi/jkl" << "C:\\abc" << "def\\" << "ghi\\jkl";
        QTest::newRow("win 4") << "C:/abc/def/ghi/jkl" << "C:\\abc\\" << "def" << "ghi\\jkl";
#endif
    }

    void test_PathCombine2()
    {
        QFETCH(QString, result);
        QFETCH(QString, path1);
        QFETCH(QString, path2);
        QFETCH(QString, path3);

        QCOMPARE(FS::PathCombine(path1, path2, path3), result);
    }

    void test_copy()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            c();

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
            }
            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_with_blacklist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            RegexpMatcher::Ptr re = std::make_shared<RegexpMatcher>("[.]?mcmeta");
            c.matcher(re);
            c();

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
            }
            QVERIFY(!target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_with_whitelist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            RegexpMatcher::Ptr re = std::make_shared<RegexpMatcher>("[.]?mcmeta");
            c.matcher(re);
            c.whitelist(true);
            c();

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
            }
            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(!target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_with_dot_hidden()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(folder, target_dir.path());
            c();

            auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
            }

            QVERIFY(target_dir.entryList(filter).contains(".secret_folder"));
            target_dir.cd(".secret_folder");
            QVERIFY(target_dir.entryList(filter).contains(".secret_file.txt"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_copy_single_file()
    {
        QTemporaryDir tempDir;
        tempDir.setAutoRemove(true);

        {
            QString file = QFINDTESTDATA("testdata/FileSystem/test_folder/pack.mcmeta");

            qDebug() << "From:" << file << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "pack.mcmeta"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::copy c(file, target_dir.filePath("pack.mcmeta"));
            c();

            auto filter = QDir::Filter::Files;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
            }

            QVERIFY(target_dir.entryList(filter).contains("pack.mcmeta"));
        }
    }

    void test_getDesktop() { QCOMPARE(FS::getDesktopDir(), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)); }

    void test_link()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(false);
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(!entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());
            QVERIFY(lnk_info.isSymLink());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_hard_link()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            // use working dir to prevent makeing a hard link to a tmpfs or across devices
            QTemporaryDir tempDir("./tmp");
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();
            FS::create_link lnk(folder, target_dir.path());
            lnk.useHardLinks(true);
            lnk.debug(true);
            if (!lnk()) {
                qDebug() << "Link Failed!" << lnk.getOSError().value() << lnk.getOSError().message().c_str();
            }

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                QVERIFY(!entry_lnk_info.isSymLink());
                QFileInfo entry_orig_info(QDir(folder).filePath(entry));
                if (!entry_lnk_info.isDir()) {
                    qDebug() << "hard link equivalency?" << entry_lnk_info.absoluteFilePath() << "vs" << entry_orig_info.absoluteFilePath();
                    QVERIFY(fs::equivalent(fs::path(StringUtils::toStdString(entry_lnk_info.absoluteFilePath())),
                                           fs::path(StringUtils::toStdString(entry_orig_info.absoluteFilePath()))));
                }
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());
            QVERIFY(!lnk_info.isSymLink());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_blacklist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            RegexpMatcher::Ptr re = std::make_shared<RegexpMatcher>("[.]?mcmeta");
            lnk_tsk.matcher(re);
            lnk_tsk.linkRecursively(true);
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(!target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_whitelist()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            RegexpMatcher::Ptr re = std::make_shared<RegexpMatcher>("[.]?mcmeta");
            lnk_tsk.matcher(re);
            lnk_tsk.linkRecursively(true);
            lnk_tsk.whitelist(true);
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            for (auto entry : target_dir.entryList()) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(!target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_dot_hidden()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(true);
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                if (!entry_lnk_info.isDir())
                    QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(target_dir.entryList(filter).contains(".secret_folder"));
            target_dir.cd(".secret_folder");
            QVERIFY(target_dir.entryList(filter).contains(".secret_file.txt"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_single_file()
    {
        QTemporaryDir tempDir;
        tempDir.setAutoRemove(true);

        {
            QString file = QFINDTESTDATA("testdata/FileSystem/test_folder/pack.mcmeta");

            qDebug() << "From:" << file << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "pack.mcmeta"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(file, target_dir.filePath("pack.mcmeta"));
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            auto filter = QDir::Filter::Files;

            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
            }

            QFileInfo lnk_info(target_dir.filePath("pack.mcmeta"));
            QVERIFY(lnk_info.exists());
            QVERIFY(lnk_info.isSymLink());

            QVERIFY(target_dir.entryList(filter).contains("pack.mcmeta"));
        }
    }

    void test_link_with_max_depth()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(true);
            lnk_tsk.setMaxDepth(0);
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            QVERIFY(!QFileInfo(target_dir.path()).isSymLink());

            auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;
            for (auto entry : target_dir.entryList(filter)) {
                qDebug() << entry;
                if (entry == "." || entry == "..")
                    continue;
                QFileInfo entry_lnk_info(target_dir.filePath(entry));
                QVERIFY(entry_lnk_info.isSymLink());
            }

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());
            QVERIFY(!lnk_info.isSymLink());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_link_with_no_max_depth()
    {
        QString folder = QFINDTESTDATA("testdata/FileSystem/test_folder");
        auto f = [&folder]() {
            QTemporaryDir tempDir;
            tempDir.setAutoRemove(true);
            qDebug() << "From:" << folder << "To:" << tempDir.path();

            QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
            qDebug() << tempDir.path();
            qDebug() << target_dir.path();

            LinkTask lnk_tsk(folder, target_dir.path());
            lnk_tsk.linkRecursively(true);
            lnk_tsk.setMaxDepth(-1);
            QObject::connect(&lnk_tsk, &Task::finished, [&lnk_tsk] {
                QVERIFY2(lnk_tsk.wasSuccessful(), "Task finished but was not successful when it should have been.");
            });
            lnk_tsk.start();

            QVERIFY2(QTest::qWaitFor([&lnk_tsk]() { return lnk_tsk.isFinished(); }, 100000), "Task didn't finish as it should.");

            std::function<void(QString)> verify_check = [&verify_check](QString check_path) {
                QDir check_dir(check_path);
                auto filter = QDir::Filter::Files | QDir::Filter::Dirs | QDir::Filter::Hidden;
                for (auto entry : check_dir.entryList(filter)) {
                    QFileInfo entry_lnk_info(check_dir.filePath(entry));
                    qDebug() << entry << check_dir.filePath(entry);
                    if (!entry_lnk_info.isDir()) {
                        QVERIFY(entry_lnk_info.isSymLink());
                    } else if (entry != "." && entry != "..") {
                        qDebug() << "Decending tree to verify symlinks:" << check_dir.filePath(entry);
                        verify_check(entry_lnk_info.filePath());
                    }
                }
            };

            verify_check(target_dir.path());

            QFileInfo lnk_info(target_dir.path());
            QVERIFY(lnk_info.exists());

            QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
            QVERIFY(target_dir.entryList().contains("assets"));
        };

        // first try variant without trailing /
        QVERIFY(!folder.endsWith('/'));
        f();

        // then variant with trailing /
        folder.append('/');
        QVERIFY(folder.endsWith('/'));
        f();
    }

    void test_path_depth()
    {
        QCOMPARE(FS::pathDepth(""), 0);
        QCOMPARE(FS::pathDepth("."), 0);
        QCOMPARE(FS::pathDepth("foo.txt"), 0);
        QCOMPARE(FS::pathDepth("./foo.txt"), 0);
        QCOMPARE(FS::pathDepth("./bar/foo.txt"), 1);
        QCOMPARE(FS::pathDepth("../bar/foo.txt"), 0);
        QCOMPARE(FS::pathDepth("/bar/foo.txt"), 1);
        QCOMPARE(FS::pathDepth("baz/bar/foo.txt"), 2);
        QCOMPARE(FS::pathDepth("/baz/bar/foo.txt"), 2);
        QCOMPARE(FS::pathDepth("./baz/bar/foo.txt"), 2);
        QCOMPARE(FS::pathDepth("/baz/../bar/foo.txt"), 1);
    }

    void test_path_trunc()
    {
        QCOMPARE(FS::pathTruncate("", 0), QDir::toNativeSeparators(""));
        QCOMPARE(FS::pathTruncate("foo.txt", 0), QDir::toNativeSeparators(""));
        QCOMPARE(FS::pathTruncate("foo.txt", 1), QDir::toNativeSeparators(""));
        QCOMPARE(FS::pathTruncate("./bar/foo.txt", 0), QDir::toNativeSeparators("./bar"));
        QCOMPARE(FS::pathTruncate("./bar/foo.txt", 1), QDir::toNativeSeparators("./bar"));
        QCOMPARE(FS::pathTruncate("/bar/foo.txt", 1), QDir::toNativeSeparators("/bar"));
        QCOMPARE(FS::pathTruncate("bar/foo.txt", 1), QDir::toNativeSeparators("bar"));
        QCOMPARE(FS::pathTruncate("baz/bar/foo.txt", 2), QDir::toNativeSeparators("baz/bar"));
#if defined(Q_OS_WIN)
        QCOMPARE(FS::pathTruncate("C:\\bar\\foo.txt", 1), QDir::toNativeSeparators("C:\\bar"));
#endif
    }
};

QTEST_GUILESS_MAIN(FileSystemTest)

#include "FileSystem_test.moc"
