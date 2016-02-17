/*
* error.h Error handling
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

#ifndef _ERROR_H
#define _ERROR_H 1

#include <stdarg.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define fail_neg(eval)  real_fail_neg  (eval, #eval, __ASSERT_FUNCTION, __FILE__, __LINE__)
#define fail_null(eval) real_fail_null (eval, #eval, __ASSERT_FUNCTION, __FILE__, __LINE__)
#define fail_if(eval)   real_fail_if   (eval, #eval, __ASSERT_FUNCTION, __FILE__, __LINE__)

extern bool d_all;
extern bool d_hdv_pat;
extern bool d_hdv_pmt;
extern bool d_hdv_pids;
extern bool d_hdv_pes;
extern bool d_hdv_packet;
extern bool d_hdv_video;
extern bool d_hdv_sonya1;

void d_hdv_pid_add( int p );
bool d_hdv_pid_check( int p );

#define DEBUG_PARAMS( type, prel, postl, msg... ) do { if ( (type) || d_all ) sendEventParams( prel, postl, msg ); } while (0)
#define DEBUG_RAW( type, msg... ) DEBUG_PARAMS( type, 0, 0, msg )
#define DEBUG( type, msg... ) DEBUG_PARAMS( type, 1, 1, msg )

#define sendEvent( msg... ) sendEventParams( 2, 1, msg )

void sendEventParams( int clearline, int newline, const char *format, ... );
void real_fail_neg ( int eval, const char * eval_str, const char * func, const char * file, int line );
void real_fail_null ( const void * eval, const char * eval_str, const char * func, const char * file, int line );
void real_fail_if ( bool eval, const char * eval_str, const char * func, const char * file, int line );

#ifdef __cplusplus
}
#endif

#endif
