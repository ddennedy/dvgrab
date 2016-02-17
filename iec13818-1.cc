/*
* iec13818-1.cc
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

#include <string>
using std::string;

#include <string.h>

#include "hdvframe.h"
#include "iec13818-1.h"

HDVPacket::HDVPacket( HDVFrame *f, HDVStreamParams *p ) :
	frame( f ),
	params( p )
{
}

HDVPacket::~HDVPacket()
{
}

unsigned char HDVPacket::sync_byte() { return GetBits( 0, 8 ); }
bool HDVPacket::transport_error_indicator() { return GetBits( 8, 1 ); }
bool HDVPacket::payload_unit_start_indicator() { return GetBits( 9, 1 ); }
bool HDVPacket::transport_priority() { return GetBits( 10, 1 ); }
unsigned short HDVPacket::pid() { return GetBits( 11, 13 ); }
unsigned char HDVPacket::transport_scrambling_control() { return GetBits( 24, 2 ); }
unsigned char HDVPacket::adaptation_field_control() { return GetBits( 26, 2 ); }
unsigned char HDVPacket::continuity_counter() { return GetBits( 28, 4 ); }

unsigned char *HDVPacket::pointer_field()
{
	// If this is a PSI packet, the payload is _after_ a "pointer_field"; if this is a
	// PES packet, there is no pointer_field.  Only way to tell if it's PSI or PES
	// is keep track of what PIDs correspond to.  Ick.  Wish a damn bit had been used
	// instead.  Or dump the "pointer_field" completely.
	// For simplicity, assume only the video and audio streams are PES.
	if ( is_video_packet() || is_audio_packet() )
		return 0;
	else
		return payload_unit_start_indicator() ? &data[4] : 0;
}

bool HDVPacket::is_program_association_packet() { return !pid(); }
PAT *HDVPacket::program_association_table()
{
	if ( is_program_association_packet() )
		return &pat;
	else
		return 0;
}

bool HDVPacket::is_program_map_packet() { return pid() && ( pid() == params->program_map_PID ); }
PMT *HDVPacket::program_map_table()
{
	if ( is_program_map_packet() )
		return &pmt;
	else
		return 0;
}

bool HDVPacket::is_video_packet() { return pid() && ( pid() == params->video_stream_PID ); }
bool HDVPacket::is_audio_packet() { return pid() && ( pid() == params->audio_stream_PID ); }
bool HDVPacket::is_sony_private_a0_packet() { return pid() && ( pid() == params->sony_private_a0_PID ); }
bool HDVPacket::is_sony_private_a1_packet() { return pid() && ( pid() == params->sony_private_a1_PID ); }
bool HDVPacket::is_null_packet() { return pid() && ( pid() == PID_NULL_PACKET ); }

SonyA1 *HDVPacket::GetSonyA1()
{
	if ( is_sony_private_a1_packet() )
		return &sonyA1;
	else
		return 0;
}

unsigned char *HDVPacket::adaptation_field()
{
	int pos = 4;

	if ( pointer_field() )
		pos += (*pointer_field()) + 1;

	switch ( adaptation_field_control() )
	{
	case 0: return 0; /* reserved value; ignore this packet */
	case 1: return 0; /* payload only */
	case 2: return &data[pos]; /* adaptation field only */
	case 3: return &data[pos]; /* adaptation field and payload */
	}
	return 0;
}

unsigned char *HDVPacket::payload()
{
	int pos = PayloadOffset();

	if ( pos < 0 )
		return 0;
	else
		return &data[pos];
}

int HDVPacket::PayloadOffset()
{
	int pos = 4;

	if ( pointer_field() )
		pos += (*pointer_field()) + 1;

	switch ( adaptation_field_control() )
	{
	case 0: return -1; /* reserved value; ignore this packet */
	case 1: return pos; /* payload only */
	case 2: return -1; /* adaptation field only */
	case 3: return pos+(*adaptation_field()); /* adaptation field and payload */
	}

	return -1;
}

int HDVPacket::PayloadLength()
{
	if ( payload() )
		return GetLength() - PayloadOffset();
	else
		return 0;
}

void HDVPacket::SetData( unsigned char *d )
{
	data = d;

	if ( is_program_association_packet() )
		pat.SetData( payload(), PayloadLength() );

	if ( is_program_map_packet() )
		pmt.SetData( payload(), PayloadLength() );

	if ( is_sony_private_a1_packet() )
		sonyA1.SetData( payload(), PayloadLength() );
}

unsigned char HDVPacket::GetData( int pos )
{
	if ( pos < GetLength() )
		return data[pos];
	else
		return 0;
}

int HDVPacket::GetLength()
{
	return HDV_PACKET_SIZE;
}

