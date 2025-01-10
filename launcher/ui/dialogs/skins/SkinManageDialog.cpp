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

#include "SkinManageDialog.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWidget.h"
#include "ui_SkinManageDialog.h"

#include <FileSystem.h>
#include <QAction>
#include <QDialog>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QListView>
#include <QMimeDatabase>
#include <QPainter>
#include <QUrl>

#include "Application.h"
#include "DesktopServices.h"
#include "Json.h"
#include "QObjectPtr.h"

#include "minecraft/auth/Parsers.h"
#include "minecraft/skins/CapeChange.h"
#include "minecraft/skins/SkinDelete.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/SkinUpload.h"

#include "net/Download.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/instanceview/InstanceDelegate.h"

SkinManageDialog::SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct)
    : QDialog(nullptr)
    , m_acct(acct)
    , m_ui(new Ui::SkinManageDialog)
    , m_list(this, APPLICATION->settings()->get("SkinsDir").toString(), acct)
{
    m_ui->setupUi(this);

    setWindowModality(Qt::WindowModal);

    auto contentsWidget = m_ui->listView;
    contentsWidget->setViewMode(QListView::IconMode);
    contentsWidget->setFlow(QListView::LeftToRight);
    contentsWidget->setIconSize(QSize(48, 48));
    contentsWidget->setMovement(QListView::Static);
    contentsWidget->setResizeMode(QListView::Adjust);
    contentsWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    contentsWidget->setSpacing(5);
    contentsWidget->setWordWrap(false);
    contentsWidget->setWrapping(true);
    contentsWidget->setUniformItemSizes(true);
    contentsWidget->setTextElideMode(Qt::ElideRight);
    contentsWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    contentsWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentsWidget->installEventFilter(this);
    contentsWidget->setItemDelegate(new ListViewDelegate(this));

    contentsWidget->setAcceptDrops(true);
    contentsWidget->setDropIndicatorShown(true);
    contentsWidget->viewport()->setAcceptDrops(true);
    contentsWidget->setDragDropMode(QAbstractItemView::DropOnly);
    contentsWidget->setDefaultDropAction(Qt::CopyAction);

    contentsWidget->installEventFilter(this);
    contentsWidget->setModel(&m_list);

    connect(contentsWidget, SIGNAL(doubleClicked(QModelIndex)), SLOT(activated(QModelIndex)));

    connect(contentsWidget->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            SLOT(selectionChanged(QItemSelection, QItemSelection)));
    connect(m_ui->listView, &QListView::customContextMenuRequested, this, &SkinManageDialog::show_context_menu);

    setupCapes();

    m_ui->listView->setCurrentIndex(m_list.index(m_list.getSelectedAccountSkin()));

    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
}

SkinManageDialog::~SkinManageDialog()
{
    delete m_ui;
}

void SkinManageDialog::activated(QModelIndex index)
{
    m_selectedSkinKey = index.data(Qt::UserRole).toString();
    accept();
}

void SkinManageDialog::selectionChanged(QItemSelection selected, [[maybe_unused]] QItemSelection deselected)
{
    if (selected.empty())
        return;

    QString key = selected.first().indexes().first().data(Qt::UserRole).toString();
    if (key.isEmpty())
        return;
    m_selectedSkinKey = key;
    auto skin = getSelectedSkin();
    if (!skin)
        return;

    m_ui->selectedModel->updateScene(skin);
    m_ui->capeCombo->setCurrentIndex(m_capesIdx.value(skin->getCapeId()));
    m_ui->steveBtn->setChecked(skin->getModel() == SkinModel::CLASSIC);
    m_ui->alexBtn->setChecked(skin->getModel() == SkinModel::SLIM);
}

void SkinManageDialog::delayed_scroll(QModelIndex model_index)
{
    auto contentsWidget = m_ui->listView;
    contentsWidget->scrollTo(model_index);
}

void SkinManageDialog::on_openDirBtn_clicked()
{
    DesktopServices::openPath(m_list.getDir(), true);
}

void SkinManageDialog::on_fileBtn_clicked()
{
    auto filter = QMimeDatabase().mimeTypeForName("image/png").filterString();
    QString raw_path = QFileDialog::getOpenFileName(this, tr("Select Skin Texture"), QString(), filter);
    if (raw_path.isNull()) {
        return;
    }
    auto message = m_list.installSkin(raw_path, {});
    if (!message.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Selected file is not a valid skin"), message, QMessageBox::Critical)->show();
        return;
    }
}

