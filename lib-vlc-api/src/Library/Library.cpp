/*****************************************************************************
 * Library.cpp: Multimedia library
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

/** \file
  * This file the library contains class implementation.
  * It's the backend part of the Library widget of vlmc.
  * It can load and unload Medias (Medias.h/Media.cpp)
  * It can load and unload Clips (Clip.h/Clip.cpp)
  */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Library.h"
#include "Media/Clip.h"
#include "Media/Media.h"
#include "MediaLibraryModel.h"
#include "Project/Project.h"
#include "Settings/Settings.h"
#include "Tools/VlmcDebug.h"

#include <QVariant>
#include <QHash>
#include <QUuid>

Library::Library( Settings* vlmcSettings, Settings *projectSettings )
    : m_initialized( false )
    , m_cleanState( true )
    , m_settings( new Settings )
{
    // Setting up the external media library
    m_ml.reset( NewMediaLibrary() );
    m_ml->setVerbosity( medialibrary::LogLevel::Warning );
    m_model = new MediaLibraryModel( *m_ml, this );

    auto s = vlmcSettings->createVar( SettingValue::List, QStringLiteral( "vlmc/mlDirs" ), QVariantList(),
                        "Media Library folders", "List of folders VLMC will search for media files",
                         SettingValue::Folders );
    connect( s, &SettingValue::changed, this, &Library::mlDirsChanged );
    auto ws = vlmcSettings->value( "vlmc/WorkspaceLocation" );
    connect( ws, &SettingValue::changed, this, &Library::workspaceChanged );

    // Setting up the project section of the Library
    m_settings->createVar( SettingValue::List, QStringLiteral( "medias" ), QVariantList(), "", "", SettingValue::Nothing );
    connect( m_settings.get(), &Settings::postLoad, this, &Library::postLoad, Qt::DirectConnection );
    connect( m_settings.get(), &Settings::preSave, this, &Library::preSave, Qt::DirectConnection );
    projectSettings->addSettings( QStringLiteral( "Library" ), *m_settings );
}

void
Library::preSave()
{
    QVariantList l;
    for ( auto val : m_media )
        l << val->toVariant();
    m_settings->value( "medias" )->set( l );
    setCleanState( true );
}

void
Library::postLoad()
{
    for ( const auto& var : m_settings->value( "medias" )->get().toList() )
    {
        auto map = var.toMap();
        auto m = Media::fromVariant( map );
        addMedia( m );
        if ( map.contains( "clips" ) == true )
        {
            const auto& subClipsList = map["clips"].toList();
            for ( const auto& subClip : subClipsList )
                m->loadSubclip( subClip.toMap() );
        }
    }
}

Library::~Library()
{
}

void
Library::addMedia( QSharedPointer<Media> media )
{
    setCleanState( false );
    if ( m_media.contains( media->id() ) )
        return;
    m_media[media->id()] = media;
    m_clips[media->baseClip()->uuid()] = media->baseClip();
    emit clipAdded( media->baseClip()->uuid().toString() );
    vlmcDebug() << "Clip" << media->baseClip()->uuid().toString() << "is added to Library";
    connect( media.data(), &Media::subclipAdded, [this]( QSharedPointer<Clip> c ) {
        m_clips[c->uuid()] = c;
        emit clipAdded( c->uuid().toString() );
        vlmcDebug() << "Clip" << c->uuid().toString() << "is added to Library";
        setCleanState( false );
    });
    connect( media.data(), &Media::subclipRemoved, [this]( const QUuid& uuid ) {
        m_clips.remove( uuid );
        emit clipRemoved( uuid.toString() );
        vlmcDebug() << "Clip" << uuid.toString() << "is removed in Library";
        // This seems wrong, for instance if we undo a clip splitting
        setCleanState( false );
    } );
}

bool
Library::isInCleanState() const
{
    return m_cleanState;
}

QSharedPointer<Media>
Library::media( qint64 mediaId )
{
    return m_media.value( mediaId );
}

medialibrary::MediaPtr
Library::mlMedia( qint64 mediaId )
{
    return m_ml->media( mediaId );
}

MediaLibraryModel*
Library::model() const
{
    return m_model;
}

QSharedPointer<Clip>
Library::clip( const QUuid& uuid )
{
    return m_clips.value( uuid );

}

void
Library::clear()
{
    m_media.clear();
    m_clips.clear();
    setCleanState( true );
}

void
Library::setCleanState( bool newState )
{
    if ( newState != m_cleanState )
    {
        m_cleanState = newState;
        emit cleanStateChanged( newState );
    }
}

