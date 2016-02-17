/*
* hdvframe.cc -- utilities for processing HDV frames
* Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
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

#include <string.h>
#include "hdvframe.h"

HDVFrame::HDVFrame( HDVStreamParams *p )
{
	Clear();

	params = p;

	packet = new HDVPacket( this, params );
}

HDVFrame::~HDVFrame()
{
	delete packet;
}

bool HDVFrame::IsHDV()
{
	return true;
}

bool HDVFrame::CouldBeJVCP25()
{
	return repeatFirstField && ( 50 == frameRate );
}

bool HDVFrame::CanStartNewStream()
{
	return isGOP;
}

void HDVFrame::Clear()
{
	isComplete = false;
	isRecordingDateSet = false;
	isTimeCodeSet = false;
	isNewRecording = false;
	isGOP = false;
	width = 0;
	height = 0;
	frameRate = 0;
	repeatFirstField = false;
	lastVideoDataLen = 0;
	lastAudioDataLen = 0;

	Frame::Clear();
}

void HDVFrame::SetDataLen( int len )
{
	int old_len = GetDataLen();

	if ( !old_len )
	{
		if ( params->carryover_length > 0 )
		{
			memmove( &data[ params->carryover_length ], data, len );
			memcpy( data, params->carryover_data, params->carryover_length );
			len += params->carryover_length;
			params->carryover_length = 0;
		}

		DEBUG_RAW( d_hdv_video, "->\n<- New HDVFrame:" );
	}

	Frame::SetDataLen( len );

	ProcessFrame( old_len );
}

bool HDVFrame::GetRecordingDate( struct tm &rd )
{
	if ( !isRecordingDateSet )
		return false;

	memcpy(&rd, &recordingDate, sizeof(rd));

	return true;
}

bool HDVFrame::GetTimeCode( TimeCode &tc )
{
	if ( !isTimeCodeSet )
		return false;

	memcpy(&tc, &timeCode, sizeof(tc));

	return true;
}

float HDVFrame::GetFrameRate()
{
	return frameRate;
}

bool HDVFrame::IsNewRecording()
{
	return isNewRecording;
}

bool HDVFrame::IsGOP()
{
	return isGOP;
}

bool HDVFrame::IsComplete( void )
{
	return isComplete;
}

int HDVFrame::GetWidth()
{
	return width;
}

int HDVFrame::GetHeight()
{
	return height;
}

void HDVFrame::ProcessFrame( unsigned int start )
{
	for ( int i = start; i+HDV_PACKET_SIZE-1 < GetDataLen() && !IsComplete(); i += HDV_PACKET_SIZE )
	{
		if ( i+HDV_PACKET_SIZE > DATA_BUFFER_LEN )
		{
			sendEvent( "\aERROR:HDV Frame out of buffer space, completing packet early" );
			isComplete = true;
			return;
		}

		if ( HDV_PACKET_MARKER == data[i] )
		{
			packet->SetData( &data[i] );
			ProcessPacket();
		}
		else
		{
			// The stream has to be synced on packet boundries.
			// This could be changed to do in-code packet marker searching/syncing,
			// but it doesn't do that right now.
			sendEvent( "Invalid packet sync_byte 0x%02x!", data[i] );
		}
	}
}

void HDVFrame::ProcessPacket()
{
	DEBUG( d_hdv_pids, "PID %04x", packet->pid() );
	packet->Dump();

	if ( packet->is_program_association_packet() )
		ProcessPAT();
	else if ( packet->is_program_map_packet() )
		ProcessPMT();
	else if ( packet->is_sony_private_a1_packet() )
		ProcessSonyA1();
	else if ( packet->is_video_packet() )
		ProcessVideo();
	else if ( packet->is_audio_packet() )
		ProcessAudio();
	else if ( packet->is_null_packet() )
		return;
}

void HDVFrame::ProcessPAT()
{
	PAT *pat = packet->program_association_table();
	pat->Dump();

	// NOTE - this assumes a non-changing PAT; once we process the first one, skip the rest.
	if ( params->program_map_PID )
		return;

	// Set the PMT PID if this is a PAT.
	if ( pat && pat->program_map_PID() > 0 && !params->program_map_PID )
		params->program_map_PID = pat->program_map_PID();
}

void HDVFrame::ProcessPMT()
{
	PMT *pmt = packet->program_map_table();
	pmt->Dump();

	// NOTE - this assumes a non-changing PMT; once we get the video/audio PIDs, skip the rest.
	if ( params->video_stream_PID && params->audio_stream_PID )
		return;

	PMT_element *elem;
	for ( int i = 0; ( elem = pmt->GetPmtElement( i ) ); i++ )
	{
		switch ( elem->stream_type() )
		{
		case STREAM_VIDEO:
			params->video_stream_PID = elem->elementary_PID();
			break;
		case STREAM_AUDIO:
			params->audio_stream_PID = elem->elementary_PID();
			break;
		case STREAM_SONY_PRIVATE_A0:
			params->sony_private_a0_PID = elem->elementary_PID();
			break;
		case STREAM_SONY_PRIVATE_A1:
			params->sony_private_a1_PID = elem->elementary_PID();
			break;
		}
	}
}

void HDVFrame::ProcessVideo()
{
	params->video.AddPacket( packet );

	if ( params->video.IsComplete() )
	{
		// This carryover (probably) will not be needed if iec13818-2 slice macroblock parsing is added
		// until then, once the iec13818-2 parser detects a new PES packet and completes this HDVFrame,
		// all data since the last video or audio packet is carried over to the next HDVFrame.
		int lastDataLen = lastAudioDataLen > lastVideoDataLen ? lastAudioDataLen : lastVideoDataLen;
		params->carryover_length = GetDataLen() - lastDataLen;
		if ( params->carryover_length > CARRYOVER_DATA_MAX_SIZE )
		{
			sendEvent( "\aERROR: too much carryover data (%d bytes), DROPPING DATA!\n", params->carryover_length );
			params->carryover_length = CARRYOVER_DATA_MAX_SIZE;
		}
		memcpy( params->carryover_data, &data[lastDataLen], params->carryover_length );
		Frame::SetDataLen( lastDataLen );

		SetComplete();
	}
	else
	{
		lastVideoDataLen = GetDataLen();
	}
}

void HDVFrame::ProcessAudio()
{
	lastAudioDataLen = GetDataLen();
}

void HDVFrame::ProcessSonyA1()
{
	SonyA1 *a1 = packet->GetSonyA1();
	a1->Dump();

	struct tm *rd = &params->recordingDate;
	TimeCode *tc = &params->timeCode;

	rd->tm_year = 100 + a1->year();
	rd->tm_mon = a1->month() - 1;
	rd->tm_mday = a1->day();
	rd->tm_hour = a1->hour();
	rd->tm_min = a1->minute();
	rd->tm_sec = a1->second();

	tc->hour = a1->timecode_hour();
	tc->min = a1->timecode_minute();
	tc->sec = a1->timecode_second();
	tc->frame = a1->timecode_frame();

	params->isRecordingDateSet = true;
	params->isTimeCodeSet = true;

	isNewRecording = a1->scene_start();
}

void HDVFrame::SetComplete()
{
	Video *v = &params->video;

	if ( v->width )
		params->width = v->width;

	if ( v->height )
		params->height = v->height;

	if ( v->frameRate )
		params->frameRate = v->frameRate;

	if ( v->hasGOP )
		isGOP = true;

	if ( v->isTimeCodeSet )
	{
		memcpy( &params->gopTimeCode, &v->timeCode, sizeof( params->gopTimeCode ) );
		params->isGOPTimeCodeSet = true;
	}

	if ( params->isRecordingDateSet )
	{
		memcpy( &recordingDate, &params->recordingDate, sizeof( recordingDate ) );
		isRecordingDateSet = true;
	}

	if ( params->isTimeCodeSet )
	{
		memcpy( &timeCode, &params->timeCode, sizeof( timeCode ) );
		isTimeCodeSet = true;
	}
	else if ( params->isGOPTimeCodeSet )
	{
		memcpy( &timeCode, &params->gopTimeCode, sizeof( timeCode ) );
		isTimeCodeSet = true;
	}

	width = params->width;
	height = params->height;
	frameRate = params->frameRate;
	repeatFirstField = v->repeat_first_field;
	isComplete = true;
	v->Clear();
}



///////////////////
/// HDVStreamParams

HDVStreamParams::HDVStreamParams() :
	program_map_PID( 0 ),
	video_stream_PID( 0 ),
	audio_stream_PID( 0 ),
	sony_private_a0_PID( 0 ),
	sony_private_a1_PID( 0 ),
	width( 0 ), height( 0 ), frameRate( 0 ),
	carryover_length( 0 ),
	isRecordingDateSet( false ),
	isTimeCodeSet( false ),
	isGOPTimeCodeSet( false )
{
}

HDVStreamParams::~HDVStreamParams()
{
}
