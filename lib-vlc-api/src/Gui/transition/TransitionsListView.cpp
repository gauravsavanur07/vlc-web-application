#include "TransitionsListView.h"

#include "Backend/IBackend.h"
#include "Backend/IInfo.h"
#include "Backend/ITransition.h"

#include <QWidget>
#include <QJsonObject>
#include <QQuickView>
#include <QQmlContext>
#include <QDrag>
#include <QMimeData>

TransitionsListView::TransitionsListView( QWidget* parent )
    : QObject( parent )
{
    setObjectName( QStringLiteral( "TransitionsListView" ) );
    auto view = new QQuickView;
    m_container = QWidget::createWindowContainer( view, parent );
    m_container->setMinimumSize( 100, 1 );
    m_container->setObjectName( objectName() );
    view->rootContext()->setContextProperty( QStringLiteral( "view" ), this );
    view->setSource( QUrl( QStringLiteral( "qrc:/QML/TransitionsListView.qml" ) ) );
    view->setResizeMode( QQuickView::SizeRootObjectToView );
}

TransitionsListView::~TransitionsListView()
{

}

QWidget*
TransitionsListView::container()
{
    return m_container;
}

QJsonArray
TransitionsListView::transitions()
{
    QJsonArray array;
    for ( auto p : Backend::instance()->availableTransitions() )
    {
        auto info = p.second;
        QJsonObject jInfo;
        jInfo[QStringLiteral( "identifier" )] = QString::fromStdString( info->identifier() );
        jInfo[QStringLiteral( "name" )] = QString::fromStdString( info->name() );
        jInfo[QStringLiteral( "description" )] = QString::fromStdString( info->description() );
        jInfo[QStringLiteral( "author" )] = QString::fromStdString( info->author() );
        array.append( jInfo );
    }
    return array;
}

void
TransitionsListView::startDrag( const QString& transitionId )
{
    QDrag* drag = new QDrag( this );
    QMimeData* mimeData = new QMimeData;

    mimeData->setData( QStringLiteral( "vlmc/transition_id" ), transitionId.toUtf8() );

    drag->setMimeData( mimeData );
    drag->exec();
}
