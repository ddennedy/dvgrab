/*
* dvframe.cc -- utilities for processing DV-format frames
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

/** Code for handling raw DV frame data
 
    These methods are for handling the raw DV frame data. It contains methods for 
    getting info and retrieving the audio data.
 
    \file dvframe.cc
*/

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include "dvframe.h"

VideoInfo::VideoInfo() : width( 0 ), height( 0 ), isPAL( false )
{}

#ifndef HAVE_LIBDV
bool DVFrame::maps_initialized = false;
int DVFrame::palmap_ch1[ 2000 ];
int DVFrame::palmap_ch2[ 2000 ];
int DVFrame::palmap_2ch1[ 2000 ];
int DVFrame::palmap_2ch2[ 2000 ];

int DVFrame::ntscmap_ch1[ 2000 ];
int DVFrame::ntscmap_ch2[ 2000 ];
int DVFrame::ntscmap_2ch1[ 2000 ];
int DVFrame::ntscmap_2ch2[ 2000 ];

short DVFrame::compmap[ 4096 ];
#endif


/**ructor
 
    All DVFrame objects share a set of lookup maps,
    which are initalized once (we are using a variant of the Singleton pattern). 
 
*/

DVFrame::DVFrame()
{
#ifdef HAVE_LIBDV

	decoder = dv_decoder_new( 0, 0, 0 );
	decoder->quality = DV_QUALITY_BEST;
	decoder->audio->arg_audio_emphasis = 2;
	dv_set_audio_correction ( decoder, DV_AUDIO_CORRECT_AVERAGE );
	FILE* libdv_log = fopen( "/dev/null", "w" );
	dv_set_error_log( decoder, libdv_log );
#else

	if ( maps_initialized == false )
	{

		for ( int n = 0; n < 1944; ++n )
		{
			int sequence1 = ( ( n / 3 ) + 2 * ( n % 3 ) ) % 6;
			int sequence2 = sequence1 + 6;
			int block = 3 * ( n % 3 ) + ( ( n % 54 ) / 18 );

			block = 6 + block * 16;
			{
				register int byte = 8 + 2 * ( n / 54 );
				palmap_ch1[ n ] = sequence1 * 150 * 80 + block * 80 + byte;
				palmap_ch2[ n ] = sequence2 * 150 * 80 + block * 80 + byte;
				byte += ( n / 54 );
				palmap_2ch1[ n ] = sequence1 * 150 * 80 + block * 80 + byte;
				palmap_2ch2[ n ] = sequence2 * 150 * 80 + block * 80 + byte;
			}
		}
		for ( int n = 0; n < 1620; ++n )
		{
			int sequence1 = ( ( n / 3 ) + 2 * ( n % 3 ) ) % 5;
			int sequence2 = sequence1 + 5;
			int block = 3 * ( n % 3 ) + ( ( n % 45 ) / 15 );

			block = 6 + block * 16;
			{
				register int byte = 8 + 2 * ( n / 45 );
				ntscmap_ch1[ n ] = sequence1 * 150 * 80 + block * 80 + byte;
				ntscmap_ch2[ n ] = sequence2 * 150 * 80 + block * 80 + byte;
				byte += ( n / 45 );
				ntscmap_2ch1[ n ] = sequence1 * 150 * 80 + block * 80 + byte;
				ntscmap_2ch2[ n ] = sequence2 * 150 * 80 + block * 80 + byte;
			}
		}
		for ( int y = 0x700; y <= 0x7ff; ++y )
			compmap[ y ] = ( y - 0x600 ) << 6;
		for ( int y = 0x600; y <= 0x6ff; ++y )
			compmap[ y ] = ( y - 0x500 ) << 5;
		for ( int y = 0x500; y <= 0x5ff; ++y )
			compmap[ y ] = ( y - 0x400 ) << 4;
		for ( int y = 0x400; y <= 0x4ff; ++y )
			compmap[ y ] = ( y - 0x300 ) << 3;
		for ( int y = 0x300; y <= 0x3ff; ++y )
			compmap[ y ] = ( y - 0x200 ) << 2;
		for ( int y = 0x200; y <= 0x2ff; ++y )
			compmap[ y ] = ( y - 0x100 ) << 1;
		for ( int y = 0x000; y <= 0x1ff; ++y )
			compmap[ y ] = y;
		for ( int y = 0x800; y <= 0xfff; ++y )
			compmap[ y ] = -1 - compmap[ 0xfff - y ];
		maps_initialized = true;
	}
#endif
	for ( int n = 0; n < 4; n++ )
		audio_buffers[ n ] = ( int16_t * ) malloc( 2 * DV_AUDIO_MAX_SAMPLES * sizeof( int16_t ) );
}


