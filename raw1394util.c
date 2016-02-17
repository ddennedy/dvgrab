/*
* raw1394util.c -- libraw1394 utilities
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
*
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/rom1394.h>

#include "raw1394util.h"

#define MOTDCT_SPEC_ID    0x00005068


/** Open the raw1394 device and get a handle.
 *  
 * \param port A 0-based number indicating which host adapter to use.
 * \return a raw1394 handle.
 */
int raw1394_get_num_ports( void )
{
	int n_ports;
	struct raw1394_portinfo pinf[ 16 ];
	raw1394handle_t handle;

	/* get a raw1394 handle */
	if ( !( handle = raw1394_new_handle() ) )
	{
		fprintf( stderr, "raw1394 - failed to get handle: %s.\n", strerror( errno ) );
		exit( EXIT_FAILURE );
	}

	if ( ( n_ports = raw1394_get_port_info( handle, pinf, 16 ) ) < 0 )
	{
		fprintf( stderr, "raw1394 - failed to get port info: %s.\n", strerror( errno ) );
		raw1394_destroy_handle( handle );
		exit( EXIT_FAILURE );
	}
	raw1394_destroy_handle( handle );

	return n_ports;
}

/** Open the raw1394 device and get a handle.
 *  
 * \param port A 0-based number indicating which host adapter to use.
 * \return a raw1394 handle.
 */
raw1394handle_t raw1394_open( int port )
{
	int n_ports;
	struct raw1394_portinfo pinf[ 16 ];
	raw1394handle_t handle;

	/* get a raw1394 handle */
#ifdef RAW1394_V_0_8

	handle = raw1394_get_handle();
#else

	handle = raw1394_new_handle();
#endif

	if ( !handle )
	{
		fprintf( stderr, "raw1394 - failed to get handle: %s.\n", strerror( errno ) );
		exit( EXIT_FAILURE );
	}

	if ( ( n_ports = raw1394_get_port_info( handle, pinf, 16 ) ) < 0 )
	{
		fprintf( stderr, "raw1394 - failed to get port info: %s.\n", strerror( errno ) );
		raw1394_destroy_handle( handle );
		exit( EXIT_FAILURE );
	}

	/* tell raw1394 which host adapter to use */
	if ( raw1394_set_port( handle, port ) < 0 )
	{
		fprintf( stderr, "raw1394 - failed to set set port: %s.\n", strerror( errno ) );
		exit( EXIT_FAILURE );
	}

	return handle;
}


void raw1394_close( raw1394handle_t handle )
{
	raw1394_destroy_handle( handle );
}

int discoverAVC( int* port, octlet_t* guid )
{
	rom1394_directory rom_dir;
	raw1394handle_t handle;
	int device = -1;
	int i, j = 0;
	int m = raw1394_get_num_ports();

	if ( *port >= 0 )
	{
		/* search on explicit port */
		j = *port;
		m = *port + 1;
	}

	for ( ; j < m && device == -1; j++ )
	{
		handle = raw1394_open( j );
		for ( i = 0; i < raw1394_get_nodecount( handle ); ++i )
		{
			if ( *guid > 1 )
			{
				/* select explicitly by GUID */
				if ( *guid == rom1394_get_guid( handle, i ) )
				{
					device = i;
					*port = j;
					break;
				}
			}
			else
			{
				/* select first AV/C Tape Reccorder Player node */
				if ( rom1394_get_directory( handle, i, &rom_dir ) < 0 )
				{
					rom1394_free_directory( &rom_dir );
					fprintf( stderr, "error reading config rom directory for node %d\n", i );
					continue;
				}
				if ( ( ( rom1394_get_node_type( &rom_dir ) == ROM1394_NODE_TYPE_AVC ) &&
				         avc1394_check_subunit_type( handle, i, AVC1394_SUBUNIT_TYPE_VCR ) ) ||
				       ( rom_dir.unit_spec_id == MOTDCT_SPEC_ID ) )
				{
					rom1394_free_directory( &rom_dir );
					octlet_t my_guid, *pguid = ( *guid == 1 )? guid : &my_guid;
					*pguid = rom1394_get_guid( handle, i );
					fprintf( stderr, "Found AV/C device with GUID 0x%08x%08x\n",
						(quadlet_t) (*pguid>>32), (quadlet_t) (*pguid & 0xffffffff));
					device = i;
					*port = j;
					break;
				}
				rom1394_free_directory( &rom_dir );
			}
		}
		raw1394_close( handle );
	}

	return device;
}

void reset_bus( int port )
{
	raw1394handle_t handle;

	handle = raw1394_open( port );
	raw1394_reset_bus( handle );
	raw1394_close( handle );
}
