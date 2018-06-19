#ifndef TRACK_H
#define TRACK_H

#include <QUuid>
#include <QSharedPointer>

#include "SequenceWorkflow.h"

class Transition;

class Track
{
public:
    explicit Track( Workflow::TrackType type );
    ~Track();

    Workflow::TrackType     type() const;

    bool                    addClip( QSharedPointer<SequenceWorkflow::ClipInstance> clipInstance, qint64 pos );
    bool                    moveClip( const QUuid& uuid, qint64 pos );
    bool                    resizeClip( const QUuid& uuid, qint64 newBegin, qint64 newEnd, qint64 newPos );
    bool                    removeClip( const QUuid& uuid );


    bool                    addTransition( QSharedPointer<Transition> transition );
    bool                    moveTransition( const QUuid& uuid, qint64 begin, qint64 end );
    QSharedPointer<Transition>     removeTransition( const QUuid& uuid );

    Backend::IInput&        input();

private:
    struct ClipInstance {
        ClipInstance() = default;
        ClipInstance( QSharedPointer<SequenceWorkflow::ClipInstance> clip,
                      quint32                                        internalTrackId );
        QSharedPointer<SequenceWorkflow::ClipInstance>      clip;
        quint32                                             internalTrackId;
    };

    quint32                                                         internalTrackId( const QUuid& uuid );
    QSharedPointer<SequenceWorkflow::ClipInstance>                  clip( const QUuid& uuid );

    QSharedPointer<Backend::ITrack>               track( quint32 trackId );
    inline QSharedPointer<Backend::ITrack>        track( const QUuid& uuid );
    quint32                 insertableTrackIndex( QSharedPointer<SequenceWorkflow::ClipInstance> clip,
                                                  qint64 pos = -1, qint64 begin = -1, qint64 end = -1 );

    Workflow::TrackType                                                 m_type;

    QMap<QUuid, QSharedPointer<ClipInstance>>                           m_clips;
    QMap<QUuid, QSharedPointer<Transition>>                             m_transitions;

    QList<QSharedPointer<Backend::ITrack>>                              m_tracks;
    std::unique_ptr<Backend::IMultiTrack>                               m_multitrack;
};

#endif // TRACK_H
