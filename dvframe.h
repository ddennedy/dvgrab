/*
* dvframe.h -- utilities for process DV-format frames
* Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
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

#ifndef _DVFRAME_H
#define _DVFRAME_H 1

#include "frame.h"

#ifdef HAVE_LIBDV
#include <libdv/dv.h>
#include <libdv/dv_types.h>
#else
#define DV_AUDIO_MAX_SAMPLES 1944
#ifndef CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif
#endif

#define FRAME_MAX_WIDTH 720
#define FRAME_MAX_HEIGHT 576

typedef struct Pack
{
	/// the five bytes of a packet
	unsigned char data[ 5 ];
}
Pack;

typedef struct AudioInfo
{
	int frames;
	int frequency;
	int samples;
	int channels;
	int quantization;
}
AudioInfo;


class VideoInfo
{
public:
	int width;
	int height;
	bool isPAL;
	TimeCode timeCode;
	struct tm	recDate;

	VideoInfo();

	//    string GetTimeCodeString();
	//    string GetRecDateString();
}
;


class DVFrame : public Frame
{
public:
#ifdef HAVE_LIBDV

	dv_decoder_t *decoder;
#endif

	int16_t *audio_buffers[ 4 ];

public:
	DVFrame();
	~DVFrame();

	void SetDataLen( int len );

	// Meta-data
	bool GetTimeCode( TimeCode &timeCode );
	bool GetRecordingDate( struct tm &recDate );
	bool IsNewRecording( void );
	bool IsNormalSpeed( void );
	bool IsComplete( void );

	// Video info
#ifdef HAVE_LIBDV
	int GetWidth();
	int GetHeight();
#endif
	float GetFrameRate();

	// HDV cd DV
	bool IsHDV();


	bool GetSSYBPack( int packNum, Pack &pack );
	bool GetVAUXPack( int packNum, Pack &pack );
	bool GetAAUXPack( int packNum, Pack &pack );
	bool GetAudioInfo( AudioInfo &info );
	bool GetVideoInfo( VideoInfo &info );
	int GetFrameSize( void );
	bool IsPAL( void );
	int ExtractAudio( void *sound );
	void ExtractHeader( void );

#ifdef HAVE_LIBDV
	int ExtractAudio( int16_t **channels );
	int ExtractRGB( void *rgb );
	int ExtractPreviewRGB( void *rgb );
	int ExtractYUV( void *yuv );
	int ExtractPreviewYUV( void *yuv );
	bool IsWide( void );
	void SetRecordingDate( time_t *datetime, int frame );
	void SetTimeCode( int frame );
	void Deinterlace( void *image, int bpp );
#endif

private:
#ifndef HAVE_LIBDV
	/// flag for initializing the lookup maps once at startup
	static bool maps_initialized;
	/// lookup tables for collecting the shuffled audio data
	static int palmap_ch1[ 2000 ];
	static int palmap_ch2[ 2000 ];
	static int palmap_2ch1[ 2000 ];
	static int palmap_2ch2[ 2000 ];
	static int ntscmap_ch1[ 2000 ];
	static int ntscmap_ch2[ 2000 ];
	static int ntscmap_2ch1[ 2000 ];
	static int ntscmap_2ch2[ 2000 ];
	static short compmap[ 4096 ];
#endif
};

#endif
