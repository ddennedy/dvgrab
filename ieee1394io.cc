/*
* ieee1394io.cc -- asynchronously grabbing DV data
* Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
* Copyright (C) 2003-2009 Dan Dennedy <dan@dennedy.org>
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

/**
    \page The IEEE 1394 Reader Class
 
    This text explains how the IEEE 1394 Reader class works.
 
    The IEEE1394Reader object maintains a connection to a DV
    camcorder. It reads DV frames from the camcorder and stores them
    in a queue. The frames can then be retrieved from the buffer and
    displayed, stored, or processed in other ways.
 
    The IEEE1394Reader class supports asynchronous operation: it
    starts a separate thread, which reads as fast as possible from the
    ieee1394 interface card to make sure that no frames are
    lost. Since the buffer can be configured to hold many frames, no
    frames will be lost even if the disk access is temporarily slow.
 
    There are two queues available in an IEEE1394Reader object. One
    queue holds empty frames, the other holds frames filled with DV
    content just read from the interface. During operation the reader
    thread takes unused frames from the inFrames queue, fills them and
    places them in the outFrame queue. The program can then take
    frames from the outFrames queue, process them and finally put
    them back in the inFrames queue.
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <deque>
#include <iostream>
#include <typeinfo>

using std::endl;

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <libavc1394/rom1394.h>

#include "ieee1394io.h"
#include "dvframe.h"
#include "hdvframe.h"
#include "error.h"

/** Initializes the IEEE1394Reader object.
 
    The object is initialized with port and channel number. These
    parameters define the interface card and the iso channel on which
    the camcorder sends its data.
 
    The object contains a list of empty frames, which are allocated
    here. 50 frames (2 seconds) should be enough in most cases.
 
    \param c the iso channel number to use
    \param bufSize the number of frames to allocate for the frames buffer
 */


IEEE1394Reader::IEEE1394Reader( int c, int bufSize, bool hdv ) :
	droppedFrames( 0 ),
	badFrames( 0 ),
	currentFrame( NULL ),
	channel( c ),
	isRunning( false ),
	isHDV( hdv )
{
	Frame * frame;

	/* Create empty frames and put them in our inFrames queue */
	for ( int i = 0; i < bufSize; ++i )
	{
		if ( isHDV )
			frame = new HDVFrame( &hdvStreamParams );
		else
			frame = new DVFrame();

		inFrames.push_back( frame );
	}

	/* Initialize mutexes */
	pthread_mutex_init( &mutex, NULL );

	/* Initialise mutex and condition for action triggerring */
	pthread_mutex_init( &condition_mutex, NULL );
	pthread_cond_init( &condition, NULL );

}


/** Destroys the IEEE1394Reader object.
 
    In particular, it deletes all frames in the inFrames and outFrames
    queues, as well as the one currently in use.  Note that one or
    more frames may have been taken out of the queues by a user of the
    IEEE1394Reader class.
 
*/

IEEE1394Reader::~IEEE1394Reader()
{
	Frame * frame;

	for ( int i = inFrames.size(); i > 0; --i )
	{
		frame = inFrames[ 0 ];
		inFrames.pop_front();
		delete frame;
	}
	for ( int i = outFrames.size(); i > 0; --i )
	{
		frame = outFrames[ 0 ];
		outFrames.pop_front();
		delete frame;
	}
	if ( currentFrame != NULL )
	{
		delete currentFrame;
		currentFrame = NULL;
	}
	pthread_mutex_destroy( &condition_mutex );
	pthread_cond_destroy( &condition );
}


/** Fetches the next frame from the output queue
 
    The outFrames contains a list of frames to be processed (saved,
    displayed) by the user of this class.  Copy the first frame
    (actually only a pointer to it) and remove it from the queue.
 
    \note If this returns NULL, wait some time (1/25 sec.) before
    calling it again.
 
    \return a pointer to the current frame, or NULL if no frames are
    in the queue
 
 */