void HDVPacket::Dump()
{
	bool b = d_hdv_packet || d_hdv_pid_check( pid() );

	DEBUG_PARAMS( b, 1, 0, "HDVPacket FLAG=%02x ERR=%s,P_IND=%s,PRI=%s PID=%04x SCR=%d,ADAPT=%d,CONT=%02d : ",
		sync_byte(),
		transport_error_indicator() ? "T" : "F",
		payload_unit_start_indicator() ? "T" : "F",
		transport_priority() ? "T" : "F",
		pid(),
		transport_scrambling_control(),
		adaptation_field_control(),
		continuity_counter()
		);

	DUMP_RAW_DATA( b, data, 4, HDV_PACKET_SIZE );

	DEBUG_PARAMS( b, 0, 1, "" );
}



//////////////
/// PAT packet

PAT::PAT() : length( 0 ) { }

PAT::~PAT() { }

unsigned char PAT::table_id() { return GetBits( 0, 8 ); }
bool PAT::section_syntax_indicator() { return GetBits( 8, 1 ); }
unsigned short PAT::section_length() { return GetBits( 12, 12 ); }
unsigned short PAT::transport_stream_id() { return GetBits( 24, 16 ); }
unsigned char PAT::version_number() { return GetBits( 42, 5 ); }
bool PAT::current_next_indicator() { return GetBits( 47, 1 ); }
unsigned char PAT::section_number() { return GetBits( 48, 8 ); }
unsigned char PAT::last_section_number() { return GetBits( 56, 8 ); }

#define NUM_PROGRAMS ( ( section_length() - 9U ) / 4U )

int PAT::program_number( unsigned int n )
{
	if ( n < NUM_PROGRAMS )
		return GetBits( 64 + ( n * 32 ), 16 );
	else
		return 0;
}

int PAT::pid( unsigned int n )
{
	if ( n < NUM_PROGRAMS )
		return GetBits( 83 + ( n * 32 ), 13 );
	else
		return 0;
}

int PAT::network_PID()
{
	for ( unsigned int n = 0; n < NUM_PROGRAMS; n++ )
	{
		if ( program_number( n ) == 0 )
			return pid( n );
	}

	return -1;
}

int PAT::program_map_PID()
{
	for ( unsigned int n = 0; n < NUM_PROGRAMS; n++ )
	{
		if ( program_number( n ) != 0 )
			return pid( n );
	}

	return -1;
}

void PAT::SetData( unsigned char *d, int len )
{
	data = d;
	length = len;
}

unsigned char PAT::GetData( int pos )
{
	if ( pos < GetLength() )
		return data[pos];
	else
		return 0;
}

int PAT::GetLength()
{
	return length;
}

void PAT::Dump()
{
	DEBUG_PARAMS( d_hdv_pat, 1, 0, "PAT TID=%02x SYN=%s LEN=%04x TSID=%04x VER=%02x IND=%s NUM=%02x LAST=%02x :",
		table_id(),
		section_syntax_indicator() ? "T" : "F",
		section_length(),
		transport_stream_id(),
		version_number(),
		current_next_indicator() ? "T" : "F",
		section_number(),
		last_section_number()
		);

	for ( unsigned int n = 0; n < NUM_PROGRAMS; n++ )
		DEBUG_RAW( d_hdv_pat, " PROG_NUM=%04x,PID=%04x", program_number( n ), pid( n ) );

	DEBUG_PARAMS( d_hdv_pat, 0, 1, "" );
}



///////////////
/// PMT element

PMT_element::PMT_element() : length( 0 ) { }

PMT_element::~PMT_element() { }

unsigned char PMT_element::stream_type() { return GetBits( 0, 8 ); }
unsigned short PMT_element::elementary_PID() { return GetBits( 11, 13 ); }
unsigned short PMT_element::ES_info_length() { return GetBits( 28, 12 ); }

unsigned char *PMT_element::descriptor( unsigned int n )
{
	int start = 5;

	for ( unsigned int i = 0, j = 0; i < ES_info_length(); i += data[start+i+1]+2, j++ )
		if ( j == n )
			return &data[start+i];

	return 0;
}

void PMT_element::SetData( unsigned char *d, int len )
{
	data = d;
	length = len;
}

unsigned char PMT_element::GetData( int pos )
{
	if ( pos < GetLength() )
		return data[pos];
	else
		return 0;
}

int PMT_element::GetLength()
{
	return length;
}

void PMT_element::Dump()
{
	DEBUG_RAW( d_hdv_pmt, "{TYPE=%02x PID=%04x LEN=%04x ",
		stream_type(),
		elementary_PID(),
		ES_info_length()
		);

	unsigned char *desc;

	// For "Registration" descriptors, the registered format identifiers
	// are listed here:
	// http://www.smpte-ra.org/mpegreg/mpegreg.html
	for ( int i = 0; ( desc = descriptor( i ) ); i++ )
	{
		DEBUG_RAW( d_hdv_pmt, "DESC=[" );
		DUMP_RAW_DATA( d_hdv_pmt, desc, 0, desc[1]+2 );
		DEBUG_RAW( d_hdv_pmt, "\b] " );
	}

	DEBUG_RAW( d_hdv_pmt, "\b} " );
}



