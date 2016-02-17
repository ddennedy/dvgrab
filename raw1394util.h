/*
* raw1394util.h -- libraw1394 utilities
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

#ifndef RAW1394UTIL_H
#define RAW1394UTIL_H 1

#include <libraw1394/raw1394.h>

#ifdef __cplusplus
extern "C"
{
#endif

	int raw1394_get_num_ports( void );
	raw1394handle_t raw1394_open( int port );
	void raw1394_close( raw1394handle_t handle );
	int discoverAVC( int * port, octlet_t* guid );
	void reset_bus( int port );

#ifdef __cplusplus
}
#endif
#endif