Frame* IEEE1394Reader::GetFrame()
{
	Frame * frame = NULL;

	pthread_mutex_lock( &mutex );

	if ( outFrames.size() > 0 )
	{
		frame = outFrames[ 0 ];
		outFrames.pop_front();
	}
	pthread_mutex_unlock( &mutex );

	return frame;
}


/** Put back a frame to the queue of available frames
*/

void IEEE1394Reader::DoneWithFrame( Frame* frame )
{
	pthread_mutex_lock( &mutex );
	inFrames.push_back( frame );
	pthread_mutex_unlock( &mutex );
}


/** Return the number of dropped frames since last call
*/

int IEEE1394Reader::GetDroppedFrames( void )
{
	pthread_mutex_lock( &mutex );
	int n = droppedFrames;
	droppedFrames = 0;
	pthread_mutex_unlock( &mutex );
	return n;
}


/** Return the number of incomplete frames since last call
*/

int IEEE1394Reader::GetBadFrames( void )
{
	pthread_mutex_lock( &mutex );
	int n = badFrames;
	badFrames = 0;
	pthread_mutex_unlock( &mutex );
	return n;
}


/** Throw away all currently available frames.
 
    All frames in the outFrames queue are put back to the inFrames
    queue.  Also the currentFrame is put back too.  */

void IEEE1394Reader::Flush()
{
	Frame * frame = NULL;

	for ( int i = outFrames.size(); i > 0; --i )
	{
		frame = outFrames[ 0 ];
		outFrames.pop_front();
		inFrames.push_back( frame );
	}
	if ( currentFrame != NULL )
	{
		inFrames.push_back( currentFrame );
		currentFrame = NULL;
	}
}

bool IEEE1394Reader::WaitForAction( int seconds )
{
	pthread_mutex_lock( &mutex );
	int size = outFrames.size( );
	pthread_mutex_unlock( &mutex );

	if ( size == 0 )
	{
		pthread_mutex_lock( &condition_mutex );
		if ( seconds == 0 )
		{
			pthread_cond_wait( &condition, &condition_mutex );
			pthread_mutex_unlock( &condition_mutex );
			pthread_mutex_lock( &mutex );
			size = outFrames.size( );
		}
		else
		{
			struct timeval tp;
			struct timespec ts;
			int result;

			gettimeofday( &tp, NULL );
			ts.tv_sec = tp.tv_sec + seconds;
			ts.tv_nsec = tp.tv_usec * 1000;

			result = pthread_cond_timedwait( &condition, &condition_mutex, &ts );
			pthread_mutex_unlock( &condition_mutex );
			pthread_mutex_lock( &mutex );

			if ( result == ETIMEDOUT )
				size = 0;
			else
				size = outFrames.size();
		}
		pthread_mutex_unlock( &mutex );
	}

	return size != 0;
}

void IEEE1394Reader::TriggerAction( )
{
	pthread_mutex_lock( &condition_mutex );
	pthread_cond_signal( &condition );
	pthread_mutex_unlock( &condition_mutex );
}


/** Initializes the raw1394Reader object.
 
    The object is initialized with port and channel number. These
    parameters define the interface card and the iso channel on which
    the camcorder sends its data.
 
    \param p the number of the interface card to use
    \param c the iso channel number to use
    \param bufSize the number of frames to allocate for the frames buffer
 */


iec61883Reader::iec61883Reader( int p, int c, int bufSize,
	BusResetHandler resetHandler, BusResetHandlerData data, bool hdv ) :
		IEEE1394Reader( c, bufSize, hdv ), m_port( p ), m_resetHandler( resetHandler),
		m_resetHandlerData( data )
{
	m_handle = NULL;
	m_iec61883_mpeg2 = NULL;
	m_iec61883_dv = NULL;
}


iec61883Reader::~iec61883Reader()
{
	Close();
}


