/*****************************************************************************
 * Panel.cpp: a simple panel with buttons
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Clement CHAVANCE <kinder@vlmc.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Panel.h"
#include "SettingsDialog.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QIcon>
#include <QString>
#include <QToolButton>
#include <QSizePolicy>
#include <QSize>

const int   Panel::M_ICON_HEIGHT = 64;

Panel::Panel( QWidget* parent ) : QWidget( parent )
{
    m_layout = new QVBoxLayout( this );
    m_buttons = new QButtonGroup( this );

    m_buttons->setExclusive( true );
    m_layout->setMargin( 0 );
    m_layout->setSpacing( 1 );
    m_layout->insertSpacerItem( 1, new QSpacerItem( 1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding ) );

    connect( m_buttons, SIGNAL( buttonPressed(int) ),
             this, SIGNAL( changePanel(int) ) );

    setSizePolicy( QSizePolicy::Expanding,
                   QSizePolicy::Expanding );
}

void
Panel::addButton( const char* name,
                          const QIcon& icon,
                          int index )
{
    QToolButton*    button = new QToolButton( this );

    m_buttonsNames[button] = name;
    button->setText( tr( name ) );
    button->setIcon( icon );
    button->setAutoRaise( true );
    button->setCheckable( true );
    button->setIconSize( QSize( Panel::M_ICON_HEIGHT,
                                Panel::M_ICON_HEIGHT) );
    button->setToolButtonStyle( Qt::ToolButtonTextUnderIcon  );
    button->setSizePolicy( QSizePolicy::Expanding,
                           QSizePolicy::Minimum );

    if ( m_buttons->buttons().isEmpty() )
        button->setChecked( true );

    m_buttons->addButton( button, index );
    m_layout->insertWidget( m_layout->count() - 1, button );
}

void
Panel::retranslate()
{
    QMap<QToolButton*, const char*>::iterator       it = m_buttonsNames.begin();
    QMap<QToolButton*, const char*>::iterator       ite = m_buttonsNames.end();

    while ( it != ite )
    {
        it.key()->setText( SettingsDialog::tr( it.value() ) );
        ++it;
    }
}