//////////////
/// PMT packet

PMT::PMT() : length( 0 ), nPmtElements( 0 )
{
}

PMT::~PMT()
{
}

unsigned char PMT::table_id() { return GetBits( 0, 8 ); }
bool PMT::section_syntax_indicator() { return GetBits( 8, 1 ); }
unsigned short PMT::section_length() { return GetBits( 12, 12 ); }
unsigned short PMT::program_number() { return GetBits( 24, 16 ); }
unsigned char PMT::version_number() { return GetBits( 42, 5 ); }
bool PMT::current_next_indicator() { return GetBits( 47, 1 ); }
unsigned char PMT::section_number() { return GetBits( 48, 8 ); }
unsigned char PMT::last_section_number() { return GetBits( 56, 8 ); }
unsigned short PMT::PCR_PID() { return GetBits( 67, 13 ); }
unsigned short PMT::program_info_length() { return GetBits( 84, 12 ); }

unsigned char *PMT::descriptor( unsigned int n )
{
	int start = 12;

	for ( unsigned int i = 0, j = 0; i < program_info_length(); i += data[start+i+1]+2, j++ )
		if ( j == n )
			return &data[start+i];

	return 0;
}

PMT_element *PMT::GetPmtElement( unsigned int n )
{
	if ( n < nPmtElements )
		return &pmtElement[n];
	else
		return 0;
}

void PMT::SetData( unsigned char *d, int len )
{
	data = d;
	length = len;

	CreatePmtElements();
}

unsigned char PMT::GetData( int pos )
{
	if ( pos < GetLength() )
		return data[pos];
	else
		return 0;
}

int PMT::GetLength()
{
	return length;
}

void PMT::CreatePmtElements()
{
	int start = 12 + program_info_length();
	int end = 3 + section_length() - 4;
	int i;

	nPmtElements = 0;

	for ( i = start; i < end && nPmtElements < MAX_PMT_ELEMENTS; nPmtElements++ )
	{
		// Set the data with min len first, then reset with the actual len
		pmtElement[nPmtElements].SetData( &data[i], 5 );
		int len = 5 + pmtElement[nPmtElements].ES_info_length();
		pmtElement[nPmtElements].SetData( &data[i], len );
		i += len;
	}
}

void PMT::Dump()
{
	DEBUG_PARAMS( d_hdv_pmt, 1, 0, "PMT TID=%02x SYN=%s SECLEN=%04x PROG#=%04x VER#=%02x IND=%s SEC#=%02x LAST#=%02x PCRPID=%02x PROGLEN=%02x ",
		table_id(),
		section_syntax_indicator() ? "T" : "F",
		section_length(),
		program_number(),
		version_number(),
		current_next_indicator() ? "T" : "F",
		section_number(),
		last_section_number(),
		PCR_PID(),
		program_info_length()
		);

	unsigned char *desc;

	for ( int i = 0; ( desc = descriptor( i ) ); i++ )
	{
		DEBUG_RAW( d_hdv_pmt, "DESC=[" );
		DUMP_RAW_DATA( d_hdv_pmt, desc, 0, desc[1]+2 );
		DEBUG_RAW( d_hdv_pmt, "\b] " );
	}

	PMT_element *elem;

	for ( int i = 0; ( elem = GetPmtElement( i ) ); i++ )
		elem->Dump();

	DEBUG_PARAMS( d_hdv_pmt, 0, 1, "" );
}



/////////////
// PES Packet

PES::PES()
{
	Clear();
}

PES::~PES()
{
}

void PES::AddData( unsigned char *d, int l )
{
	if ( length + l < MAX_PES_SIZE )
	{
		memcpy( &data[length], d, l );
		length += l;
	}
	else
	{
		printf("PES data overflow!\n");
	}
}

unsigned char *PES::GetBuffer() { return data; }

unsigned char PES::GetData( int pos )
{
	if ( pos < GetLength() )
		return data[pos];
	else
		return 0;
}

int PES::GetLength()
{
	return length;
}

void PES::Clear()
{
	length = 0;
	packetDataOffset = -1;
}