/** Start receiving DV frames
 
    The ieee1394 subsystem is initialized with the parameters provided
    to the constructor (port and channel).  The received frames can be
    retrieved from the outFrames queue.
 
*/

bool iec61883Reader::StartThread()
{
	if ( isRunning )
		return true;
	pthread_mutex_lock( &mutex );
	currentFrame = NULL;
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
		return false;
	}
}


/** Stop the receiver thread.
 
    The receiver thread is being canceled. It will finish the next
    time it calls the pthread_testcancel() function.  After it is
    canceled, we turn off iso receive and close the ieee1394
    subsystem.  We also remove all frames in the outFrames queue that
    have not been processed until now.
 
*/

void iec61883Reader::StopThread()
{
	if ( isRunning )
	{
		isRunning = false;
		pthread_join( thread, NULL );
		StopReceive();
		Close();
		Flush();
		TriggerAction( );
	}
}


void iec61883Reader::ResetHandler( void )
{
	if ( m_resetHandler )
		m_resetHandler( const_cast< void* >( m_resetHandlerData ) );
}

int iec61883Reader::ResetHandlerProxy( raw1394handle_t handle, unsigned int generation )
{
	iec61883Reader *self = NULL;
	void *userdata = raw1394_get_userdata( handle );

	if ( typeid( iec61883_mpeg2_t ) == typeid( userdata ) )
	{
		iec61883_mpeg2_t mpeg2 = static_cast< iec61883_mpeg2_t >( userdata );
		self = static_cast< iec61883Reader* >( iec61883_mpeg2_get_callback_data( mpeg2 ) );
	}
	else if ( typeid( iec61883_dv_t ) == typeid( userdata ) )
	{
		iec61883_dv_t dv = static_cast< iec61883_dv_t >( userdata );
		iec61883_dv_fb_t dvfb = static_cast< iec61883_dv_fb_t >( iec61883_dv_get_callback_data( dv ) );
		self = static_cast< iec61883Reader* >( iec61883_dv_fb_get_callback_data( dvfb ) );
	}
	if ( self )
		self->ResetHandler();

	return 0;
}


/** Open the raw1394 interface
 
    \return success/failure
*/

bool iec61883Reader::Open()
{
	bool success;

	assert( m_handle == 0 );

	try
	{
		m_handle = raw1394_new_handle_on_port( m_port );
		if ( m_handle == NULL )
			return false;
		raw1394_set_bus_reset_handler( m_handle, this->ResetHandlerProxy );

		if ( isHDV )
		{
			m_iec61883_mpeg2 = iec61883_mpeg2_recv_init( m_handle, Mpeg2HandlerProxy, this );
			success = ( m_iec61883_mpeg2 != NULL );
		}
		else
		{
			m_iec61883_dv = iec61883_dv_fb_init( m_handle, DvHandlerProxy, this );
			success = ( m_iec61883_dv != NULL );
		}
	}
	catch ( string exc )
	{
		Close();
		sendEvent( exc.c_str() );
		success = false;
	}
	return success;
}


/** Close the raw1394 interface
 
*/

void iec61883Reader::Close()
{
	if ( m_iec61883_dv != NULL )
	{
		iec61883_dv_fb_close( m_iec61883_dv );
		m_iec61883_dv = NULL;
	}
	else if ( m_iec61883_mpeg2 != NULL )
	{
		iec61883_mpeg2_close( m_iec61883_mpeg2 );
		m_iec61883_mpeg2 = NULL;
	}
	if ( m_handle )
	{
		raw1394_destroy_handle( m_handle );
		m_handle = NULL;
	}
}

bool iec61883Reader::StartReceive()
{
	bool success;

	/* Starting iso receive */
	try
	{
		if ( isHDV )
			fail_neg( iec61883_mpeg2_recv_start( m_iec61883_mpeg2, channel ) );
		else
			fail_neg( iec61883_dv_fb_start( m_iec61883_dv, channel ) );
		success = true;
	}
	catch ( string exc )
	{
		sendEvent( exc.c_str() );
		success = false;
	}
	return success;
}


