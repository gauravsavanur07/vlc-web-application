/*****************************************************************************
 * PreviewRuler.cpp : Slider/Ruler used into the PreviewWidget
 * with backward compatibility with QAbstractSlider.
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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

#include <QPainter>
#include <QPolygon>
#include <QBrush>
#include "PreviewRuler.h"
#include "Tools/RendererEventWatcher.h"

PreviewRuler::PreviewRuler( QWidget* parent ) :
        QWidget( parent ),
        m_renderer( nullptr ),
        m_frame( 0 )
{
    setMouseTracking( true );
    m_isSliding = false;
    m_markerStart = MARKER_DEFAULT;
    m_markerStop = MARKER_DEFAULT;
}

void
PreviewRuler::setRenderer( AbstractRenderer* renderer )
{
    if ( m_renderer )
        m_renderer->disconnect( this );
    m_renderer = renderer;

    connect( m_renderer->eventWatcher().data(), &RendererEventWatcher::positionChanged,
             this, &PreviewRuler::updateTimecode );
    connect( m_renderer->eventWatcher().data(), SIGNAL( stopped() ),
             this, SLOT( clear() ) );
    connect( m_renderer, SIGNAL( lengthChanged(qint64) ), this, SLOT( update() ) );
}

void
PreviewRuler::paintEvent( QPaintEvent * event )
{
    Q_UNUSED( event );

    QPainter painter( this );
    QRect marks( 0, 3, width() - 1, MARK_LARGE + 1 );

    painter.setPen( QPen( QColor( 50, 50, 50 ) ) );
    painter.setBrush( QBrush( QColor( 50, 50, 50 ) ) );
    painter.drawRect( marks );

    if ( m_renderer != nullptr && m_renderer->length() > 0 )
    {
        qreal linesToDraw = 0;
        qreal spacing = 0;
        QRect r = marks.adjusted( 1, 0, -1, 0 );

        // Draw the marks
        if ( r.width() / 2  >= m_renderer->length() )
        {   // Every frame
            painter.setPen( QPen( Qt::cyan ) );
            linesToDraw = (qreal)m_renderer->length();
            if ( linesToDraw > 0 )
            {
                spacing = (qreal)r.width() / linesToDraw;
                for ( int step = 0; step < linesToDraw; ++step )
                    painter.drawLine( QLineF( r.left() + step * spacing, r.height() - MARK_XSMALL, r.left() + step * spacing, r.bottom() ) );
            }
        }
        if ( r.width() / 2 >= ( m_renderer->length() / m_renderer->getFps() ) )
        {   // Every second
            painter.setPen( QPen( Qt::green ) );
            linesToDraw = (qreal)m_renderer->length() / m_renderer->getFps();
            if ( linesToDraw > 0 )
            {
                spacing = (qreal)r.width() / linesToDraw;
                for ( int step = 0; step < linesToDraw; ++step )
                    painter.drawLine( QLineF( r.left() + step * spacing, r.height() - MARK_XSMALL, r.left() + step * spacing, r.bottom() ) );
            }
        }
        else if ( r.width() / 2 >= ( m_renderer->length() / m_renderer->getFps() / 12 ) )
        {   // Every 5 seconds
            painter.setPen( QPen( Qt::green ) );
            linesToDraw = (qreal)m_renderer->length() / m_renderer->getFps() / 12;
            if ( linesToDraw > 0 )
            {
                spacing = (qreal)r.width() / linesToDraw;
                for ( int step = 0; step < linesToDraw; ++step )
                    painter.drawLine( QLineF( r.left() + step * spacing, r.height() - MARK_SMALL, r.left() + step * spacing, r.bottom() ) );
            }
        }
        if ( r.width() / 2 >= ( m_renderer->length() / m_renderer->getFps() / 60 ) )
        {   // Every minute
            painter.setPen( QPen( Qt::yellow ) );
            linesToDraw = (qreal)m_renderer->length() / m_renderer->getFps() / 60;
            if ( linesToDraw > 0 )
            {
                spacing = (qreal)r.width() / linesToDraw;
                for ( int step = 0; step < linesToDraw; ++step )
                    painter.drawLine( QLineF( r.left() + step * spacing, r.height() - MARK_MEDIUM, r.left() + step * spacing, r.bottom() ) );

            }
        }
        else if ( r.width() / 2 >= ( m_renderer->length() / m_renderer->getFps() / 60 / 12 ) )
        {   // Every 5 minutes
            painter.setPen( QPen( Qt::yellow ) );
            linesToDraw = (qreal)m_renderer->length() / m_renderer->getFps() / 60 / 12;
            if ( linesToDraw > 0 )
            {
                spacing = (qreal)r.width() / linesToDraw;
                for ( int step = 0; step < linesToDraw; ++step )
                    painter.drawLine( QLineF( r.left() + step * spacing, r.height() - MARK_MEDIUM, r.left() + step * spacing, r.bottom() ) );

            }
        }
        if ( r.width() / 2 >= ( m_renderer->length() / m_renderer->getFps() / 60 / 60 ) )
        {   // Every hour
            painter.setPen( QPen( Qt::red ) );
            linesToDraw = (qreal)m_renderer->length() / m_renderer->getFps() / 60 / 60;
            if ( linesToDraw > 0 )
            {
                spacing = (qreal)r.width() / linesToDraw;
                for ( int step = 0; step < linesToDraw; ++step )
                    painter.drawLine( QLineF( r.left() + step * spacing, r.height() - MARK_LARGE, r.left() + step * spacing, r.bottom() ) );
            }
        }

        // Draw the markers (if any)
        painter.setPen( QPen( Qt::green, 2 ) );

        if ( m_markerStart > MARKER_DEFAULT )
        {
            int markerPos = m_markerStart * width() / m_renderer->length();
            QPolygon marker( 4 );
            marker.setPoints( 4,
                              markerPos + 8,    1,
                              markerPos,        1,
                              markerPos,        20,
                              markerPos + 8,    20 );
            painter.drawPolyline( marker );
        }
        if ( m_markerStop > MARKER_DEFAULT )
        {
            int markerPos = m_markerStop * width() / m_renderer->length();
            QPolygon marker( 4 );
            marker.setPoints( 4,
                              markerPos - 8,    1,
                              markerPos,        1,
                              markerPos,        20,
                              markerPos - 8,    20 );
            painter.drawPolyline( marker );
        }
    }


    // Draw the pointer
    painter.setRenderHint( QPainter::Antialiasing );
    painter.setPen( QPen( Qt::white ) );
    QPolygon cursor( 3 );

    int cursorPos;

    if ( m_renderer != nullptr && m_renderer->length() > 0 )
    {
        cursorPos = m_frame * width() / m_renderer->length();
    }
    else
        cursorPos = 0;

    cursorPos = qMin( qMax( cursorPos, 0 ), width() );
    cursor.setPoints( 3, cursorPos - 5, 20, cursorPos + 5, 20, cursorPos, 9 );
    painter.setBrush( QBrush( QColor( 26, 82, 225, 255 ) ) );
    painter.drawPolygon( cursor );
}

void
PreviewRuler::mousePressEvent( QMouseEvent* event )
{
    m_isSliding = true;
    if ( m_renderer->length() > 0 )
    {
        setFrame( (qreal)(event->pos().x() * m_renderer->length() ) / width(), true );
    }
}

void
PreviewRuler::mouseMoveEvent( QMouseEvent* event )
{
    if ( m_isSliding )
    {
        if ( m_renderer->length() > 0 )
        {
            qint64 pos = event->pos().x();
            pos = qBound( (qint64)0, m_renderer->length(), pos );
            setFrame( (qreal)(pos * m_renderer->length() ) / width(), true );
        }
    }
}

void
PreviewRuler::mouseReleaseEvent( QMouseEvent* )
{
    m_isSliding = false;
}

void
PreviewRuler::setFrame( qint64 frame, bool broadcastEvent /*= false*/ )
{
    m_frame = frame;
    if ( m_isSliding && broadcastEvent == true )
    {
        emit frameChanged( frame, Vlmc::PreviewCursor );
    }
    update();
}

void
PreviewRuler::updateTimecode( qint64 frames /*= -1*/ )
{
    setFrame( frames );
    if ( m_renderer->length() > 0 )
    {
        float fps = m_renderer->getFps();

        if ( fps > 0 )
        {
            if ( frames == -1 )
                frames = m_renderer->getCurrentFrame();

            int h = frames / fps / 60 / 60;
            frames -= h * fps * 60 * 60;

            int m = frames / fps / 60;
            frames -= m * fps * 60;

            int s = frames / fps;
            frames -= s * fps;

            emit timeChanged( h, m, s, frames );
        }
    }
}

void
PreviewRuler::setMarker( Marker m )
{
    if ( m == Start )
        m_markerStart = m_frame;
    else
        m_markerStop = m_frame;
    update();
}

qint64
PreviewRuler::getMarker( Marker m ) const
{
    return ( m == Start ? m_markerStart : m_markerStop );
}

void
PreviewRuler::hideMarker( Marker m )
{
    if ( m == Start )
        m_markerStart = -1;
    else if ( m == Stop )
        m_markerStop = -1;
}

void
PreviewRuler::clear()
{
    m_markerStart = MARKER_DEFAULT;
    m_markerStop = MARKER_DEFAULT;
    m_frame = 0;
}