DVFrame::~DVFrame()
{
#ifdef HAVE_LIBDV
	dv_decoder_free( decoder );
#endif

	for ( int n = 0; n < 4; n++ )
		free( audio_buffers[ n ] );
}


void DVFrame::SetDataLen( int len )
{
	Frame::SetDataLen( len );

	ExtractHeader();
}

bool DVFrame::IsHDV()
{
	return false;
}


/** gets a subcode data packet
 
    This function returns a SSYB packet from the subcode data section.
 
    \param packNum the SSYB package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool DVFrame::GetSSYBPack( int packNum, Pack &pack )
{
#ifdef WITH_LIBDV
	pack.data[ 0 ] = packNum;
#ifdef HAVE_LIBDV_1_0

	dv_get_vaux_pack( decoder, packNum, &pack.data[ 1 ] );
#else

	int id;
	if ( ( id = decoder->ssyb_pack[ packNum ] ) != 0xff )
	{
		pack.data[ 1 ] = decoder->ssyb_data[ id ][ 0 ];
		pack.data[ 2 ] = decoder->ssyb_data[ id ][ 1 ];
		pack.data[ 3 ] = decoder->ssyb_data[ id ][ 2 ];
		pack.data[ 4 ] = decoder->ssyb_data[ id ][ 3 ];
	}
#endif
	return true;

#else

	/* number of DIF sequences is different for PAL and NTSC */

	int seqCount = IsPAL() ? 12 : 10;

	/* process all DIF sequences */

	for ( int i = 0; i < seqCount; ++i )
	{

		/* there are two DIF blocks in the subcode section */

		for ( int j = 0; j < 2; ++j )
		{

			/* each block has 6 packets */

			for ( int k = 0; k < 6; ++k )
			{

				/* calculate address: 150 DIF blocks per sequence, 80 bytes
				per DIF block, subcode blocks start at block 1, block and
				packet have 3 bytes header, packet is 8 bytes long
				(including header) */

				const unsigned char *s = &data[ i * 150 * 80 + 1 * 80 + j * 80 + 3 + k * 8 + 3 ];
				// printf("ssyb %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
				// j * 6 + k, s[0], s[1], s[2], s[3], s[4]);
				if ( s[ 0 ] == packNum )
				{
					//					printf("GetSSYBPack[%x]: sequence %d, block %d, packet %d\n", packNum,i,j,k);
					pack.data[ 0 ] = s[ 0 ];
					pack.data[ 1 ] = s[ 1 ];
					pack.data[ 2 ] = s[ 2 ];
					pack.data[ 3 ] = s[ 3 ];
					pack.data[ 4 ] = s[ 4 ];
					return true;
				}
			}
		}
	}
	return false;
#endif
}


