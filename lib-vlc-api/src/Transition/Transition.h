#ifndef TRANSITION_H
#define TRANSITION_H

#include "Workflow/Types.h"
#include "Workflow/Helper.h"
#include "Workflow/SequenceWorkflow.h"

#include <QUuid>
#include <QSharedPointer>

namespace Backend {
class ITransition;
class IMultiTrack;
}

class Transition : public Workflow::Helper
{
public:
    explicit Transition( const QString& identifier, qint64 begin, qint64 end, Workflow::TrackType type );
    virtual ~Transition();

    virtual const QUuid&    uuid() const override;
    void                    setUuid( const QUuid& uuid );

    virtual qint64          begin() const override;
    virtual qint64          end() const override;

    virtual void            setBegin( qint64 begin ) override;
    virtual void            setEnd( qint64 end ) override;
    virtual qint64          length() const override;
    virtual void            setBoundaries( qint64 begin, qint64 end ) override;

    Workflow::TrackType     type() const;
    void                    setType( Workflow::TrackType type );

    void                    setTracks( quint32 trackAId, quint32 trackBId );
    void                    addTransition( QSharedPointer<Backend::ITransition> transition );

    void                    apply( Backend::IMultiTrack& multitrack );
    void                    apply( Backend::IMultiTrack& multitrack, quint32 trackAId, quint32 trackBId );

    QVariant                toVariant() const;

private:
    QString                 m_identifier;
    qint64                  m_begin;
    qint64                  m_end;
    Workflow::TrackType     m_type;

    QList<QSharedPointer<Backend::ITransition>>                 m_transitions;
};

#endif // TRANSITION_H
