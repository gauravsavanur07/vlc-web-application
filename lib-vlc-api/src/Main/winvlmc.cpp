/*****************************************************************************
 * winvlmc.cpp: VLMC launcher for win32
 *****************************************************************************
 * Copyright (C) 2008-2010 VideoLAN
 *
 * Authors: Hugo Beauzee-Luyssen <hugo@vlmc.org>
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

#include <windows.h>

int VLMCmain( int, char** );

LONG WINAPI
vlc_exception_filter(struct _EXCEPTION_POINTERS *)
{
    if(IsDebuggerPresent())
    {
        //If a debugger is present, pass the exception to the debugger with EXCEPTION_CONTINUE_SEARCH
        return EXCEPTION_CONTINUE_SEARCH;
    }
    //TODO: get backtrace
    //TODO: relaunch?
    return 0;
}

int
main( int argc, char **argv )
{
    HINSTANCE h_Kernel32 = LoadLibraryW( L"kernel32.dll" );
    if( h_Kernel32 )
    {
        BOOL (WINAPI * mySetDllDirectoryA)(const char* lpPathName);
        /* Do NOT load any library from cwd. */
        mySetDllDirectoryA = (BOOL WINAPI (*)(const char*)) GetProcAddress( h_Kernel32, "SetDllDirectoryA" );
        if ( mySetDllDirectoryA )
            mySetDllDirectoryA( "" );
        FreeLibrary( h_Kernel32 );
    }
    return VLMCmain( argc, argv );
}