void iec61883Reader::StopReceive()
{
	if ( m_iec61883_dv != NULL )
	{
		iec61883_dv_fb_stop( m_iec61883_dv );
	}
	else if ( m_iec61883_mpeg2 != NULL )
	{
		iec61883_mpeg2_recv_stop( m_iec61883_mpeg2 );
	}
}

int iec61883Reader::Mpeg2HandlerProxy( unsigned char *data, int length,
	unsigned int dropped, void *callback_data )
{
	iec61883Reader *self = static_cast< iec61883Reader* >( callback_data );
	return self->Handler( data, length, dropped );
}

int iec61883Reader::DvHandlerProxy( unsigned char *data, int length,
	int complete, void *callback_data )
{
	iec61883Reader *self = static_cast< iec61883Reader* >( callback_data );
	return self->Handler( data, length, !complete );
}

int iec61883Reader::Handler( unsigned char *data, int length, int dropped )
{
	badFrames += dropped;

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

	memcpy( &currentFrame->data[currentFrame->GetDataLen()], data, length );
	currentFrame->AddDataLen( length );

	if ( currentFrame->IsComplete( ) )
	{
		pthread_mutex_lock( &mutex );
		outFrames.push_back( currentFrame );
		currentFrame = NULL;
		TriggerAction( );
		pthread_mutex_unlock( &mutex );
	}

	return 0;
}


/** The thread responsible for polling the raw1394 interface.
 
    Though this is an infinite loop, it can be canceled by StopThread,
    but only in the pthread_testcancel() function.
 
*/
void* iec61883Reader::ThreadProxy( void* arg )
{
	iec61883Reader* self = static_cast< iec61883Reader* >( arg );
	return self->Thread();
}

void* iec61883Reader::Thread()
{
	struct pollfd raw1394_poll;
	int result;
		
	raw1394_poll.fd = raw1394_get_fd( m_handle );
	raw1394_poll.events = POLLIN | POLLERR | POLLHUP | POLLPRI;

	while ( isRunning )
	{
		while ( ( result = poll( &raw1394_poll, 1, 200 ) ) < 0 )
		{
			if ( !( errno == EAGAIN || errno == EINTR ) )
			{
				perror( "error: raw1394 poll" );
				break;
			}
		}
		if ( result > 0 && ( ( raw1394_poll.revents & POLLIN )
		        || ( raw1394_poll.revents & POLLPRI ) ) )
			result = raw1394_loop_iterate( m_handle );
	}
	return NULL;
}


iec61883Connection::iec61883Connection( int port, int node ) :
	m_node( node | 0xffc0 ), m_channel( -1 ), m_bandwidth( 0 ),
	m_outputPort( -1 ), m_inputPort( -1 )
{
	m_handle = raw1394_new_handle_on_port( port );
	if ( m_handle )
	{
		m_channel = iec61883_cmp_connect( m_handle, m_node, &m_outputPort,
			raw1394_get_local_id( m_handle ), &m_inputPort, &m_bandwidth );
		if ( m_channel < 0 )
			m_channel = 63;
	}
}

iec61883Connection::~iec61883Connection( )
{
	if ( m_handle )
	{
		iec61883_cmp_disconnect( m_handle, m_node, m_outputPort,
			raw1394_get_local_id (m_handle), m_inputPort,
			m_channel, m_bandwidth );
		raw1394_destroy_handle( m_handle );
	}
}

void iec61883Connection::CheckConsistency( int port, int node )
{
	raw1394handle_t handle = raw1394_new_handle_on_port( port );
	if ( handle )
	{
		iec61883_cmp_normalize_output( handle, 0xffc0 | node );
		raw1394_destroy_handle( handle );
	}
}


