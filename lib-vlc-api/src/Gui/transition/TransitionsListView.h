#ifndef TRANSITIONSLISTVIEW_H
#define TRANSITIONSLISTVIEW_H

#include <QObject>
#include <QJsonArray>

class TransitionsListView : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY( TransitionsListView )

    public:
        explicit TransitionsListView( QWidget* parent = 0);
        virtual ~TransitionsListView();

        QWidget*            container();

        Q_INVOKABLE
        QJsonArray          transitions();

    public slots:
        void    startDrag( const QString& transitionId );

    private:
        QWidget*            m_container;
};

#endif // TRANSITIONSLISTVIEW_H
