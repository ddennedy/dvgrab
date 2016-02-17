/*
* ieee1394io.cc -- grab DV/MPEG-2 from V4L2
* Copyright (C) 2007-2008 Dan Dennedy <dan@dennedy.org>
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


#include "v4l2reader.h"

#ifdef HAVE_LINUX_VIDEODEV2_H

#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <assert.h>
#include <poll.h>
#include <string.h>

#include "error.h"
#include "dvframe.h"


v4l2Reader::v4l2Reader( const char *filename, int frames, bool hdv )
	: IEEE1394Reader( 0, frames, hdv )
	, m_device( filename )
	, m_fd( -1 )
	, m_bufferCount( 0 )
	, m_buffers( 0 )
{
}

v4l2Reader::~v4l2Reader()
{
	Close();
}

bool v4l2Reader::Open( void )
{
	bool success = true;
	
	try
	{
		// Open device file
		fail_neg( m_fd = open( m_device, O_RDWR | O_NONBLOCK, 0 ) );

		// Validate capabilities
		struct v4l2_capability cap;
		fail_neg( ioctl( VIDIOC_QUERYCAP, &cap ) );
		fail_if( !( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ) );
		fail_if( !( cap.capabilities & V4L2_CAP_STREAMING ) );
	
		// Signal MMAP capture
		struct v4l2_requestbuffers req;
		memset( &req, 0, sizeof( req ) );
		req.count = 4;
		req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory = V4L2_MEMORY_MMAP;
		fail_neg( ioctl( VIDIOC_REQBUFS, &req ) );
		fail_if( req.count < 2 );

		// Allocate mmap buffers tracking list
		m_buffers = static_cast< struct buffer* >( calloc( req.count, sizeof( struct buffer ) ) );
		fail_null( m_buffers );
		for ( m_bufferCount = 0; m_bufferCount < req.count; m_bufferCount++ )
		{
			struct v4l2_buffer buf;
			memset( &buf, 0, sizeof( buf ) );
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = m_bufferCount;
			fail_neg( ioctl( VIDIOC_QUERYBUF, &buf ) );
			m_buffers[ m_bufferCount ].length = buf.length;

			// MMAP
			m_buffers[ m_bufferCount ].start = mmap( NULL, buf.length,
				PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset );
			fail_if( m_buffers[ m_bufferCount ].start == MAP_FAILED );
		}
		
	}
	catch ( std::string exc )
	{
		Close();
		sendEvent( exc.c_str() );
		success = false;
	}

	return success;
}

void v4l2Reader::Close( void )
{
	if ( m_buffers )
	{
		// Release mmaped buffers
		for ( unsigned int i = 0; i < m_bufferCount; i++ )
			munmap( m_buffers[i].start, m_buffers[i].length );
	
		// Release mmap buffers tracking list
		free( m_buffers );
		m_buffers = NULL;
	}

	// Close device file
	if ( m_fd > -1 )
	{
		fail_neg( close( m_fd ) );
		m_fd = -1;
	}
}

bool v4l2Reader::StartReceive( void )
{
	bool success = true;
	try
	{
		// Enqueue all V4L2 buffers
		for ( unsigned int i = 0; i < m_bufferCount; i++ )
		{
			struct v4l2_buffer buf;
			memset( &buf, 0, sizeof( buf ) );
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;
			buf.index = i;
			fail_neg( ioctl( VIDIOC_QBUF, &buf ) );
		}

		// Tell V4L2 to start
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fail_neg( ioctl( VIDIOC_STREAMON, &type ) );
	}
	catch ( std::string exc )
	{
		sendEvent( exc.c_str() );
		success = false;
	}
	return success;
}

void v4l2Reader::StopReceive( void )
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if ( ioctl( VIDIOC_STREAMOFF, &type ) < 0 )
		sendEvent( "VIDIOC_STREAMOFF failed" );
}

bool v4l2Reader::StartThread( void )
{
	if ( isRunning )
		return true;
	if ( Open() && StartReceive() )
	{
		isRunning = true;
		pthread_create( &thread, NULL, ThreadProxy, this );
		pthread_mutex_unlock( &mutex );
		return true;
	}
	else
	{
		Close();
		pthread_mutex_unlock( &mutex );
		TriggerAction( );
		return false;
	}
}

void v4l2Reader::StopThread( void )
{
	if ( isRunning )
	{
		isRunning = false;
		pthread_join( thread, NULL );
		StopReceive();
		Close();
		Flush();
	}
	TriggerAction( );
}

void* v4l2Reader::ThreadProxy( void *arg )
{
	v4l2Reader* self = static_cast< v4l2Reader* >( arg );
	return self->Thread();
}

void* v4l2Reader::Thread( void )
{
	struct pollfd v4l2_poll;
	int result;
		
	v4l2_poll.fd = m_fd;
	v4l2_poll.events = POLLIN | POLLERR | POLLHUP | POLLPRI;

	while ( isRunning )
	{
		while ( ( result = poll( &v4l2_poll, 1, 200 ) ) < 0 )
		{
			if ( !( errno == EAGAIN || errno == EINTR ) )
			{
				perror( "error: v4l2 poll" );
				break;
			}
		}
		if ( result > 0 && ( ( v4l2_poll.revents & POLLIN )
		        || ( v4l2_poll.revents & POLLPRI ) ) )
			if ( ! Handler( ) )
				isRunning = false;
	}
	return NULL;
}

bool v4l2Reader::Handler( void )
{
	bool success = true;
	try
	{
		// Get the current V4L2 buffer
		struct v4l2_buffer buf;
		memset( &buf, 0, sizeof( buf ) );
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		int result = ioctl( VIDIOC_DQBUF, &buf );
		if ( result < 0 && errno != EAGAIN && errno != EIO )
			fail_neg( result );
		assert( buf.index < m_bufferCount );
		
		// Get a new dvgrab buffer (frame)
		if ( currentFrame == NULL )
		{
			if ( inFrames.size() > 0 )
			{
				pthread_mutex_lock( &mutex );
				currentFrame = inFrames.front();
				currentFrame->Clear();
				inFrames.pop_front();
				pthread_mutex_unlock( &mutex );
			}
			else
			{
				droppedFrames++;
				return 0;
			}
		}
	
		// Copy the data from V4L2 to dvgrab
		size_t length = CLAMP( m_buffers[buf.index].length, 0, sizeof( currentFrame->data ) );
		memcpy( currentFrame->data, m_buffers[buf.index].start, length );
		currentFrame->AddDataLen( length );
		fail_neg( ioctl( VIDIOC_QBUF, &buf ) );

		// Signal dvgrab buffer ready
		if ( currentFrame->IsComplete( ) )
		{
			pthread_mutex_lock( &mutex );
			outFrames.push_back( currentFrame );
			currentFrame = NULL;
			TriggerAction( );
			pthread_mutex_unlock( &mutex );
		}
	}
	catch ( std::string exc )
	{
		sendEvent( exc.c_str() );
		success = false;
	}
	return success;
}

int v4l2Reader::ioctl( int request, void *arg )
{
	int r;
	do r = ::ioctl( m_fd, request, arg );
	while ( -1 == r && EINTR == errno );
	return r;
}

#endif
