/*
* riff.h library for RIFF file format i/o
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

#ifndef _RIFF_H
#define _RIFF_H 1

#include <vector>
using std::vector;

#include "endian_types.h"

#define QUADWORD int64_le_t
#define DWORD int32_le_t
#define LONG u_int32_le_t
#define WORD int16_le_t
#define BYTE u_int8_le_t
#define FOURCC u_int32_t      // No endian conversion needed.

#define RIFF_NO_PARENT (-1)
#define RIFF_LISTSIZE (4)
#define RIFF_HEADERSIZE (8)

#ifdef __cplusplus
extern "C"
{
	FOURCC make_fourcc( const char * s );
}
#endif

class RIFFDirEntry
{
public:
	FOURCC type;
	FOURCC name;
	off_t length;
	off_t offset;
	int parent;
	int written;

	RIFFDirEntry();
	RIFFDirEntry( FOURCC t, FOURCC n, int l, int o, int p );
};


class RIFFFile
{
public:
	RIFFFile();
	RIFFFile( const RIFFFile& );
	virtual ~RIFFFile();
	RIFFFile& operator=( const RIFFFile& );

	virtual bool Open( const char *s );
	virtual bool Create( const char *s );
	virtual void Close();
	virtual int AddDirectoryEntry( FOURCC type, FOURCC name, off_t length, int list );
	virtual void SetDirectoryEntry( int i, FOURCC type, FOURCC name, off_t length, off_t offset, int list );
	virtual void SetDirectoryEntry( int i, RIFFDirEntry &entry );
	virtual void GetDirectoryEntry( int i, FOURCC &type, FOURCC &name, off_t &length, off_t &offset, int &list ) const;
	virtual RIFFDirEntry GetDirectoryEntry( int i ) const;
	virtual off_t GetFileSize( void ) const;
	virtual void PrintDirectoryEntry( int i ) const;
	virtual void PrintDirectoryEntryData( const RIFFDirEntry &entry ) const;
	virtual void PrintDirectory( void ) const;
	virtual int FindDirectoryEntry( FOURCC type, int n = 0 ) const;
	virtual void ParseChunk( int parent );
	virtual void ParseList( int parent );
	virtual void ParseRIFF( void );
	virtual void ReadChunk( int chunk_index, void *data );
	virtual void WriteChunk( int chunk_index, const void *data );
	virtual void WriteRIFF( void );

protected:
	int fd;

private:
	vector<RIFFDirEntry> directory;
};
#endif
