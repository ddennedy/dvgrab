/*
* srt.cc -- Class for writing SRT subtitle files
* Copyright (C) 2008 Pelayo Bernedo <bernedo@freenet.de>
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

#include <iomanip>
#include <string.h>

#include "srt.h"

SubtitleWriter::SubtitleWriter() :
		frameRate( 0.0 ),
		lastYear( 0 ), lastMonth( 0 ), lastDay( 0 ), lastHour( 0 ), lastMinute( 0 ),
		frameCount( 0 ), lastFrameWritten( 0 ), titleNumber( 1 ), timeCodeValid( false )
{}

SubtitleWriter::~SubtitleWriter()
{
	finishFile();
}


void SubtitleWriter::finishFile()
{
	if ( frameCount > lastFrameWritten + 1 )
	{
		if ( os_fr )
		{
			writeSubtitleFrame();
		}
		if ( os_tc )
		{
			writeSubtitleCode();
		}
	}
}

void SubtitleWriter::newFile ( const char *videoName )
{
	const char *dot, *cp;

	finishFile();
	os_fr.close();
	os_tc.close();

	frameRate = 0.0;
	lastYear = lastMonth = lastDay = lastHour = lastMinute = 0;
	frameCount = 0;
	lastFrameWritten = 0;
	titleNumber = 1;
	nextCodeToWrite.hour = nextCodeToWrite.min = nextCodeToWrite.sec = 0;
	nextCodeToWrite.frame = 0;
	timeCodeValid = false;

	dot = 0;
	cp = videoName;
	while ( *cp )
	{
		if ( *cp == '.' )
		{
			dot = cp;
		}
		++cp;
	}

	// Now dot points to the extension.
	char srtName[FILENAME_MAX];
	size_t baseLen;

	if ( dot )
	{
		baseLen = dot - videoName;
	}
	else
	{
		baseLen = cp - videoName;
	}

	if ( baseLen + 6 >= FILENAME_MAX )
	{
		os_tc.open( "datecode.srt1" );
		os_fr.open( "datecode.srt" );
	}
	else
	{
		memcpy( srtName, videoName, baseLen );
		strcpy( srtName + baseLen, ".srt1" );
		os_tc.open( srtName );
		strcpy( srtName + baseLen, ".srt0" );
		os_fr.open( srtName );
	}
}


void SubtitleWriter::frameToTime ( int frameNum, int *hh, int *mm, int *ss, int *millis )
{
	int t = int( frameNum * 1000 / frameRate );
	*millis = t % 1000;
	t /= 1000;
	*ss = t % 60;
	t /= 60;
	*mm = t % 60;
	*hh = t / 60;
}

void SubtitleWriter::codeToTime ( const TimeCode &tc, int *hh, int *mm, int *ss, int *millis )
{
	*hh = tc.hour;
	*mm = tc.min;
	*ss = tc.sec;
	*millis = int( tc.frame / frameRate * 1000 );
}

void SubtitleWriter::writeSubtitleFrame()
{
	int hh1, mm1, ss1, millis1, hh2, mm2, ss2, millis2;

	frameToTime( lastFrameWritten + 1, &hh1, &mm1, &ss1, &millis1 );
	frameToTime( frameCount -1, &hh2, &mm2, &ss2, &millis2 );
	if ( os_fr )
	{
		os_fr << titleNumber << '\n' << std::setfill( '0' );
		os_fr << std::setw( 2 ) << hh1 << ':' << std::setw( 2 ) << mm1
		<< ':' << std::setw( 2 ) << ss1 << ',' << std::setw( 3 )
		<< millis1 << " --> " << std::setw( 2 ) << hh2 << ':'
		<< std::setw( 2 ) << mm2 << ':' << std::setw( 2 ) << ss2
		<< ',' << std::setw( 3 ) << millis2 << '\n';
		os_fr << lastYear << '-' << std::setw( 2 ) << lastMonth
		<< '-' << std::setw( 2 ) << lastDay << ' '
		<< std::setw( 2 ) << lastHour << ':' << std::setw( 2 )
		<< lastMinute << "\n\n";
		os_fr.flush();
	}

	lastFrameWritten = frameCount;
}

void SubtitleWriter::writeSubtitleCode()
{
	int hh1, mm1, ss1, millis1, hh2, mm2, ss2, millis2;

	codeToTime( nextCodeToWrite, &hh1, &mm1, &ss1, &millis1 );
	codeToTime( codeNow, &hh2, &mm2, &ss2, &millis2 );
	if ( os_tc )
	{
		os_tc << titleNumber << '\n' << std::setfill( '0' );
		os_tc << std::setw( 2 ) << hh1 << ':' << std::setw( 2 ) << mm1
		<< ':' << std::setw( 2 ) << ss1 << ',' << std::setw( 3 )
		<< millis1 << " --> " << std::setw( 2 ) << hh2 << ':'
		<< std::setw( 2 ) << mm2 << ':' << std::setw( 2 ) << ss2
		<< ',' << std::setw( 3 ) << millis2 << '\n';
		os_tc << lastYear << '-' << std::setw( 2 ) << lastMonth
		<< '-' << std::setw( 2 ) << lastDay << ' '
		<< std::setw( 2 ) << lastHour << ':' << std::setw( 2 )
		<< lastMinute << "\n\n";
		os_tc.flush();
	}
	titleNumber++;
}

void SubtitleWriter::addRecordingDate ( struct tm &rd, const TimeCode &tc )
{
	int y, m, d, hh, mm;

	++frameCount;
	y = rd.tm_year + 1900;
	m = rd.tm_mon + 1;
	d = rd.tm_mday;
	hh = rd.tm_hour;
	mm = rd.tm_min;

	if ( y != lastYear || m != lastMonth || d != lastDay ||
	        hh != lastHour || mm != lastMinute )
	{
		// We must write a new subtitle.
		if ( lastYear != 0 )
		{
			writeSubtitleFrame();
			writeSubtitleCode();
			nextCodeToWrite = tc;
			titleNumber++;
		}
		lastYear = y;
		lastMonth = m;
		lastDay = d;
		lastHour = hh;
		lastMinute = mm;
	}
	codeNow = tc;
	if ( !timeCodeValid )
	{
		nextCodeToWrite = tc;
		timeCodeValid = true;
	}
}
