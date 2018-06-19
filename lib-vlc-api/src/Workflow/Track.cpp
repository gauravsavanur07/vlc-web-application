#include "Track.h"

#include "Backend/MLT/MLTTrack.h"
#include "Backend/MLT/MLTMultiTrack.h"
#include "Transition/Transition.h"
#include "Tools/VlmcDebug.h"

Track::Track( Workflow::TrackType type )
    : m_type( type )
    , m_multitrack( new Backend::MLT::MLTMultiTrack )
{
    track( 1 ); // Prepare the first two tracks so that transitions could be inserted
}

Track::~Track()
{

}

Workflow::TrackType
Track::type() const
{
    return m_type;
}

bool
Track::addClip( QSharedPointer<SequenceWorkflow::ClipInstance> clipInstance, qint64 pos )
{
    auto index = insertableTrackIndex( clipInstance, pos );
    if ( track( index )->insertAt( *clipInstance->clip->input(), pos ) == true )
    {
        clipInstance->pos = pos;
        m_clips[clipInstance->uuid] = QSharedPointer<ClipInstance>::create( clipInstance, index );
        return true;
    }
    return false;
}

bool
Track::moveClip( const QUuid& uuid, qint64 pos )
{
    auto c = clip( uuid );
    if ( !c )
        return false;
    auto index = insertableTrackIndex( c, pos );
    if ( index == internalTrackId( uuid ) )
    {
        bool ret = track( uuid )->move( c->pos, pos );
        if ( ret == false )
            return false;
        c->pos = pos;
        return true;
    }
    else
    {
        bool ret = removeClip( uuid );
        if ( ret == false )
            return false;
        return addClip( c, pos );
    }
}

bool
Track::resizeClip( const QUuid& uuid, qint64 newBegin, qint64 newEnd, qint64 newPos )
{
    auto c = clip( uuid );
    if ( !c )
        return false;
    auto index = insertableTrackIndex( c, newPos, newBegin, newEnd );
    if ( index == internalTrackId( uuid ) )
    {
        auto t = track( internalTrackId( uuid ) );
        bool ret = t->resizeClip( t->clipIndexAt( c->pos ), newBegin, newEnd );
        if ( ret == false )
            return false;
        ret = t->move( c->pos, newPos );
        if ( ret == false )
            return false;
        c->pos = newPos;
        return true;
    }
    else
    {
        bool ret = removeClip( uuid );
        if ( ret == false )
            return false;
        c->clip->setBoundaries( newBegin, newEnd );
        return addClip( c, newPos );
    }
}

bool
Track::removeClip( const QUuid& uuid )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
    {
        vlmcCritical() << "Track: Couldn't find a clip:" << uuid;
        return false;
    }
    auto t = track( it.value()->internalTrackId );
    t->remove( t->clipIndexAt( it.value()->clip->pos ) );
    m_clips.erase( it );
    return true;
}

bool
Track::addTransition( QSharedPointer<Transition> transition )
{
    m_transitions.insert( transition->uuid(), transition );
    transition->apply( *m_multitrack );
    return true;
}

bool
Track::moveTransition( const QUuid& uuid, qint64 begin, qint64 end )
{
    auto it = m_transitions.find( uuid );
    if ( it == m_transitions.end() )
        return false;
    auto transition = it.value();
    if ( m_multitrack->length() - 1 < end )
        return false;
    transition->setBoundaries( begin, end );
    return true;
}

QSharedPointer<Transition>
Track::removeTransition( const QUuid& uuid )
{
    auto it = m_transitions.find( uuid );
    if ( it == m_transitions.end() )
        return {};
    auto transition = it.value();
    m_transitions.erase( it );
    return transition;
}

Backend::IInput&
Track::input()
{
    return *m_multitrack.get();
}

quint32
Track::internalTrackId( const QUuid& uuid )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
    {
        vlmcCritical() << "Track: Couldn't find a clip:" << uuid;
        return {};
    }
    return it.value()->internalTrackId;
}

QSharedPointer<SequenceWorkflow::ClipInstance>
Track::clip( const QUuid& uuid )
{
    auto it = m_clips.find( uuid );
    if ( it == m_clips.end() )
    {
        vlmcCritical() << "Track: Couldn't find a clip:" << uuid;
        return {};
    }
    return it.value()->clip;
}

QSharedPointer<Backend::ITrack>
Track::track( quint32 trackId )
{
    int index = static_cast<int>( trackId );
    while ( m_tracks.size() - 1 < index )
    {
        auto t = QSharedPointer<Backend::ITrack>( new Backend::MLT::MLTTrack );
        if ( m_type == Workflow::AudioTrack )
            t->hide( Backend::HideType::Video );
        else
            t->hide( Backend::HideType::Audio );
        m_multitrack->setTrack( *t, m_tracks.size() );
        m_tracks << t;
    }
    for ( auto& transition : m_transitions )
        transition->setTracks( 0, index );
    return m_tracks[index];
}

QSharedPointer<Backend::ITrack>
Track::track( const QUuid& uuid )
{
    return track( internalTrackId( uuid ) );
}

quint32
Track::insertableTrackIndex( QSharedPointer<SequenceWorkflow::ClipInstance> clip,
                             qint64 pos, qint64 begin, qint64 end  )
{
    quint32 index = 0;
    pos = pos == -1 ? clip->pos : pos;
    auto length = ( begin == -1 || end == -1 ) ? clip->clip->length() : end - begin + 1;
    for ( const auto& c : m_clips )
    {
        if ( c->clip->uuid == clip->uuid )
            continue;
        // Collision detection
        if ( c->clip->pos <= pos + length - 1 &&
             pos <= c->clip->pos + c->clip->clip->length() - 1 )
        {
            index = qMax( index, c->internalTrackId + 1 );
        }
    }
    return index;
}

Track::ClipInstance::ClipInstance( QSharedPointer<SequenceWorkflow::ClipInstance> clip,
                                   quint32 internalTrackId )
    : clip( clip )
    , internalTrackId( internalTrackId )
{

}
