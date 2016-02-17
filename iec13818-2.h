/*
* iec13818-2.h
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

#ifndef _IEC13818_2_H
#define _IEC13818_2_H 1

#include "iec13818-1.h"

#define START_CODE_VALUE { (unsigned char)0x00, (unsigned char)0x00, (unsigned char)0x01 }
#define PICTURE_START_CODE_VALUE 0x00
#define SLICE_START_CODE_MIN 0x01
#define SLICE_START_CODE_MAX 0xaf
#define USER_DATA_START_CODE_VALUE 0xb2
#define SEQUENCE_HEADER_CODE_VALUE 0xb3
#define SEQUENCE_ERROR_CODE_VALUE 0xb4
#define EXTENSION_START_CODE_VALUE 0xb5
#define SEQUENCE_END_CODE_VALUE 0xb7
#define GROUP_START_CODE_VALUE 0xb8
#define SYSTEM_START_CODE_MIN 0xb9
#define SYSTEM_START_CODE_MAX 0xff

#define START_CODE_PREFIX( n ) ( GetData(n) == 0 && GetData((n)+1) == 0 && GetData((n)+2) == 1 )
#define START_CODE( n, code ) ( START_CODE_PREFIX(n) && GetData((n)+3) == (code) )
#define START_CODE_RANGE( n, min, max ) ( START_CODE_PREFIX(n) && GetData((n)+3) >= (min) && GetData((n)+3) <= (max) )

#define PICTURE_START_CODE( n ) ( START_CODE( (n), PICTURE_START_CODE_VALUE ) )
#define SLICE_START_CODE( n ) ( START_CODE_RANGE( (n), SLICE_START_CODE_MIN, SLICE_START_CODE_MAX ) )
#define USER_DATA_START_CODE( n ) ( START_CODE( (n), USER_DATA_START_CODE_VALUE ) )
#define SEQUENCE_HEADER_CODE( n ) ( START_CODE( (n), SEQUENCE_HEADER_CODE_VALUE ) )
#define SEQUENCE_ERROR_CODE( n ) ( START_CODE( (n), SEQUENCE_ERROR_CODE_VALUE ) )
#define EXTENSION_START_CODE( n ) ( START_CODE( (n), EXTENSION_START_CODE_VALUE ) )
#define SEQUENCE_END_CODE( n ) ( START_CODE( (n), SEQUENCE_END_CODE_VALUE ) )
#define GROUP_START_CODE( n ) ( START_CODE( (n), GROUP_START_CODE_VALUE ) )
#define SYSTEM_START_CODE( n ) ( START_CODE_RANGE( (n), SYSTEM_START_CODE_MIN, SYSTEM_START_CODE_MAX ) )

#define EXTENSION_ID( n, code ) ( EXTENSION_START_CODE( n ) && ( GetBits(((n)+4)*8,4) == (code) ) )

#define SEQUENCE_EXTENSION_ID( n ) ( EXTENSION_ID( n, SEQUENCE_EXTENSION_ID_VALUE ) )
#define SEQUENCE_DISPLAY_EXTENSION_ID( n ) ( EXTENSION_ID( n, SEQUENCE_DISPLAY_EXTENSION_ID_VALUE ) )
#define QUANT_MATRIX_EXTENSION_ID( n ) ( EXTENSION_ID( n, QUANT_MATRIX_EXTENSION_ID_VALUE ) )
#define COPYRIGHT_EXTENSION_ID( n ) ( EXTENSION_ID( n, COPYRIGHT_EXTENSION_ID_VALUE ) )
#define SEQUENCE_SCALABLE_EXTENSION_ID( n ) ( EXTENSION_ID( n, SEQUENCE_SCALABLE_EXTENSION_ID_VALUE ) )
#define PICTURE_DISPLAY_EXTENSION_ID( n ) ( EXTENSION_ID( n, PICTURE_DISPLAY_EXTENSION_ID_VALUE ) )
#define PICTURE_CODING_EXTENSION_ID( n ) ( EXTENSION_ID( n, PICTURE_CODING_EXTENSION_ID_VALUE ) )
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID( n ) ( EXTENSION_ID( n, PICTURE_SPATIAL_SCALABLE_EXTENSION_ID_VALUE ) )
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID( n ) ( EXTENSION_ID( n, PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID_VALUE ) )

#define SEQUENCE_EXTENSION_ID_VALUE 0x1
#define SEQUENCE_DISPLAY_EXTENSION_ID_VALUE 0x2
#define QUANT_MATRIX_EXTENSION_ID_VALUE 0x3
#define COPYRIGHT_EXTENSION_ID_VALUE 0x4
#define SEQUENCE_SCALABLE_EXTENSION_ID_VALUE 0x5
#define PICTURE_DISPLAY_EXTENSION_ID_VALUE 0x7
#define PICTURE_CODING_EXTENSION_ID_VALUE 0x8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID_VALUE 0x9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID_VALUE 0xa

#define FRAMERATE_LOOKUP( n ) ( \
  (n) == 1 ? 24000 / 1001 : \
  (n) == 2 ? 24 : \
  (n) == 3 ? 25 : \
  (n) == 4 ? 30000 / 1001 : \
  (n) == 5 ? 30 : \
  (n) == 6 ? 50 : \
  (n) == 7 ? 60000 / 1001 : \
  (n) == 8 ? 60 : \
  0 )



class Video;

///////////////
// VideoSection

class VideoSection
{
public:
	VideoSection( Video *video );
	virtual ~VideoSection();

	virtual void Clear();
	virtual void SetOffset( int o );
	virtual void AddLength( int l );
	virtual bool IsComplete();
	virtual unsigned char GetData( int pos );
	virtual unsigned long GetBits( int offset, int len ) { GETBITS( offset, len ); }

	virtual int GetCompleteLength() { return -1; }
	virtual void Dump() { }

protected:
	Video *video;

	int offset;
	int length;
};



//////////
// Picture

class Picture : public VideoSection
{
public:
	Picture( Video *v );
	~Picture();

	int GetCompleteLength();
	void Dump();

	unsigned int picture_start_code();
	unsigned short temporal_reference();
	unsigned char picture_coding_type();
	unsigned short vbv_delay();
	bool full_pel_forward_vector();
	unsigned char forward_f_code();
	bool full_pel_backward_vector();
	unsigned char backward_f_code();
	unsigned char extra_information_picture( int n );
};



//////////////////
// Sequence Header

class SequenceHeader : public VideoSection
{
public:
	SequenceHeader( Video *v );
	~SequenceHeader();

	int GetCompleteLength();
	void Dump();

	unsigned int sequence_header_code();
	unsigned short horizontal_size_value();
	unsigned short vertical_size_value();
	unsigned char aspect_ratio_information();
	unsigned char frame_rate_code();
	unsigned int bit_rate_value();
	unsigned short vbv_buffer_size_value();
	bool constrained_parameters_flag();
	bool load_intra_quantiser_matrix();
	unsigned char intra_quantiser_matrix( unsigned int n );
	bool load_non_intra_quantiser_matrix();
	unsigned char non_intra_quantiser_matrix( unsigned int n );
};



////////////////////
// SequenceExtension

class SequenceExtension : public VideoSection
{
public:
	SequenceExtension( Video *v );
	~SequenceExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned char profile_and_level_indication();
	bool progressive_sequence();
	unsigned char chroma_format();
	unsigned char horizontal_size_extension();
	unsigned char vertical_size_extension();
	unsigned short bit_rate_extension();
	unsigned char vbv_buffer_size_extension();
	bool low_delay();
	unsigned char frame_rate_extension_n();
	unsigned char frame_rate_extension_d();
};



///////////////////////////
// SequenceDisplayExtension

class SequenceDisplayExtension : public VideoSection
{
public:
	SequenceDisplayExtension( Video *v );
	~SequenceDisplayExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned char video_format();
	bool colour_description();
	unsigned char colour_primaries();
	unsigned char transfer_characteristics();
	unsigned char matrix_coefficients();
	unsigned short display_horizontal_size();
	bool marker_bit();
	unsigned short display_vertical_size();
};



///////////////////////
// QuantMatrixExtension

class QuantMatrixExtension : public VideoSection
{
public:
	QuantMatrixExtension( Video *v );
	~QuantMatrixExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	bool load_intra_quantiser_matrix();
	unsigned char intra_quantiser_matrix( unsigned int n );
	bool load_non_intra_quantiser_matrix();
	unsigned char non_intra_quantiser_matrix( unsigned int n );
	bool load_chroma_intra_quantiser_matrix();
	unsigned char chroma_intra_quantiser_matrix( unsigned int n );
	bool load_chroma_non_intra_quantiser_matrix();
	unsigned char chroma_non_intra_quantiser_matrix( unsigned int n );
};



/////////////////////
// CopyrightExtension

class CopyrightExtension : public VideoSection
{
public:
	CopyrightExtension( Video *v );
	~CopyrightExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	bool copyright_flag();
	unsigned char copyright_identifier();
	bool original_or_copy();
	unsigned int copyright_number_1();
	unsigned int copyright_number_2();
	unsigned int copyright_number_3();
};



////////////////////////////
// SequenceScalableExtension

#define DATA_PARTITIONING 0x00
#define SPATIAL_SCALABILITY 0x01
#define SNR_SCALABILITY 0x02
#define TEMPORAL_SCALABILITY 0x03

class SequenceScalableExtension : public VideoSection
{
public:
	SequenceScalableExtension( Video *v );
	~SequenceScalableExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned char scalable_mode();
	unsigned char layer_id();
	unsigned short lower_layer_prediction_horizontal_size();
	unsigned short lower_layer_prediction_vertical_size();
	unsigned char horizontal_subsampling_factor_m();
	unsigned char horizontal_subsampling_factor_n();
	unsigned char vertical_subsampling_factor_m();
	unsigned char vertical_subsampling_factor_n();
	bool picture_mux_enable();
	bool mux_to_progressive_sequence();
	unsigned char picture_mux_order();
	unsigned char picture_mux_factor();
};



//////////////////////////
// PictureDisplayExtension

class PictureDisplayExtension : public VideoSection
{
public:
	PictureDisplayExtension( Video *v );
	~PictureDisplayExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned short frame_centre_horizontal_offset( unsigned int n );
	unsigned short frame_centre_vertical_offset( unsigned int n );

	unsigned char number_of_frame_centre_offsets();
};



/////////////////////////
// PictureCodingExtension

#define PICTURE_STRUCTURE_TOP_FIELD 0x1
#define PICTURE_STRUCTURE_BOTTOM_FIELD 0x2
#define PICTURE_STRUCTURE_FRAME 0x3

class PictureCodingExtension : public VideoSection
{
public:
	PictureCodingExtension( Video *v );
	~PictureCodingExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned char f_code00();
	unsigned char f_code01();
	unsigned char f_code10();
	unsigned char f_code11();
	unsigned char intra_dc_precision();
	unsigned char picture_structure();
	bool top_field_first();
	bool frame_pred_frame_dct();
	bool concealment_motion_vectors();
	bool q_scale_type();
	bool intra_vlc_format();
	bool alternate_scan();
	bool repeat_first_field();
	bool chroma_420_type();
	bool progressive_frame();
	bool composite_display_flag();
	bool v_axis();
	unsigned char field_sequence();
	bool sub_carrier();
	unsigned char burst_amplitude();
	unsigned char sub_carrier_phase();
};



//////////////////////////////////
// PictureSpatialScalableExtension

class PictureSpatialScalableExtension : public VideoSection
{
public:
	PictureSpatialScalableExtension( Video *v );
	~PictureSpatialScalableExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned short lower_layer_temporal_reference();
	unsigned short lower_layer_horizontal_offset();
	unsigned short lower_layer_vertical_offset();
	unsigned char spatial_temporal_weight_code_table_index();
	bool lower_layer_progressive_frame();
	bool lower_layer_deinterlaced_field_select();
};



///////////////////////////////////
// PictureTemporalScalableExtension

class PictureTemporalScalableExtension : public VideoSection
{
public:
	PictureTemporalScalableExtension( Video *v );
	~PictureTemporalScalableExtension();

	int GetCompleteLength();
	void Dump();

	unsigned int extension_start_code();
	unsigned char extension_start_code_identifier();
	unsigned char reference_select_code();
	unsigned short forward_temporal_reference();
	unsigned short backward_temporal_reference();
};



///////////
// UserData

class UserData : public VideoSection
{
public:
	UserData( Video *v );
	~UserData();

	int GetCompleteLength();
	void Dump();
};



////////
// Group

class Group : public VideoSection
{
public:
	Group( Video *v );
	~Group();

	int GetCompleteLength();
	void Dump();

	unsigned int group_start_code();
	unsigned int time_code();
	bool closed_gop();
	bool broken_link();

	bool drop_frame_flag();
	unsigned char time_code_hours();
	unsigned char time_code_minutes();
	unsigned char time_code_seconds();
	unsigned char time_code_pictures();
};



////////
// Slice

class Slice : public VideoSection
{
public:
	Slice( Video *v );
	~Slice();

	int GetCompleteLength();
	void Dump();

	unsigned int slice_start_code();
	unsigned char slice_vertical_position_extension();
	unsigned char priority_breakpoint();
	unsigned char quantiser_scale_code();
	bool intra_slice_flag();
	bool intra_slice();
	bool extra_bit_slice( unsigned int n );
	unsigned char extra_information_slice( unsigned int n );

	//FIXME - TODO - parse macroblocks, etc.
	//this Clear() and last_pos aren't needed if macroblock parsing is added.
	void Clear();
private:
	int last_pos;
};



////////
// Video

class Video
{
public:
	Video();
	~Video();

	void Clear();
	void AddPacket( HDVPacket *packet );
	int GetLength();
	unsigned char *GetBuffer();
	unsigned char GetData( int pos );
	unsigned long GetBits( int o, int l ) { GETBITS( o, l ); }
	bool IsComplete();

	void ProcessPacket();

	int width, height;
	float frameRate;

	TimeCode timeCode;
	bool isTimeCodeSet;
	bool hasGOP;

	// Needed for iec13818-2 parsing.
	int repeat_first_field;
	int top_field_first;
	int picture_structure;
	int progressive_sequence;
	int scalable_mode;

private:
	PES pes;

	bool isComplete;

	int offset;
	int sectionStart;

	VideoSection *currentSection;
	VideoSection *lastSection;

	int sliceCount;

	Picture *picture;
	SequenceHeader *sequenceHeader;
	SequenceExtension *sequenceExtension;
	SequenceDisplayExtension *sequenceDisplayExtension;
	QuantMatrixExtension *quantMatrixExtension;
	CopyrightExtension *copyrightExtension;
	SequenceScalableExtension *sequenceScalableExtension;
	PictureDisplayExtension *pictureDisplayExtension;
	PictureCodingExtension *pictureCodingExtension;
	PictureSpatialScalableExtension *pictureSpatialScalableExtension;
	PictureTemporalScalableExtension *pictureTemporalScalableExtension;
	UserData *userData;
	Group *group;
	Slice *slice;
};



#endif
