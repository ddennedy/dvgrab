/*
* stringutils.cc -- C++ STL string functions
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <sstream>
using std::ostringstream;
#include <stdio.h>

#include "stringutils.h"

string StringUtils::replaceAll ( string haystack, string needle, string s )
{
	string::size_type pos = 0;
	while ( ( pos = haystack.find ( needle, pos ) ) != string::npos )
	{
		haystack.erase ( pos, needle.length() );
		haystack.insert ( pos, s );
	}
	return haystack;
}

string StringUtils::stripWhite ( string s )
{
	ostringstream dest;
	char c;
	for ( string::size_type pos = 0; pos < s.size(); ++pos )
	{
		c = s.at( pos );
		if ( c != 0x20 && c != 0x09 && c != 0x0d && c != 0x0a )
			dest << c;
	}
	dest << std::ends;
	return dest.str();
}


bool StringUtils::begins( string source, string sub )
{
	return ( source.substr ( 0, sub.length() ) == sub );
}

bool StringUtils::ends( string source, string sub )
{
	if ( sub.length() >= source.length() )
		return false;
	return ( source.substr( source.length() - sub.length() ) == sub );
}

string StringUtils::ltos( long num )
{
	char s[ 81 ];

	sprintf ( s, "%ld", num );
	return string( s );
}

string StringUtils::itos( int num )
{
	char s[ 81 ];

	sprintf ( s, "%d", num );
	return string( s );
}

string StringUtils::toLower( string s )
{
	transform( s.begin(), s.end(), s.begin(), (int(*)(int)) tolower );
	return s;
}

string StringUtils::toUpper( string s )
{
	transform( s.begin(), s.end(), s.begin(), (int(*)(int)) toupper );
	return s;
}
