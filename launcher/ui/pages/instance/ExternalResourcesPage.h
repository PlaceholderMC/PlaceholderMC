#pragma once

#include <QMainWindow>
#include <QSortFilterProxyModel>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/Setting.h"
#include "ui/pages/BasePage.h"

class ResourceFolderModel;

namespace Ui {
class ExternalResourcesPage;
}

/* This page is used as a base for pages in which the user can manage external resources
 * related to the game, such as mods, shaders or resource packs. */
class ExternalResourcesPage : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit ExternalResourcesPage(BaseInstance* instance, std::shared_ptr<ResourceFolderModel> model, QWidget* parent = nullptr);
    virtual ~ExternalResourcesPage();

    virtual QString displayName() const override = 0;
    virtual QIcon icon() const override = 0;
    virtual QString id() const override = 0;
    virtual QString helpPage() const override = 0;

    virtual bool shouldDisplay() const override = 0;
    QString extraHeaderInfoString();

    void openedImpl() override;
    void closedImpl() override;

    void retranslate() override;

   protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    bool listFilter(QKeyEvent* ev);
    QMenu* createPopupMenu() override;

   public slots:
    virtual void updateActions();
    virtual void updateFrame(const QModelIndex& current, const QModelIndex& previous);

   protected slots:
    void itemActivated(const QModelIndex& index);
    void filterTextChanged(const QString& newContents);

    virtual void addItem();
    void removeItem();
    virtual void removeItems(const QItemSelection& selection);

    virtual void enableItem();
    virtual void disableItem();

    virtual void viewHomepage();

    virtual void viewFolder();
    virtual void viewConfigs();

    void ShowContextMenu(const QPoint& pos);
    void ShowHeaderContextMenu(const QPoint& pos);

    void enableUpdates();
    void disableUpdates();

    void addCategory();
    void removeCategory();
    void removeAllCategories();

   protected:
    BaseInstance* m_instance = nullptr;

    Ui::ExternalResourcesPage* ui = nullptr;
    std::shared_ptr<ResourceFolderModel> m_model;
    QSortFilterProxyModel* m_filterModel = nullptr;

    QString m_fileSelectionFilter;
    QString m_viewFilter;

    std::shared_ptr<Setting> m_wide_bar_setting = nullptr;
};
