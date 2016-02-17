/*
* hdvframe.h
* Copyright (C) 2007 Dan Streetman <ddstreet@ieee.org>
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

#ifndef _HDVFRAME_H
#define _HDVFRAME_H 1

#include "frame.h"
#include "iec13818-1.h"
#include "iec13818-2.h"

#define MPEG2_JVC_P25	(1<<0)

class HDVStreamParams
{
public:
	HDVStreamParams();
	~HDVStreamParams();

	unsigned short program_map_PID;

	unsigned short video_stream_PID;
	unsigned short audio_stream_PID;

	unsigned short sony_private_a0_PID;
	unsigned short sony_private_a1_PID;

	Video video;

#define CARRYOVER_DATA_MAX_SIZE (4 * DATA_BUFFER_LEN)
	unsigned char carryover_data[CARRYOVER_DATA_MAX_SIZE];
	int carryover_length;

	int width, height;
	float frameRate;

	struct tm recordingDate;
	bool isRecordingDateSet;
	TimeCode timeCode;
	bool isTimeCodeSet;
	TimeCode gopTimeCode;
	bool isGOPTimeCodeSet;
};

class HDVFrame : public Frame
{
public:
	HDVFrame( HDVStreamParams *p );
	~HDVFrame();

	void SetDataLen( int len );
	void Clear();

	// Meta-data
	bool GetTimeCode( TimeCode &tc );
	bool GetRecordingDate( struct tm &rd );
	bool IsNewRecording();
	bool IsGOP();
	bool IsComplete();

	// Video info
	int GetWidth();
	int GetHeight();
	float GetFrameRate();

	// HDV or DV
	bool IsHDV();
	bool CanStartNewStream();
	bool CouldBeJVCP25();

	// This is public so the reader can set the last
	// HDVFrame as complete at end of stream/file
	void SetComplete();

protected:
	void ProcessFrame( unsigned int start );
	void ProcessPacket();
	void ProcessPAT();
	void ProcessPMT();
	void ProcessVideo();
	void ProcessAudio();
	void ProcessSonyA1();

protected:
	HDVStreamParams *params;
	HDVPacket *packet;

	struct tm recordingDate;
	bool isRecordingDateSet;
	TimeCode timeCode;
	bool isTimeCodeSet;
	bool isNewRecording;
	bool isComplete;
	bool isGOP;
	int width;
	int height;
	float frameRate;

	int lastVideoDataLen;
	int lastAudioDataLen;

	bool repeatFirstField;
};

#endif
