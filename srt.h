/*
* srt.h -- Class for writing SRT subtitle files
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

#ifndef DVGRAB_SRT_H
#define DVGRAB_SRT_H

#include <fstream>
#include <time.h>
#include "frame.h"

class SubtitleWriter
{
	std::ofstream os_tc, os_fr; // Time code and frame number based.
	float frameRate;

	int lastYear, lastMonth, lastDay;
	int lastHour, lastMinute;
	int frameCount, lastFrameWritten;
	TimeCode codeNow, nextCodeToWrite;
	int titleNumber;
	bool timeCodeValid;

	void frameToTime( int frameNo, int *hh, int *mm, int *ss, int *millis );
	void codeToTime( const TimeCode &tc, int *hh, int *mm, int *ss, int *millis );
	void writeSubtitleFrame();
	void writeSubtitleCode();
	void finishFile();
public:
	SubtitleWriter();
	~SubtitleWriter();
	void newFile( const char *videoName );
	void setFrameRate( float fr ) { frameRate = fr; }
	bool hasFrameRate() const { return frameRate != 0.0; }
	void addRecordingDate( struct tm &rd, const TimeCode &tc );
};

#endif