/** gets a video auxiliary data packet
 
    Every DIF block in the video auxiliary data section contains 15
    video auxiliary data packets, for a total of 45 VAUX packets. As
    the position of a VAUX packet is fixed, we could directly look it
    up, but I choose to walk through all data as with the other
    GetXXXX routines.
 
    \param packNum the VAUX package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool DVFrame::GetVAUXPack( int packNum, Pack &pack )
{
#ifdef WITH_LIBDV
	pack.data[ 0 ] = packNum;
	dv_get_vaux_pack( decoder, packNum, &pack.data[ 1 ] );
	//cerr << "VAUX: 0x"
	//<< setw(2) << setfill('0') << hex << (int) pack.data[0]
	//<< setw(2) << setfill('0') << hex << (int) pack.data[1]
	//<< setw(2) << setfill('0') << hex << (int) pack.data[2]
	//<< setw(2) << setfill('0') << hex << (int) pack.data[3]
	//<< setw(2) << setfill('0') << hex << (int) pack.data[4]
	//<< endl;
	return true;

#else
	/* number of DIF sequences is different for PAL and NTSC */

	int seqCount = IsPAL() ? 12 : 10;

	/* process all DIF sequences */

	for ( int i = 0; i < seqCount; ++i )
	{

		/* there are three DIF blocks in the VAUX section */

		for ( int j = 0; j < 3; ++j )
		{

			/* each block has 15 packets */

			for ( int k = 0; k < 15; ++k )
			{

				/* calculate address: 150 DIF blocks per sequence, 80 bytes
				per DIF block, vaux blocks start at block 3, block has 3
				bytes header, packets have no header and are 5 bytes
				long. */

				const unsigned char *s = &data[ i * 150 * 80 + 3 * 80 + j * 80 + 3 + k * 5 ];
				// printf("vaux %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
				// j * 15 + k, s[0],  s[1],  s[2],  s[3],  s[4]);
				if ( s[ 0 ] == packNum )
				{
					pack.data[ 0 ] = s[ 0 ];
					pack.data[ 1 ] = s[ 1 ];
					pack.data[ 2 ] = s[ 2 ];
					pack.data[ 3 ] = s[ 3 ];
					pack.data[ 4 ] = s[ 4 ];
					return true;
				}
			}
		}
	}
	return false;
#endif
}


/** gets an audio auxiliary data packet
 
    Every DIF block in the audio section contains 5 bytes audio
    auxiliary data and 72 bytes of audio data.  The function searches
    through all DIF blocks although AAUX packets are only allowed in
    certain defined DIF blocks.
 
    \param packNum the AAUX package id to return
    \param pack a reference to the variable where the result is stored
    \return true for success, false if no pack could be found */

bool DVFrame::GetAAUXPack( int packNum, Pack &pack )
{
#ifdef WITH_LIBDV
	bool done = false;
	switch ( packNum )
	{
	case 0x50:
		memcpy( pack.data, &decoder->audio->aaux_as, 5 );
		done = true;
		break;

	case 0x51:
		memcpy( pack.data, &decoder->audio->aaux_asc, 5 );
		done = true;
		break;

#ifdef HAVE_LIBDV_1_0

	case 0x52:
		memcpy( pack.data, decoder->audio->aaux_as1, 5 );
		done = true;
		break;

	case 0x53:
		memcpy( pack.data, decoder->audio->aaux_asc1, 5 );
done = true:
		       break;
#else

	default:
		break;
#endif

	}
	if ( done )
		return true;
#endif

	/* number of DIF sequences is different for PAL and NTSC */

	int seqCount = IsPAL() ? 12 : 10;

	/* process all DIF sequences */

	for ( int i = 0; i < seqCount; ++i )
	{

		/* there are nine audio DIF blocks */

		for ( int j = 0; j < 9; ++j )
		{

			/* calculate address: 150 DIF blocks per sequence, 80 bytes
			   per DIF block, audio blocks start at every 16th beginning
			   with block 6, block has 3 bytes header, followed by one
			   packet. */

			const unsigned char *s = &data[ i * 150 * 80 + 6 * 80 + j * 16 * 80 + 3 ];
			if ( s[ 0 ] == packNum )
			{
				// printf("aaux %d: %2.2x %2.2x %2.2x %2.2x %2.2x\n",
				// j, s[0], s[1], s[2], s[3], s[4]);
				pack.data[ 0 ] = s[ 0 ];
				pack.data[ 1 ] = s[ 1 ];
				pack.data[ 2 ] = s[ 2 ];
				pack.data[ 3 ] = s[ 3 ];
				pack.data[ 4 ] = s[ 4 ];
				return true;
			}
		}
	}
	return false;
}


/** gets the date and time of recording of this frame
 
    Returns a struct tm with date and time of recording of this frame.
 
    This code courtesy of Andy (http://www.videox.net/) 
 
    \param recDate the time and date of recording of this frame
    \return true for success, false if no date or time information could be found */