unsigned int PES::packet_start_code_prefix() { return GetBits( 0, 24 ); }
unsigned char PES::stream_id() { return GetBits( 24, 8 ); }
unsigned short PES::PES_packet_length() { return GetBits( 32, 16 ); }
unsigned char PES::PES_scrambling_control() { return IsHeaderPresent() ? GetBits( 50, 2 ) : 0; }
bool PES::PES_priority() { return IsHeaderPresent() ? GetBits( 52, 1 ) : 0; }
bool PES::data_alignment_indicator() { return IsHeaderPresent() ? GetBits( 53, 1 ) : 0; }
bool PES::copyright() { return IsHeaderPresent() ? GetBits( 54, 1 ) : 0; }
bool PES::original_or_copy() { return IsHeaderPresent() ? GetBits( 55, 1 ) : 0; }
unsigned char PES::PTS_DTS_flags() { return IsHeaderPresent() ? GetBits( 56, 2 ) : 0; }
bool PES::ESCR_flag() { return IsHeaderPresent() ? GetBits( 58, 1 ) : 0; }
bool PES::ES_rate_flag() { return IsHeaderPresent() ? GetBits( 59, 1 ) : 0; }
bool PES::DSM_trick_mode_flag() { return IsHeaderPresent() ? GetBits( 60, 1 ) : 0; }
bool PES::additional_copy_info_flag() { return IsHeaderPresent() ? GetBits( 61, 1 ) : 0; }
bool PES::PES_CRC_flag() { return IsHeaderPresent() ? GetBits( 62, 1 ) : 0; }
bool PES::PES_extension_flag() { return IsHeaderPresent() ? GetBits( 63, 1 ) : 0; }
unsigned char PES::PES_header_data_length() { return IsHeaderPresent() ? GetBits( 64, 8 ) : 0; }

bool PES::IsHeaderPresent()
{
	// Check the spec for the actual IDs, I'm too lazy to put them here.
	switch ( stream_id() )
	{
	case 0xbc:
	case 0xbf:
	case 0xf0:
	case 0xf1:
	case 0xff:
	case 0xf2:
	case 0xf8:
		return false;
	default:
		return true;
	}
}

int PES::GetPacketDataOffset()
{
	if ( packetDataOffset < 0 && GetLength() > 9 )
	{
		if ( IsHeaderPresent() )
			packetDataOffset = 9 + PES_header_data_length();
		else
			packetDataOffset = 6;
	}

	return packetDataOffset;
}

unsigned char PES::PES_packet_data_byte( int n )
{
	return GetData( GetPacketDataOffset() + n );
}

int PES::GetPacketDataLength()
{
	return GetLength() - GetPacketDataOffset() - 1;
}

void PES::Dump()
{
	DEBUG_PARAMS( d_hdv_pes, 1, 0, "PES START=%06x SID=%02x PES_LEN=%d : GetLength %d : data bytes",
		packet_start_code_prefix(), stream_id(), PES_packet_length(), GetLength() );

	for ( int i = 0; i < 16; i++ )
		DEBUG_RAW( d_hdv_pes, " 0x%02x", PES_packet_data_byte( i ) );

	DEBUG_PARAMS( d_hdv_pes, 0, 1, "" );
}



///////////////////
/// Sony Private A1
/// (not part of IEC13818-1 spec)

SonyA1::SonyA1() : length( 0 )
{
}

SonyA1::~SonyA1()
{
}

unsigned char SonyA1::year() { return BCD( GetBits(352, 8) ); }
unsigned char SonyA1::month() { return BCD( GetBits(347, 5) ); }
unsigned char SonyA1::day() { return BCD( GetBits(338, 6) ); }
unsigned char SonyA1::hour() { return BCD( GetBits(386, 6) ); }
unsigned char SonyA1::minute() { return BCD( GetBits(377, 7) ); }
unsigned char SonyA1::second() { return BCD( GetBits(369, 7) ); }

unsigned char SonyA1::timecode_hour() { return BCD( GetBits(322, 6) ); }
unsigned char SonyA1::timecode_minute() { return BCD( GetBits(313, 7) ); }
unsigned char SonyA1::timecode_second() { return BCD( GetBits(305, 7) ); }
unsigned char SonyA1::timecode_frame() { return BCD( GetBits(298, 6) ); }

bool SonyA1::scene_start() { return !GetBits(394, 1); }

void SonyA1::SetData( unsigned char *d, int len )
{
	data = d;
	length = len;
}

unsigned char SonyA1::GetData( int pos )
{
	if ( pos < GetLength() )
		return data[pos];
	else
		return 0;
}

int SonyA1::GetLength()
{
	return length;
}

void SonyA1::Dump()
{
	DEBUG( d_hdv_sonya1, "Record date : %04d/%02d/%02d %02d:%02d:%02d Timecode : %02d:%02d:%02d.%02d Scene Start : %s",
		2000 + year(), month(), day(), hour(), minute(), second(),
		timecode_hour(), timecode_minute(), timecode_second(), timecode_frame(),
		scene_start() ? "T" : "F"
		);
}

