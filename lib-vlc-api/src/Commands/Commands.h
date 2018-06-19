/*****************************************************************************
 * Commands.h: Contains all the implementation of VLMC commands.
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

#ifndef COMMANDS_H
#define COMMANDS_H

#include "config.h"
#include "Workflow/SequenceWorkflow.h"

#ifdef HAVE_GUI
# include <QUndoCommand>
#endif
#include <QObject>
#include <QUuid>
#include <QSharedPointer>

#include <memory>

class   Clip;
class   EffectHelper;
class   Transition;
class   MarkerManager;

namespace Backend
{
class IInput;
}

namespace Commands
{

    enum class Id
    {
        Move,
        Resize,
        Remove,
    };

#ifdef HAVE_GUI
    class       Generic : public QObject, public QUndoCommand
#else
    class       Generic : public QObject
#endif
    {
        Q_OBJECT

        public:
            Generic();
            virtual void    internalRedo() = 0;
            virtual void    internalUndo() = 0;
            void            redo();
            void            undo();
            bool            isValid() const;
#ifndef HAVE_GUI
            void            setText( const QString& text ) ;
            QString         text() const;
#endif
        private:
            bool            m_valid;
            QString         m_text;
        protected slots:
            virtual void    retranslate() = 0;
            void            invalidate();
        signals:
            void            invalidated();
    };

    namespace   Clip
    {
        class   Add : public Generic
        {
            public:
                Add( std::shared_ptr<SequenceWorkflow> const& workflow, const QUuid& uuid, quint32 trackId, qint32 pos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();

            private:
                std::shared_ptr<SequenceWorkflow> m_workflow;
                QUuid                       m_libraryUuid;
                QUuid                       m_audioInstanceUuid;
                QUuid                       m_videoInstanceUuid;
                quint32                     m_trackId;
                qint64                      m_pos;
        };

        class   Move : public Generic
        {
            public:
                Move( std::shared_ptr<SequenceWorkflow> const& workflow, const QString& uuid, quint32 trackId, qint64 pos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
                virtual int     id() const;
                virtual bool    mergeWith( const Generic* command );

            private:
                std::shared_ptr<SequenceWorkflow> m_workflow;
                struct Info
                {
                    QUuid           uuid;
                    quint32         newTrackId;
                    quint32         oldTrackId;
                    qint64          newPos;
                    qint64          oldPos;
                };
                QVector<Info>       m_infos;
        };

        class   Remove : public Generic
        {
            public:
                Remove( std::shared_ptr<SequenceWorkflow> const& workflow, const QUuid& uuid );
                virtual void internalRedo();
                virtual void internalUndo();
                virtual void    retranslate();
                virtual int     id() const;
                virtual bool    mergeWith( const Generic* command );

            private:
                std::shared_ptr<SequenceWorkflow>       m_workflow;
                QVector<QSharedPointer<SequenceWorkflow::ClipInstance>>     m_clips;
        };

        /**
         *  \brief  This command is used to resize a clip.
         *  \param  newBegin: The clip's new beginning
         *  \param  newEnd: The clip's new end
         *  \param  newPos: if the clip was resized from the beginning, it is moved
         *                  so we have to know its new position
        */
        class   Resize : public Generic
        {
            public:
                Resize( std::shared_ptr<SequenceWorkflow> const& workflow,
                        const QUuid& uuid, qint64 newBegin, qint64 newEnd, qint64 newPos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
                virtual bool    mergeWith( const Generic* cmd );
                virtual int     id() const;

            private:
                std::shared_ptr<SequenceWorkflow> m_workflow;
                struct Info
                {
                    QSharedPointer<SequenceWorkflow::ClipInstance>  clip;
                    qint64                      newBegin;
                    qint64                      oldBegin;
                    qint64                      newEnd;
                    qint64                      oldEnd;
                    qint64                      newPos;
                    qint64                      oldPos;
                };
                QVector<Info>                   m_infos;
        };

        class   Split : public Generic
        {
            public:
                Split( std::shared_ptr<SequenceWorkflow> const& workflow,
                       const QUuid& uuid, qint64 newClipPos, qint64 newClipBegin );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                std::shared_ptr<SequenceWorkflow> m_workflow;
                QSharedPointer<SequenceWorkflow::ClipInstance>  m_toSplit;
                quint32                           m_trackId;
                QSharedPointer<::Clip>      m_newClip;
                QUuid                       m_newClipInstanceUuid;
                qint64                      m_newClipPos;
                qint64                      m_newClipBegin;
                qint64                      m_oldEnd;
        };

        class   Link : public Generic
        {
            public:
                Link( std::shared_ptr<SequenceWorkflow> const& workflow,
                      const QUuid& clipA, const QUuid& clipB );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                std::shared_ptr<SequenceWorkflow> m_workflow;
                QUuid     m_clipA;
                QUuid     m_clipB;
        };

        class   Unlink : public Generic
        {
            public:
                Unlink( std::shared_ptr<SequenceWorkflow> const& workflow,
                      const QUuid& clipA, const QUuid& clipB );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                std::shared_ptr<SequenceWorkflow> m_workflow;
                QUuid     m_clipA;
                QUuid     m_clipB;
        };
    }

    namespace   Effect
    {
        class   Add : public Generic
        {
            public:
                Add( std::shared_ptr<EffectHelper> const& helper, Backend::IInput* target );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                std::shared_ptr<EffectHelper>   m_helper;
                Backend::IInput*      m_target;
        };

        class   Move : public Generic
        {
            public:
                Move( std::shared_ptr<EffectHelper> const& helper, std::shared_ptr<Backend::IInput> const& from, Backend::IInput* to, qint64 pos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                std::shared_ptr<EffectHelper>       m_helper;
                std::shared_ptr<Backend::IInput>    m_from;
                Backend::IInput      *m_to;
                qint64          m_oldPos;
                qint64          m_newPos;
                qint64          m_newEnd;
                qint64          m_oldEnd;
        };

        class   Resize : public Generic
        {
            public:
                Resize( std::shared_ptr<EffectHelper> const& helper, qint64 newBegin, qint64 newEnd );
                virtual void        internalRedo();
                virtual void        internalUndo();
                virtual void        retranslate();
            private:
                std::shared_ptr<EffectHelper>       m_helper;
                qint64              m_newBegin;
                qint64              m_newEnd;
                qint64              m_oldBegin;
                qint64              m_oldEnd;
        };

        class   Remove : public Generic
        {
            public:
                Remove( std::shared_ptr<EffectHelper> const& helper );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                std::shared_ptr<EffectHelper>       m_helper;
                std::shared_ptr<Backend::IInput>    m_target;
        };
    }

    namespace Transition
    {
        class   Add : public Generic
        {
            public:
                Add( std::shared_ptr<SequenceWorkflow> const& workflow,
                     const QString& identifier, qint64 begin, qint64 end,
                     quint32 trackId, Workflow::TrackType type );
                Add( std::shared_ptr<SequenceWorkflow> const& workflow,
                     const QString& identifier, qint64 begin, qint64 end,
                     quint32 trackAId, quint32 trackBId, Workflow::TrackType type );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();

                const QUuid&    uuid();
            private:
                QString                                     m_identifier;
                qint64                                      m_begin;
                qint64                                      m_end;
                quint32                                     m_trackId;
                quint32                                     m_trackAId;
                quint32                                     m_trackBId;
                Workflow::TrackType                         m_type;
                QUuid                                       m_uuid;
                QSharedPointer<SequenceWorkflow::TransitionInstance>                m_transitionInstance;
                std::shared_ptr<SequenceWorkflow>           m_workflow;
        };

        class   Move : public Generic
        {
            public:
                Move( std::shared_ptr<SequenceWorkflow> const& workflow,
                      const QUuid& uuid, qint64 begin, qint64 end );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                QUuid                                       m_uuid;
                qint64                                      m_begin;
                qint64                                      m_end;
                qint64                                      m_oldBegin;
                qint64                                      m_oldEnd;
                std::shared_ptr<SequenceWorkflow>           m_workflow;
        };

        class   MoveBetweenTracks : public Generic
        {
            public:
                MoveBetweenTracks( std::shared_ptr<SequenceWorkflow> const& workflow,
                                   const QUuid& uuid, quint32 trackAId, quint32 trackBId );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                QUuid                                       m_uuid;
                quint32                                     m_trackAId;
                quint32                                     m_trackBId;
                quint32                                     m_oldTrackAId;
                quint32                                     m_oldTrackBId;
                std::shared_ptr<SequenceWorkflow>           m_workflow;
        };

        class   Remove : public Generic
        {
            public:
                Remove( std::shared_ptr<SequenceWorkflow> const& workflow,
                        const QUuid& uuid );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();
            private:
                QUuid                                       m_uuid;
                QSharedPointer<SequenceWorkflow::TransitionInstance>                m_transitionInstance;
                std::shared_ptr<SequenceWorkflow>           m_workflow;
        };
    }

#ifdef HAVE_GUI
    // Gui commands
    namespace   Marker
    {
        class   Add : public Generic
        {
            public:
                // We use a raw pointer because it will only be accessed from Timeline itself.
                // Plus, Timeline is a QObject and has a parent so we don't worry about memory leak :)
                Add( QSharedPointer<MarkerManager> markerManager, quint64 pos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();

            private:
                QSharedPointer<MarkerManager>   m_markerManager;
                quint64                         m_pos;
        };

        class   Move : public Generic
        {
            public:
                Move( QSharedPointer<MarkerManager> markerManager, quint64 oldPos, quint64 newPos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();

            private:
                QSharedPointer<MarkerManager>   m_markerManager;
                quint64                         m_oldPos;
                quint64                         m_newPos;
        };

        class   Remove : public Generic
        {
            public:
                Remove( QSharedPointer<MarkerManager> markerManager, quint64 pos );
                virtual void    internalRedo();
                virtual void    internalUndo();
                virtual void    retranslate();

            private:
                QSharedPointer<MarkerManager>   m_markerManager;
                quint64                         m_pos;
        };
    }
#endif
}

#endif // COMMANDS_H