bool DVFrame::GetRecordingDate( struct tm &recDate )
{
#ifdef HAVE_LIBDV
	return dv_get_recording_datetime_tm( decoder, ( struct tm * ) &recDate );
#else

	Pack pack62;
	Pack pack63;

	if ( GetSSYBPack( 0x62, pack62 ) == false )
		return false;

	int day = pack62.data[ 2 ];
	int month = pack62.data[ 3 ];
	int year = pack62.data[ 4 ];

	if ( GetSSYBPack( 0x63, pack63 ) == false )
		return false;

	int sec = pack63.data[ 2 ];
	int min = pack63.data[ 3 ];
	int hour = pack63.data[ 4 ];

	sec = ( sec & 0xf ) + 10 * ( ( sec >> 4 ) & 0x7 );
	min = ( min & 0xf ) + 10 * ( ( min >> 4 ) & 0x7 );
	hour = ( hour & 0xf ) + 10 * ( ( hour >> 4 ) & 0x3 );
	year = ( year & 0xf ) + 10 * ( ( year >> 4 ) & 0xf );
	month = ( month & 0xf ) + 10 * ( ( month >> 4 ) & 0x1 );
	day = ( day & 0xf ) + 10 * ( ( day >> 4 ) & 0x3 );

	if ( year < 25 )
		year += 2000;
	else
		year += 1900;

	recDate.tm_sec = sec;
	recDate.tm_min = min;
	recDate.tm_hour = hour;
	recDate.tm_mday = day;
	recDate.tm_mon = month - 1;
	recDate.tm_year = year - 1900;
	recDate.tm_wday = -1;
	recDate.tm_yday = -1;
	recDate.tm_isdst = -1;

	/* sanity check of the results */

	if ( mktime( &recDate ) == -1 )
		return false;
	return true;
#endif
}


/** gets the timecode information of this frame
 
    Returns a string with the timecode of this frame. The timecode is
    the relative location of this frame on the tape, and is defined
    by hour, minute, second and frame (within the last second).
     
    \param timeCode the TimeCode struct
    \return true for success, false if no timecode information could be found */

bool DVFrame::GetTimeCode( TimeCode &timeCode )
{
#ifdef HAVE_LIBDV
	int timestamp[ 4 ];

	if ( dv_get_timestamp_int( decoder, timestamp ) ) {
		timeCode.hour = timestamp[ 0 ];
		timeCode.min = timestamp[ 1 ];
		timeCode.sec = timestamp[ 2 ];
		timeCode.frame = timestamp[ 3 ];
		return true;
	} else {
		timeCode.hour = 0;
		timeCode.min = 0;
		timeCode.sec = 0;
		timeCode.frame = 0;
		return false;
	}
#else

	Pack tc;

	if ( GetSSYBPack( 0x13, tc ) == false )
		return false;

	int frame = tc.data[ 1 ];
	int sec = tc.data[ 2 ];
	int min = tc.data[ 3 ];
	int hour = tc.data[ 4 ];

	timeCode.frame = ( frame & 0xf ) + 10 * ( ( frame >> 4 ) & 0x3 );
	timeCode.sec = ( sec & 0xf ) + 10 * ( ( sec >> 4 ) & 0x7 );
	timeCode.min = ( min & 0xf ) + 10 * ( ( min >> 4 ) & 0x7 );
	timeCode.hour = ( hour & 0xf ) + 10 * ( ( hour >> 4 ) & 0x3 );
	return true;
#endif
}


/** gets the audio properties of this frame
 
    get the sampling frequency and the number of samples in this particular DV frame (which can vary)
 
    \param info the AudioInfo record
    \return true, if audio properties could be determined */