int iec61883Connection::Reconnect( void )
{
	return iec61883_cmp_reconnect( m_handle, m_node, &m_outputPort,
		raw1394_get_local_id( m_handle ), &m_inputPort,
		&m_bandwidth, m_channel );
}


/** Initializes the AVC object.
 
    \param p the number of the interface card to use (port)
 */


AVC::AVC( int p ) : port( p )
{
	pthread_mutex_init( &avc_mutex, NULL );
	avc_handle = NULL;
	int numcards;
	struct raw1394_portinfo pinf[ 16 ];

	try
	{
		avc_handle = raw1394_new_handle();
		if ( avc_handle == 0 )
			return ;
		fail_neg( numcards = raw1394_get_port_info( avc_handle, pinf, 16 ) );
		fail_neg( raw1394_set_port( avc_handle, port ) );

	}
	catch ( string exc )
	{
		if ( avc_handle != NULL )
			raw1394_destroy_handle( avc_handle );
		avc_handle = NULL;
		sendEvent( exc.c_str() );
	}
	return ;
}


/** Destroys the AVC object.
 
*/

AVC::~AVC()
{
	if ( avc_handle != NULL )
	{
		pthread_mutex_lock( &avc_mutex );
		raw1394_destroy_handle( avc_handle );
		avc_handle = NULL;
		pthread_mutex_unlock( &avc_mutex );
	}
}


/** See if a node_id is still valid and pointing to an AV/C Recorder.
 
	If the node_id is not valid, then look for the first AV/C device on
	the bus;
	
	\param phyID The node_id to check.
	\return The same node_id if valid, a new node_id if not valid and a
	        another AV/C recorder exists, or -1 if not valid and no
			AV/C recorders exist.
   
*/
int AVC::isPhyIDValid( int phyID )
{
	int value = -1;
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		int currentNode, nodeCount;
		rom1394_directory rom1394_dir;

		nodeCount = raw1394_get_nodecount( avc_handle );

		if ( phyID >= 0 && phyID < nodeCount )
		{
			rom1394_get_directory( avc_handle, phyID, &rom1394_dir );
			if ( rom1394_get_node_type( &rom1394_dir ) == ROM1394_NODE_TYPE_AVC )
			{
				if ( avc1394_check_subunit_type( avc_handle, phyID, AVC1394_SUBUNIT_TYPE_VCR ) )
					value = phyID;
			}
			rom1394_free_directory( &rom1394_dir );
		}

		// look for a new AVC recorder
		for ( currentNode = 0; value == -1 && currentNode < nodeCount; currentNode++ )
		{
			rom1394_get_directory( avc_handle, currentNode, &rom1394_dir );
			if ( rom1394_get_node_type( &rom1394_dir ) == ROM1394_NODE_TYPE_AVC )
			{
				if ( avc1394_check_subunit_type( avc_handle, currentNode, AVC1394_SUBUNIT_TYPE_VCR ) )
				{
					// set Preferences to the newly found AVC node and return
					//octlet_t guid = rom1394_get_guid( avc_handle, currentNode );
					//snprintf( Preferences::getInstance().avcGUID, 64, "%08x%08x", (quadlet_t) (guid>>32),
					//(quadlet_t) (guid & 0xffffffff) );
					value = currentNode;
				}
			}
			rom1394_free_directory( &rom1394_dir );
		}
	}
	pthread_mutex_unlock( &avc_mutex );
	return value;
}


/** Do not do anything but let raw1394 make necessary
    callbacks (bus reset)
*/
void AVC::Noop( void )
{
	struct pollfd raw1394_poll;
	raw1394_poll.fd = raw1394_get_fd( avc_handle );
	raw1394_poll.events = POLLIN | POLLPRI;
	raw1394_poll.revents = 0;
	if ( poll( &raw1394_poll, 1, 100 ) > 0 )
	{
		if ( ( raw1394_poll.revents & POLLIN )
		        || ( raw1394_poll.revents & POLLPRI ) )
			raw1394_loop_iterate( avc_handle );
	}
}


