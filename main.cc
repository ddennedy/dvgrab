/*
* main.cc -- A DV/1394 capture utility
* Copyright (C) 2000-2002 Arne Schirmacher <arne@schirmacher.de>
* Copyright (C) 2003-2008 Dan Dennedy <dan@dennedy.org>
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

/** the dvgrab main program
 
    contains the main logic
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// C++ includes

#include <string>
#include <iostream>
using std::cout;
using std::endl;

// C includes

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <sys/time.h> 
#include <sys/resource.h>
#include <sys/mman.h>

// local includes

#include "io.h"
#include "dvgrab.h"
#include "error.h"

bool g_done = false;

void signal_handler( int sig )
{
	g_done = true;
}

int rt_raisepri (int pri)
{
#ifdef _SC_PRIORITY_SCHEDULING
	struct sched_param scp;

	/*
	 * Verify that scheduling is available
	 */
	if ( sysconf( _SC_PRIORITY_SCHEDULING ) == -1)
	{
// 		sendEvent( "Warning: RR-scheduler not available, disabling." );
		return -1;
	}
	else 
	{
		memset( &scp, '\0', sizeof( scp ) );
		scp.sched_priority = sched_get_priority_max( SCHED_RR ) - pri;
		if ( sched_setscheduler( 0, SCHED_RR, &scp ) < 0 )
		{
// 			sendEvent( "Warning: Cannot set RR-scheduler" );
			return -1;
		}
	}
	return 0;

#else
	return -1;
#endif
}

int main( int argc, char *argv[] )
{
	int ret = 0;

	fcntl( fileno( stderr ), F_SETFL, O_NONBLOCK );
	try
	{
		char c;
		DVgrab dvgrab( argc, argv );

		signal( SIGINT, signal_handler );
		signal( SIGTERM, signal_handler );
		signal( SIGHUP, signal_handler );
		signal( SIGPIPE, signal_handler );

		if ( rt_raisepri( 1 ) != 0 )
			setpriority( PRIO_PROCESS, 0, -20 );

#if _POSIX_MEMLOCK > 0
		mlockall( MCL_CURRENT | MCL_FUTURE );
#endif

		if ( dvgrab.isInteractive() )
		{
			term_init();
			fprintf( stderr, "Going interactive. Press '?' for help.\n" );
			while ( !g_done )
			{
				dvgrab.status( );
				if ( ( c = term_read() ) != -1 )
					if ( !dvgrab.execute( c ) )
						break;
			}
			term_exit();
		}
		else
		{
			dvgrab.startCapture();
			while ( !g_done )
				if ( dvgrab.done() )
					break;
			dvgrab.stopCapture();
		}
	}
	catch ( std::string s )
	{
		fprintf( stderr, "Error: %s\n", s.c_str() );
		fflush( stderr );
		ret = 1;
	}
	catch ( ... )
	{
		fprintf( stderr, "Error: unknown\n" );
		fflush( stderr );
		ret = 1;
	}

	fprintf( stderr, "\n" );
	return ret;
}