QPixmap previewCape(QImage capeImage)
{
    return QPixmap::fromImage(capeImage.copy(1, 1, 10, 16).scaled(80, 128, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}
void SkinManageDialog::setupCapes()
{
    // FIXME: add a model for this, download/refresh the capes on demand
    auto& accountData = *m_acct->accountData();
    int index = 0;
    m_ui->capeCombo->addItem(tr("No Cape"), QVariant());
    auto currentCape = accountData.minecraftProfile.currentCape;
    if (currentCape.isEmpty()) {
        m_ui->capeCombo->setCurrentIndex(index);
    }

    auto capesDir = FS::PathCombine(m_list.getDir(), "capes");
    NetJob::Ptr job{ new NetJob(tr("Download capes"), APPLICATION->network()) };
    bool needsToDownload = false;
    for (auto& cape : accountData.minecraftProfile.capes) {
        auto path = FS::PathCombine(capesDir, cape.id + ".png");
        if (cape.data.size()) {
            QImage capeImage;
            if (capeImage.loadFromData(cape.data, "PNG") && capeImage.save(path)) {
                m_capes[cape.id] = capeImage;
                continue;
            }
        }
        if (QFileInfo(path).exists()) {
            continue;
        }
        if (!cape.url.isEmpty()) {
            needsToDownload = true;
            job->addNetAction(Net::Download::makeFile(cape.url, path));
        }
    }
    if (needsToDownload) {
        ProgressDialog dlg(this);
        dlg.execWithTask(job.get());
    }
    for (auto& cape : accountData.minecraftProfile.capes) {
        index++;
        QImage capeImage;
        if (!m_capes.contains(cape.id)) {
            auto path = FS::PathCombine(capesDir, cape.id + ".png");
            if (QFileInfo(path).exists() && capeImage.load(path)) {
                m_capes[cape.id] = capeImage;
            }
        }
        if (!capeImage.isNull()) {
            m_ui->capeCombo->addItem(previewCape(capeImage), cape.alias, cape.id);
        } else {
            m_ui->capeCombo->addItem(cape.alias, cape.id);
        }

        m_capesIdx[cape.id] = index;
    }
}

void SkinManageDialog::on_capeCombo_currentIndexChanged(int index)
{
    auto id = m_ui->capeCombo->currentData();
    auto cape = m_capes.value(id.toString(), {});
    if (!cape.isNull()) {
        m_ui->capeImage->setPixmap(previewCape(cape).scaled(size() * (1. / 3), Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    m_ui->selectedModel->updateCape(cape);
    if (auto skin = getSelectedSkin(); skin) {
        skin->setCapeId(id.toString());
        m_ui->selectedModel->updateScene(skin);
    }
}

void SkinManageDialog::on_steveBtn_toggled(bool checked)
{
    if (auto skin = getSelectedSkin(); skin) {
        skin->setModel(checked ? SkinModel::CLASSIC : SkinModel::SLIM);
        m_ui->selectedModel->updateScene(skin);
    }
}

void SkinManageDialog::accept()
{
    auto skin = m_list.skin(m_selectedSkinKey);
    if (!skin) {
        reject();
        return;
    }
    auto path = skin->getPath();

    ProgressDialog prog(this);
    NetJob::Ptr skinUpload{ new NetJob(tr("Change skin"), APPLICATION->network(), 1) };

    if (!QFile::exists(path)) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Skin file does not exist!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }

    skinUpload->addNetAction(SkinUpload::make(m_acct->accessToken(), skin->getPath(), skin->getModelString()));

    auto selectedCape = skin->getCapeId();
    if (selectedCape != m_acct->accountData()->minecraftProfile.currentCape) {
        skinUpload->addNetAction(CapeChange::make(m_acct->accessToken(), selectedCape));
    }

    skinUpload->addTask(m_acct->refresh().staticCast<Task>());
    if (prog.execWithTask(skinUpload.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Upload"), tr("Failed to upload skin!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }
    skin->setURL(m_acct->accountData()->minecraftProfile.skin.url);
    QDialog::accept();
}

void SkinManageDialog::on_resetBtn_clicked()
{
    ProgressDialog prog(this);
    NetJob::Ptr skinReset{ new NetJob(tr("Reset skin"), APPLICATION->network(), 1) };
    skinReset->addNetAction(SkinDelete::make(m_acct->accessToken()));
    skinReset->addTask(m_acct->refresh().staticCast<Task>());
    if (prog.execWithTask(skinReset.get()) != QDialog::Accepted) {
        CustomMessageBox::selectable(this, tr("Skin Delete"), tr("Failed to delete current skin!"), QMessageBox::Warning)->exec();
        reject();
        return;
    }
    QDialog::accept();
}

void SkinManageDialog::show_context_menu(const QPoint& pos)
{
    QMenu myMenu(tr("Context menu"), this);
    myMenu.addAction(m_ui->action_Rename_Skin);
    myMenu.addAction(m_ui->action_Delete_Skin);

    myMenu.exec(m_ui->listView->mapToGlobal(pos));
}

bool SkinManageDialog::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_ui->listView) {
        if (ev->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                case Qt::Key_Delete:
                    on_action_Delete_Skin_triggered(false);
                    return true;
                case Qt::Key_F2:
                    on_action_Rename_Skin_triggered(false);
                    return true;
                default:
                    break;
            }
        }
    }
    return QDialog::eventFilter(obj, ev);
}

void SkinManageDialog::on_action_Rename_Skin_triggered(bool checked)
{
    if (!m_selectedSkinKey.isEmpty()) {
        m_ui->listView->edit(m_ui->listView->currentIndex());
    }
}

void SkinManageDialog::on_action_Delete_Skin_triggered(bool checked)
{
    if (m_selectedSkinKey.isEmpty())
        return;

    if (m_list.getSkinIndex(m_selectedSkinKey) == m_list.getSelectedAccountSkin()) {
        CustomMessageBox::selectable(this, tr("Delete error"), tr("Can not delete skin that is in use."), QMessageBox::Warning)->exec();
        return;
    }

    auto skin = m_list.skin(m_selectedSkinKey);
    if (!skin)
        return;

    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\".\n"
                                                    "Are you sure?")
                                                     .arg(skin->name()),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response == QMessageBox::Yes) {
        if (!m_list.deleteSkin(m_selectedSkinKey, true)) {
            m_list.deleteSkin(m_selectedSkinKey, false);
        }
    }
}

