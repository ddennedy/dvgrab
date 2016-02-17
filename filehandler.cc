/*
* filehandler.cc -- saving DV data into different file formats
* Copyright (C) 2000 Arne Schirmacher <arne@schirmacher.de>
* Raw DV, JPEG, and Quicktime portions Copyright (C) 2003-2008 Dan Dennedy <dan@dennedy.org>
* Portions of Quicktime code borrowed from Arthur Peters' dv_utils.
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

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

using std::cout;
using std::endl;
using std::ostringstream;
using std::setw;
using std::setfill;
using std::ends;

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "filehandler.h"
#include "error.h"
#include "riff.h"
#include "avi.h"
#include "frame.h"
#include "dvframe.h"
#include "affine.h"
#include "stringutils.h"

FileTracker *FileTracker::instance = NULL;

FileTracker::FileTracker( ) : mode( CAPTURE_MOVIE_APPEND )
{
	return ;
	sendEvent( ">> Constructing File Capture tracker" );
}

FileTracker::~FileTracker( )
{
	return ;
	sendEvent( ">> Destroying File Capture tracker" );
}

FileTracker &FileTracker::GetInstance( )
{
	if ( instance == NULL )
		instance = new FileTracker();

	return *instance;
}

void FileTracker::SetMode( FileCaptureMode mode )
{
	this->mode = mode;
}

FileCaptureMode FileTracker::GetMode( )
{
	return this->mode;
}

char *FileTracker::Get( int index )
{
	return list[ index ];
}

void FileTracker::Add( const char *file )
{
	return ;
	if ( this->mode != CAPTURE_IGNORE )
	{
		sendEvent( ">>>> Registering %s with the tracker", file );
		list.push_back( strdup( file ) );
	}
}

unsigned int FileTracker::Size( )
{
	return list.size();
}

void FileTracker::Clear( )
{
	while ( Size() > 0 )
	{
		free( list[ Size() - 1 ] );
		list.pop_back( );
	}
	this->mode = CAPTURE_MOVIE_APPEND;
}

FileHandler::FileHandler() :
        done( false ), autoSplit( false ), timeSplit(0), maxFrameCount( 0 ), isNewFile( false ), isFirstFile( -1 ),
	lastCollectionFreeSpace( 0 ), currentCollectionSize( 0 ), framesWritten( 0 ), filename( "" )
{
	prevTimeCode.sec = -1;
}


FileHandler::~FileHandler()
{
}


void FileHandler::CollectionCounterUpdate()
{
	if ( GetSizeSplitMode() == 1) 
	{
		currentCollectionSize += GetFileSize();

		// Shove this cut into the last collection if there's room
		if (currentCollectionSize <= lastCollectionFreeSpace) 
		{
			currentCollectionSize = 0;
			lastCollectionFreeSpace = lastCollectionFreeSpace - currentCollectionSize;
		}
		else
			lastCollectionFreeSpace = 0;

		// Start a new collection if we've gone over the Minimum Collection Size
		if (currentCollectionSize >= GetMinColSize())
		{
			if (currentCollectionSize < GetMaxColSize())
				lastCollectionFreeSpace = GetMaxColSize() - currentCollectionSize;
			else
				lastCollectionFreeSpace = 0;
			currentCollectionSize = 0; 
		}
	}
}


bool FileHandler::GetAutoSplit()
{
	return autoSplit;
}

int FileHandler::GetTimeSplit()
{
	return timeSplit;
}


bool FileHandler::GetTimeStamp()
{
	return timeStamp;
}


bool FileHandler::GetTimeSys()
{
	return timeSys;
}


bool FileHandler::GetTimeCode()
{
	return timeCode;
}


string FileHandler::GetBaseName()
{
	return base;
}


string FileHandler::GetExtension()
{
	return extension;
}


int FileHandler::GetMaxFrameCount()
{
	return maxFrameCount;
}

off_t FileHandler::GetMaxFileSize()
{
	return maxFileSize;
}

int FileHandler::GetSizeSplitMode()
{
	return sizeSplitMode;
}

off_t FileHandler::GetMaxColSize()
{
	return maxColSize;
}

off_t FileHandler::GetMinColSize()
{
	return minColSize;
}


void FileHandler::SetAutoSplit( bool flag )
{
	autoSplit = flag;
}


void FileHandler::SetTimeSplit( int secs )
{
	timeSplit = secs;
}


void FileHandler::SetTimeStamp( bool flag )
{
	timeStamp = flag;
}


void FileHandler::SetTimeSys( bool flag )
{
	timeSys = flag;
}


void FileHandler::SetTimeCode( bool flag )
{
	timeCode = flag;
}


void FileHandler::SetBaseName( const string& s )
{
	base = s;
}


void FileHandler::SetMaxFrameCount( int count )
{
	assert( count >= 0 );
	maxFrameCount = count;
}


void FileHandler::SetEveryNthFrame( int every )
{
	assert ( every > 0 );

	everyNthFrame = every;
}


void FileHandler::SetMaxFileSize( off_t size )
{
	assert ( size >= 0 );
	maxFileSize = size;
}

void FileHandler::SetSizeSplitMode( int mode )
{
	sizeSplitMode  = mode;
}

void FileHandler::SetMaxColSize( off_t size )
{
	maxColSize  = size;
}


void FileHandler::SetMinColSize( off_t size )
{
	minColSize  = size;
}

void FileHandler::SetFilmRate( bool flag)
{
	filmRate = flag;
}

void FileHandler::SetRemove2332( bool flag)
{
	remove2332 = flag;
}

bool FileHandler::Done()
{
	return done;
}


bool FileHandler::WriteFrame( Frame *frame )
{
	/* If the file size, collection size, or max frame count would be exceeded
	 * by this frame, and we can start a new file on this frame, close the current file.
	 * If the autosplit flag is set, a new file will be created.
	 */
	if ( FileIsOpen() && frame->CanStartNewStream() )
	{
		bool startNewFile = false;
		off_t newFileSize = GetFileSize() + frame->GetDataLen();
		bool maxFileSizeExceeded = newFileSize >= GetMaxFileSize();
		bool maxColSizeExceeded = GetCurrentCollectionSize() + newFileSize >= GetMaxColSize();

		if ( GetFileSize() > 0 )
		{
			if ( GetMaxFileSize() > 0 && maxFileSizeExceeded )
				startNewFile = true;

			if ( GetMaxColSize() > 0 && GetSizeSplitMode() == 1 && maxColSizeExceeded )
				startNewFile = true;
		}

		if ( GetMaxFrameCount() > 0 && framesWritten >= GetMaxFrameCount() )
			startNewFile = true;

		if ( startNewFile )
		{
			CollectionCounterUpdate();
			Close();
			done = !GetAutoSplit();
		}
	}

	TimeCode tc;
	int time_diff = 0;
	if ( frame->GetTimeCode( tc ) )
		time_diff = TIMECODE_TO_SEC(tc) - TIMECODE_TO_SEC(prevTimeCode);
	bool discontinuity = prevTimeCode.sec != -1 && ( time_diff > 1 || time_diff < 0 );

	bool isTimeSplit = false;
	if ( timeSplit != 0 )
	{
		struct tm rd;
		if ( frame->GetRecordingDate( rd ) )
		{
			time_t now = mktime( &rd );
			if ( now >= prevTime + timeSplit )
				isTimeSplit = true;
			prevTime = now;
		}
	}

	// If the user wants autosplit, start a new file if a new recording is detected
	// either by explicit frame flag or a timecode discontinuity
	if ( FileIsOpen() && ( ( GetAutoSplit() && ( frame->IsNewRecording() || discontinuity ) )
		|| isTimeSplit ) )
	{
		CollectionCounterUpdate();
		Close();
	}

	isNewFile = false;

	if ( ! FileIsOpen() )
	{
		static int counter = 0;

		ostringstream stimestamp, stimecode;
		prevTimeCode.sec = -1;

		if ( GetTimeStamp() || GetTimeSys() )
		{
			struct tm	date;

			if ( ( GetTimeStamp() && ! frame->GetRecordingDate( date ) ) || GetTimeSys() )
			{
				time_t timesys;
			
				time( &timesys );
				localtime_r( &timesys, &date );
			}

			stimestamp << setfill( '0' )
				<< setw( 4 ) << date.tm_year + 1900 << '.'
				<< setw( 2 ) << date.tm_mon + 1 << '.'
				<< setw( 2 ) << date.tm_mday << '_'
				<< setw( 2 ) << date.tm_hour << '-'
				<< setw( 2 ) << date.tm_min << '-'
				<< setw( 2 ) << date.tm_sec;
		}

		if ( GetTimeCode() )
		{
			TimeCode tc;
			
			if ( frame->GetTimeCode( tc ) )
			{	
				stimecode << setfill( '0' )
					<< setw( 2 ) << tc.hour << ':'
					<< setw( 2 ) << tc.min << ':'
					<< setw( 2 ) << tc.sec << ':'
					<< setw( 2 ) << tc.frame;
			}
			else
			{
				stimecode << "EE:EE:EE:EE";
			}
		}

		if ( GetTimeStamp() || GetTimeSys() || GetTimeCode() )
		{
			ostringstream sb;

			sb << GetBaseName();
			if ( ( GetTimeStamp() || GetTimeSys() ) && GetTimeCode() )
			{
				sb << stimestamp.str() << "--" << stimecode.str();
			}
			else if ( GetTimeCode() )
			{
				sb << stimecode.str();
			}
			else 
			{
				sb << stimestamp.str();
			}
			sb << GetExtension() << ends;
			filename = sb.str();
		}
		else
		{
			struct stat stats;
			do
			{
				ostringstream sb;
				sb << GetBaseName() << setfill( '0' ) << setw( 3 ) << ++ counter
					<< GetExtension() << ends;
				filename = sb.str();
			}
			while ( stat( filename.c_str(), &stats ) == 0 );
		}

		if ( ! Create( filename ) )
		{
			sendEvent( ">>> Error creating file!" );
			return false;
		}
		isNewFile = true;
		if ( isFirstFile == -1 )
			isFirstFile = 1;
		else if ( isFirstFile == 1 )
			isFirstFile = 0;
		framesWritten = 0;
		framesToSkip = 0;
	}

	/* write frame */

	if ( framesToSkip == 0 )
	{
		if ( 0 > Write( frame ) )
		{
			sendEvent( ">>> Error writing frame!" );
			return false;
		}
		framesToSkip = everyNthFrame;
		++framesWritten;
	}
	framesToSkip--;

	if ( !frame->GetTimeCode( prevTimeCode ) )
		prevTimeCode.sec = -1;

	return true;
}


