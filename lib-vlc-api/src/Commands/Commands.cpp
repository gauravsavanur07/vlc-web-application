/*****************************************************************************
 * Commands.cpp: Contains all the implementation of VLMC commands.
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *          Ludovic Fauvet <etix@l0cal.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Commands.h"
#include "Project/Project.h"
#include "Main/Core.h"
#include "Media/Clip.h"
#include "Media/Media.h"
#include "EffectsEngine/EffectHelper.h"
#include "Workflow/SequenceWorkflow.h"
#include "Workflow/MainWorkflow.h"
#include "AbstractUndoStack.h"
#include "Backend/IFilter.h"
#include "Library/Library.h"
#include "Transition/Transition.h"

#ifdef HAVE_GUI
# include "Gui/timeline/MarkerManager.h"
#endif

Commands::Generic::Generic() :
        m_valid( true )
{
    //This is connected using a direct connection to ensure the view can be refreshed
    //just after the signal has been emited.

    //FIXME: there is no signal retranslateRequired in QUndoStack class
    //       <3 qt4 connects
    // connect( Core::instance()->undoStack(), SIGNAL( retranslateRequired() ),
    //          this, SLOT( retranslate() ), Qt::DirectConnection );
}

void
Commands::Generic::invalidate()
{
    setText( tr( "Invalid action" ) );
    m_valid = false;
    emit invalidated();
}

bool
Commands::Generic::isValid() const
{
    return m_valid;
}

#ifndef HAVE_GUI
void
Commands::Generic::setText( const QString& text )
{
    m_text = text;
}

QString
Commands::Generic::text() const
{
    return m_text;
}
#endif

void
Commands::Generic::redo()
{
    if ( m_valid == true )
        internalRedo();
}

void
Commands::Generic::undo()
{
    if ( m_valid == true )
        internalUndo();
}

Commands::Clip::Add::Add( std::shared_ptr<SequenceWorkflow> const& workflow,
                          const QUuid& uuid, quint32 trackId, qint32 pos ) :
        m_workflow( workflow ),
        m_libraryUuid( uuid ),
        m_trackId( trackId ),
        m_pos( pos )
{
    retranslate();
}

void
Commands::Clip::Add::internalRedo()
{
    auto clip = Core::instance()->library()->clip( m_libraryUuid );
    if ( clip == nullptr )
    {
        invalidate();
        return;
    }
    if ( clip->media()->hasAudioTracks() )
    {
        // In case we are redoing, we are feeding the addClip method with the previously generated UUID
        // so that future operations could still rely on the same instance being present
        m_audioInstanceUuid = m_workflow->addClip( clip, m_trackId, m_pos, m_audioInstanceUuid, true );
        if ( m_audioInstanceUuid.isNull() == true )
            invalidate();
    }
    if ( clip->media()->hasVideoTracks() )
    {
        m_videoInstanceUuid = m_workflow->addClip( clip, m_trackId, m_pos, m_videoInstanceUuid, false );
        if ( m_videoInstanceUuid.isNull() == true )
            invalidate();
    }
    if ( m_audioInstanceUuid.isNull() == false && m_videoInstanceUuid.isNull() == false )
        m_workflow->linkClips( m_audioInstanceUuid, m_videoInstanceUuid );
}

void
Commands::Clip::Add::internalUndo()
{
    if ( m_audioInstanceUuid.isNull() == false &&
         m_workflow->removeClip( m_audioInstanceUuid ) == nullptr )
            invalidate();
    if ( m_videoInstanceUuid.isNull() == false &&
         m_workflow->removeClip( m_videoInstanceUuid ) == nullptr )
            invalidate();
}

void
Commands::Clip::Add::retranslate()
{
    setText( tr( "Adding clip to track %1" ).arg( m_trackId ) );
}

Commands::Clip::Move::Move(  std::shared_ptr<SequenceWorkflow> const& workflow,
                             const QString& uuid, quint32 trackId, qint64 pos ) :
    m_workflow( workflow ),
    m_infos( {{ uuid, trackId, workflow->trackId( uuid ),pos, workflow->position( uuid ) }} )
{
    retranslate();
}

void
Commands::Clip::Move::retranslate()
{
    setText( QObject::tr( "Moving clip(s)", "", m_infos.count() ) );
}

int
Commands::Clip::Move::id() const
{
    return static_cast<int>( Commands::Id::Move );
}

bool
Commands::Clip::Move::mergeWith( const Generic* command )
{
    auto cmd = static_cast<const Move*>( command );
    if ( cmd->m_infos.count() > 1 )
        return false;
    const auto& clip = m_workflow->clip( m_infos[0].uuid );
    const auto& linkedClips = clip->linkedClips;
    if ( linkedClips.contains( cmd->m_infos[0].uuid ) == false )
        return false;
    m_infos += cmd->m_infos[0];
    return true;
}

void
Commands::Clip::Move::internalRedo()
{
    for ( const auto& info : m_infos )
    {
        if ( m_workflow->moveClip( info.uuid, info.newTrackId, info.newPos ) == false )
            invalidate();
    }
}

void
Commands::Clip::Move::internalUndo()
{
    for ( const auto& info : m_infos )
    {
        if ( m_workflow->moveClip( info.uuid, info.oldTrackId, info.oldPos ) == false )
            invalidate();
    }
}

Commands::Clip::Remove::Remove( std::shared_ptr<SequenceWorkflow> const& workflow,
                                const QUuid& uuid )
    : m_workflow( workflow )
{
    auto clip = workflow->clip( uuid );
    m_clips.append( clip );
    retranslate();
}

void
Commands::Clip::Remove::retranslate()
{
    setText( tr( "Removing clip " ) );
}

int
Commands::Clip::Remove::id() const
{
    return static_cast<int>( Commands::Id::Remove );
}

bool
Commands::Clip::Remove::mergeWith( const Generic* command )
{
    auto cmd = static_cast<const Remove*>( command );
    if ( cmd->m_clips.count() > 1 )
        return false;
    const auto& linkedClips = m_clips[0]->linkedClips;
    if ( linkedClips.contains( cmd->m_clips[0]->uuid ) == false )
        return false;
    m_clips += cmd->m_clips[0];
    return true;
}

void
Commands::Clip::Remove::internalRedo()
{
    for ( const auto& clip : m_clips )
        m_workflow->removeClip( clip->uuid );
}

void
Commands::Clip::Remove::internalUndo()
{
    for ( const auto& clip : m_clips )
    {
        auto uuid = m_workflow->addClip( clip->clip, clip->trackId, clip->pos, clip->uuid, clip->isAudio );
        if ( uuid.isNull() == true )
            invalidate();
    }
}

Commands::Clip::Resize::Resize( std::shared_ptr<SequenceWorkflow> const& workflow,
                                const QUuid& uuid, qint64 newBegin, qint64 newEnd, qint64 newPos ) :
    m_workflow( workflow )
{
    auto clip = workflow->clip( uuid );
    if ( clip == nullptr )
    {
        invalidate();
        return;
    }
    m_infos.append( Info{
        clip,
        newBegin,
        clip->clip->begin(),
        newEnd,
        clip->clip->end(),
        newPos,
        clip->pos
    });
    retranslate();
}

void
Commands::Clip::Resize::retranslate()
{
    setText( tr( "Resizing clip" ) );
}

bool
Commands::Clip::Resize::mergeWith( const Generic* command )
{
    auto cmd = static_cast<const Resize*>( command );
    if ( cmd->m_infos.count() > 1 )
        return false;
    const auto& linkedClips = m_infos[0].clip->linkedClips;
    if ( linkedClips.contains( cmd->m_infos[0].clip->uuid ) == false )
        return false;
    m_infos += cmd->m_infos[0];
    return true;
}

int
Commands::Clip::Resize::id() const
{
    return static_cast<int>( Commands::Id::Resize );
}

void
Commands::Clip::Resize::internalRedo()
{
    for ( const auto& info : m_infos )
    {
        if ( m_workflow->resizeClip( info.clip->uuid, info.newBegin, info.newEnd, info.newPos ) == false )
            invalidate();
    }
}

void
Commands::Clip::Resize::internalUndo()
{
    for ( const auto& info : m_infos )
    {
        if ( m_workflow->resizeClip( info.clip->uuid, info.oldBegin, info.oldEnd, info.oldPos ) == false )
            invalidate();
    }
}

Commands::Clip::Split::Split( std::shared_ptr<SequenceWorkflow> const& workflow,
                              const QUuid& uuid, qint64 newClipPos, qint64 newClipBegin ) :
    m_workflow( workflow ),
    m_toSplit( workflow->clip( uuid ) ),
    m_trackId( workflow->trackId( uuid ) ),
    m_newClip( nullptr ),
    m_newClipPos( newClipPos ),
    m_newClipBegin( newClipBegin )
{
    if ( !m_toSplit )
    {
        invalidate();
        return;
    }
    m_newClip = m_toSplit->clip->media()->cut( newClipBegin - m_toSplit->clip->begin(),
                                         m_toSplit->clip->end() - m_toSplit->clip->begin() );
    m_oldEnd = m_toSplit->clip->end();
    retranslate();
}

void
Commands::Clip::Split::retranslate()
{
    setText( tr("Splitting clip") );
}

void
Commands::Clip::Split::internalRedo()
{
    if ( !m_toSplit )
    {
        invalidate();
        return;
    }
    //If we don't remove 1, the clip will end exactly at the starting frame (ie. they will
    //be rendering at the same time)
    bool ret = m_workflow->resizeClip( m_toSplit->uuid, m_toSplit->clip->begin(),
                                       m_newClipBegin - 1, m_toSplit->pos );
    if ( ret == false )
    {
        invalidate();
        return;
    }

    m_newClipInstanceUuid = m_workflow->addClip( m_newClip, m_trackId, m_newClipPos, m_newClipInstanceUuid,
                                                 m_toSplit->isAudio );
    if ( m_newClipInstanceUuid.isNull() == true )
        invalidate();
}

void
Commands::Clip::Split::internalUndo()
{
    if ( m_workflow->removeClip( m_newClipInstanceUuid ) == nullptr )
    {
        invalidate();
        return;
    }
    m_workflow->resizeClip( m_toSplit->uuid, m_toSplit->clip->begin(),
                            m_oldEnd, m_toSplit->pos );
}

Commands::Clip::Link::Link( std::shared_ptr<SequenceWorkflow> const& workflow,
                            const QUuid& clipA, const QUuid& clipB )
    : m_workflow( workflow )
    , m_clipA( clipA )
    , m_clipB( clipB )
{
    retranslate();
}

void
Commands::Clip::Link::retranslate()
{
    setText( tr( "Linking clip" ) );
}

void
Commands::Clip::Link::internalRedo()
{
    auto ret = m_workflow->linkClips( m_clipA, m_clipB );
    if ( ret == false )
        invalidate();
}

void
Commands::Clip::Link::internalUndo()
{
    auto ret = m_workflow->unlinkClips( m_clipA, m_clipB );
    if ( ret == false )
        invalidate();
}

Commands::Clip::Unlink::Unlink( std::shared_ptr<SequenceWorkflow> const& workflow,
                            const QUuid& clipA, const QUuid& clipB )
    : m_workflow( workflow )
    , m_clipA( clipA )
    , m_clipB( clipB )
{
    retranslate();
}

void
Commands::Clip::Unlink::retranslate()
{
    setText( tr( "Unlinking clips" ) );
}

void
Commands::Clip::Unlink::internalRedo()
{
    auto ret = m_workflow->unlinkClips( m_clipA, m_clipB );
    if ( ret == false )
        invalidate();
}

void
Commands::Clip::Unlink::internalUndo()
{
    auto ret = m_workflow->linkClips( m_clipA, m_clipB );
    if ( ret == false )
        invalidate();
}

Commands::Effect::Add::Add( std::shared_ptr<EffectHelper> const& helper, Backend::IInput* target )
    : m_helper( helper )
    , m_target( target )
{
    retranslate();
}

void
Commands::Effect::Add::retranslate()
{
    setText( tr( "Adding effect %1" ).arg( m_helper->name() ) );
}

void
Commands::Effect::Add::internalRedo()
{
    m_target->attach( *m_helper->filter() );
}

void
Commands::Effect::Add::internalUndo()
{
    m_target->detach( *m_helper->filter() );
}

Commands::Effect::Move::Move( std::shared_ptr<EffectHelper> const& helper, std::shared_ptr<Backend::IInput> const& from, Backend::IInput* to,
                              qint64 pos)
    : m_helper( helper )
    , m_from( from )
    , m_to( to )
    , m_newPos( pos )
{
    m_oldPos = helper->begin();
    m_oldEnd = helper->end();
    m_newEnd = helper->end() - helper->begin() + pos;
    retranslate();
}

void
Commands::Effect::Move::retranslate()
{
    setText( tr( "Moving effect %1" ).arg( m_helper->name() ) );
}

void
Commands::Effect::Move::internalRedo()
{
    if ( m_from->sameClip( *m_to ) == false )
    {
        m_from->detach( *m_helper->filter() );
        m_to->attach( *m_helper->filter() );
        m_helper->setBoundaries( m_newPos, m_newEnd );

    }
    else
        m_helper->setBoundaries( m_newPos, m_newEnd );
}

void
Commands::Effect::Move::internalUndo()
{
    if ( m_from->sameClip( *m_to ) == false )
    {
        m_to->detach( *m_helper->filter() );
        m_from->attach( *m_helper->filter() );
        m_helper->setBoundaries( m_oldPos, m_oldEnd );
    }
    else
        m_helper->setBoundaries( m_oldPos, m_oldEnd );
}

Commands::Effect::Resize::Resize( std::shared_ptr<EffectHelper> const& helper, qint64 newBegin, qint64 newEnd )
    : m_helper( helper )
    , m_newBegin( newBegin )
    , m_newEnd( newEnd )
{
    m_oldBegin = helper->begin();
    m_oldEnd = helper->end();
    retranslate();
}

void
Commands::Effect::Resize::retranslate()
{
    setText( tr( "Resizing effect %1" ).arg( m_helper->name() ) );
}

void
Commands::Effect::Resize::internalRedo()
{
    m_helper->setBoundaries( m_newBegin, m_newEnd );
}

void
Commands::Effect::Resize::internalUndo()
{
    m_helper->setBoundaries( m_oldBegin, m_oldEnd );
}

Commands::Effect::Remove::Remove( std::shared_ptr<EffectHelper> const& helper )
    : m_helper( helper )
    , m_target( helper->filter()->input() )
{

}

void
Commands::Effect::Remove::retranslate()
{
    setText( tr( "Deleting effect %1" ).arg( m_helper->name() ) );
}

void
Commands::Effect::Remove::internalRedo()
{
    m_target->detach( *m_helper->filter() );
}

void
Commands::Effect::Remove::internalUndo()
{
    m_helper->setTarget( m_target.get() );
}

Commands::Transition::Add::Add( std::shared_ptr<SequenceWorkflow> const& workflow,
                                const QString& identifier, qint64 begin, qint64 end,
                                quint32 trackId, Workflow::TrackType type )
    : m_identifier( identifier )
    , m_begin( begin )
    , m_end( end )
    , m_trackId( trackId )
    , m_trackAId( 0 )
    , m_trackBId( 0 )
    , m_type( type )
    , m_workflow( workflow )
{
    retranslate();
}

Commands::Transition::Add::Add( std::shared_ptr<SequenceWorkflow> const& workflow,
                                const QString& identifier, qint64 begin, qint64 end,
                                quint32 trackAId, quint32 trackBId,
                                Workflow::TrackType type )
    : m_identifier( identifier )
    , m_begin( begin )
    , m_end( end )
    , m_trackAId( trackAId )
    , m_trackBId( trackBId )
    , m_type( type )
    , m_workflow( workflow )
{
    retranslate();
}

void
Commands::Transition::Add::internalRedo()
{
    if ( m_uuid.isNull() == true )
    {
        if ( m_trackBId > 0 )
            m_uuid = m_workflow->addTransitionBetweenTracks( m_identifier, m_begin, m_end, m_trackAId, m_trackBId, m_type );
        else
            m_uuid = m_workflow->addTransition( m_identifier, m_begin, m_end, m_trackId, m_type );
    }
    else
        m_workflow->addTransition( m_transitionInstance );
}

void
Commands::Transition::Add::internalUndo()
{
    m_transitionInstance = m_workflow->removeTransition( m_uuid );
}

void
Commands::Transition::Add::retranslate()
{
    setText( tr( "Adding transition" ) );
}

const QUuid&
Commands::Transition::Add::uuid()
{
    return m_uuid;
}

Commands::Transition::Move::Move( std::shared_ptr<SequenceWorkflow> const& workflow,
                                  const QUuid& uuid, qint64 begin, qint64 end )
    : m_uuid( uuid )
    , m_begin( begin )
    , m_end( end )
    , m_workflow( workflow )
{
    auto transitionInstance = m_workflow->transition( uuid );
    if ( transitionInstance == nullptr )
        invalidate();
    else
    {
        m_oldBegin = transitionInstance->transition->begin();
        m_oldEnd = transitionInstance->transition->end();
    }
    retranslate();
}

void
Commands::Transition::Move::internalRedo()
{
    bool ret = m_workflow->moveTransition( m_uuid, m_begin, m_end );
    if ( ret == false )
        invalidate();
}

void
Commands::Transition::Move::internalUndo()
{
    bool ret = m_workflow->moveTransition( m_uuid, m_oldBegin, m_oldEnd );
    if ( ret == false )
        invalidate();
}

void
Commands::Transition::Move::retranslate()
{
    setText( tr( "Moving transition" ) );
}

Commands::Transition::MoveBetweenTracks::MoveBetweenTracks( std::shared_ptr<SequenceWorkflow> const& workflow,
                                                            const QUuid& uuid, quint32 trackAId, quint32 trackBId )
    : m_uuid( uuid )
    , m_trackAId( trackAId )
    , m_trackBId( trackBId )
    , m_workflow( workflow )
{
    auto transitionInstance = m_workflow->transition( uuid );
    if ( transitionInstance == nullptr )
        invalidate();
    else
    {
        m_oldTrackAId = transitionInstance->trackAId;
        m_oldTrackBId = transitionInstance->trackBId;
    }
    retranslate();
}

void
Commands::Transition::MoveBetweenTracks::internalRedo()
{
    bool ret = m_workflow->moveTransitionBetweenTracks( m_uuid, m_trackAId, m_trackBId );
    if ( ret == false )
        invalidate();
}

void
Commands::Transition::MoveBetweenTracks::internalUndo()
{
    bool ret = m_workflow->moveTransitionBetweenTracks( m_uuid, m_oldTrackAId, m_oldTrackBId );
    if ( ret == false )
        invalidate();
}

void
Commands::Transition::MoveBetweenTracks::retranslate()
{
    setText( tr( "Moving transition" ) );
}

Commands::Transition::Remove::Remove( std::shared_ptr<SequenceWorkflow> const& workflow, const QUuid& uuid )
    : m_uuid( uuid )
    , m_workflow( workflow )
{
    retranslate();
}

void
Commands::Transition::Remove::internalRedo()
{
    m_transitionInstance = m_workflow->removeTransition( m_uuid );
}

void
Commands::Transition::Remove::internalUndo()
{
    m_workflow->addTransition( m_transitionInstance );
}

void Commands::Transition::Remove::retranslate()
{
    setText( tr( "Removing transition" ) );
}

#ifdef HAVE_GUI
Commands::Marker::Add::Add( QSharedPointer<MarkerManager> markerManager, quint64 pos )
    : m_markerManager( markerManager )
    , m_pos( pos )
{
    retranslate();
}

void
Commands::Marker::Add::internalRedo()
{
    m_markerManager->addMarker( m_pos );
}

void
Commands::Marker::Add::internalUndo()
{
    m_markerManager->removeMarker( m_pos );
}

void
Commands::Marker::Add::retranslate()
{
    setText( tr( "Adding marker at %1" ).arg( m_pos ) );
}

Commands::Marker::Move::Move( QSharedPointer<MarkerManager> markerManager, quint64 oldPos, quint64 newPos )
    : m_markerManager( markerManager )
    , m_oldPos( oldPos )
    , m_newPos( newPos )
{
    retranslate();
}

void
Commands::Marker::Move::internalRedo()
{
    m_markerManager->moveMarker( m_oldPos, m_newPos );
}

void
Commands::Marker::Move::internalUndo()
{
    m_markerManager->moveMarker( m_newPos, m_oldPos );
}

void
Commands::Marker::Move::retranslate()
{
    setText( tr( "Moving marker from %1 to %2" ).arg( m_oldPos ).arg( m_newPos ) );
}

Commands::Marker::Remove::Remove( QSharedPointer<MarkerManager> markerManager, quint64 pos )
    : m_markerManager( markerManager )

    , m_pos( pos )
{
    retranslate();
}

void
Commands::Marker::Remove::internalRedo()
{
    m_markerManager->removeMarker( m_pos );
}

void
Commands::Marker::Remove::internalUndo()
{
    m_markerManager->addMarker( m_pos );
}

void
Commands::Marker::Remove::retranslate()
{
    setText( tr( "Removing marker at %1" ).arg( m_pos ) );
}
#endif
