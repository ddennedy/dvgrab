/*
* iec13818-1.h
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

#ifndef _IEC13818_1_H
#define _IEC13818_1_H 1

#include "error.h"

#define HDV_PACKET_SIZE 188
#define HDV_PACKET_MARKER 0x47

#define STREAM_VIDEO (0x02)
#define STREAM_AUDIO (0x03)
#define STREAM_SONY_PRIVATE_A0 (0xa0)
#define STREAM_SONY_PRIVATE_A1 (0xa1)

#define PID_NULL_PACKET (0x1fff)

#define DUMP_RAW_DATA( type, bytes, start, end ) do { \
	int i, j; \
	for ( j = end; ( j > start+1 ) && ( bytes[j-1] == 0xff ) && ( bytes[j-2] == 0xff ); j-- ); \
	for ( i = start; i < j; i++ ) DEBUG_RAW( type, "%02x ", bytes[ i ] ); \
	if ( j < end ) DEBUG_RAW( type, "\b*%d ", end - j + 1 ); \
} while(0)

#define BCD(c) ( ((((c) >> 4) & 0x0f) * 10) + ((c) & 0x0f) )

#define TOBYTES( n ) ( ( n + 7 ) / 8 )
static char bitmask[8] = { 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff };
#define GETBITS( offset, len ) do { \
	unsigned long value = 0; \
	while ( len > 0 ) \
	{ \
		int bits = 8 - ( offset % 8 ); \
		unsigned char data = GetData( offset / 8 ) & bitmask[bits-1]; \
		if ( len > bits ) \
			value += data << ( len - bits ); \
		else if ( len < bits ) \
			value += data >> ( bits - len ); \
		else \
			value += data; \
		offset += bits; \
		len -= bits; \
	} \
	return value; \
} while (0)



class PAT
{
public:
	PAT();
	~PAT();

	void SetData( unsigned char *d, int len );
	int GetLength();
	unsigned char GetData( int pos );
	unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }
	void Dump();

	unsigned char table_id();
	bool section_syntax_indicator();
	unsigned short section_length();
	unsigned short transport_stream_id();
	unsigned char version_number();
	bool current_next_indicator();
	unsigned char section_number();
	unsigned char last_section_number();

	int program_number( unsigned int n );
	int pid( unsigned int n );

	int network_PID();
	int program_map_PID();

protected:
	unsigned char *data;
	int length;
};



class PMT_element
{
public:
	PMT_element();
	~PMT_element();

	void SetData( unsigned char *d, int len );
	int GetLength();
	unsigned char GetData( int pos );
	unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }
	void Dump();

	unsigned char stream_type();
	unsigned short elementary_PID();
	unsigned short ES_info_length();

	unsigned char *descriptor( unsigned int n );

protected:
	unsigned char *data;
	int length;
};

class PMT
{
public:
	PMT();
	~PMT();

	void SetData( unsigned char *d, int len );
	int GetLength();
	unsigned char GetData( int pos );
	unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }
	void Dump();

	unsigned char table_id();
	bool section_syntax_indicator();
	unsigned short section_length();
	unsigned short program_number();
	unsigned char version_number();
	bool current_next_indicator();
	unsigned char section_number();
	unsigned char last_section_number();
	unsigned short PCR_PID();
	unsigned short program_info_length();

	unsigned char *descriptor( unsigned int n );

	PMT_element *GetPmtElement( unsigned int n );

protected:
	void CreatePmtElements();

protected:
	unsigned char *data;
	int length;

#define MAX_PMT_ELEMENTS 32
	PMT_element pmtElement[MAX_PMT_ELEMENTS];
	unsigned int nPmtElements;
};



class PES
{
public:
	PES();
	~PES();

	void Clear();
	void AddData( unsigned char *d, int l );
	unsigned char *GetBuffer();
	unsigned char GetData( int pos );
	unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }
	int GetLength();
	void Dump();

	unsigned int packet_start_code_prefix();
	unsigned char stream_id();
	unsigned short PES_packet_length();
	unsigned char PES_scrambling_control();
	bool PES_priority();
	bool data_alignment_indicator();
	bool copyright();
	bool original_or_copy();
	unsigned char PTS_DTS_flags();
	bool ESCR_flag();
	bool ES_rate_flag();
	bool DSM_trick_mode_flag();
	bool additional_copy_info_flag();
	bool PES_CRC_flag();
	bool PES_extension_flag();
	unsigned char PES_header_data_length();

	bool PES_private_data_flag();
	bool pack_header_field_flag();
	bool program_packet_sequence_counter_flag();
	bool P_STD_buffer_flag();
	bool PES_extension_flag_2();

	int GetPacketDataOffset();
	int GetPacketDataLength();

	unsigned char PES_packet_data_byte( int n );

protected:
	bool IsHeaderPresent();

private:
#define MAX_PES_SIZE 1024*1024
	unsigned char data[MAX_PES_SIZE];
	int length;

	int packetDataOffset;
};



class SonyA1
{
public:
	SonyA1();
	~SonyA1();

	void SetData( unsigned char *d, int len );
	int GetLength();
	unsigned char GetData( int pos );
	unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }
	void Dump();

	unsigned char year();
	unsigned char month();
	unsigned char day();
	unsigned char hour();
	unsigned char minute();
	unsigned char second();

	unsigned char timecode_hour();
	unsigned char timecode_minute();
	unsigned char timecode_second();
	unsigned char timecode_frame();

	bool scene_start();

protected:
	unsigned char *data;
	int length;
};



class HDVFrame;
class HDVStreamParams;

class HDVPacket
{
public:
	HDVPacket( HDVFrame *f, HDVStreamParams *p );
	~HDVPacket();

	void SetData( unsigned char *d );
	int GetLength();
	unsigned char GetData( int pos );
	unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }
	void Dump();

	unsigned char sync_byte();
	bool transport_error_indicator();
	bool payload_unit_start_indicator();
	bool transport_priority();
	unsigned short pid();
	unsigned char transport_scrambling_control();
	unsigned char adaptation_field_control();
	unsigned char continuity_counter();

	unsigned char *pointer_field();

	unsigned char *adaptation_field();

	unsigned char *payload();
	int PayloadOffset();
	int PayloadLength();

	bool is_program_association_packet();
	PAT *program_association_table();

	bool is_program_map_packet();
	PMT *program_map_table();

	bool is_video_packet();
	bool is_audio_packet();
	bool is_sony_private_a0_packet();
	bool is_sony_private_a1_packet();
	bool is_null_packet();
	SonyA1 *GetSonyA1();

protected:
	unsigned char *data;

	HDVFrame *frame;
	HDVStreamParams *params;

	PAT pat;
	PMT pmt;
	SonyA1 sonyA1;
};



#endif