bool DVFrame::GetAudioInfo( AudioInfo &info )
{
#ifdef HAVE_LIBDV
	info.frequency = decoder->audio->frequency;
	info.samples = decoder->audio->samples_this_frame;
	info.frames = ( decoder->audio->aaux_as.pc3.system == 1 ) ? 50 : 60;
	info.channels = decoder->audio->num_channels;
	info.quantization = ( decoder->audio->aaux_as.pc4.qu == 0 ) ? 16 : 12;
	return true;
#else

	int af_size;
	int smp;
	int flag;
	Pack pack50;


	info.channels = 2;
	info.quantization = 16;

	/* Check whether this frame has a valid AAUX source packet
	(header == 0x50). If so, get the audio samples count. If not,
	skip this audio data. */

	if ( GetAAUXPack( 0x50, pack50 ) == true )
	{

		/* get size, sampling type and the 50/60 flag. The number of
		audio samples is dependend on all of these. */

		af_size = pack50.data[ 1 ] & 0x3f;
		smp = ( pack50.data[ 4 ] >> 3 ) & 0x07;
		flag = pack50.data[ 3 ] & 0x20;

		if ( flag == 0 )
		{
			info.frames = 60;
			switch ( smp )
			{
			case 0:
				info.frequency = 48000;
				info.samples = 1580 + af_size;
				break;
			case 1:
				info.frequency = 44100;
				info.samples = 1452 + af_size;
				break;
			case 2:
				info.frequency = 32000;
				info.samples = 1053 + af_size;
				info.quantization = 12;
				break;
			}
		}
		else
		{ // 50 frames (PAL)
			info.frames = 50;
			switch ( smp )
			{
			case 0:
				info.frequency = 48000;
				info.samples = 1896 + af_size;
				break;
			case 1:
				info.frequency = 44100;
				info.samples = 0; // I don't know
				break;
			case 2:
				info.frequency = 32000;
				info.samples = 1264 + af_size;
				info.quantization = 12;
				break;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
#endif
}


bool DVFrame::GetVideoInfo( VideoInfo &info )
{
	GetTimeCode( info.timeCode );
	GetRecordingDate( info.recDate );
	info.isPAL = IsPAL();
	return true;
}


/** gets the size of the frame
 
    Depending on the type (PAL or NTSC) of the frame, the length of the frame is returned 
 
    \return the length of the frame in Bytes */

int DVFrame::GetFrameSize( void )
{
	return IsPAL() ? 144000 : 120000;
}


/** get the video frame rate
 
    \return frames per second
*/
float DVFrame::GetFrameRate()
{
	return IsPAL() ? 25.0 : 30000.0 / 1001.0;
}


/** checks whether the frame is in PAL or NTSC format
 
    \todo function can't handle "empty" frame
    \return true for PAL frame, false for a NTSC frame
*/

bool DVFrame::IsPAL( void )
{
	unsigned char dsf = data[ 3 ] & 0x80;
	bool pal = ( dsf == 0 ) ? false : true;
#ifdef HAVE_LIBDV

	if ( !pal )
	{
		pal = ( dv_system_50_fields ( decoder ) == 1 ) ? true : pal;
	}
#endif
	return pal;
}


/** checks whether this frame is the first in a new recording
 
    To determine this, the function looks at the recStartPoint bit in
    AAUX pack 51.
 
    \return true if this frame is the start of a new recording
*/

bool DVFrame::IsNewRecording()
{
#ifdef HAVE_LIBDV
	return ( decoder->audio->aaux_asc.pc2.rec_st == 0 );
#else

	Pack aauxSourceControl;

	/* if we can't find the packet, we return "no new recording" */

	if ( GetAAUXPack( 0x51, aauxSourceControl ) == false )
		return false;

	unsigned char recStartPoint = aauxSourceControl.data[ 2 ] & 0x80;

	return recStartPoint == 0 ? true : false;
#endif
}


/** checks whether this frame is playing at normal speed
 
    To determine this, the function looks at the speed bit in
    AAUX pack 51.
 
    \return true if this frame is playing at normal speed 
*/

bool DVFrame::IsNormalSpeed()
{
	bool normal_speed = true;

#ifdef HAVE_LIBDV
	/* don't do audio if speed is not 1 */
	if ( decoder->std == e_dv_std_iec_61834 )
	{
		normal_speed = ( decoder->audio->aaux_asc.pc3.speed == 0x20 );
	}
	else if ( decoder->std == e_dv_std_smpte_314m )
	{
		if ( decoder->audio->aaux_as.pc3.system )
			/* PAL */
			normal_speed = ( decoder->audio->aaux_asc.pc3.speed == 0x64 );
		else
			/* NTSC */
			normal_speed = ( decoder->audio->aaux_asc.pc3.speed == 0x78 );
	}
#endif

	return ( normal_speed );
}


/** check whether we have received as many bytes as expected for this frame
 
    \return true if this frames is completed, false otherwise */

bool DVFrame::IsComplete( void )
{
	return GetDataLen() == GetFrameSize();
}


/** retrieves the audio data from the frame
 
    The DV frame contains audio data mixed in the video data blocks, 
    which can be retrieved easily using this function.
 
    The audio data consists of 16 bit, two channel audio samples (a 16 bit word for channel 1, followed by a 16 bit word
    for channel 2 etc.)
 
    \param sound a pointer to a buffer that holds the audio data
    \return the number of bytes put into the buffer, or 0 if no audio data could be retrieved */

int DVFrame::ExtractAudio( void *sound )
{
	AudioInfo info;

	if ( GetAudioInfo( info ) == true )
	{
#ifdef HAVE_LIBDV

		int n, i;
		int16_t* s = ( int16_t * ) sound;

		dv_decode_full_audio( decoder, data, ( int16_t ** ) audio_buffers );
		for ( n = 0; n < info.samples; ++n )
			for ( i = 0; i < info.channels; i++ )
				*s++ = audio_buffers[ i ][ n ];

	}
	else
		info.samples = 0;

#else
		/* Collect the audio samples */
		char* s = ( char * ) sound;

		switch ( info.frequency )
		{
		case 32000:

			/* This is 4 channel audio */

			if ( IsPAL() )
			{
				short * p = ( short* ) sound;
				for ( int n = 0; n < info.samples; ++n )
				{
					register int r = ( ( unsigned char* ) data ) [ palmap_2ch1[ n ] + 1 ]; // LSB
					*p++ = compmap[ ( ( ( unsigned char* ) data ) [ palmap_2ch1[ n ] ] << 4 ) + ( r >> 4 ) ];   // MSB
					*p++ = compmap[ ( ( ( unsigned char* ) data ) [ palmap_2ch1[ n ] + 1 ] << 4 ) + ( r & 0x0f ) ];
				}


			}
			else
			{
				short* p = ( short* ) sound;
				for ( int n = 0; n < info.samples; ++n )
				{
					register int r = ( ( unsigned char* ) data ) [ ntscmap_2ch1[ n ] + 1 ]; // LSB
					*p++ = compmap[ ( ( ( unsigned char* ) data ) [ ntscmap_2ch1[ n ] ] << 4 ) + ( r >> 4 ) ];   // MSB
					*p++ = compmap[ ( ( ( unsigned char* ) data ) [ ntscmap_2ch1[ n ] + 1 ] << 4 ) + ( r & 0x0f ) ];
				}
			}
			break;

		case 44100:
		case 48000:

			/* this can be optimized significantly */

			if ( IsPAL() )
			{
				for ( int n = 0; n < info.samples; ++n )
				{
					*s++ = ( ( char* ) data ) [ palmap_ch1[ n ] + 1 ]; /* LSB */
					*s++ = ( ( char* ) data ) [ palmap_ch1[ n ] ];     /* MSB */
					*s++ = ( ( char* ) data ) [ palmap_ch2[ n ] + 1 ]; /* LSB */
					*s++ = ( ( char* ) data ) [ palmap_ch2[ n ] ];     /* MSB */
				}
			}
			else
			{
				for ( int n = 0; n < info.samples; ++n )
				{
					*s++ = ( ( char* ) data ) [ ntscmap_ch1[ n ] + 1 ]; /* LSB */
					*s++ = ( ( char* ) data ) [ ntscmap_ch1[ n ] ];     /* MSB */
					*s++ = ( ( char* ) data ) [ ntscmap_ch2[ n ] + 1 ]; /* LSB */
					*s++ = ( ( char* ) data ) [ ntscmap_ch2[ n ] ];     /* MSB */
				}
			}
			break;

			/* we can't handle any other format in the moment */

		default:
			info.samples = 0;
		}

	}
	else
		info.samples = 0;

#endif

	return info.samples * info.channels * 2;
}

#ifdef HAVE_LIBDV

/** retrieves the audio data from the frame
 
    The DV frame contains audio data mixed in the video data blocks, 
    which can be retrieved easily using this function.
 
    The audio data consists of 16 bit, two channel audio samples (a 16 bit word for channel 1, followed by a 16 bit word
    for channel 2 etc.)
 
    \param channels an array of buffers of audio data, one per channel, up to four channels
    \return the number of bytes put into the buffer, or 0 if no audio data could be retrieved */

int DVFrame::ExtractAudio( int16_t **channels )
{
	AudioInfo info;

	if ( GetAudioInfo( info ) == true )
		dv_decode_full_audio( decoder, data, channels );
	else
		info.samples = 0;

	return info.samples * info.channels * 2;
}

void DVFrame::ExtractHeader( void )
{
	dv_parse_header( decoder, data );
	dv_parse_packs( decoder, data );
}

void DVFrame::Deinterlace( void * image, int bpp )
{
	int width = GetWidth( ) * bpp;
	int height = GetHeight( );
	for ( int i = 0; i < height; i += 2 )
		memcpy( ( uint8_t * ) image + width * ( i + 1 ), ( uint8_t * ) image + width * i, width );
}

int DVFrame::ExtractRGB( void * rgb )
{
	unsigned char * pixels[ 3 ];
	int pitches[ 3 ];

	pixels[ 0 ] = ( unsigned char* ) rgb;
	pixels[ 1 ] = NULL;


	pixels[ 2 ] = NULL;

	pitches[ 0 ] = 720 * 3;
	pitches[ 1 ] = 0;
	pitches[ 2 ] = 0;

	dv_decode_full_frame( decoder, data, e_dv_color_rgb, pixels, pitches );
	return 0;
}

int DVFrame::ExtractPreviewRGB( void * rgb )
{
	ExtractRGB( rgb );
	return 0;
}

int DVFrame::ExtractYUV( void * yuv )
{
	unsigned char * pixels[ 3 ];
	int pitches[ 3 ];

	pixels[ 0 ] = ( unsigned char* ) yuv;
	pitches[ 0 ] = decoder->width * 2;

	dv_decode_full_frame( decoder, data, e_dv_color_yuv, pixels, pitches );
	return 0;
}

int DVFrame::ExtractPreviewYUV( void * yuv )
{
	ExtractYUV( yuv );
	return 0;
}


/** Get the frame aspect ratio.
 
	Indicates whether frame aspect ration is normal (4:3) or wide (16:9).
 
    \return true if the frame is wide (16:9), false if unknown or normal.
*/
bool DVFrame::IsWide( void )
{
	return dv_format_wide( decoder ) > 0;
}


/** Get the frame image width.
 
    \return the width in pixels.
*/
int DVFrame::GetWidth()
{
	return decoder->width;
}


/** Get the frame image height.
 
    \return the height in pixels.
*/
int DVFrame::GetHeight()
{
	return decoder->height;
}


/** Set the RecordingDate of the frame.
 
    This updates the calendar date and time and the timecode.
    However, timecode is derived from the time in the datetime
    parameter and frame number. Use SetTimeCode for more control
    over timecode.
 
    \param datetime A simple time value containing the
           RecordingDate and time information. The time in this
           structure is automatically incremented by one second
           depending on the frame parameter and updatded.
    \param frame A zero-based running frame sequence/serial number.
           This is used both in the timecode as well as a timestamp on
           dif block headers.
*/
void DVFrame::SetRecordingDate( time_t * datetime, int frame )
{
	dv_encode_metadata( data, IsPAL(), IsWide(), datetime, frame );
}

/** Set the TimeCode of the frame.
 
    This function takes a zero-based frame counter and automatically
    derives the timecode.
 
    \param frame The frame counter.
*/
void DVFrame::SetTimeCode( int frame )
{
	dv_encode_timecode( data, IsPAL(), frame );
}
#else
void DVFrame::ExtractHeader( void )
{}
#endif
