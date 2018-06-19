/*****************************************************************************
 * SequenceWorkflow.h : Manages tracks in a single sequence
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

#ifndef SEQUENCEWORKFLOW_H
#define SEQUENCEWORKFLOW_H

#include <memory>
#include <tuple>

#include <QUuid>
#include <QMap>

#include "Media/Clip.h"
#include "Types.h"

class Track;
class Transition;

namespace Backend
{
class IMultiTrack;
class ITrack;
class IInput;
}

namespace ClipTupleIndex
{
    enum {
        Clip,
        TrackId,
        Position
    };
}

class SequenceWorkflow : public QObject
{
    Q_OBJECT

    public:
        SequenceWorkflow( size_t trackCount = 64 );
        ~SequenceWorkflow();

        struct ClipInstance
        {
            ClipInstance() = default;
            ClipInstance( QSharedPointer<::Clip> c, const QUuid& uuid, quint32 tId, qint64 p, bool isAudio );
            QSharedPointer<::Clip>  clip;
            QUuid                   uuid;
            quint32                 trackId;
            qint64                  pos;
            QVector<QUuid>          linkedClips;
            // true is this instance represents an audio track, false otherwise
            bool                    isAudio;

            ///
            /// \brief duplicateClipForResize   Duplicates the used clip for enabling it to be resize independently
            /// \param begin        The new begining for this clip
            /// \param end          The new end for this clip
            ///
            bool                    duplicateClipForResize( qint64 begin, qint64 end );

        private:
            // true if this instance now contains its own Clip
            bool                    m_hasClonedClip;
        };

        struct TransitionInstance
        {
            TransitionInstance() = default;
            TransitionInstance( QSharedPointer<Transition>  transition, quint32 trackAId, quint32 trackBId, bool isInTrack );
            QSharedPointer<Transition>  transition;
            quint32                 trackAId;
            quint32                 trackBId;
            bool                    isInTrack; // If it's in a track, the transition is between clips.

            QVariant                toVariant() const;
        };

        /**
         * @brief addClip   Adds a clip to the sequence workflow
         * @param clip      A library clip's UUID
         * @param trackId   The target track
         * @param pos       Clip's position in the track
         * @param uuid      The new clip instance UUID. If this is a default created UUID, a new UUID
         *                  will be generated.
         * @return          The given instance UUID, or the newly generated one, representing a new
         *                  clip instance in the sequence workflow.
         *                  This instance UUID must be used to manipulate this new clip instance
         */
        QUuid                   addClip( QSharedPointer<::Clip> clip, quint32 trackId, qint64 pos,
                                         const QUuid& uuid, bool isAudioClip );
        bool                    moveClip( const QUuid& uuid, quint32 trackId, qint64 pos );
        bool                    resizeClip( const QUuid& uuid, qint64 newBegin,
                                            qint64 newEnd, qint64 newPos );
        QSharedPointer<ClipInstance>    removeClip( const QUuid& uuid );
        bool                    linkClips( const QUuid& uuidA, const QUuid& uuidB );
        bool                    unlinkClips( const QUuid& uuidA, const QUuid& uuidB );

        QUuid                   addTransition( const QString& identifier, qint64 begin, qint64 end,
                                               quint32 trackId, Workflow::TrackType type );
        QUuid                   addTransitionBetweenTracks( const QString& identifier, qint64 begin, qint64 end,
                                               quint32 trackAId, quint32 trackBId, Workflow::TrackType type );
        bool                    addTransition( QSharedPointer<TransitionInstance> transition );
        bool                    moveTransition( const QUuid& uuid, qint64 begin, qint64 end );
        bool                    moveTransitionBetweenTracks( const QUuid& uuid, quint32 trackAId, quint32 trackBId );
        QSharedPointer<TransitionInstance>     removeTransition( const QUuid& uuid );

        QVariant                toVariant() const;
        void                    loadFromVariant( const QVariant& variant );
        void                    clear();

        QSharedPointer<ClipInstance>    clip( const QUuid& uuid );
        QSharedPointer<TransitionInstance>      transition( const QUuid& uuid );
        quint32                 trackId( const QUuid& uuid );
        qint64                  position( const QUuid& uuid );

        Backend::IInput*        input();
        Backend::IInput*        trackInput( quint32 trackId );

    private:

        inline QSharedPointer<Track>   track( quint32 trackId, bool audio );

        QMap<QUuid, QSharedPointer<ClipInstance>>       m_clips;
        QMap<QUuid, QSharedPointer<TransitionInstance>>         m_transitions;

        QList<QSharedPointer<Track>>    m_tracks[Workflow::NbTrackType];
        QList<std::shared_ptr<Backend::IMultiTrack>>    m_multiTracks;
        std::unique_ptr<Backend::IMultiTrack>           m_multitrack;
        const size_t                    m_trackCount;

    signals:
        void                    clipAdded( QString );
        void                    clipRemoved( QString );
        void                    clipLinked( QString, QString );
        void                    clipUnlinked( QString, QString );
        void                    clipMoved( QString );
        void                    clipResized( QString );

        void                    transitionAdded( const QString& uuid );
        void                    transitionMoved( const QString& uuid );
        void                    transitionRemoved( const QString& uuid );
};

#endif // SEQUENCEWORKFLOW_H
