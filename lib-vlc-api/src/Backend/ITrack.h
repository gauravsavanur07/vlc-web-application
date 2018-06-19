/*****************************************************************************
 * ITrack.h: Defines an interface of a track
 *****************************************************************************
 * Copyright (C) 2008-2016 VideoLAN
 *
 * Authors: Yikei Lu <luyikei.qmltu@gmail.com>
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

#ifndef ITRACK_H
#define ITRACK_H

#include "IInput.h"
#include <string>

namespace Backend
{
    enum class HideType
    {
        None,
        Video,
        Audio,
        VideoAndAudio
    };

    class ITrack : virtual public IInput
    {
    public:
        virtual ~ITrack() = default;

        virtual bool        insertAt( IInput& input, int64_t startFrame ) = 0;
        virtual void        remove( int index ) = 0;
        virtual bool        append( IInput& input ) = 0;
        // src and dist are frames.
        virtual bool        move( int64_t src, int64_t dist ) = 0;
        virtual IInput*     clip( int index ) const = 0;
        virtual IInput*     clipAt( int64_t position ) const = 0 ;
        virtual bool        resizeClip( int clip, int64_t begin, int64_t end ) = 0;
        virtual int         clipIndexAt( int64_t position ) = 0;
        virtual int         count() const = 0;
        virtual void        clear() = 0;

        virtual void        hide( HideType hideType ) = 0;
    };
}

#endif // ITRACK_H
