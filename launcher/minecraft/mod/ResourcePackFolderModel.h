#pragma once

#include "ResourceFolderModel.h"

#include "ResourcePack.h"

class ResourcePackFolderModel : public ResourceFolderModel {
    Q_OBJECT
   public:
    enum Columns {
        ActiveColumn = 0,
        ImageColumn,
        NameColumn,
        PackFormatColumn,
        DateColumn,
        ProviderColumn,
        SizeColumn,
        LockUpdateCoumn,
        NUM_COLUMNS
    };

    explicit ResourcePackFolderModel(const QDir& dir, BaseInstance* instance, bool is_indexed, bool create_dir, QObject* parent = nullptr);

    QString id() const override { return "resourcepacks"; }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] Resource* createResource(const QFileInfo& file) override { return new ResourcePack(file); }
    [[nodiscard]] Task* createParseTask(Resource&) override;

    RESOURCE_HELPERS(ResourcePack)
};
