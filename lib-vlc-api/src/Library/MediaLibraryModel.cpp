/*****************************************************************************
 * MediaLibraryModel.cpp
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "config.h"

#include "MediaLibraryModel.h"
#include <QUrl>

MediaLibraryModel::MediaLibraryModel( medialibrary::IMediaLibrary& ml, QObject *parent )
    : QAbstractListModel(parent)
    , m_ml( ml )
{
    qRegisterMetaType<medialibrary::MediaPtr>();
}

void MediaLibraryModel::addMedia( medialibrary::MediaPtr media )
{
    auto size = m_media.size();
    beginInsertRows( QModelIndex(), size, size );
    m_media.push_back( media );
    endInsertRows();
}

medialibrary::MediaPtr
MediaLibraryModel::findMedia( qint64 mediaId )
{
    auto it = std::find_if( begin( m_media ), end( m_media ), [mediaId](medialibrary::MediaPtr m) {
        return m->id() == mediaId;
    });
    if ( it == end( m_media ) )
        return nullptr;
    return *it;
}

void MediaLibraryModel::updateMedia( medialibrary::MediaPtr media )
{
    auto m = createIndex( media->id(), 0 );
    emit dataChanged( m, m );
}

bool MediaLibraryModel::removeMedia( int64_t mediaId )
{
    auto it = std::find_if( begin( m_media ), end( m_media ), [mediaId](medialibrary::MediaPtr m) {
        return m->id() == mediaId;
    });
    if ( it == end( m_media ) )
        return false;
    auto idx = it - begin( m_media );
    beginRemoveRows(QModelIndex(), idx, idx );
    m_media.erase( it );
    endRemoveRows();
    return true;
}

int MediaLibraryModel::rowCount( const QModelIndex& index ) const
{
    // A valid index is any row, which doesn't have child
    // An invalid index is the root node, which does have children
    if ( index.isValid() == true )
        return 0;
    return m_media.size();
}

QVariant MediaLibraryModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() || index.row() < 0 || index.row() >= m_media.size() )
        return QVariant();

    medialibrary::MediaPtr m;
    m = m_media.at( static_cast<size_t>( index.row() ) );

    switch ( role )
    {
    case Qt::DisplayRole:
    case Roles::Title:
        return QVariant( QUrl::fromPercentEncoding( QByteArray( m->title().c_str() ) ) );
#ifdef WITH_GUI
    case Qt::DecorationRole:
        return QPixmap( QString::fromStdString( m->thumbnail() ) );
#endif
    case Roles::ThumbnailPath:
        return QVariant( QString::fromStdString( m->thumbnail() ) );
    case Roles::Duration:
        return QVariant::fromValue( m->duration() );
    case Roles::Id:
        return QVariant::fromValue( m->id() );
    case Qt::UserRole:
        return QVariant::fromValue( m );
    default:
        break;
    }

    return QVariant();
}

QHash<int, QByteArray>
MediaLibraryModel::roleNames() const
{
    return {
        { Roles::Title, "title" },
        { Roles::ThumbnailPath, "thumbnailPath" },
        { Roles::Duration, "duration" },
        { Roles::Id, "id" }
    };
}

void MediaLibraryModel::refresh()
{
    beginResetModel();
    const auto& audioFiles = m_ml.audioFiles();
    const auto& videoFiles = m_ml.videoFiles();
    m_media.insert( m_media.end(), audioFiles.begin(), audioFiles.end() );
    m_media.insert( m_media.end(), videoFiles.begin(), videoFiles.end() );
    endResetModel();
}
