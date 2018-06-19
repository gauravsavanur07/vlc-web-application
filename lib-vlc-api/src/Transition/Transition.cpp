#include "Transition.h"

#include "Backend/MLT/MLTTrack.h"
#include "Backend/MLT/MLTTransition.h"
#include "Backend/MLT/MLTMultiTrack.h"
#include "Workflow/Track.h"
#include "Tools/VlmcDebug.h"

#include <mlt++/MltPlaylist.h>
#include <mlt++/MltTransition.h>

Transition::Transition( const QString& identifier, qint64 position, qint64 endPosition, Workflow::TrackType type )
    : Helper( QUuid::createUuid() )
    , m_identifier( identifier )
    , m_begin( position )
    , m_end( endPosition )
    , m_type( type )
{
    if ( identifier == QStringLiteral( "dissolve" ) )
    {
        if ( m_type == Workflow::AudioTrack )
        {
            auto mix = new Backend::MLT::MLTTransition( "mix" );
            mix->properties()->set( "start", -1 );
            addTransition( QSharedPointer<Backend::ITransition>( mix ) );
        }
        else if ( m_type == Workflow::VideoTrack )
        {
            auto luma = new Backend::MLT::MLTTransition( "luma" );
            addTransition( QSharedPointer<Backend::ITransition>( luma ) );
        }
    }
    else
        addTransition( QSharedPointer<Backend::ITransition>(
                           new Backend::MLT::MLTTransition( qPrintable( identifier ) ) ) );
}

Transition::~Transition()
{
}

const QUuid&
Transition::uuid() const
{
    return m_uuid;
}

void
Transition::setUuid( const QUuid& uuid )
{
    m_uuid = uuid;
}

qint64
Transition::begin() const
{
    return m_begin;
}

qint64
Transition::end() const
{
    return m_end;
}

void
Transition::setBegin( qint64 begin )
{
    m_begin = begin;
    for ( auto& transition : m_transitions )
        transition->setBoundaries( m_begin, m_end );
}

void
Transition::setEnd( qint64 end )
{
    m_end = end;
    for ( auto& transition : m_transitions )
        transition->setBoundaries( m_begin, m_end );
}

qint64
Transition::length() const
{
    return m_end - m_begin + 1;
}

void
Transition::setBoundaries( qint64 begin, qint64 end )
{
    m_begin = begin;
    m_end = end;
    for ( auto& transition : m_transitions )
        transition->setBoundaries( begin, end );
}

Workflow::TrackType
Transition::type() const
{
    return m_type;
}

void
Transition::setType( Workflow::TrackType type )
{
    m_type = type;
}

void
Transition::setTracks( quint32 trackAId, quint32 trackBId )
{
    for ( auto& transition : m_transitions )
        dynamic_cast<Backend::MLT::MLTTransition*>( transition.data() )->transition()->set_tracks( trackAId, trackBId );
}

void
Transition::addTransition( QSharedPointer<Backend::ITransition> transition )
{
    m_transitions << transition;
    transition->setBoundaries( m_begin, m_end );
}

void
Transition::apply( Backend::IMultiTrack& multitrack )
{
    for ( auto& transition : m_transitions )
        if ( multitrack.count() >= 2 )
            multitrack.addTransition( *transition.data(), 0, multitrack.count() - 1 );
}

void
Transition::apply( Backend::IMultiTrack& multitrack, quint32 trackAId, quint32 trackBId )
{
    for ( auto& transition : m_transitions )
        if ( multitrack.count() >= 2 )
            multitrack.addTransition( *transition.data(), trackAId, trackBId );
}

QVariant
Transition::toVariant() const
{
    QVariantHash h;
    return QVariantHash{
        { "uuid", uuid().toString() },
        { "begin", begin() },
        { "end", end() },
        { "length", length() },
        { "audio", type() == Workflow::AudioTrack },
        { "identifier", m_identifier }
    };
}
