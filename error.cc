/*
* error.cc Error handling
* Copyright (C) 2000 - 2002 Arne Schirmacher <arne@schirmacher.de>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// C includes

#include <errno.h>
#include <string.h>

// C++ includes

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdio>

using std::ostringstream;
using std::string;
using std::endl;
using std::ends;

// local includes

#include "error.h"

static bool needNewLine = false;

#define MAX_DEBUG_PIDS 512
static int pids[MAX_DEBUG_PIDS];
static int n_pids = 0;

bool d_all;
bool d_hdv_pat;
bool d_hdv_pmt;
bool d_hdv_pids;
bool d_hdv_pes;
bool d_hdv_packet;
bool d_hdv_video;
bool d_hdv_sonya1;

void d_hdv_pid_add( int pid )
{
	if ( n_pids < MAX_DEBUG_PIDS )
		pids[n_pids++] = pid;
}

bool d_hdv_pid_check( int pid )
{
	for ( int i=0; i<n_pids; i++ )
		if ( pids[i] == pid )
			return true;
	return false;
}

void sendEventParams( int preline, int postline, const char *format, ... )
{
	va_list list;
	va_start( list, format );
	char line[ 1024 ];
	int ret;

	if ( needNewLine )
	{
		if ( preline == 1 )
			fprintf( stderr, "\n" );
		else if ( preline == 2 )
			fprintf( stderr, "\r%-80.80s\r", " " );
	}

	if ( postline )
		ret = snprintf( line, 1024, "%s\n", format );
	else
		ret = snprintf( line, 1024, "%s", format );

	if ( ret > 0 )
		vfprintf( stderr, line, list );
	va_end( list );

	if ( postline )
		needNewLine = false;
	else
		needNewLine = true;
}

void real_fail_neg( int eval, const char *eval_str, const char *func, const char *file, int line )
{
	if ( eval < 0 )
	{
		string exc;
		ostringstream sb;

		sb << file << ":" << line << ": In function \"" << func << "\": \"" << eval_str << "\" evaluated to " << eval;
		if ( errno != 0 )
			sb << endl << file << ":" << line << ": errno: " << errno << " (" << strerror( errno ) << ")";
		sb << ends;
		exc = sb.str();
		throw exc;
	}
}


/** error handler for NULL result codes
 
    Whenever this is called with a NULL argument, it will throw an
    exception. Typically used with functions like malloc() and new().
 
*/

void real_fail_null( const void *eval, const char *eval_str, const char *func, const char *file, int line )
{
	if ( eval == NULL )
	{

		string exc;
		ostringstream sb;

		sb << file << ":" << line << ": In function \"" << func << "\": " << eval_str << " is NULL" << ends;
		exc = sb.str();
		throw exc;
	}
}


void real_fail_if( bool eval, const char *eval_str, const char *func, const char *file, int line )
{
	if ( eval == true )
	{

		string exc;
		ostringstream sb;

		sb << file << ":" << line << ": In function \"" << func << "\": condition \"" << eval_str << "\" is true";
		if ( errno != 0 )
			sb << endl << file << ":" << line << ": errno: " << errno << " (" << strerror( errno ) << ")";
		sb << ends;
		exc = sb.str();
		throw exc;
	}
}
