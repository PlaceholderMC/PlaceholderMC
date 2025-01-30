#pragma once

#include "Resource.h"
#include "minecraft/mod/DataPack.h"

#include <QImage>
#include <QMutex>
#include <QPixmap>
#include <QPixmapCache>

class Version;

/* TODO:
 *
 * Store localized descriptions
 * */

class ResourcePack : public DataPack {
    Q_OBJECT
   public:
    ResourcePack(QObject* parent = nullptr) : DataPack(parent) {}
    ResourcePack(QFileInfo file_info) : DataPack(file_info) {}

    /** Gets, respectively, the lower and upper versions supported by the set pack format. */
    [[nodiscard]] std::pair<Version, Version> compatibleVersions() const;

    /** Gets the image of the resource pack, converted to a QPixmap for drawing, and scaled to size. */
    [[nodiscard]] QPixmap image(QSize size, Qt::AspectRatioMode mode = Qt::AspectRatioMode::IgnoreAspectRatio) const;

    /** Thread-safe. */
    void setImage(QImage new_image) const;

    /** Thread-safe. */
    void setPackFormat(int new_format_id);

    virtual QString directory() { return "/assets"; }

   protected:
    /** The resource pack's image file cache key, for access in the QPixmapCache global instance.
     *
     *  The 'was_ever_used' state simply identifies whether the key was never inserted on the cache (true),
     *  so as to tell whether a cache entry is inexistent or if it was just evicted from the cache.
     */
    struct {
        QPixmapCache::Key key;
        bool was_ever_used = false;
    } mutable m_pack_image_cache_key;
};
