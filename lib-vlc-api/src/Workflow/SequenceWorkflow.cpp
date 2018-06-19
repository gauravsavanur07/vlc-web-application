/*****************************************************************************
 * SequenceWorkflow.cpp : Manages tracks in a single sequence
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Yikei Lu    <luyikei.qmltu@gmail.com>
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

#include "SequenceWorkflow.h"

#include "Backend/MLT/MLTTrack.h"
#include "Backend/MLT/MLTMultiTrack.h"
#include "EffectsEngine/EffectHelper.h"
#include "Track.h"
#include "Workflow/MainWorkflow.h"
#include "Main/Core.h"
#include "Library/Library.h"
#include "Tools/VlmcDebug.h"
#include "Media/Media.h"
#include "Transition/Transition.h"

SequenceWorkflow::SequenceWorkflow( size_t trackCount )
    : m_multitrack( new Backend::MLT::MLTMultiTrack )
    , m_trackCount( trackCount )
{
    for ( int i = 0; i < trackCount; ++i )
    {
        auto audioTrack = QSharedPointer<Track>( new Track( Workflow::AudioTrack ) );
        m_tracks[Workflow::AudioTrack] <<  audioTrack;
        auto videoTrack = QSharedPointer<Track>( new Track( Workflow::VideoTrack ) );
        m_tracks[Workflow::VideoTrack] << videoTrack;

        auto multitrack = std::shared_ptr<Backend::IMultiTrack>( new Backend::MLT::MLTMultiTrack );
        multitrack->setTrack( audioTrack->input(), 0 );
        multitrack->hide( Backend::HideType::Video, 0 );
        multitrack->setTrack( videoTrack->input(), 1 );
        multitrack->hide( Backend::HideType::Audio, 1 );
        m_multiTracks << multitrack;

        m_multitrack->setTrack( *multitrack, i );
    }
}

SequenceWorkflow::~SequenceWorkflow()
{
}

QUuid
SequenceWorkflow::addClip( QSharedPointer<::Clip> clip, quint32 trackId, qint64 pos, const QUuid& uuid, bool isAudioClip )
{
    auto t = track( trackId, isAudioClip );
    auto c = QSharedPointer<ClipInstance>::create( clip,
                                           uuid.isNull() == true ? QUuid::createUuid() : uuid,
                                           trackId, pos, isAudioClip );
    auto ret = t->addClip( c, pos );
    if ( ret == false )
        return {};
    vlmcDebug() << "adding" << (isAudioClip ? "audio" : "video") <<  "clip instance:" << c->uuid;
    m_clips.insert( c->uuid, c ) ;
    clip->setOnTimeline( true );
    emit clipAdded( c->uuid.toString() );
    return c->uuid;
}

bool
SequenceWorkflow::moveClip( const QUuid& uuid, quint32 trackId, qint64 pos )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
    {
        vlmcCritical() << "Couldn't find a clip:" << uuid;
        return false;
    }
    auto& c = it.value();
    auto oldTrackId = c->trackId;
    auto oldPosition = c->pos;
    if ( oldPosition == pos && oldTrackId == trackId )
        return true;
    auto t = track( oldTrackId, c->isAudio );
    if ( trackId != oldTrackId )
    {
        // Don't call removeClip/addClip as they would destroy & recreate clip instances for nothing.
        // Simply fiddle with the track to move the clip around
        t->removeClip( uuid );
        auto newTrack = track( trackId, c->isAudio );
        if ( newTrack->addClip( c, pos ) == false )
            return false;
        c->trackId = trackId;
    }
    else
    {
        bool ret = t->moveClip( c->uuid, pos );
        if ( ret == false )
            return false;
    }
    c->pos = pos;
    emit clipMoved( uuid.toString() );
    // CAUTION: You must not move a clip to a place where it would overlap another clip!
    return true;
}

bool
SequenceWorkflow::resizeClip( const QUuid& uuid, qint64 newBegin, qint64 newEnd, qint64 newPos )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
    {
        vlmcCritical() << "Couldn't find a clip:" << uuid;
        return false;
    }
    auto& c = it.value();
    auto trackId = c->trackId;
    auto position = c->pos;
    auto t = track( trackId, c->isAudio );
    bool ret;
    // This will only duplicate the clip once; no need to panic about endless duplications
    if ( c->duplicateClipForResize( newBegin, newEnd ) == true )
    {
        vlmcDebug() << "Duplicating clip for resize" << c->uuid << "is now using" << c->clip->uuid();
        t->removeClip( uuid );
        ret = t->addClip( c, position );
    }
    else
        ret = t->resizeClip( uuid, newBegin, newEnd, newPos );
    if ( ret == false )
        return false;
    c->pos = newPos;
    emit clipResized( uuid.toString() );
    return ret;
}

QSharedPointer<SequenceWorkflow::ClipInstance>
SequenceWorkflow::removeClip( const QUuid& uuid )
{
    vlmcDebug() << "Removing clip instance" << uuid;
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
    {
        vlmcCritical() << "Couldn't find a sequence workflow clip:" << uuid;
        return {};
    }
    auto c = it.value();
    auto clip = c->clip;
    auto trackId = c->trackId;
    auto t = track( trackId, c->isAudio );
    t->removeClip( uuid );
    m_clips.erase( it );
    clip->disconnect( this );
    bool onTimeline = false;
    for ( const auto& clipInstance : m_clips )
        if ( clipInstance->clip->uuid() == clip->uuid() )
            onTimeline = true;
    clip->setOnTimeline( onTimeline );
    emit clipRemoved( uuid.toString() );
    return c;

}

bool
SequenceWorkflow::linkClips( const QUuid& uuidA, const QUuid& uuidB )
{
    auto clipA = clip( uuidA );
    auto clipB = clip( uuidB );
    if ( !clipA || !clipB )
    {
        vlmcCritical() << "Couldn't find clips:" << uuidA << "and" << uuidB;
        return false;
    }
    clipA->linkedClips.append( uuidB );
    clipB->linkedClips.append( uuidA );
    emit clipLinked( uuidA.toString(), uuidB.toString() );
    return true;
}

bool
SequenceWorkflow::unlinkClips( const QUuid& uuidA, const QUuid& uuidB )
{
    vlmcDebug() << "Unlinking clips" << uuidA.toString() << "and" << uuidB.toString();
    auto clipA = clip( uuidA );
    auto clipB = clip( uuidB );
    if ( !clipA || !clipB )
    {
        vlmcCritical() << "Couldn't find clips:" << uuidA << "and" << uuidB;
        return false;
    }
    bool ret = true;
    if ( clipA->linkedClips.removeOne( uuidB ) == false )
    {
        ret = false;
        vlmcWarning() << "Failed to unlink" << uuidB << "from Clip instance" << uuidA;
    }
    if ( clipB->linkedClips.removeOne( uuidA ) == false )
    {
        ret = false;
        vlmcWarning() << "Failed to unlink" << uuidA << "from Clip instance" << uuidB;
    }
    if ( ret == true )
        emit clipUnlinked( uuidA.toString(), uuidB.toString() );
    return ret;
}

QUuid
SequenceWorkflow::addTransition( const QString& identifier, qint64 begin, qint64 end,
                                 quint32 trackId, Workflow::TrackType type )
{
    auto t = track( trackId, type == Workflow::AudioTrack );
    auto transition = QSharedPointer<Transition>::create( identifier, begin, end, type );
    t->addTransition( transition );
    m_transitions.insert( transition->uuid(), QSharedPointer<TransitionInstance>::create( transition, trackId, 0, true ) );
    emit transitionAdded( transition->uuid().toString() );
    return transition->uuid();
}

QUuid
SequenceWorkflow::addTransitionBetweenTracks( const QString& identifier, qint64 begin, qint64 end,
                                              quint32 trackAId, quint32 trackBId,
                                              Workflow::TrackType type )
{
    auto transition = QSharedPointer<Transition>::create( identifier, begin, end, type );
    m_transitions.insert( transition->uuid(), QSharedPointer<TransitionInstance>::create( transition, trackAId, trackBId, false ) );
    transition->apply( *m_multitrack, trackAId, trackBId );
    emit transitionAdded( transition->uuid().toString() );
    return transition->uuid();
}

bool
SequenceWorkflow::addTransition( QSharedPointer<TransitionInstance> transitionInstance )
{
    auto transition = transitionInstance->transition;
    auto t = track( transitionInstance->trackAId, transition->type() == Workflow::AudioTrack );
    m_transitions.insert( transition->uuid(), transitionInstance );
    emit transitionAdded( transition->uuid().toString() );
    return t->addTransition( transition );
}

bool
SequenceWorkflow::moveTransition( const QUuid& uuid, qint64 begin, qint64 end )
{
    auto it = m_transitions.find( uuid );
    if ( it == m_transitions.end() )
        return false;
    auto transitionInstance = it.value();
    auto transition = transitionInstance->transition;
    if ( transition->begin() == begin && transition->end() == end )
        return true;
    if ( transitionInstance->isInTrack == true )
    {
        auto t = track( transitionInstance->trackAId, transition->type() == Workflow::AudioTrack );
        emit transitionMoved( uuid.toString() );
        return t->moveTransition( uuid, begin, end );
    }
    else
    {
        transition->setBoundaries( begin, end );
        emit transitionMoved( uuid.toString() );
        return true;
    }
}

bool
SequenceWorkflow::moveTransitionBetweenTracks( const QUuid& uuid, quint32 trackAId, quint32 trackBId )
{
    auto it = m_transitions.find( uuid );
    if ( it == m_transitions.end() )
        return false;
    auto transitionInstance = it.value();
    if ( transitionInstance->isInTrack == true )
        return false; // Not allowed
    auto transition = transitionInstance->transition;
    if ( transitionInstance->trackAId == trackAId && transitionInstance->trackBId == trackBId )
        return true;
    transition->setTracks( trackAId, trackBId );
    transitionInstance->trackAId = trackAId;
    transitionInstance->trackBId = trackBId;
    emit transitionMoved( uuid.toString() );
    return true;
}

QSharedPointer<SequenceWorkflow::TransitionInstance>
SequenceWorkflow::removeTransition( const QUuid& uuid )
{
    auto it = m_transitions.find( uuid );
    if ( it == m_transitions.end() )
        return {};
    auto transitionInstance = it.value();
    if ( transitionInstance->isInTrack == true )
    {
        auto transition = transitionInstance->transition;
        auto t = track( transitionInstance->trackAId, transition->type() == Workflow::AudioTrack );
        t->removeTransition( uuid );
    }
    m_transitions.erase( it );
    emit transitionRemoved( uuid.toString() );
    return transitionInstance;
}

QVariant
SequenceWorkflow::toVariant() const
{
    QVariantList transitions;
    for ( const auto& t : m_transitions )
        transitions << t->toVariant();

    QVariantList l;
    for ( auto it = m_clips.begin(); it != m_clips.end(); ++it )
    {
        auto c = it.value();
        QVariantHash h = {
            { "uuid", c->uuid.toString() },
            { "clipUuid", c->clip->uuid().toString() },
            { "position", c->pos },
            { "trackId", c->trackId },
            { "filters", EffectHelper::toVariant( c->clip->input() ) },
            { "isAudio",c->isAudio }
        };
        QList<QVariant> linkedClipList;
        for ( const auto& uuid : c->linkedClips )
            linkedClipList.append( uuid.toString() );
        h["linkedClips"] = linkedClipList;
        l << h;
    }
    QVariantHash h{ { "transitions", transitions }, { "clips", l },
                    { "filters", EffectHelper::toVariant( m_multitrack.get() ) } };
    return h;
}

void
SequenceWorkflow::loadFromVariant( const QVariant& variant )
{
    for ( auto& var : variant.toMap()["transitions"].toList() )
    {
        auto m = var.toMap();
        bool inTrack = m["isInTrack"].toBool();
        if ( inTrack == true )
            addTransition( m["identifier"].toString(), m["begin"].toLongLong(), m["end"].toLongLong(),
                    m["trackId"].toUInt(), m["audio"].toBool() ? Workflow::AudioTrack : Workflow::VideoTrack );
        else
            addTransitionBetweenTracks( m["identifier"].toString(), m["begin"].toLongLong(), m["end"].toLongLong(),
                    m["trackAId"].toUInt(), m["trackBId"].toUInt(), m["audio"].toBool() ? Workflow::AudioTrack : Workflow::VideoTrack );
    }

    for ( auto& var : variant.toMap()["clips"].toList() )
    {
        auto m = var.toMap();
        auto clip = Core::instance()->library()->clip( m["clipUuid"].toUuid() );

        if ( clip == nullptr )
        {
            vlmcCritical() << "Couldn't find an acceptable library clip to be added.";
            continue;
        }

        Q_ASSERT( m.contains( "uuid" ) && m.contains( "isAudio" ) );

        auto uuid = m["uuid"].toUuid();
        auto isAudio = m["isAudio"].toBool();
        //FIXME: Add missing clip type handling. We don't know if we're adding an audio clip or not
        addClip( clip, m["trackId"].toUInt(), m["position"].toLongLong(), uuid, isAudio );
        auto c = m_clips[uuid];

        auto linkedClipsList = m["linkedClips"].toList();
        for ( const auto& uuidVar : linkedClipsList )
        {
            auto linkedClipUuid = uuidVar.toUuid();
            c->linkedClips.append( linkedClipUuid );
            auto it = m_clips.find( linkedClipUuid );
            if ( it != m_clips.end() )
                emit clipLinked( uuid.toString(), linkedClipUuid.toString() );
        }

        EffectHelper::loadFromVariant( m["filters"], clip->input() );
    }
    EffectHelper::loadFromVariant( variant.toMap()["filters"], m_multitrack.get() );
}

void
SequenceWorkflow::clear()
{
    while ( !m_clips.empty() )
        removeClip( m_clips.begin().key() );
}

QSharedPointer<SequenceWorkflow::ClipInstance>
SequenceWorkflow::clip( const QUuid& uuid )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
        return {};
    return it.value();
}

QSharedPointer<SequenceWorkflow::TransitionInstance>
SequenceWorkflow::transition( const QUuid& uuid )
{
    auto it = m_transitions.find( uuid );
    if ( it == m_transitions.end() )
        return {};
    return it.value();
}

quint32
SequenceWorkflow::trackId( const QUuid& uuid )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
        return 0;
    return it.value()->trackId;
}

qint64
SequenceWorkflow::position( const QUuid& uuid )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
        return 0;
    return it.value()->pos;
}

Backend::IInput*
SequenceWorkflow::input()
{
    return m_multitrack.get();
}

Backend::IInput*
SequenceWorkflow::trackInput( quint32 trackId )
{
    Q_ASSERT( trackId < m_multiTracks.size() );
    return m_multiTracks[trackId].get();
}

QSharedPointer<Track>
SequenceWorkflow::track( quint32 trackId, bool isAudio )
{
    if ( trackId >= m_trackCount )
        return {};
    if ( isAudio == true )
        return m_tracks[Workflow::AudioTrack][static_cast<int>( trackId )];
    return m_tracks[Workflow::VideoTrack][static_cast<int>( trackId )];
}

SequenceWorkflow::ClipInstance::ClipInstance(QSharedPointer<::Clip> c, const QUuid& uuid, quint32 tId, qint64 p, bool isAudio )
    : clip( c )
    , uuid( uuid )
    , trackId( tId )
    , pos( p )
    , isAudio( isAudio )
    , m_hasClonedClip( false )
{
}

bool
SequenceWorkflow::ClipInstance::duplicateClipForResize( qint64 begin, qint64 end )
{
    if ( m_hasClonedClip == true )
        return false;
    clip = clip->media()->cut( begin, end );
    m_hasClonedClip = true;
    return true;
}

SequenceWorkflow::TransitionInstance::TransitionInstance( QSharedPointer<Transition> transition,
                                                          quint32 trackAId, quint32 trackBId, bool isInTrack )
    : transition( transition )
    , trackAId( trackAId )
    , trackBId( trackBId )
    , isInTrack( isInTrack )
{

}

QVariant
SequenceWorkflow::TransitionInstance::toVariant() const
{
    auto h = transition->toVariant().toHash();
    h["isInTrack"] = isInTrack;
    if ( isInTrack == true )
    {
        h["trackId"] = trackAId;
    }
    else
    {
        h["trackAId"] = trackAId;
        h["trackBId"] = trackBId;
    }
    return h;
}
