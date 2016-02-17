/*
* iec13818-2.cc
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

#include "hdvframe.h"
#include "iec13818-2.h"

///////////////
// Video Stream

Video::Video()
{
	picture = new Picture( this );
	sequenceHeader = new SequenceHeader( this );
	sequenceExtension = new SequenceExtension( this );
	sequenceDisplayExtension = new SequenceDisplayExtension( this );
	quantMatrixExtension = new QuantMatrixExtension( this );
	copyrightExtension = new CopyrightExtension( this );
	sequenceScalableExtension = new SequenceScalableExtension( this );
	pictureDisplayExtension = new PictureDisplayExtension( this );
	pictureCodingExtension = new PictureCodingExtension( this );
	pictureSpatialScalableExtension = new PictureSpatialScalableExtension( this );
	pictureTemporalScalableExtension = new PictureTemporalScalableExtension( this );
	userData = new UserData( this );
	group = new Group( this );
	slice = new Slice( this );

	Clear();
}

Video::~Video()
{
	delete picture;
	delete sequenceHeader;
	delete sequenceExtension;
	delete sequenceDisplayExtension;
	delete quantMatrixExtension;
	delete copyrightExtension;
	delete sequenceScalableExtension;
	delete pictureDisplayExtension;
	delete pictureCodingExtension;
	delete pictureSpatialScalableExtension;
	delete pictureTemporalScalableExtension;
	delete userData;
	delete group;
	delete slice;
}

void Video::AddPacket( HDVPacket *packet )
{
	if ( offset > 0 && packet->payload_unit_start_indicator() )
	{
		if ( lastSection == slice )
			DEBUG_RAW( d_hdv_video, "*%d", sliceCount );
		DEBUG_RAW( d_hdv_video, "]" );
		isComplete = true;
	}
	else
	{
		pes.AddData( packet->payload(), packet->PayloadLength() );
		ProcessPacket();
	}
}

void Video::Clear()
{
	pes.Clear();

	currentSection = 0;
	lastSection = 0;

	sliceCount = 0;
	
	offset = 0;
	sectionStart = 0;

	width = 0;
	height = 0;
	frameRate = 0;

	isTimeCodeSet = false;
	hasGOP = false;

	repeat_first_field = -1;
	top_field_first = -1;
	picture_structure = -1;
	progressive_sequence = -1;
	scalable_mode = -1;

	isComplete = false;
}

unsigned char *Video::GetBuffer() { return &pes.GetBuffer()[pes.GetPacketDataOffset()]; }

unsigned char Video::GetData( int pos )
{
	if ( pos < pes.GetPacketDataLength() )
		return pes.PES_packet_data_byte( pos );
	else
		return 0;
}

int Video::GetLength()
{
	return pes.GetPacketDataLength();
}

bool Video::IsComplete()
{
	return isComplete;
}

void Video::ProcessPacket()
{
	while ( ( offset + 4 ) < GetLength() )
	{
		int currentLen = GetLength() - offset;

		if ( !currentSection )
		{
			const char *dstr = NULL;
			bool restart = false;
			if ( START_CODE_PREFIX( offset ) )
			{
				int start_code = GetData( offset + 3 );
				int extension_code;

				switch ( start_code )
				{
				case SEQUENCE_HEADER_CODE_VALUE:
					dstr = "H";
					currentSection = sequenceHeader;
					break;
				case PICTURE_START_CODE_VALUE:
					dstr = "P";
					currentSection = picture;
					break;
				case EXTENSION_START_CODE_VALUE:
					extension_code = GetBits( ( offset + 4 ) * 8, 4 );
					switch ( extension_code )
					{
					case SEQUENCE_EXTENSION_ID_VALUE:
						dstr = "SE";
						currentSection = sequenceExtension;
						break;
					case SEQUENCE_DISPLAY_EXTENSION_ID_VALUE:
						dstr = "SDE";
						currentSection = sequenceDisplayExtension;
						break;
					case QUANT_MATRIX_EXTENSION_ID_VALUE:
						dstr = "QME";
						currentSection = quantMatrixExtension;
						break;
					case COPYRIGHT_EXTENSION_ID_VALUE:
						dstr = "CE";
						currentSection = copyrightExtension;
						break;
					case SEQUENCE_SCALABLE_EXTENSION_ID_VALUE:
						dstr = "SSE";
						currentSection = sequenceScalableExtension;
						break;
					case PICTURE_DISPLAY_EXTENSION_ID_VALUE:
						dstr = "PDE";
						currentSection = pictureDisplayExtension;
						break;
					case PICTURE_CODING_EXTENSION_ID_VALUE:
						dstr = "PCE";
						currentSection = pictureCodingExtension;
						break;
					case PICTURE_SPATIAL_SCALABLE_EXTENSION_ID_VALUE:
						dstr = "PSSE";
						currentSection = pictureSpatialScalableExtension;
						break;
					case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID_VALUE:
						dstr = "PTSE";
						currentSection = pictureTemporalScalableExtension;
						break;
					default:
						DEBUG( d_hdv_video, "Unknown Extension %x", extension_code );
						offset++;
						restart = true;
						break;
					}
					break;
				case USER_DATA_START_CODE_VALUE:
					dstr = "U";
					currentSection = userData;
					break;
				case GROUP_START_CODE_VALUE:
					dstr = "G";
					currentSection = group;
					break;
				case SEQUENCE_END_CODE_VALUE:
					dstr = "END";
					offset += 4;
					restart = true;
					break;
				default:
					if ( SLICE_START_CODE_MIN <= start_code && start_code <= SLICE_START_CODE_MAX )
					{
						if ( lastSection == slice )
						{
							sliceCount++;
						}
						else
						{
							dstr = "S";
							sliceCount = 1;
						}
						currentSection = slice;
					}
					else
					{
						DEBUG( d_hdv_video, "Unknown Start Code %x", start_code );
						offset++;
						restart = true;
					}
					break;
				}
			}
			else
			{
				DEBUG_RAW( d_hdv_video, "%02x ", GetData(offset) );
				offset++;
				restart = true;
			}

			if ( dstr )
			{
				if ( lastSection == slice && currentSection != slice )
					DEBUG_RAW( d_hdv_video, "*%d]", sliceCount );

				DEBUG_RAW( d_hdv_video, "[%s", dstr );

				if ( restart )
					DEBUG_RAW( d_hdv_video, "]" );
			}

			if ( restart )
				continue;

			currentSection->Clear();
			currentSection->SetOffset( offset );
			sectionStart = offset;
		}

		currentSection->AddLength( currentLen );

		if ( currentSection->IsComplete() )
		{
			if ( currentSection != slice )
				DEBUG_RAW( d_hdv_video, "]" );

			if ( currentSection == sequenceHeader )
			{
				width = sequenceHeader->horizontal_size_value();
				height = sequenceHeader->vertical_size_value();
				frameRate = FRAMERATE_LOOKUP( sequenceHeader->frame_rate_code() );
			}
			else if ( currentSection == pictureCodingExtension )
			{
				repeat_first_field = pictureCodingExtension->repeat_first_field() ? 1 : 0;
				top_field_first = pictureCodingExtension->top_field_first() ? 1 : 0;
				picture_structure = pictureCodingExtension->picture_structure();
			}
			else if ( currentSection == group )
			{
				timeCode.hour = group->time_code_hours();
				timeCode.min = group->time_code_minutes();
				timeCode.sec = group->time_code_seconds();
				timeCode.frame = group->time_code_pictures();
				isTimeCodeSet = true;

				hasGOP = true;
			}
			else if ( currentSection == sequenceExtension )
				progressive_sequence = sequenceExtension->progressive_sequence() ? 1 : 0;
			else if ( currentSection == sequenceScalableExtension )
				scalable_mode = sequenceScalableExtension->scalable_mode();

			currentLen = currentSection->GetCompleteLength() - ( offset - sectionStart );
			lastSection = currentSection;
			currentSection = 0;
		}

		offset += currentLen;
	}
}



///////////////
// VideoSection

VideoSection::VideoSection( Video *v )
{
	video = v;
}

VideoSection::~VideoSection()
{
}

void VideoSection::Clear()
{
	offset = 0;
	length = 0;
}

void VideoSection::SetOffset( int o ) { offset = o; }
void VideoSection::AddLength( int l ) { length += l; }
bool VideoSection::IsComplete() { return GetCompleteLength() > 0; }

unsigned char VideoSection::GetData( int pos ) { return video->GetData( pos + offset ); }



//////////
// Picture

Picture::Picture( Video *v ) : VideoSection( v )
{
}

Picture::~Picture()
{
}

int Picture::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 61;

	if ( blen < bits )
		return -1;

	if ( picture_coding_type() == 2 )
		bits += 4;
	else if ( picture_coding_type() == 3 )
		bits += 8;

	if ( blen < bits )
		return -1;

	while ( GetBits( bits, 1 ) )
	{
		bits += 9;

		if ( blen < bits )
			return -1;
	}

	return TOBYTES( bits );
}

void Picture::Dump()
{
	DEBUG( d_hdv_video, "Picture section" );
}

unsigned int Picture::picture_start_code() { return GetBits( 0, 32 ); }
unsigned short Picture::temporal_reference() { return GetBits( 32, 10 ); }
unsigned char Picture::picture_coding_type() { return GetBits( 42, 3 ); }
unsigned short Picture::vbv_delay() { return GetBits( 45, 16 ); }
bool Picture::full_pel_forward_vector()
{
	if ( picture_coding_type() == 2 || picture_coding_type() == 3 )
		return GetBits( 61, 1 );
	else
		return 0;
}
unsigned char Picture::forward_f_code()
{
	if ( picture_coding_type() == 2 || picture_coding_type() == 3 )
		return GetBits( 62, 3 );
	else
		return 0;
}
bool Picture::full_pel_backward_vector()
{
	if ( picture_coding_type() == 3 )
		return GetBits( 65, 1 );
	else
		return 0;
}
unsigned char Picture::backward_f_code()
{
	if ( picture_coding_type() == 3 )
		return GetBits( 66, 3 );
	else
		return 0;
}
unsigned char Picture::extra_information_picture( int n )
{
	int bits = 61;
	int i = 0;

	if ( picture_coding_type() == 2 )
		bits += 4;
	else if ( picture_coding_type() == 3 )
		bits += 8;

	while ( GetBits( bits, 1 ) )
	{
		if ( n == i )
			return GetBits( bits + 1, 8 );

		bits += 9;
		i++;
	}

	return 0;
}



//////////////////
// Sequence Header

SequenceHeader::SequenceHeader( Video *v ) : VideoSection( v )
{
}

SequenceHeader::~SequenceHeader()
{
}

int SequenceHeader::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 95;

	if ( blen < bits )
		return -1;

	if ( load_intra_quantiser_matrix() )
		bits += 512;

	bits += 1;

	if ( blen < bits )
		return -1;

	if ( load_non_intra_quantiser_matrix() )
		bits += 512;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void SequenceHeader::Dump()
{
	DEBUG( d_hdv_video, "SequenceHeader section H %d V %d aspect %d rate %d bitrate %d",
	       horizontal_size_value(), vertical_size_value(), aspect_ratio_information(), frame_rate_code(), bit_rate_value() );
}

unsigned int SequenceHeader::sequence_header_code() { return GetBits( 0, 32 ); }
unsigned short SequenceHeader::horizontal_size_value() { return GetBits( 32, 12 ); }
unsigned short SequenceHeader::vertical_size_value() { return GetBits( 44, 12 ); }
unsigned char SequenceHeader::aspect_ratio_information() { return GetBits( 56, 4 ); }
unsigned char SequenceHeader::frame_rate_code() { return GetBits( 60, 4 ); }
unsigned int SequenceHeader::bit_rate_value() { return GetBits( 64, 18 ); }
unsigned short SequenceHeader::vbv_buffer_size_value() { return GetBits( 83, 10 ); }
bool SequenceHeader::constrained_parameters_flag() { return GetBits( 93, 1 ); }
bool SequenceHeader::load_intra_quantiser_matrix() { return GetBits( 94, 1 ); }
unsigned char SequenceHeader::intra_quantiser_matrix( unsigned int n )
{
	if ( load_intra_quantiser_matrix() && n < 64 )
		return GetBits( 95 + ( n * 8 ), 8 );
	else
		return 0;
}
bool SequenceHeader::load_non_intra_quantiser_matrix()
{
	return load_intra_quantiser_matrix() ? GetBits( 607, 1 ) : GetBits( 95, 1 );
}
unsigned char SequenceHeader::non_intra_quantiser_matrix( unsigned int n )
{
	if ( load_non_intra_quantiser_matrix() && n < 64 )
		return load_intra_quantiser_matrix() ? GetBits( 608 + ( n * 8 ), 8 ) : GetBits( 96 + ( n * 8 ), 8 );
	else
		return 0;
}



////////////////////
// SequenceExtension

SequenceExtension::SequenceExtension( Video *v ) : VideoSection( v )
{
}

SequenceExtension::~SequenceExtension()
{
}

int SequenceExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 80;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void SequenceExtension::Dump()
{
	DEBUG( d_hdv_video, "SequenceExtension section" );
}

unsigned int SequenceExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char SequenceExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned char SequenceExtension::profile_and_level_indication() { return GetBits( 36, 8 ); }
bool SequenceExtension::progressive_sequence() { return GetBits( 44, 1 ); }
unsigned char SequenceExtension::chroma_format() { return GetBits( 45, 2 ); }
unsigned char SequenceExtension::horizontal_size_extension() { return GetBits( 47, 2 ); }
unsigned char SequenceExtension::vertical_size_extension() { return GetBits( 49, 2 ); }
unsigned short SequenceExtension::bit_rate_extension() { return GetBits( 51, 12 ); }
unsigned char SequenceExtension::vbv_buffer_size_extension() { return GetBits( 64, 8 ); }
bool SequenceExtension::low_delay() { return GetBits( 72, 1 ); }
unsigned char SequenceExtension::frame_rate_extension_n() { return GetBits( 73, 2 ); }
unsigned char SequenceExtension::frame_rate_extension_d() { return GetBits( 75, 5 ); }



///////////////////////////
// SequenceDisplayExtension

SequenceDisplayExtension::SequenceDisplayExtension( Video *v ) : VideoSection( v )
{
}

SequenceDisplayExtension::~SequenceDisplayExtension()
{
}

int SequenceDisplayExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 40;

	if ( blen < bits )
		return -1;

	if ( colour_description() )
		bits += 24;

	if ( blen < bits )
		return -1;

	bits += 29;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void SequenceDisplayExtension::Dump()
{
	DEBUG( d_hdv_video, "SequenceDisplayExtension section" );
}

unsigned int SequenceDisplayExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char SequenceDisplayExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned char SequenceDisplayExtension::video_format() { return GetBits( 36, 3 ); }
bool SequenceDisplayExtension::colour_description() { return GetBits( 39, 1 ); }
unsigned char SequenceDisplayExtension::colour_primaries() { return colour_description() ? GetBits( 40, 8 ) : 0; }
unsigned char SequenceDisplayExtension::transfer_characteristics() { return colour_description() ? GetBits( 48, 8 ) : 0; }
unsigned char SequenceDisplayExtension::matrix_coefficients() { return colour_description() ? GetBits( 56, 8 ) : 0; }
unsigned short SequenceDisplayExtension::display_horizontal_size()
{
	return colour_description() ? GetBits( 64, 14 ) : GetBits( 40, 14 );
}
bool SequenceDisplayExtension::marker_bit()
{
	return colour_description() ? GetBits( 78, 1 ) : GetBits( 54, 1 );
}
unsigned short SequenceDisplayExtension::display_vertical_size()
{
	return colour_description() ? GetBits( 79, 14 ) : GetBits( 55, 14 );
}



///////////////////////
// QuantMatrixExtension

QuantMatrixExtension::QuantMatrixExtension( Video *v ) : VideoSection( v )
{
}

QuantMatrixExtension::~QuantMatrixExtension()
{
}

int QuantMatrixExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 37;

	if ( blen < bits )
		return -1;

	if ( load_intra_quantiser_matrix() )
		bits += 512;

	bits += 1;

	if ( blen < bits )
		return -1;

	if ( load_non_intra_quantiser_matrix() )
		bits += 512;

	bits += 1;

	if ( blen < bits )
		return -1;

	if ( load_chroma_intra_quantiser_matrix() )
		bits += 512;

	bits += 1;

	if ( blen < bits )
		return -1;

	if ( load_chroma_non_intra_quantiser_matrix() )
		bits += 512;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void QuantMatrixExtension::Dump()
{
	DEBUG( d_hdv_video, "QuantMatrixExtension section" );
}

unsigned int QuantMatrixExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char QuantMatrixExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
bool QuantMatrixExtension::load_intra_quantiser_matrix() { return GetBits( 36, 1 ); }
unsigned char QuantMatrixExtension::intra_quantiser_matrix( unsigned int n )
{
	int pos = 37;

	if ( load_intra_quantiser_matrix() && n < 64 )
		return GetBits( pos + ( 8 * n ), 8 );
	else
		return 0;
}
bool QuantMatrixExtension::load_non_intra_quantiser_matrix()
{
	int pos = 37;

	if ( load_intra_quantiser_matrix() )
		pos += 512;

	return GetBits( pos, 1 );
}
unsigned char QuantMatrixExtension::non_intra_quantiser_matrix( unsigned int n )
{
	int pos = 38;

	if ( load_intra_quantiser_matrix() )
		pos += 512;

	if ( load_non_intra_quantiser_matrix() && n < 64 )
		return GetBits( pos + ( 8 * n ), 8 );
	else
		return 0;
}
bool QuantMatrixExtension::load_chroma_intra_quantiser_matrix()
{
	int pos = 38;

	if ( load_intra_quantiser_matrix() )
		pos += 512;

	if ( load_non_intra_quantiser_matrix() )
		pos += 512;

	return GetBits( pos, 1 );
}
unsigned char QuantMatrixExtension::chroma_intra_quantiser_matrix( unsigned int n )
{
	int pos = 39;

	if ( load_intra_quantiser_matrix() )
		pos += 512;

	if ( load_non_intra_quantiser_matrix() )
		pos += 512;

	if ( load_chroma_intra_quantiser_matrix() && n < 64 )
		return GetBits( pos + ( 8 * n ), 8 );
	else
		return 0;
}
bool QuantMatrixExtension::load_chroma_non_intra_quantiser_matrix()
{
	int pos = 39;

	if ( load_intra_quantiser_matrix() )
		pos += 512;

	if ( load_non_intra_quantiser_matrix() )
		pos += 512;

	if ( load_chroma_intra_quantiser_matrix() )
		pos += 512;

	return GetBits( pos, 1 );
}
unsigned char QuantMatrixExtension::chroma_non_intra_quantiser_matrix( unsigned int n )
{
	int pos = 40;

	if ( load_intra_quantiser_matrix() )
		pos += 512;

	if ( load_non_intra_quantiser_matrix() )
		pos += 512;

	if ( load_chroma_intra_quantiser_matrix() )
		pos += 512;

	if ( load_chroma_non_intra_quantiser_matrix() && n < 64 )
		return GetBits( pos + ( 8 * n ), 8 );
	else
		return 0;
}



/////////////////////
// CopyrightExtension

CopyrightExtension::CopyrightExtension( Video *v ) : VideoSection( v )
{
}

CopyrightExtension::~CopyrightExtension()
{
}

int CopyrightExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 110;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void CopyrightExtension::Dump()
{
	DEBUG( d_hdv_video, "CopyrightExtension section" );
}

unsigned int CopyrightExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char CopyrightExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
bool CopyrightExtension::copyright_flag() { return GetBits( 36, 1 ); }
unsigned char CopyrightExtension::copyright_identifier() { return GetBits( 37, 8 ); }
bool CopyrightExtension::original_or_copy() { return GetBits( 45, 1 ); }
unsigned int CopyrightExtension::copyright_number_1() { return GetBits( 54, 20 ); }
unsigned int CopyrightExtension::copyright_number_2() { return GetBits( 65, 22 ); }
unsigned int CopyrightExtension::copyright_number_3() { return GetBits( 88, 22 ); }



////////////////////////////
// SequenceScalableExtension

SequenceScalableExtension::SequenceScalableExtension( Video *v ) : VideoSection( v )
{
}

SequenceScalableExtension::~SequenceScalableExtension()
{
}

int SequenceScalableExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 42;

	if ( blen < bits )
		return -1;

	if ( scalable_mode() == SPATIAL_SCALABILITY )
	{
		bits += 48;
	}
	else if ( scalable_mode() == TEMPORAL_SCALABILITY )
	{
		bits += 1;

		if ( blen < bits )
			return -1;

		if ( picture_mux_enable() )
			bits += 1;

		bits += 6;
	}

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void SequenceScalableExtension::Dump()
{
	DEBUG( d_hdv_video, "SequenceScalableExtension section" );
}

unsigned int SequenceScalableExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char SequenceScalableExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned char SequenceScalableExtension::scalable_mode() { return GetBits( 36, 2 ); }
unsigned char SequenceScalableExtension::layer_id() { return GetBits( 38, 4 ); }
unsigned short SequenceScalableExtension::lower_layer_prediction_horizontal_size()
{
	return scalable_mode() == SPATIAL_SCALABILITY ? GetBits( 42, 14 ) : 0;
}
unsigned short SequenceScalableExtension::lower_layer_prediction_vertical_size()
{
	return scalable_mode() == SPATIAL_SCALABILITY ? GetBits( 57, 14 ) : 0;
}
unsigned char SequenceScalableExtension::horizontal_subsampling_factor_m()
{
	return scalable_mode() == SPATIAL_SCALABILITY ? GetBits( 71, 5 ) : 0;
}
unsigned char SequenceScalableExtension::horizontal_subsampling_factor_n()
{
	return scalable_mode() == SPATIAL_SCALABILITY ? GetBits( 76, 5 ) : 0;
}
unsigned char SequenceScalableExtension::vertical_subsampling_factor_m()
{
	return scalable_mode() == SPATIAL_SCALABILITY ? GetBits( 81, 5 ) : 0;
}
unsigned char SequenceScalableExtension::vertical_subsampling_factor_n()
{
	return scalable_mode() == SPATIAL_SCALABILITY ? GetBits( 86, 5 ) : 0;
}
bool SequenceScalableExtension::picture_mux_enable()
{
	return scalable_mode() == TEMPORAL_SCALABILITY ? GetBits( 42, 1 ) : 0;
}
bool SequenceScalableExtension::mux_to_progressive_sequence()
{
	if ( scalable_mode() == TEMPORAL_SCALABILITY )
		return picture_mux_enable() ? GetBits( 43, 1 ) : 0;
	else
		return 0;
}
unsigned char SequenceScalableExtension::picture_mux_order()
{
	if ( scalable_mode() == TEMPORAL_SCALABILITY )
		return picture_mux_enable() ? GetBits( 44, 3 ) : GetBits( 43, 3 );
	else
		return 0;
}
unsigned char SequenceScalableExtension::picture_mux_factor()
{
	if ( scalable_mode() == TEMPORAL_SCALABILITY )
		return picture_mux_enable() ? GetBits( 47, 3 ) : GetBits( 46, 3 );
	else
		return 0;
}



//////////////////////////
// PictureDisplayExtension

PictureDisplayExtension::PictureDisplayExtension( Video *v ) : VideoSection( v )
{
}

PictureDisplayExtension::~PictureDisplayExtension()
{
}

int PictureDisplayExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 36;

	if ( blen < bits )
		return -1;

	bits += 34 * number_of_frame_centre_offsets();

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void PictureDisplayExtension::Dump()
{
	DEBUG( d_hdv_video, "PictureDisplayExtension section" );
}

unsigned int PictureDisplayExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char PictureDisplayExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned short PictureDisplayExtension::frame_centre_horizontal_offset( unsigned int n )
{
	if ( n < number_of_frame_centre_offsets() )
		return GetBits( 36 + ( n * 34 ), 16 );
	else
		return 0;
}
unsigned short PictureDisplayExtension::frame_centre_vertical_offset( unsigned int n )
{
	if ( n < number_of_frame_centre_offsets() )
		return GetBits( 53 + ( n * 34 ), 16 );
	else
		return 0;
}

unsigned char PictureDisplayExtension::number_of_frame_centre_offsets()
{
	// These should be set in the stream before this, but if not return the minimum.
	if ( video->progressive_sequence < 0 || video->repeat_first_field < 0 ||
			video->top_field_first < 0 || video->picture_structure < 0 )
		return 1;

	bool picture_structure_is_field = video->picture_structure == PICTURE_STRUCTURE_TOP_FIELD ||
		video->picture_structure == PICTURE_STRUCTURE_BOTTOM_FIELD;

	// Straight from the spec.  Ick.
	if ( video->progressive_sequence == 1 ) {
		if ( video->repeat_first_field == 1 ) {
			if ( video->top_field_first == 1 )
				return 3;
			else
				return 2;
		} else {
			return 1;
		}
	} else {
		if ( picture_structure_is_field ) {
			return 1;
		} else {
			if ( video->repeat_first_field == 1 )
				return 3;
			else
				return 2;
		}
	}
}



/////////////////////////
// PictureCodingExtension

PictureCodingExtension::PictureCodingExtension( Video *v ) : VideoSection( v )
{
}

PictureCodingExtension::~PictureCodingExtension()
{
}

int PictureCodingExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 66;

	if ( blen < bits )
		return -1;

	if ( composite_display_flag() )
		bits += 20;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void PictureCodingExtension::Dump()
{
	DEBUG( d_hdv_video, "PictureCodingExtension section" );
}

unsigned int PictureCodingExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char PictureCodingExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned char PictureCodingExtension::f_code00() { return GetBits( 36, 4 ); }
unsigned char PictureCodingExtension::f_code01() { return GetBits( 40, 4 ); }
unsigned char PictureCodingExtension::f_code10() { return GetBits( 44, 4 ); }
unsigned char PictureCodingExtension::f_code11() { return GetBits( 48, 4 ); }
unsigned char PictureCodingExtension::intra_dc_precision() { return GetBits( 52, 2 ); }
unsigned char PictureCodingExtension::picture_structure() { return GetBits( 54, 2 ); }
bool PictureCodingExtension::top_field_first() { return GetBits( 56, 1 ); }
bool PictureCodingExtension::frame_pred_frame_dct() { return GetBits( 57, 1 ); }
bool PictureCodingExtension::concealment_motion_vectors() { return GetBits( 58, 1 ); }
bool PictureCodingExtension::q_scale_type() { return GetBits( 59, 1 ); }
bool PictureCodingExtension::intra_vlc_format() { return GetBits( 60, 1 ); }
bool PictureCodingExtension::alternate_scan() { return GetBits( 61, 1 ); }
bool PictureCodingExtension::repeat_first_field() { return GetBits( 62, 1 ); }
bool PictureCodingExtension::chroma_420_type() { return GetBits( 63, 1 ); }
bool PictureCodingExtension::progressive_frame() { return GetBits( 64, 1 ); }
bool PictureCodingExtension::composite_display_flag() { return GetBits( 65, 1 ); }
bool PictureCodingExtension::v_axis() { return composite_display_flag() ? GetBits( 66, 1 ) : 0; }
unsigned char PictureCodingExtension::field_sequence() { return composite_display_flag() ? GetBits( 67, 3 ) : 0; }
bool PictureCodingExtension::sub_carrier() { return composite_display_flag() ? GetBits( 70, 1 ) : 0; }
unsigned char PictureCodingExtension::burst_amplitude() { return composite_display_flag() ? GetBits( 71, 7 ) : 0; }
unsigned char PictureCodingExtension::sub_carrier_phase() { return composite_display_flag() ? GetBits( 78, 8 ) : 0; }



//////////////////////////////////
// PictureSpatialScalableExtension

PictureSpatialScalableExtension::PictureSpatialScalableExtension( Video *v ) : VideoSection( v )
{
}

PictureSpatialScalableExtension::~PictureSpatialScalableExtension()
{
}

int PictureSpatialScalableExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 82;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void PictureSpatialScalableExtension::Dump()
{
	DEBUG( d_hdv_video, "PictureSpatialScalableExtension section" );
}

unsigned int PictureSpatialScalableExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char PictureSpatialScalableExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned short PictureSpatialScalableExtension::lower_layer_temporal_reference() { return GetBits( 36, 10 ); }
unsigned short PictureSpatialScalableExtension::lower_layer_horizontal_offset() { return GetBits( 47, 15 ); }
unsigned short PictureSpatialScalableExtension::lower_layer_vertical_offset() { return GetBits( 63, 15 ); }
unsigned char PictureSpatialScalableExtension::spatial_temporal_weight_code_table_index() { return GetBits( 78, 2 ); }
bool PictureSpatialScalableExtension::lower_layer_progressive_frame() { return GetBits( 80, 1 ); }
bool PictureSpatialScalableExtension::lower_layer_deinterlaced_field_select() { return GetBits( 81, 1 ); }



///////////////////////////////////
// PictureTemporalScalableExtension

PictureTemporalScalableExtension::PictureTemporalScalableExtension( Video *v ) : VideoSection( v )
{
}

PictureTemporalScalableExtension::~PictureTemporalScalableExtension()
{
}

int PictureTemporalScalableExtension::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 59;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void PictureTemporalScalableExtension::Dump()
{
	DEBUG( d_hdv_video, "PictureTemporalScalableExtension section" );
}

unsigned int PictureTemporalScalableExtension::extension_start_code() { return GetBits( 0, 32 ); }
unsigned char PictureTemporalScalableExtension::extension_start_code_identifier() { return GetBits( 32, 4 ); }
unsigned char PictureTemporalScalableExtension::reference_select_code() { return GetBits( 36, 2 ); }
unsigned short PictureTemporalScalableExtension::forward_temporal_reference() { return GetBits( 38, 10 ); }
unsigned short PictureTemporalScalableExtension::backward_temporal_reference() { return GetBits( 49, 10 ); }



///////////
// UserData

UserData::UserData( Video *v ) : VideoSection( v )
{
}

UserData::~UserData()
{
}

int UserData::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 32;

	if ( blen < bits )
		return -1;

	while ( !START_CODE_PREFIX( bits / 8 ) )
	{
		bits += 8;

		if ( blen < bits )
			return -1;
	}

	return TOBYTES( bits );
}

void UserData::Dump()
{
	DEBUG( d_hdv_video, "UserData section length %d", GetCompleteLength() );
}



////////
// Group

Group::Group( Video *v ) : VideoSection( v )
{
}

Group::~Group()
{
}

int Group::GetCompleteLength()
{
	int blen = length * 8;
	int bits = 59;

	if ( blen < bits )
		return -1;

	return TOBYTES( bits );
}

void Group::Dump()
{
	DEBUG( d_hdv_video, "Group of pictures closed_gap %d broken_link %d time code %02d:%02d:%02d.%02d",
		closed_gop(), broken_link(),
		time_code_hours(),
		time_code_minutes(),
		time_code_seconds(),
		time_code_pictures()
		);
}

unsigned int Group::group_start_code() { return GetBits( 0, 32 ); }
unsigned int Group::time_code() { return GetBits( 32, 25 ); }
bool Group::closed_gop() { return GetBits( 57, 1 ); }
bool Group::broken_link() { return GetBits( 58, 1 ); }

bool Group::drop_frame_flag() { return GetBits( 32, 1 ); }
unsigned char Group::time_code_hours() { return GetBits( 33, 5 ); }
unsigned char Group::time_code_minutes() { return GetBits( 38, 6 ); }
unsigned char Group::time_code_seconds() { return GetBits( 45, 6 ); }
unsigned char Group::time_code_pictures() { return GetBits( 51, 6 ); }



////////
// Slice

Slice::Slice( Video *v ) : VideoSection( v )
{
}

Slice::~Slice()
{
}

int Slice::GetCompleteLength()
{
	//FIXME - replace with macroblock parsing
	if ( !last_pos )
	{
		int blen = length * 8;
		int bits = 32;

		if ( video->width > 2800 )
			bits += 3;

		if ( video->scalable_mode == DATA_PARTITIONING )
			bits += 7;

		bits += 5;

		if ( blen < bits )
			return -1;

		if ( intra_slice_flag() )
		{
			bits += 9;

			if ( blen < bits )
				return -1;

			while ( GetBits( bits, 1 ) )
			{
				bits += 9;

				if ( blen < bits )
					return -1;
			}
		}

		bits += 1;

		if ( blen < bits )
			return -1;

		last_pos = TOBYTES( bits );
	}

	while ( ( last_pos + 3 ) < length )
	{
		if ( START_CODE_PREFIX(last_pos) )
			return last_pos;
		else
			last_pos++;
	}

	return -1;
}

//FIXME - remove this when macroblock parsing is added
void Slice::Clear()
{
	last_pos = 0;
	VideoSection::Clear();
}

void Slice::Dump()
{
	DEBUG( d_hdv_video, "Slice section." );
}

unsigned int Slice::slice_start_code() { return GetBits( 0, 32 ); }
unsigned char Slice::slice_vertical_position_extension()
{
	return video->width > 2800 ? GetBits( 32, 3 ) : 0;
}
unsigned char Slice::priority_breakpoint()
{
	int bits = 32;

	if ( video->width > 2800 )
		bits += 3;

	return video->scalable_mode == DATA_PARTITIONING ? GetBits( bits, 7 ) : 0;
}
unsigned char Slice::quantiser_scale_code()
{
	int bits = 32;

	if ( video->width > 2800 )
		bits += 3;

	if ( video->scalable_mode == DATA_PARTITIONING )
		bits += 7;

	return GetBits( bits, 5 );
}
bool Slice::intra_slice_flag()
{
	int bits = 32;

	if ( video->width > 2800 )
		bits += 3;

	if ( video->scalable_mode == DATA_PARTITIONING )
		bits += 7;

	bits += 5;

	return GetBits( bits, 1 ); // Note, this is bit only exists if it's a 1...see spec
}
bool Slice::intra_slice()
{
	int bits = 32;

	if ( video->width > 2800 )
		bits += 3;

	if ( video->scalable_mode == DATA_PARTITIONING )
		bits += 7;

	bits += 6;

	return intra_slice_flag() ? GetBits( bits, 1 ) : 0;
}
bool Slice::extra_bit_slice( unsigned int n )
{
	unsigned int p = 0;
	int bits = 32;

	if ( video->width > 2800 )
		bits += 3;

	if ( video->scalable_mode == DATA_PARTITIONING )
		bits += 7;

	bits += 14;

	if ( !intra_slice_flag() )
		return 0;

	while ( GetBits( bits + ( p * 9 ), 1 ) )
	{
		if ( n == p )
			return GetBits( bits + ( p * 9 ), 1 );
		else
			p++;
	}

	return 0;
}
unsigned char Slice::extra_information_slice( unsigned int n )
{
	unsigned int p = 0;
	int bits = 32;

	if ( video->width > 2800 )
		bits += 3;

	if ( video->scalable_mode == DATA_PARTITIONING )
		bits += 7;

	bits += 14;

	if ( !intra_slice_flag() )
		return 0;

	while ( GetBits( bits + ( p * 9 ), 1 ) )
	{
		if ( n == p )
			return GetBits( bits + 1 + ( p * 9 ), 8 );
		else
			p++;
	}

	return 0;
}