void
Library::mlDirsChanged(const QVariant& value)
{
    // We can't handle this event without an initialized media library, and therefor without a valid
    // workspace. In theory, the workspace SettingValue is created before the mlDirs,
    // so it should be loaded before.
    Q_ASSERT( m_initialized == true );

    const auto list = value.toStringList();
    Q_ASSERT( list.empty() == false );
    for ( const auto f : list )
        // To use FsDiscoverer
        m_ml->discover( std::string( "file://" ) + f.toStdString() );
}

void
Library::workspaceChanged(const QVariant& workspace)
{
    Q_ASSERT( workspace.isNull() == false && workspace.canConvert<QString>() );

    if ( m_initialized == false )
    {
        auto w = workspace.toString().toStdString();
        Q_ASSERT( w.empty() == false );
        // Initializing the medialibrary doesn't start new folders discovery.
        // This will happen after the first call to IMediaLibrary::discover()
        m_ml->initialize( w + "/ml.db", w + "/thumbnails/", this );
        m_initialized = true;
        m_ml->start();
        m_ml->reload();
    }
    //else FIXME, and relocate the media library
}

void
Library::onMediaAdded( std::vector<medialibrary::MediaPtr> mediaList )
{
    for ( auto m : mediaList )
    {
        QMetaObject::invokeMethod( m_model, "addMedia",
                                   Qt::QueuedConnection,
                                   Q_ARG( medialibrary::MediaPtr, m ) );
    }
}

void
Library::onMediaUpdated( std::vector<medialibrary::MediaPtr> mediaList )
{
    for ( auto m : mediaList )
    {
        QMetaObject::invokeMethod( m_model, "updateMedia",
                                   Qt::QueuedConnection,
                                   Q_ARG( medialibrary::MediaPtr, m ) );
    }
}

void
Library::onMediaDeleted( std::vector<int64_t> mediaList )
{
    for ( auto id : mediaList )
        QMetaObject::invokeMethod( m_model, "removeMedia",
                                   Qt::QueuedConnection,
                                   Q_ARG( int64_t, id ) );
}

void
Library::onMediaThumbnailReady( medialibrary::MediaPtr media, bool success )
{
}

void
Library::onArtistsAdded( std::vector<medialibrary::ArtistPtr> )
{
}

void
Library::onArtistsModified( std::vector<medialibrary::ArtistPtr> )
{
}

void
Library::onArtistsDeleted( std::vector<int64_t> )
{
}

void
Library::onAlbumsAdded( std::vector<medialibrary::AlbumPtr> )
{
}

void
Library::onAlbumsModified( std::vector<medialibrary::AlbumPtr> )
{
}

void
Library::onAlbumsDeleted( std::vector<int64_t> )
{
}

void
Library::onTracksAdded( std::vector<medialibrary::AlbumTrackPtr> )
{
}

void
Library::onTracksDeleted( std::vector<int64_t> )
{
}

void
Library::onDiscoveryStarted( const std::string& )
{
}

void
Library::onDiscoveryProgress( const std::string& entryPoint )
{
    emit discoveryProgress( QString::fromStdString( entryPoint ) );
}

void
Library::onDiscoveryCompleted( const std::string& entryPoint )
{
    if ( entryPoint.empty() == true )
        QMetaObject::invokeMethod( m_model, "refresh",
                                   Qt::QueuedConnection );

    emit discoveryCompleted( QString::fromStdString( entryPoint ) );
}

void
Library::onParsingStatsUpdated( uint32_t percent )
{
    emit progressUpdated( static_cast<int>( percent ) );
}

void
Library::onPlaylistsAdded( std::vector<medialibrary::PlaylistPtr> )
{
}

void
Library::onPlaylistsModified( std::vector<medialibrary::PlaylistPtr> )
{
}

void
Library::onPlaylistsDeleted( std::vector<int64_t> )
{
}

void
Library::onReloadStarted( const std::string& )
{
}

void
Library::onReloadCompleted( const std::string& entryPoint )
{
    if ( entryPoint.empty() == true )
    {
        for ( auto media : m_ml->videoFiles() )
            QMetaObject::invokeMethod( m_model, "addMedia",
                                       Qt::QueuedConnection,
                                       Q_ARG( medialibrary::MediaPtr, media ) );

        for ( auto media : m_ml->audioFiles() )
            QMetaObject::invokeMethod( m_model, "addMedia",
                                       Qt::QueuedConnection,
                                       Q_ARG( medialibrary::MediaPtr, media ) );
    }
}

void
Library::onEntryPointRemoved( const std::string&, bool )
{
}

void
Library::onEntryPointBanned( const std::string&, bool )
{
}

void
Library::onEntryPointUnbanned( const std::string&, bool )
{
}

void
Library::onBackgroundTasksIdleChanged( bool )
{

}
