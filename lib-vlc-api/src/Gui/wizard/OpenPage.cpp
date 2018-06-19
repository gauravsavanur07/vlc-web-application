/*****************************************************************************
 * OpenPage.cpp: Wizard project openning page
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

#include "Main/Core.h"
#include "OpenPage.h"
#include "ProjectWizard.h"
#include "WelcomePage.h"
#include "Project/Project.h"

OpenPage::OpenPage( QWidget *parent ) :
    QWizardPage(parent)
{
    ui.setupUi( this );

    setTitle( tr( "Project wizard" ) );
    setSubTitle( tr( "Ready to load this project" ) );

    setFinalPage( true );
}

void
OpenPage::changeEvent( QEvent *e )
{
    QWizardPage::changeEvent( e );
    switch ( e->type() ) {
    case QEvent::LanguageChange:
        ui.retranslateUi( this );
        break;
    default:
        break;
    }
}

bool
OpenPage::validatePage()
{
    if ( WelcomePage::projectPath().isEmpty() == false )
        return Core::instance()->project()->load( WelcomePage::projectPath() );
    return false;
}

int
OpenPage::nextId() const
{
    // This an ending page
    return -1;
}