int AVC::Play( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
		{
			if ( !avc1394_vcr_is_recording( avc_handle, phyID ) &&
				avc1394_vcr_is_playing( avc_handle, phyID ) != AVC1394_VCR_OPERAND_PLAY_FORWARD )
					avc1394_vcr_play( avc_handle, phyID );
		}
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}


int AVC::Pause( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
		{
			if ( !avc1394_vcr_is_recording( avc_handle, phyID ) &&
				( avc1394_vcr_is_playing( avc_handle, phyID ) != AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE ) )
					avc1394_vcr_pause( avc_handle, phyID );
		}
	}
	struct timespec t =
	    {
		    0, 250000000L
	    };
	nanosleep( &t, NULL );
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}


int AVC::Stop( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_stop( avc_handle, phyID );
	}
	struct timespec t =
	    {
		    0, 250000000L
	    };
	nanosleep( &t, NULL );
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}


int AVC::Rewind( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_rewind( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}


int AVC::FastForward( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_forward( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

int AVC::Forward( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_next( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

int AVC::Back( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_previous( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

int AVC::NextScene( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_next_index( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

int AVC::PreviousScene( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_previous_index( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

int AVC::Record( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_record( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

int AVC::Shuttle( int phyID, int speed )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_trick_play( avc_handle, phyID, speed );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

unsigned int AVC::TransportStatus( int phyID )
{
	quadlet_t val = 0;
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			val = avc1394_vcr_status( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return val;
}

bool AVC::Timecode( int phyID, char* timecode )
{
	bool result = false;
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
		{
			quadlet_t request[ 2 ];
			quadlet_t *response;

			request[ 0 ] = AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_TAPE_RECORDER | AVC1394_SUBUNIT_ID_0 |
			               AVC1394_VCR_COMMAND_TIME_CODE | AVC1394_VCR_OPERAND_TIME_CODE_STATUS;
			request[ 1 ] = 0xFFFFFFFF;
			response = avc1394_transaction_block( avc_handle, phyID, request, 2, 1 );
			if ( response && response[1] != 0xffffffff )
			{
				sprintf( timecode, "%2.2x:%2.2x:%2.2x:%2.2x",
			         response[ 1 ] & 0x000000ff,
			         ( response[ 1 ] >> 8 ) & 0x000000ff,
			         ( response[ 1 ] >> 16 ) & 0x000000ff,
			         ( response[ 1 ] >> 24 ) & 0x000000ff );
				result = true;
			}
		}

	}
	pthread_mutex_unlock( &avc_mutex );
	return result;
}

int AVC::getNodeId( const char *guid )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		for ( int currentNode = 0; currentNode < raw1394_get_nodecount( avc_handle ); currentNode++ )
		{
			octlet_t currentGUID = rom1394_get_guid( avc_handle, currentNode );
			char currentGUIDStr[ 65 ];
			snprintf( currentGUIDStr, 64, "%08x%08x", ( quadlet_t ) ( currentGUID >> 32 ),
			          ( quadlet_t ) ( currentGUID & 0xffffffff ) );
			if ( strncmp( currentGUIDStr, guid, 64 ) == 0 )
			{
				pthread_mutex_unlock( &avc_mutex );
				return currentNode;
			}
		}
		pthread_mutex_unlock( &avc_mutex );
	}
	return -1;
}

int AVC::Reverse( int phyID )
{
	pthread_mutex_lock( &avc_mutex );
	if ( avc_handle != NULL )
	{
		if ( phyID >= 0 )
			avc1394_vcr_reverse( avc_handle, phyID );
	}
	pthread_mutex_unlock( &avc_mutex );
	return 0;
}

bool AVC::isHDV( int phyID ) const
{
	int retry = 2;
	quadlet_t response = avc1394_transaction( avc_handle, phyID,
		AVC1394_CTYPE_STATUS | AVC1394_SUBUNIT_TYPE_TAPE_RECORDER | AVC1394_SUBUNIT_ID_0
		| AVC1394_VCR_COMMAND_OUTPUT_SIGNAL_MODE | 0xFF, retry );
	response = AVC1394_GET_OPERAND0( response );
// 	fprintf(stderr, "%s: 0x%x\n", __PRETTY_FUNCTION__, response);
	return ( response == 0x10 || response == 0x90 || response == 0x1A || response == 0x9A );
}

/** Start receiving DV frames
 
    The received frames can be retrieved from the outFrames queue.
 
*/
bool pipeReader::StartThread()
{
	pthread_mutex_lock( &mutex );
	currentFrame = NULL;
	pthread_create( &thread, NULL, ThreadProxy, this );
	pthread_mutex_unlock( &mutex );
	return true;
}


/** Stop the receiver thread.
 
    The receiver thread is being canceled. It will finish the next
    time it calls the pthread_testcancel() function. We also remove all frames 
    in the outFrames queue that have not been processed until now.
 
*/
void pipeReader::StopThread()
{
	pthread_cancel( thread );
	pthread_join( thread, NULL );
	Flush();
}


bool pipeReader::Handler()
{
	bool ret = true;

	pthread_mutex_lock( &mutex );
	if ( currentFrame == NULL && inFrames.size() > 0 )
	{
		currentFrame = inFrames.front();
		currentFrame->Clear();
		inFrames.pop_front();
		//printf("reader < buf: buffer %d, output %d\n", inFrames.size(), outFrames.size());
		//fflush(stdout);
	}
	pthread_mutex_unlock( &mutex );
	if ( currentFrame != NULL )
	{
		if ( isHDV )
		{
			void *buf = &currentFrame->data[currentFrame->GetDataLen()];
			if ( ret = ( fread( buf, IEC61883_MPEG2_TSP_SIZE, 1, file ) == 1 ) )
				currentFrame->AddDataLen( IEC61883_MPEG2_TSP_SIZE );
			else
				((HDVFrame*)currentFrame)->SetComplete();
		}
		else
		{
			if ( ret = ( fread( currentFrame->data, 120000, 1, file ) == 1 ) )
			{
				currentFrame->SetDataLen( 120000 );

				if ( currentFrame->data[ 3 ] & 0x80 )
					if ( ret = ( fread( currentFrame->data + 120000, 24000, 1, file ) == 1 ) )
						currentFrame->AddDataLen( 24000 );
			}
		}

		if ( ( ret && currentFrame->IsComplete() ) || ( !ret && currentFrame->GetDataLen() > 0 ) )
		{
			pthread_mutex_lock( &mutex );
			outFrames.push_back( currentFrame );
			currentFrame = NULL;
			TriggerAction( );
			pthread_mutex_unlock( &mutex );
		}
	}
	return ret;
}


/** The thread responsible for polling the rawdv interface.
 
    Though this is an infinite loop, it can be canceled by StopThread,
    but only in the pthread_testcancel() function.
 
*/
void* pipeReader::ThreadProxy( void* arg )
{
	pipeReader* self = static_cast< pipeReader* >( arg );
	return self->Thread();
}

void* pipeReader::Thread()
{
	if ( strcmp( input_file, "-" ) == 0 )
		file = stdin;
	else
		file = fopen( input_file, "rb" );

	if ( ! file )
	{
		sendEvent( "No input file" );
		return NULL;
	}

	while ( true )
	{
		if ( ! Handler() )
			break;
		pthread_testcancel();
	}

	if ( strcmp( input_file, "-" ) != 0 )
		fclose( file );

	sendEvent( "End of pipe" );
	pthread_mutex_lock( &mutex );
	if ( currentFrame ) outFrames.push_back( currentFrame );
	currentFrame = NULL;
	outFrames.push_back( currentFrame );
	TriggerAction( );
	pthread_mutex_unlock( &mutex );
	return NULL;
}