void SkinManageDialog::on_urlBtn_clicked()
{
    auto url = QUrl(m_ui->urlLine->text());
    if (!url.isValid()) {
        CustomMessageBox::selectable(this, tr("Invalid url"), tr("Invalid url"), QMessageBox::Critical)->show();
        return;
    }

    NetJob::Ptr job{ new NetJob(tr("Download skin"), APPLICATION->network()) };
    job->setAskRetry(false);

    auto path = FS::PathCombine(m_list.getDir(), url.fileName());
    job->addNetAction(Net::Download::makeFile(url, path));
    ProgressDialog dlg(this);
    dlg.execWithTask(job.get());
    SkinModel s(path);
    if (!s.isValid()) {
        CustomMessageBox::selectable(this, tr("URL is not a valid skin"),
                                     QFileInfo::exists(path) ? tr("Skin images must be 64x64 or 64x32 pixel PNG files.")
                                                             : tr("Unable to download the skin: '%1'.").arg(m_ui->urlLine->text()),
                                     QMessageBox::Critical)
            ->show();
        QFile::remove(path);
        return;
    }
    m_ui->urlLine->setText("");
    if (QFileInfo(path).suffix().isEmpty()) {
        QFile::rename(path, path + ".png");
    }
}

class WaitTask : public Task {
   public:
    WaitTask() : m_loop(), m_done(false) {};
    virtual ~WaitTask() = default;

   public slots:
    void quit()
    {
        m_done = true;
        m_loop.quit();
    }

   protected:
    virtual void executeTask()
    {
        if (!m_done)
            m_loop.exec();
        emitSucceeded();
    };

   private:
    QEventLoop m_loop;
    bool m_done;
};