static ssize_t writen( int fd, unsigned char *vptr, size_t n )
{
	size_t nleft = n;
	ssize_t nwritten;
	unsigned char *ptr = vptr;

	while ( nleft > 0 )
	{
		if ( ( nwritten = write( fd, ptr, nleft ) ) <= 0 )
		{
			if ( errno == EINTR )
			{
				nwritten = 0;
			}
			else
			{
				return -1;
			}
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}

/***************************************************************************/


RawHandler::RawHandler( const string& ext ) : fd( -1 )
{
	extension = ext;
}


RawHandler::~RawHandler()
{
	Close();
}


bool RawHandler::FileIsOpen()
{
	return fd != -1;
}


bool RawHandler::Create( const string& filename )
{
	if ( GetBaseName() == "-" )
		fd = fileno( stdout );
	else
		fd = open( filename.c_str(), O_CREAT | O_TRUNC | O_RDWR | O_NONBLOCK, 0644 );
	if ( fd != -1 )
	{
		FileTracker::GetInstance().Add( filename.c_str() );
		this->filename = filename;
	}
	return ( fd != -1 );
}


int RawHandler::Write( Frame *frame )
{
	int result = writen( fd, frame->data, frame->GetDataLen() );
	return result;
}


int RawHandler::Close()
{
	if ( fd != -1 && fd != fileno( stdin ) && fd != fileno( stdout ) )
	{
		close( fd );
		fd = -1;
	}
	return 0;
}


off_t RawHandler::GetFileSize()
{
	struct stat file_status;
	if ( fstat( fd, &file_status ) < 0 )
		return 0;
	return file_status.st_size;
}

int RawHandler::GetTotalFrames()
{
	return GetFileSize() / ( 480 * numBlocks );
}


bool RawHandler::Open( const char *s )
{
	unsigned char data[ 4 ];
	assert( fd == -1 );
	if ( strcmp( s, "-" ) == 0 )
	{
		fd = fileno( stdin );
		filename = "stdin";
	}
	else
	{
		fd = open( s, O_RDWR | O_NONBLOCK );
		if ( fd < 0 )
			return false;
		filename = s;
	}
	if ( read( fd, data, 4 ) < 0 )
		return false;
	lseek( fd, 0, SEEK_SET );
	numBlocks = ( ( data[ 3 ] & 0x80 ) == 0 ) ? 250 : 300;
	return true;

}


int RawHandler::GetFrame( Frame *frame, int frameNum )
{
	assert( fd != -1 );
	int size = 480 * numBlocks;
	if ( frameNum < 0 )
		return -1;
	off_t offset = ( ( off_t ) frameNum * ( off_t ) size );
	fail_if( lseek( fd, offset, SEEK_SET ) == ( off_t ) - 1 );
	if ( read( fd, frame->data, size ) > 0 )
		return 0;
	else
		return -1;
}


/***************************************************************************/


AVIHandler::AVIHandler( int format ) : avi( NULL ), filen( NULL ), aviFormat( format ), isOpenDML( false ),
		fccHandler( make_fourcc( "dvsd" ) ), infoSet( false )
{
	extension = ".avi";
}


AVIHandler::~AVIHandler()
{
	Close();
}


void AVIHandler::SetSampleFrame( DVFrame *sample )
{
	Pack pack;

	sample->GetAudioInfo( audioInfo );
	sample->GetVideoInfo( videoInfo );

	sample->GetAAUXPack( 0x50, pack );
	dvinfo.dwDVAAuxSrc = *( DWORD* ) ( pack.data + 1 );
	sample->GetAAUXPack( 0x51, pack );
	dvinfo.dwDVAAuxCtl = *( DWORD* ) ( pack.data + 1 );

	sample->GetAAUXPack( 0x52, pack );
	dvinfo.dwDVAAuxSrc1 = *( DWORD* ) ( pack.data + 1 );
	sample->GetAAUXPack( 0x53, pack );
	dvinfo.dwDVAAuxCtl1 = *( DWORD* ) ( pack.data + 1 );

	sample->GetVAUXPack( 0x60, pack );
	dvinfo.dwDVVAuxSrc = *( DWORD* ) ( pack.data + 1 );
	sample->GetVAUXPack( 0x61, pack );
	dvinfo.dwDVVAuxCtl = *( DWORD* ) ( pack.data + 1 );

#ifdef WITH_LIBDV

	if ( sample->decoder->std == e_dv_std_smpte_314m )
		fccHandler = make_fourcc( "dv25" );
#endif

	infoSet = true;
}


bool AVIHandler::FileIsOpen()
{
	return avi != NULL;
}

bool AVIHandler::Create( const string& filename )
{
	assert( avi == NULL );

	if ( !infoSet )
	{
		filen = &filename;
		return true;
	}

	switch ( aviFormat )
	{

	case AVI_DV1_FORMAT:
		fail_null( avi = new AVI1File );
		if ( avi->Create( filename.c_str() ) == false )
			return false;
		avi->Init( videoInfo.isPAL ? AVI_PAL : AVI_NTSC, audioInfo.frequency,
		           ( AVI_SMALL_INDEX | AVI_LARGE_INDEX ) );
		break;

	case AVI_DV2_FORMAT:
		fail_null( avi = new AVI2File );
		if ( avi->Create( filename.c_str() ) == false )
			return false;
		if ( GetOpenDML() )
			avi->Init( videoInfo.isPAL ? AVI_PAL : AVI_NTSC, audioInfo.frequency,
			           ( AVI_SMALL_INDEX | AVI_LARGE_INDEX ) );
		else
			avi->Init( videoInfo.isPAL ? AVI_PAL : AVI_NTSC, audioInfo.frequency,
			           ( AVI_SMALL_INDEX ) );
		break;

	default:
		assert( aviFormat == AVI_DV1_FORMAT || aviFormat == AVI_DV2_FORMAT );
	}

	avi->setDVINFO( dvinfo );
	avi->setFccHandler( make_fourcc( "iavs" ), fccHandler );
	avi->setFccHandler( make_fourcc( "vids" ), fccHandler );
	this->filename = filename;

	FileTracker::GetInstance().Add( filename.c_str() );
	return ( avi != NULL );
}


int AVIHandler::Write( Frame *frame )
{
	if ( !infoSet )
	{
		assert( !frame->IsHDV() );
		SetSampleFrame( (DVFrame*)frame );
		if ( ! Create( *filen ) )
		{
			sendEvent( ">>> Error creating file!" );
			return false;
		}
	}

	assert( avi != NULL );

	return ( avi->WriteFrame( frame ) ? 0 : -1 );
}


int AVIHandler::Close()
{
	if ( avi != NULL )
	{
		avi->WriteRIFF();
		delete avi;
		avi = NULL;
	}
	return 0;
}

off_t AVIHandler::GetFileSize()
{
	if ( avi )
		return avi->GetFileSize();
	else
		return 0;
}

int AVIHandler::GetTotalFrames()
{
	return avi->GetTotalFrames();
}


bool AVIHandler::Open( const char *s )
{
	assert( avi == NULL );
	fail_null( avi = new AVI1File );
	if ( avi->Open( s ) )
	{
		avi->ParseRIFF();
		if ( !( avi->verifyStreamFormat( make_fourcc( "dvsd" ) ) ||
		        avi->verifyStreamFormat( make_fourcc( "dv25" ) ) ) )
			return false;
		avi->ReadIndex();
		if ( avi->verifyStream( make_fourcc( "auds" ) ) )
			aviFormat = AVI_DV2_FORMAT;
		else
			aviFormat = AVI_DV1_FORMAT;
		isOpenDML = avi->isOpenDML();
		filename = s;
		return true;
	}
	else
		return false;

}


int AVIHandler::GetFrame( Frame *frame, int frameNum )
{
	int result = avi->GetFrame( frame, frameNum );
	return result;
}


void AVIHandler::SetOpenDML( bool flag )
{
	isOpenDML = flag;
}


bool AVIHandler::GetOpenDML()
{
	return isOpenDML;
}


/***************************************************************************/

#ifdef HAVE_LIBQUICKTIME

#ifndef HAVE_LIBDV_DV_H
#define DV_AUDIO_MAX_SAMPLES 1944
#endif

QtHandler::QtHandler() : fd( NULL ), audioBufferSize( 0 )
{
	extension = ".mov";
	Init();
}


QtHandler::~QtHandler()
{
	Close();
}

void QtHandler::Init()
{
	if ( fd != NULL )
		Close();

	fd = NULL;
	samplingRate = 0;
	samplesPerBuffer = 0;
	channels = 2;
	audioBuffer = NULL;
	audioChannelBuffer = NULL;
	isFullyInitialized = false;
}


bool QtHandler::FileIsOpen()
{
	return fd != NULL;
}


bool QtHandler::Create( const string& filename )
{
	Init();
	fd = quicktime_open( const_cast<char*>( filename.c_str() ), 0, 1 );
	this->filename = filename;

	return ( fd != NULL );
}


inline void QtHandler::DeinterlaceStereo16( void* pInput, int iBytes,
        void* pLOutput, void* pROutput )
{
	short int * piSampleInput = ( short int* ) pInput;
	short int* piSampleLOutput = ( short int* ) pLOutput;
	short int* piSampleROutput = ( short int* ) pROutput;

	while ( ( char* ) piSampleInput < ( ( char* ) pInput + iBytes ) )
	{
		*piSampleLOutput++ = *piSampleInput++;
		*piSampleROutput++ = *piSampleInput++;
	}
}

int QtHandler::Write( Frame *f )
{
	assert( !f->IsHDV() );
	int result = 0;
	DVFrame *frame = (DVFrame*)f;

	if ( ! isFullyInitialized )
	{
		AudioInfo audio;

		if ( frame->GetAudioInfo( audio ) )
		{
			/* TODO: handle 12-bit, non-linear audio */
			char compressor[] = QUICKTIME_TWOS;
			channels = 2;
			quicktime_set_audio( fd, channels, audio.frequency, 16,
			                     compressor );
		}
		else
		{
			channels = 0;
		}

		if ( filmRate || remove2332 )
		{
			char compressor[] = QUICKTIME_DV;
			quicktime_set_video( fd, 1, 720, 480, 24, compressor );
		}
		else
		{
			char compressor[] = QUICKTIME_DV;
			quicktime_set_video( fd, 1, 720, frame->IsPAL() ? 576 : 480,
			                     frame->GetFrameRate(), compressor );
		}

		if ( channels > 0 )
		{
			audioBuffer = new int16_t[ DV_AUDIO_MAX_SAMPLES * channels ];
			audioBufferSize = DV_AUDIO_MAX_SAMPLES;

			audioChannelBuffer = new short int * [ channels ];
			for ( int c = 0; c < channels; c++ )
				audioChannelBuffer[ c ] = new short int[ 3000 ];

			assert( channels <= 4 );

			for ( int c = 0; c < channels; c++ )
				audioChannelBuffers[ c ] = audioChannelBuffer[ c ];
		}
		else
		{
			audioChannelBuffer = NULL;

			for ( int c = 0; c < 4; c++ )
				audioChannelBuffers[ c ] = NULL;
		}

		isFullyInitialized = true;
	}

	if ( remove2332 )
	{
		const int frameOffset = 3;
		TimeCode tc;
		frame->GetTimeCode( tc );

		if ( ( tc.frame + frameOffset ) % 5 )
		{
			result = quicktime_write_frame( fd, const_cast<unsigned char*>( frame->data ),
			                                frame->GetFrameSize(), 0 );
		}
		else
		{
			result = 1;
		}
	}
	else
	{
		result = quicktime_write_frame( fd, const_cast<unsigned char*>( frame->data ),
		                                frame->GetFrameSize(), 0 );
	}
 
	if ( channels > 0 )
	{
		AudioInfo audio;
		frame->ExtractHeader();
		if ( frame->GetAudioInfo( audio ) && ( unsigned int ) audio.samples < audioBufferSize )
		{
			long bytesRead = frame->ExtractAudio( audioBuffer );

			DeinterlaceStereo16( audioBuffer, bytesRead,
			                     audioChannelBuffer[ 0 ],
			                     audioChannelBuffer[ 1 ] );

			quicktime_encode_audio( fd, audioChannelBuffers,
			                        NULL, bytesRead / 4 );
		}
	}
	return result;
}


int QtHandler::Close()
{
	if ( fd != NULL )
	{
		quicktime_close( fd );
		fd = NULL;
	}
	if ( audioBuffer != NULL )
	{
		delete audioBuffer;
		audioBuffer = NULL;
	}
	if ( audioChannelBuffer != NULL )
	{
		for ( int c = 0; c < channels; c++ )
			delete audioChannelBuffer[ c ];
		delete audioChannelBuffer;
		audioChannelBuffer = NULL;
	}
	return 0;
}


off_t QtHandler::GetFileSize()
{
	if ( fd )
	{
		struct stat file_status;
		if ( stat( filename.c_str(), &file_status ) == 0 )
			return file_status.st_size;
	}
	return 0;
}


int QtHandler::GetTotalFrames()
{
	return ( int ) quicktime_video_length( fd, 0 );
}


bool QtHandler::Open( const char *s )
{
	Init();

	fd = quicktime_open( ( char * ) s, 1, 1 );
	if ( fd == NULL )
	{
		fprintf( stderr, "Error opening: %s\n", s );
		return false;
	}

	if ( quicktime_has_video( fd ) <= 0 )
	{
		fprintf( stderr, "There must be at least one video track in the input file (%s).\n",
		         s );
		Close();
		return false;
	}
	if ( strncmp( quicktime_video_compressor( fd, 0 ), QUICKTIME_DV, 4 ) != 0 )
	{
		fprintf( stderr, "Video in input file (%s) must be in DV format.\n", s );
		Close();
		return false;
	}
	filename = s;

	return true;
}

int QtHandler::GetFrame( Frame *frame, int frameNum )
{
	assert( fd != NULL );
	quicktime_set_video_position( fd, frameNum, 0 );
	frame->SetDataLen( quicktime_read_frame( fd, frame->data, 0 ) );
	return 0;
}
#endif

/********************************************************************************/

#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBDV)

JPEGHandler::JPEGHandler( int quality, bool deinterlace, int width, int height,
                          bool overwrite, string temp, bool usetemp ) :
		isOpen( false ), count( 0 ), deinterlace( deinterlace ), overwrite( overwrite )
{
	extension = ".jpg";
	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress( &cinfo );
	cinfo.input_components = 3;		/* # of color components per pixel */
	cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
	jpeg_set_defaults( &cinfo );
	jpeg_set_quality( &cinfo, quality, TRUE /* limit to baseline-JPEG values */ );
	new_height = CLAMP( height, -1, 2048 );
	new_width = CLAMP( width, -1, 2048 );
	this->temp=temp;
	this->usetemp=usetemp;
}


JPEGHandler::~JPEGHandler()
{
	Close();
	jpeg_destroy_compress( &cinfo );
}

bool JPEGHandler::Create( const string& filename )
{
	this->filename = filename;
	isOpen = true;
	count = 0;
	return true;
}

/* this must be called before scaling */
/* height is fixed, returns new width */
int JPEGHandler::fixAspect( Frame *frame )
{
	assert( !frame->IsHDV() );
	DVFrame *dvframe = (DVFrame*)frame;
	int width = frame->GetWidth( );
	int height = frame->GetHeight( );
	static JSAMPLE image[ 2048 * 2048 * 3 ];
	register JSAMPLE *dest = image_buffer, *src = image;
	int new_width = dvframe->IsPAL() ? 337 : 320;
	int n = width / 2 - new_width;
	int d = width / 2;
	int a = n;

	memcpy( src, dest, width * height * 3 );

	for ( register int j = 0; j < height; j += 2 )
	{
		src = image + j * ( width * 3 );
		for ( register int i = 0; i < new_width ; i++ )
		{
			if ( a > d )
			{
				a -= d;
				src += 3;
			}
			else
				a += n;

			*dest++ = *src++;
			*dest++ = *src++;
			*dest++ = *src++;
			src += 3;
		}
	}
	return new_width;
}

bool JPEGHandler::scale( Frame *frame )
{
	int width = frame->GetWidth( );
	int height = frame->GetHeight( );
	static JSAMPLE image[ 2048 * 2048 * 3 ];
	register JSAMPLE *dest = image_buffer, *src = image;
	AffineTransform affine;
	double scale_x = ( double ) new_width / ( double ) width;
	double scale_y = ( double ) new_height / ( double ) height;

	memcpy( src, dest, width * height * 3 );

	register int i, j, x, y;
	if ( scale_x <= 1.0 && scale_y <= 1.0 )
	{
		affine.Scale( scale_x, scale_y );

		for ( j = 0; j < height; j++ )
			for ( i = 0; i < width; i++ )
			{
				x = ( int ) ( affine.MapX( i - width / 2, j - height / 2 ) );
				y = ( int ) ( affine.MapY( i - width / 2, j - height / 2 ) );
				x += new_width / 2;
				x = CLAMP( x, 0, new_width );
				y += new_height / 2;
				y = CLAMP( y, 0, new_height );
				//cout << "i = " << i << " j = " << j << " x = " << x << " y = " << y << endl;
				src = image + ( j * width * 3 ) + i * 3;
				dest = image_buffer + y * ( int ) ( new_width ) * 3 + ( int ) ( x * 3 );
				*dest++ = *src++;
				*dest++ = *src++;
				*dest++ = *src++;
			}
	}
	else if ( scale_x >= 1.0 && scale_y >= 1.0 )
	{
		affine.Scale( 1.0 / scale_x, 1.0 / scale_y );

		for ( y = 0; y < new_height; y++ )
			for ( x = 0; x < new_width; x++ )
			{
				i = ( int ) ( affine.MapX( x - new_width / 2, y - new_height / 2 ) );
				j = ( int ) ( affine.MapY( x - new_width / 2, y - new_height / 2 ) );
				i += width / 2;
				i = CLAMP( i, 0, new_width );
				j += height / 2;
				j = CLAMP( j, 0, new_height );
				//cout << "i = " << i << " j = " << j << " x = " << x << " y = " << y << endl;
				src = image + ( j * width * 3 ) + i * 3;
				dest = image_buffer + y * ( int ) ( new_width ) * 3 + ( int ) ( x * 3 );
				*dest++ = *src++;
				*dest++ = *src++;
				*dest++ = *src++;
			}
	}
	else
		return false;

	return true;
}


int JPEGHandler::Write( Frame *frame )
{
	assert( !frame->IsHDV() );
	DVFrame *dvframe = (DVFrame*)frame;
	JSAMPROW row_pointer[ 1 ];	/* pointer to JSAMPLE row[s] */
	int row_stride;		/* physical row width in image buffer */
	JDIMENSION width = frame->GetWidth();
	JDIMENSION height = frame->GetHeight();

	dvframe->ExtractHeader();
	if ( frame->IsNewRecording() && GetAutoSplit() )
		count = 0;
	dvframe->ExtractRGB( image_buffer );
	if ( deinterlace )
		dvframe->Deinterlace( image_buffer, 3 );
	if ( new_width != -1 || new_height != -1 )
	{
		if ( new_width == -1 )
			new_width = width;
		if ( new_height == -1 )
			new_height = height;
		if ( !scale( frame ) )
		{
			new_width = width;
			new_height = height;
		}
	}
	else
	{
		if ( new_width == -1 )
			new_width = width;
		if ( new_height == -1 )
			new_height = height;
	}

	if ( overwrite )
	{
		if ( StringUtils::ends( GetBaseName(), ".jpg" ) || StringUtils::ends( GetBaseName(), ".jpeg" ) )
		{
			file = GetBaseName();
		}
		else
		{
			ostringstream sb;
			sb << GetBaseName() << GetExtension() << ends;
			file = sb.str();
		}
	}
	else
	{
		ostringstream sb;
		sb << filename.substr( 0, filename.find_last_of( '.' ) ) << "-" 
			<< setfill( '0' ) << setw( 8 ) << ++count
			<< GetExtension() << ends;
		file = sb.str();
	}

	FILE *outfile;

	if ( this->usetemp ) {
		outfile = fopen( const_cast<char*>( this->temp.c_str() ), "wb" );
	} else {
		outfile = fopen( const_cast<char*>( file.c_str() ), "wb" );
	}
	if ( outfile != NULL )
		jpeg_stdio_dest( &cinfo, outfile );
	cinfo.image_width = new_width;
	cinfo.image_height = new_height;
	row_stride = cinfo.image_width * cinfo.input_components;
	jpeg_start_compress( &cinfo, TRUE );
	while ( cinfo.next_scanline < cinfo.image_height )
	{
		row_pointer[ 0 ] = &image_buffer[ cinfo.next_scanline * row_stride ];
		jpeg_write_scanlines( &cinfo, row_pointer, 1 );
	}
	jpeg_finish_compress( &cinfo );
	fclose( outfile );
	if ( this->usetemp ) {
		rename(const_cast<char*>( this->temp.c_str() ), const_cast<char*>( file.c_str() ));
	}
	return 0;
}

int JPEGHandler::Close( void )
{
	isOpen = false;
	return 0;
}

#endif


/***************************************************************************/


Mpeg2Handler::Mpeg2Handler( unsigned char flags, const string& ext ) :
	fd( -1 ), waitingForRecordingDate( true ), bufferLen( 0 ), totalFrames( 0 ),
	writerFlags( flags ), firstPayloadEntry( NULL )
{
	extension = ext;
}

Mpeg2Handler::~Mpeg2Handler()
{
	PayloadList *next;
	while ( firstPayloadEntry != NULL ) {
	    next = firstPayloadEntry->next;
	    free( firstPayloadEntry );
	    firstPayloadEntry = next;
	}	
	Close();
}

bool Mpeg2Handler::FileIsOpen()
{
	return fd != -1;
}

bool Mpeg2Handler::Create( const string& filename )
{
	if ( GetBaseName() == "-" )
		fd = fileno( stdout );
	else
		fd = open( filename.c_str(), O_CREAT | O_TRUNC | O_RDWR | O_NONBLOCK, 0644 );
	if ( fd != -1 )
	{
		FileTracker::GetInstance().Add( filename.c_str() );
		this->filename = filename;
	}
	return ( fd != -1 );
}

bool Mpeg2Handler::WriteFrame( Frame *frame )
{
	if ( waitingForRecordingDate )
	{
		struct tm rd;
		if ( !frame->GetRecordingDate( rd ) )
		{
			if ( bufferLen + frame->GetDataLen() < MPEG2_BUFFER_SIZE )
			{
				// Buffer up the first several frames until we get the recording date
				memcpy( &buffer[bufferLen], frame->data, frame->GetDataLen() );
				bufferLen += frame->GetDataLen();
				totalFrames++;
				return true;
			}
		}

		waitingForRecordingDate = false;
	}

	return FileHandler::WriteFrame( frame );
}

int Mpeg2Handler::Write( Frame *frame )
{
	int result;

	// Write any buffered data first.
	if ( bufferLen > 0 )
	{
		if ( frame->CouldBeJVCP25() && ( writerFlags & MPEG2_JVC_P25 ) )
			result = writeJVCP25( buffer, bufferLen );
		else
			result = writen( fd, buffer, bufferLen );
		if ( 0 > result )
			return result;
		bufferLen = 0;
	}

	if ( frame->CouldBeJVCP25() && ( writerFlags & MPEG2_JVC_P25 ) )
		result = writeJVCP25( frame->data, frame->GetDataLen() );
	else
		result = writen( fd, frame->data, frame->GetDataLen() );

	if ( 0 <= result )
		totalFrames++;

	return result;
}

int Mpeg2Handler::Close()
{
	if ( fd != -1 && fd != fileno( stdin ) && fd != fileno( stdout ) )
	{
		close( fd );
		fd = -1;
	}
	return 0;
}

off_t Mpeg2Handler::GetFileSize()
{
	struct stat file_status;
	if ( fstat( fd, &file_status ) < 0 )
		return 0;
	return file_status.st_size;
}

int Mpeg2Handler::GetTotalFrames()
{
	return totalFrames;
}

bool Mpeg2Handler::Open( const char *s )
{
	return false;
}

int Mpeg2Handler::GetFrame( Frame *frame, int frameNum )
{
	return -1;
}

static inline
int CorrectP25( unsigned char *data, unsigned char len, unsigned char *state )
{
	int i;

	for (i=0; i<len; i++)
	{
		switch (*state)
		{
		/* seek for start code prefix 0x00 0x00 0x01 */
		case 0:
		case 1:
			if ( 0x00 == data[i] )
				(*state)++;
			else
				*state = 0;
			break;

		case 2:
			if ( 0x01 == data[i] )
				*state = 3;
			else
				*state = 0;
			break;

		/* seek for start code values of sequence_header or */
		/* picture coding extension header */
		case 3:
			if ( SEQUENCE_HEADER_CODE_VALUE == data[i] ) /* Sequence Header? => ignore next 3 bytes */
				*state = 4;
			else if ( EXTENSION_START_CODE_VALUE == data[i] ) /* Extension Header? */
				*state = 14;
			else
				*state = 0;
			break;

		/* read over three more bytes */
		case 4:
		case 5:
		case 6:
		case 15:
		case 16:
			(*state)++;
			break;

		case 14:
			if ( PICTURE_CODING_EXTENSION_ID_VALUE  == (data[i] >> 4) )   /* Picture Coding extension ?*/
				*state = 15;
			else
				*state = 0;
			break;

		/* the eights bit has to be changed for both parameters */
		case 7:
			/* change 50 fps to 25 fps  */
				/* least significant 4 bits */
			/* 0110 = 50 fps	    */
			/* 0011 = 25 fps	    */
	
			/* works only with value of 50fps */
			/* data[i] ^= 0x05; */
			data[i] = ( data[i] & 0xF0 ) | 0x03;
			*state = 0;
			break;

		case 17:
			/* unset repeat_first_field flag */
			/* value |=  0x02 flag set	 */
			/* value &= ~0x02 flag unset	 */
			data[i] &= ~0x02;	
			*state = 0;
			break;

		default:
			/* undefined state */
			return -1;
		} /* switch */
    } /* for */
    return 0;
}


void Mpeg2Handler::ProcessPayload( unsigned char *packet, unsigned int pid, unsigned char len )
{
	static PayloadList	*current = NULL;
	
	if ( NULL != firstPayloadEntry ) /* look for the struct for the current pid */
		for ( current=firstPayloadEntry; ( ( NULL!=current ) && ( pid!=current->pid ) ); current=current->next );
	
	if (NULL == current)
	{
		current = ( PayloadList* ) malloc( sizeof(PayloadList) );
		if ( NULL == current )
		{
			fprintf( stderr, "Error allocating memory!\n" );
			exit( 1 );
		}
	
		if ( NULL == firstPayloadEntry )
			firstPayloadEntry = current;
	
		current->pid = pid;
		current->state = 0;
		current->next = NULL;
	} /* if */
	CorrectP25( packet, len, &(current->state) );
}


void Mpeg2Handler::ProcessTSPacket( unsigned char *packet )
{
	unsigned int pid;

	pid = ((packet[1] & 0x1F) << 8 ) | packet[2];
	if (0x1FFF == pid)
		return; /* throw NULL packet away */

	switch (packet[3] & 0x30)
	{
	case 0x00:		/* reserved */
	case 0x20:		/* adaptation field without payload */
		break;

	case 0x10:		/* payload only */
		ProcessPayload( &packet[4], pid, 188-4 );
		break;

	case 0x30:		/* adaptation field before payload */
		if (183 > packet[4])
		/* max_length of ts_packet
		   - 4 bytes header
		   - 1 byte adaptation-length info
		   - adaptation-length */
		ProcessPayload( &packet[188 - 4 - 1 - packet[4]], pid, 188 - 4 - 1 - packet[4] );
	    break;
	} /* switch */
}


int Mpeg2Handler::writeJVCP25( unsigned char *data, int len )
{
	static unsigned char state = 0, ts_packet[188], rest_length;
	int i;
	unsigned char next_possible_start_position = 0;
	
	for ( i = 0; i < len; i++ )
	{
		switch (state)
		{
		/* seek for HDV_PACKET_MARKER of transport packet */
		case 0:
			if ( HDV_PACKET_MARKER == data[i] )
			{
				ts_packet[0] = data[i];
				rest_length = 187;
				state = 1;
			}
			break;

		case 1:
			if (0 < rest_length)
			{
				ts_packet[ 188 - rest_length ] = data[i];
				rest_length--;
				if ( ! next_possible_start_position )
				if ( HDV_PACKET_MARKER == data[i] )
					next_possible_start_position = i;
			}
			else 
			{
				/* 188 byte seen */
				if ( HDV_PACKET_MARKER == data[i] )
				{
					/* last 188 bytes were ts packet */
					ProcessTSPacket( ts_packet );
					writen( fd, ts_packet, 188 );
					ts_packet[0] = data[i];
					rest_length = 187;
				}
				else
				{
					/* last 188 bytes are not a ts packet */
					/* scan again beginning with the next possible start of ts_packet */
					state = 0;
					/* write unchanged first bytes up to next_possible_start_position */
					writen( fd, ts_packet, next_possible_start_position + 1 );
					writeJVCP25( &ts_packet[ next_possible_start_position ],
						188 - next_possible_start_position);
					ts_packet[ 188 - rest_length ] = data[i];
				} /* if */
			} /* if */
			break;

		default:
			return -1; /* undefined state */
		} /* switch */
    } /* for */
    return 0;
}
