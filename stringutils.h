/*
* stringutils.h -- C++ STL string functions
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

#ifndef _STRINGUTILS_H
#define _STRINGUTILS_H

#include <string>
using std::string;

class StringUtils
{
public:
	static string replaceAll ( string haystack, string needle, string s );
	static string stripWhite ( string s );
	static bool begins ( string source, string sub );
	static bool ends ( string source, string sub );
	static string itos ( int num );
	static string ltos ( long num );
	static string toLower( string s );
	static string toUpper( string s );
};

#endif