void SkinManageDialog::on_userBtn_clicked()
{
    auto user = m_ui->urlLine->text();
    if (user.isEmpty()) {
        return;
    }
    MinecraftProfile mcProfile;
    auto path = FS::PathCombine(m_list.getDir(), user + ".png");

    NetJob::Ptr job{ new NetJob(tr("Download user skin"), APPLICATION->network(), 1) };
    job->setAskRetry(false);

    auto uuidOut = std::make_shared<QByteArray>();
    auto profileOut = std::make_shared<QByteArray>();

    auto uuidLoop = makeShared<WaitTask>();
    auto profileLoop = makeShared<WaitTask>();

    auto getUUID = Net::Download::makeByteArray("https://api.mojang.com/users/profiles/minecraft/" + user, uuidOut);
    auto getProfile = Net::Download::makeByteArray(QUrl(), profileOut);
    auto downloadSkin = Net::Download::makeFile(QUrl(), path);

    QString failReason;

    connect(getUUID.get(), &Task::aborted, uuidLoop.get(), &WaitTask::quit);
    connect(getUUID.get(), &Task::failed, this, [&failReason](QString reason) {
        qCritical() << "Couldn't get user UUID:" << reason;
        failReason = tr("failed to get user UUID");
    });
    connect(getUUID.get(), &Task::failed, uuidLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::aborted, profileLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::failed, profileLoop.get(), &WaitTask::quit);
    connect(getProfile.get(), &Task::failed, this, [&failReason](QString reason) {
        qCritical() << "Couldn't get user profile:" << reason;
        failReason = tr("failed to get user profile");
    });
    connect(downloadSkin.get(), &Task::failed, this, [&failReason](QString reason) {
        qCritical() << "Couldn't download skin:" << reason;
        failReason = tr("failed to download skin");
    });

    connect(getUUID.get(), &Task::succeeded, this, [uuidLoop, uuidOut, job, getProfile, &failReason] {
        try {
            QJsonParseError parse_error{};
            QJsonDocument doc = QJsonDocument::fromJson(*uuidOut, &parse_error);
            if (parse_error.error != QJsonParseError::NoError) {
                qWarning() << "Error while parsing JSON response from Minecraft skin service at " << parse_error.offset
                           << " reason: " << parse_error.errorString();
                failReason = tr("failed to parse get user UUID response");
                uuidLoop->quit();
                return;
            }
            const auto root = doc.object();
            auto id = Json::ensureString(root, "id");
            if (!id.isEmpty()) {
                getProfile->setUrl("https://sessionserver.mojang.com/session/minecraft/profile/" + id);
            } else {
                failReason = tr("user id is empty");
                job->abort();
            }
        } catch (const Exception& e) {
            qCritical() << "Couldn't load skin json:" << e.cause();
            failReason = tr("failed to parse get user UUID response");
        }
        uuidLoop->quit();
    });

    connect(getProfile.get(), &Task::succeeded, this, [profileLoop, profileOut, job, getProfile, &mcProfile, downloadSkin, &failReason] {
        if (Parsers::parseMinecraftProfileMojang(*profileOut, mcProfile)) {
            downloadSkin->setUrl(mcProfile.skin.url);
        } else {
            failReason = tr("failed to parse get user profile response");
            job->abort();
        }
        profileLoop->quit();
    });

    job->addNetAction(getUUID);
    job->addTask(uuidLoop);
    job->addNetAction(getProfile);
    job->addTask(profileLoop);
    job->addNetAction(downloadSkin);
    ProgressDialog dlg(this);
    dlg.execWithTask(job.get());

    SkinModel s(path);
    if (!s.isValid()) {
        if (failReason.isEmpty()) {
            failReason = tr("the skin is invalid");
        }
        CustomMessageBox::selectable(this, tr("Username not found"),
                                     tr("Unable to find the skin for '%1'\n because: %2.").arg(user, failReason), QMessageBox::Critical)
            ->show();
        QFile::remove(path);
        return;
    }
    m_ui->urlLine->setText("");
    s.setModel(mcProfile.skin.variant.toUpper() == "SLIM" ? SkinModel::SLIM : SkinModel::CLASSIC);
    s.setURL(mcProfile.skin.url);
    if (m_capes.contains(mcProfile.currentCape)) {
        s.setCapeId(mcProfile.currentCape);
    }
    m_list.updateSkin(&s);
}

void SkinManageDialog::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    QSize s = size() * (1. / 3);

    m_ui->selectedModel->updateScene(getSelectedSkin());
    auto id = m_ui->capeCombo->currentData();
    auto cape = m_capes.value(id.toString(), {});
    if (!cape.isNull()) {
        m_ui->capeImage->setPixmap(previewCape(cape).scaled(s, Qt::KeepAspectRatio, Qt::FastTransformation));
    }
    m_ui->selectedModel->updateCape(cape);
}

SkinModel* SkinManageDialog::getSelectedSkin()
{
    if (auto skin = m_list.skin(m_selectedSkinKey); skin && skin->isValid()) {
        return skin;
    }
    return nullptr;
}

QHash<QString, QImage> SkinManageDialog::capes()
{
    return m_capes;
}
