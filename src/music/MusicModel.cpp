#include "MusicModel.h"

#include "globals/Globals.h"
#include "globals/Helper.h"
#include "music/Album.h"

MusicModel::MusicModel(QObject* parent) :
    QAbstractItemModel(parent),
    m_rootItem{new MusicModelItem(nullptr)},
    m_newIcon{QIcon(":/img/star_blue.png")}
{
}

MusicModel::~MusicModel()
{
    delete m_rootItem;
}

int MusicModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_rootItem->columnCount();
}

QVariant MusicModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    MusicModelItem* item = getItem(index);

    if (role == Qt::DisplayRole) {
        return helper::appendArticle(item->data(0).toString());
    }
    if (role == MusicRoles::Type) {
        return static_cast<int>(item->type());
    }
    if (role == MusicRoles::IsNew) {
        return item->data(role);
    }
    if (role == Qt::ForegroundRole) {
        if (item->data(MusicRoles::HasChanged).toBool()) {
            return QColor(255, 0, 0);
        }
        return QColor(17, 51, 80);
    }
    if (role == Qt::FontRole) {
        QFont font;
        if (item->data(MusicRoles::HasChanged).toBool()) {
            font.setItalic(true);
        }
        if (MusicType(item->data(MusicRoles::Type).toInt()) == MusicType::Artist) {
            font.setBold(true);
        } else {
#ifdef Q_OS_MAC
            font.setPointSize(font.pointSize() - 2);
#endif
        }
        return font;
    }
    if (role == Qt::SizeHintRole) {
#ifdef Q_OS_WIN
        return QSize(0, 22);
#else
        return QSize(0, (MusicType(item->data(MusicRoles::Type).toInt()) == MusicType::Artist) ? 44 : 22);
#endif
    }
    if (role == MusicRoles::NumOfAlbums) {
        if (MusicType(item->data(MusicRoles::Type).toInt()) == MusicType::Artist) {
            return item->data(MusicRoles::NumOfAlbums);
        }
    } else if (role == MusicRoles::SelectionForeground) {
        return QColor(255, 255, 255);
    } else if (role == Qt::DecorationRole) {
#ifdef Q_OS_WIN
        if (item->data(MusicRoles::IsNew).toBool())
            return m_newIcon;
#endif
    }

    return QVariant();
}

MusicModelItem* MusicModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        auto item = static_cast<MusicModelItem*>(index.internalPointer());
        if (item != nullptr) {
            return item;
        }
    }
    return m_rootItem;
}

QModelIndex MusicModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid() && parent.column() != 0) {
        return {};
    }

    MusicModelItem* parentItem = getItem(parent);
    MusicModelItem* childItem = parentItem->child(row);
    if (childItem != nullptr) {
        return createIndex(row, column, childItem);
    }
    return {};
}

MusicModelItem* MusicModel::appendChild(Artist* artist)
{
    MusicModelItem* parentItem = m_rootItem;
    beginInsertRows(QModelIndex(), parentItem->childCount(), parentItem->childCount());
    MusicModelItem* item = parentItem->appendChild(artist);
    endInsertRows();
    connect(item, &MusicModelItem::sigChanged, this, &MusicModel::onSigChanged);
    connect(artist, &Artist::sigChanged, this, &MusicModel::onArtistChanged);
    connect(artist->controller(), &ArtistController::sigSaved, this, &MusicModel::onArtistChanged);
    connect(item, &MusicModelItem::sigIntChanged, this, &MusicModel::onSigChanged);
    return item;
}

QModelIndex MusicModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    }

    MusicModelItem* childItem = getItem(index);
    MusicModelItem* parentItem = childItem->parent();

    if (parentItem == m_rootItem) {
        return {};
    }

    return createIndex(parentItem->childNumber(), 0, parentItem);
}

bool MusicModel::removeRows(int position, int rows, const QModelIndex& parent)
{
    MusicModelItem* parentItem = getItem(parent);

    beginRemoveRows(parent, position, position + rows - 1);
    bool success = parentItem->removeChildren(position, rows);
    endRemoveRows();

    return success;
}

int MusicModel::rowCount(const QModelIndex& parent) const
{
    MusicModelItem* parentItem = getItem(parent);
    return parentItem->childCount();
}

void MusicModel::clear()
{
    beginRemoveRows(QModelIndex(), 0, m_rootItem->childCount());
    m_rootItem->removeChildren(0, m_rootItem->childCount());
    endRemoveRows();
}

void MusicModel::onSigChanged(MusicModelItem* artistItem, MusicModelItem* albumItem)
{
    QModelIndex artistIndex = this->index(artistItem->childNumber(), 0);
    QModelIndex index = this->index(albumItem->childNumber(), 0, artistIndex);
    emit dataChanged(index, index);
}

void MusicModel::onArtistChanged(Artist* artist)
{
    QModelIndex index = this->index(artist->modelItem()->childNumber(), 0);
    emit dataChanged(index, index);
}

QVector<Artist*> MusicModel::artists()
{
    QVector<Artist*> artists;
    for (int i = 0, n = m_rootItem->childCount(); i < n; ++i) {
        artists.append(m_rootItem->child(i)->artist());
    }
    return artists;
}

void MusicModel::removeArtist(Artist* artist)
{
    for (int i = 0, n = m_rootItem->childCount(); i < n; ++i) {
        if (m_rootItem->child(i)->artist() == artist) {
            removeRow(m_rootItem->child(i)->childNumber());
            return;
        }
    }
}

int MusicModel::hasNewArtistsOrAlbums()
{
    int newItems = 0;

    for (Artist* artist : artists()) {
        if (!artist->controller()->infoLoaded()) {
            newItems++;
        }
        for (int i = 0, n = artist->modelItem()->childNumber(); i < n; ++i) {
            if ((artist->modelItem()->child(i) != nullptr) && (artist->modelItem()->child(i)->album() != nullptr)
                && !artist->modelItem()->child(i)->album()->controller()->infoLoaded()) {
                newItems++;
            }
        }
    }

    return newItems;
}
