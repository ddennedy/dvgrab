/*
* ieee1394io.cc -- grab DV/MPEG-2 from V4L2
* Copyright (C) 2007 Dan Dennedy <dan@dennedy.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _V4L2IO_H
#define _V4L2IO_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_LINUX_VIDEODEV2_H

#include "ieee1394io.h"

class Frame;

class v4l2Reader: public IEEE1394Reader
{
public:

	v4l2Reader( const char *filename, int frames = 50, bool hdv = false );
	~v4l2Reader();

	bool Open( void );
	void Close( void );
	bool StartReceive( void );
	void StopReceive( void );
	bool StartThread( void );
	void StopThread( void );
	void* Thread( void );

private:
	struct buffer {
		void *start;
		size_t length;
	};

	static void* ThreadProxy( void *arg );
	bool Handler( void );
	int ioctl( int request, void *arg );

	const char* m_device;
	int m_fd;
	unsigned int m_bufferCount;
	struct buffer* m_buffers;
};

#endif
#endif
