/*
* dvgrab.cc -- DVGrab control class
* Copyright (C) 2003-2009 Dan Dennedy <dan@dennedy.org>
* Major rewrite of code based upon older versions of dvgrab by Arne Schirmacher
*    and some Kino code also contributed by Charles Yates.
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

#include <iostream>
#include <sstream>
#include <iomanip>
using std::cerr;
using std::endl;

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <sys/select.h>
#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <libavc1394/rom1394.h>

#include "error.h"
#include "riff.h"
#include "avi.h"
#include "dvgrab.h"
#include "raw1394util.h"
#include "smiltime.h"
#include "stringutils.h"
#include "v4l2reader.h"
#include "srt.h"

extern bool g_done;
pthread_mutex_t DVgrab::capture_mutex;
pthread_t DVgrab::capture_thread;
pthread_t DVgrab::watchdog_thread;
Frame *DVgrab::m_frame;
FileHandler *DVgrab::m_writer;
static SubtitleWriter subWriter;


DVgrab::DVgrab( int argc, char *argv[] ) :
		m_program_name( argv[0] ), m_port( -1 ), m_node( -1 ), m_reader_active( false ), m_autosplit( false ),
		m_timestamp( false ), m_channel( DEFAULT_CHANNEL ), m_frame_count( DEFAULT_FRAMES ),
		m_max_file_size( DEFAULT_SIZE ), m_collection_size( DEFAULT_CSIZE ),
		m_collection_min_cut_file_size( DEFAULT_CMINCUTSIZE ), m_sizesplitmode ( 0 ),
		m_file_format( DEFAULT_FORMAT ), m_open_dml( false ), m_frame_every( DEFAULT_EVERY ),
		m_jpeg_quality( 75 ), m_jpeg_deinterlace( false ), m_jpeg_width( -1 ), m_jpeg_height( -1 ),
		m_jpeg_overwrite( false ), m_jpeg_temp( "dvtmp.jpg" ), m_jpeg_usetemp( false ),
		m_dropped_frames( 0 ), m_bad_frames(0), m_interactive( false ), m_buffers( DEFAULT_BUFFERS ), m_total_frames( 0 ),
		m_duration( "" ), m_timeDuration( 0 ), m_noavc( false ),
		m_guid( 0 ), m_timesys( false ), m_connection( 0 ), m_raw_pipe( false ),
		m_no_stop( false ), m_timecode( false ), m_lockstep( false ), m_lockPending( false ),
		m_lockstep_maxdrops( DEFAULT_LOCKSTEP_MAXDROPS ), m_lockstep_totaldrops( DEFAULT_LOCKSTEP_TOTALDROPS ),
		m_captureActive( false ), m_avc( 0 ), m_reader( 0 ), m_hdv( false ), m_showstatus( false ),
		m_isLastTimeCodeSet( false ), m_isLastRecDateSet( false ), m_v4l2( false ), m_jvc_p25( false ),
		m_24p( false ), m_24pa( false ), m_isRecordMode( false ), m_isRewindFirst( false ),
		m_timeSplit(0), m_srt( false ), m_isNewFile(false)
{
	m_frame = 0;
	m_writer = 0;
	m_input_file_name = NULL;
	m_dst_file_name = NULL;

	getargs( argc, argv );

	if ( m_v4l2 )
	{
		if ( !m_input_file_name )
			m_input_file_name = DEFAULT_V4L2_DEVICE;
	}
	else
	{
		// if reading stdin, make sure its not a tty!
		if ( m_input_file_name && ( strcmp( m_input_file_name, "-" ) == 0 ) && ( isatty( fileno( stdin ) ) || m_interactive ) )
			throw std::string( "Can't read from tty or in interactive mode" );
	
		if ( ! m_input_file_name && ( ! m_noavc || m_port == -1 ) )
			m_node = discoverAVC( &m_port, &m_guid );
	
		if ( ( m_interactive || ! m_input_file_name ) && ( ! m_noavc && m_node == -1 ) )
			throw std::string( "no camera exists" );
	
		if ( m_file_format == MPEG2TS_FORMAT )
			m_hdv = true;
	}

	pthread_mutex_init( &capture_mutex, NULL );
	if ( m_port != -1 )
	{
		iec61883Connection::CheckConsistency( m_port, m_node );

		if ( ! m_noavc )
		{
			m_avc = new AVC( m_port );
			if ( ! m_avc )
				throw std::string( "failed to initialize AV/C" );
			if ( m_interactive )
				m_avc->Pause( m_node );
			if ( m_avc->isHDV( m_node ) )
			{
				m_file_format = MPEG2TS_FORMAT;
				m_hdv = true;
			}
		}
		
		if ( m_guid )
		{
			m_connection = new iec61883Connection( m_port, m_node );
			if ( ! m_connection )
				throw std::string( "failed to establish isochronous connection" );
			m_channel = m_connection->GetChannel();
			sendEvent( "Established connection over channel %d", m_channel );
		}
		m_reader = new iec61883Reader( m_port, m_channel, m_buffers, 
			this->testCaptureProxy, this, m_hdv );
	}
	else if ( m_v4l2 )
	{
#ifdef HAVE_LINUX_VIDEODEV2_H
		m_reader = new v4l2Reader( m_input_file_name, m_buffers, m_hdv );
#endif
	}
	else if ( m_input_file_name )
	{
		m_reader = new pipeReader( m_input_file_name, m_buffers, m_hdv );
	}
	else
		throw std::string( "invalid source specified" );

	if ( m_reader )
	{
		pthread_create( &capture_thread, NULL, captureThread, this );
		m_reader->StartThread();
	}
}

DVgrab::~DVgrab()
{
	cleanup();
}

void DVgrab::print_usage()
{
	cerr << "Usage: " << m_program_name << " [options] [file] [-]" << endl;
	cerr << "Try " << m_program_name << " --help for more information" << endl;
}

void DVgrab::print_version()
{
	cerr << PACKAGE << " " << VERSION << endl;
}

void DVgrab::print_help()
{
	print_version();
	cerr << "Usage: " << m_program_name << " [options] [file] [-]" << endl << endl;
	cerr << "Use '-' to pipe video to stdout." << endl << endl;
	cerr << "Options:" << endl << endl;
	cerr << "  -a[n],-autosplit[=n] start a new file when a new recording is detected" << endl;
	cerr << "                         or if n is supplied, after n seconds gap in recording date/time" << endl;
	cerr << "  -buffers number      the number of internal frames to buffer [default " << DEFAULT_BUFFERS << "]" << endl;
	cerr << "  -card number         card number [default automatic]" << endl;
	cerr << "  -channel number      iso channel number for listening [default " << DEFAULT_CHANNEL << "]" << endl;
	cerr << "  -cmincutsize num     min file size in MiB due to collection split [default " << DEFAULT_CMINCUTSIZE << "]" << endl;
	cerr << "  -csize number        split file when collections of files are about to exceed" << endl;
	cerr << "                          number MiB, 0 = unlimited [default " << DEFAULT_CSIZE << "]" << endl;
	cerr << "  -debug type          display (HDV) debug info, type is one or more of:" << endl;
	cerr << "                          all,pat,pmt,pids,pid=N,pes,packet,video,sonya1" << endl;
	cerr << "  -d, -duration time   total capture duration specified as a SMIL time value:" << endl;
	cerr << "                          XXX[.Y]h, XXX[.Y]min, XXX[.Y][s], XXXms," << endl;
	cerr << "                          [[HH:]MM:]SS[.ms], or smpte=[[[HH:]MM:]SS:]FF" << endl;
	cerr << "                          [default unlimited]" << endl;
	cerr << "  -every number        write every n'th frame only [default " << DEFAULT_EVERY << "]" << endl;
	cerr << "  -f -format type      save as one of the following file types [default " << DEFAULT_FORMAT_STR << "]" << endl;
	cerr << "              raw         raw DV file with a .dv extension" << endl;
	cerr << "              dif         raw DV file with a .dif extension" << endl;
	cerr << "              dv1         'Type 1' DV AVI file" << endl;
	cerr << "              dv2, avi    'Type 2' DV AVI file" << endl;
#ifdef HAVE_LIBQUICKTIME
	cerr << "              qt, mov     QuickTime DV movie" << endl;
#endif
	cerr << "              mpeg2, hdv  MPEG-2 transport stream (HDV)" << endl;
#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBDV)
	cerr << "              jpeg, jpg   sequence of JPEG files (DV only)" << endl;
#endif
	cerr << "  -F, -frames number   max number of frames per split" << endl;
	cerr << "                          0 = unlimited [default " << DEFAULT_FRAMES << "]" << endl;
	cerr << "  -guid hex            select one of multiple DV devices by its GUID" << endl;
	cerr << "                          GUID is in hexadecimal; see /sys/bus/ieee1394/devices/" << endl;
	cerr << "  -h, -help            display this help and exit" << endl;
	cerr << "  -I, -input file       read from file (\"-\" = stdin)" << endl;
	cerr << "  -i, -interactive     go interactive with camera VTR and capture control" << endl;
#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBDV)
	cerr << "  -jpeg-deinterlace    deinterlace the output by line doubling the upper field" << endl;
	cerr << "  -jpeg-height n       scale the output to the specified height (max=2048)" << endl;
	cerr << "  -jpeg-overwrite      overwrite the same file instead of creating a sequence" << endl;
	cerr << "  -jpeg-quality n      set the JPEG compression level" << endl;
	cerr << "  -jpeg-temp name      use name as temporary file output" << endl;
	cerr << "  -jpeg-width n        scale the output to the specified width (max=2048)" << endl;
#endif
	cerr << "  -jvc-p25             remove repeat_first_field flag and set fps to 25 (HDV)" << endl;
	cerr << "                       (to correct stream from JVC cam recorded in P25 mode)" << endl;
	cerr << "  -lockstep            align capture to multiple of -frames based on timecode" << endl;
	cerr << "  -lockstep_maxdrops   max consecutive frame drops before closing file" << endl;
	cerr << "                          -1 = unlimited [default " << DEFAULT_LOCKSTEP_MAXDROPS << "]" << endl;
	cerr << "  -lockstep_totaldrops max total frame drops before closing file" << endl;
	cerr << "                          -1 = unlimited [default " << DEFAULT_LOCKSTEP_TOTALDROPS << "]" << endl;
	cerr << "  -noavc               disable use of AV/C VTR control" << endl;
	cerr << "  -nostop              do not send AV/C stop command on exit" << endl;
	cerr << "  -opendml             use the OpenDML extensions to write large (>1GB)" << endl;
	cerr << "                          'Type 2' DV AVI files (requires -format dv2)" << endl;
	cerr << "  -r, recordonly       only capture when not paused while in record mode" << endl;
	cerr << "  -rewind              completely rewind the tape prior to capture" << endl;
	cerr << "  -showstatus          show the recording status while capturing" << endl;
	cerr << "  -s, -size number     max file size, 0 = unlimited [default " << DEFAULT_SIZE << "]" << endl;
	cerr << "  -srt                 write SRT files with the recording date\n";
	cerr << "  -stdin               read from stdin pipe [default = raw1394]" << endl;
	cerr << "  -timecode            put the first frame's timecode into the file name" << endl;
	cerr << "  -t, -timestamp       put the date and time of recording into the file name" << endl;
	cerr << "  -timesys             put the system date and time into the file name" << endl;
#ifdef HAVE_LINUX_VIDEODEV2_H
	cerr << "  -V, -v4l2            capture DV from V4L2 USB device (linux-uvc)" << endl;
	cerr << "                          use -input to set device file [default " << DEFAULT_V4L2_DEVICE << "]" << endl;
#endif
	cerr << "  -v, -version         display version and exit" << endl;
#ifdef HAVE_LIBQUICKTIME
	cerr << "  -24p                 use 24 fps as output rate (Quicktime Only)" << endl;
	cerr << "  -24pa                remove 2:3:3:2 pulldown for 24p Advanced (Quicktime Only)" << endl;
#endif
	cerr << endl;
	cerr << "Check out the dvgrab website for the latest version, news and other software:" << endl;
	cerr << "http://www.kinodv.org/" << endl << endl;
}

void DVgrab::set_file_format( char *format )
{
	if ( strcmp( "dv1", format ) == 0 )
		m_file_format = AVI_DV1_FORMAT;
	else if ( strcmp( "dv2", format ) == 0 || strcmp( "avi", format ) == 0 )
		m_file_format = AVI_DV2_FORMAT;
	else if ( strcmp( "raw", format ) == 0 )
		m_file_format = RAW_FORMAT;
	else if ( strcmp( "qt", format ) == 0 || strcmp( "mov", format ) == 0 )
		m_file_format = QT_FORMAT;
	else if ( strcmp( "dif", format ) == 0 )
		m_file_format = DIF_FORMAT;
#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBDV)
	else if ( strcmp( "jpeg", format ) == 0 || strcmp( "jpg", format ) == 0 )
		m_file_format = JPEG_FORMAT;
#endif
	else if ( strncmp( "mpeg2", format, 5 ) == 0 || strcmp( "hdv", format ) == 0 )
		m_file_format = MPEG2TS_FORMAT;
	else
	{
		cerr << "Unknown file format : " << format << endl;
		print_usage();
		exit( EXIT_FAILURE );
	}
}

void DVgrab::set_format_from_name( void )
{
	std::string filename = m_dst_file_name;
	if ( filename.find( '.' ) != string::npos )
	{
		std::string ext = StringUtils::toUpper( filename.substr( filename.find_last_of( '.' ) + 1 ) );

		if ( ext == "AVI" )
			m_file_format = AVI_DV2_FORMAT;
		else if ( ext == "DV" )
			m_file_format = RAW_FORMAT;
		else if ( ext == "DIF" )
			m_file_format = DIF_FORMAT;
		else if ( ext == "MOV" )
			m_file_format = QT_FORMAT;
		else if ( ext == "JPG" || ext == "JPEG" )
			m_file_format = JPEG_FORMAT;
		else if ( ext == "M2T" )
			m_file_format = MPEG2TS_FORMAT;
		else
		{
			cerr << "Unknown filename extension" << endl;
			print_usage();
			exit( EXIT_FAILURE );
		}
		if ( m_file_format != JPEG_FORMAT )
			m_dst_file_name[ filename.find_last_of( '.' ) ] = '\0';
	}
}

void DVgrab::getargs( int argc, char *argv[] )
{
	const char *opts = "a::d:hif:F:I:rs:tVv-";
	int optindex = 0;
	int c;
	struct option long_opts[] = {
		// all these use sscanf for int conversion, use val == 0xff to indicate
		{ "autosplit", optional_argument, 0, 'a' },
		{ "buffers", required_argument, &m_buffers, 0xff },
		{ "card", required_argument, &m_port, 0xff },
		{ "channel", required_argument, &m_channel, 0xff },
		{ "cmincutsize", required_argument, &m_collection_min_cut_file_size, 0xff },
		{ "csize", required_argument, &m_collection_size, 0xff },
		{ "debug", required_argument, 0, 0 },
		{ "duration", required_argument, 0, 0 },
		{ "every", required_argument, &m_frame_every, 0xff },
		{ "format", required_argument, 0, 'f' },
		{ "frames", required_argument, &m_frame_count, 0xff },
		{ "guid", required_argument, 0, 0 },
		{ "help", no_argument, 0, 'h' },
		{ "input", required_argument, 0, 'I' },
		{ "interactive", no_argument, 0, 'i'},
#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBDV)
		{ "jpeg-deinterlace", no_argument, &m_jpeg_deinterlace, true },
		{ "jpeg-height", required_argument, &m_jpeg_height, 0xff },
		{ "jpeg-overwrite", no_argument, &m_jpeg_overwrite, true },
		{ "jpeg-quality", required_argument, &m_jpeg_quality, 0xff },
		{ "jpeg-temp", required_argument, &m_jpeg_usetemp, true },
		{ "jpeg-width", required_argument, &m_jpeg_width, 0xff },
#endif
		{ "jvc-p25", no_argument, &m_jvc_p25, true },
		{ "lockstep", no_argument, &m_lockstep, true },
		{ "lockstep_maxdrops", required_argument, &m_lockstep_maxdrops, 0xff },
		{ "lockstep_totaldrops", required_argument, &m_lockstep_totaldrops, 0xff },
		{ "noavc", no_argument, &m_noavc, true },
		{ "nostop", no_argument, &m_no_stop, true },
		{ "opendml", no_argument, &m_open_dml, true },
		{ "recordonly", no_argument, 0, 'r'},
		{ "rewind", no_argument, &m_isRewindFirst, true },
		{ "showstatus", no_argument, &m_showstatus, true },
		{ "size", required_argument, &m_max_file_size, 0xff },
		{ "srt", no_argument, &m_srt, true },
		{ "stdin", no_argument, 0, 0 },
		{ "timecode", no_argument, &m_timecode, true },
		{ "timestamp", no_argument, &m_timestamp, true },
		{ "timesys", no_argument, &m_timesys, true },
#ifdef HAVE_LINUX_VIDEODEV2_H
		{ "v4l2", no_argument, 0, 'V' },
#endif
#ifdef HAVE_LIBQUICKTIME
		{ "24p", no_argument, &m_24p, true },
		{ "24pa", no_argument, &m_24pa, true },
#endif
		{ "version", no_argument, 0, 'v' },
		{ 0, 0, 0, 0 }
	};

	while ( -1 != ( c = getopt_long_only( argc, argv, opts, long_opts, &optindex ) ) )
	{
		switch ( c )
		{
		case 0:
			{
				const char *name = long_opts[optindex].name;
				if ( long_opts[optindex].val == 0xff )
				{
					if ( sscanf( optarg, "%d", long_opts[optindex].flag ) != 1 )
					{
						cerr << "Parameter " << name << " invalid value : " << optarg << endl;
						print_usage();
						exit( EXIT_FAILURE );
					}
				}
				else if ( strcmp( "guid", name ) == 0 )
				{
					if ( sscanf( optarg, "%llx", &m_guid ) != 1 )
					{
						cerr << "Parameter m_guid invalid value : " << optarg << endl;
						print_usage();
						exit( EXIT_FAILURE );
					}
				}
				else if ( strcmp( "debug", name ) == 0 )
				{
					char *str = strdup( optarg );
					char *token;
					while ( token = strsep( &str, "," ) )
					{
						if ( strcmp( token, "all" ) == 0 )
							d_all = true;
						else if ( strcmp( token, "pat" ) == 0 )
							d_hdv_pat = true;
						else if ( strcmp( token, "pmt" ) == 0 )
							d_hdv_pmt = true;
						else if ( strcmp( token, "pids" ) == 0 )
							d_hdv_pids = true;
						else if ( strcmp( token, "pes" ) == 0 )
							d_hdv_pes = true;
						else if ( strcmp( token, "packet" ) == 0 )
							d_hdv_packet = true;
						else if ( strcmp( token, "video" ) == 0 )
							d_hdv_video = true;
						else if ( strcmp( token, "sonya1" ) == 0 )
							d_hdv_sonya1 = true;
						else if ( strncmp( token, "pid=", 4 ) == 0 )
							d_hdv_pid_add( (int)strtol( &token[4], NULL, 0 ) );
						else
						{
							cerr << "Invalid debug parameter : " << token << endl;
							print_usage();
							exit( EXIT_FAILURE );
						}
					}
					if ( str ) free( str );
				}
				else if ( strcmp( "jpeg-temp", name ) == 0 )
					m_jpeg_temp = optarg;
				else if ( strcmp( "stdin", name ) == 0 )
					m_input_file_name = "-";
				else if ( strcmp( "duration", name ) == 0 )
					m_duration = optarg;
			}
			break;
		case 'a':
			if ( optarg )
				m_timeSplit = atoi( optarg );
			else
				m_autosplit = true;
			break;
		case 'd':
			m_duration = optarg;
			break;
		case 'i':
			m_interactive = true;
			break;
		case 'f':
			set_file_format( optarg );
			break;
		case 'F':
			m_frame_count = atoi( optarg );
			break;
		case 'h':
			print_help();
			exit( EXIT_SUCCESS );
			break;
		case 'I':
			m_input_file_name = optarg;
			break;
		case 'r':
			m_isRecordMode = true;
			break;
		case 's':
			m_max_file_size = atoi( optarg );
			break;
		case 't':
			m_timestamp = true;
			break;
		case 'V':
			m_v4l2 = true;
			break;
		case 'v':
			print_version();
			exit( EXIT_SUCCESS );
			break;
		default:
			print_usage();
			exit( EXIT_FAILURE );
		}
	}

	if ( optind < argc )
	{
		if ( argv[ optind ][0] == '-' )
		{
			m_raw_pipe = true;
			++optind;
		}
	}
	if ( optind < argc )
	{
		m_dst_file_name = strdup( argv[ optind++ ] );
		set_format_from_name();
	}
	if ( optind < argc )
	{
		if ( argv[ optind ][0] == '-' )
		{
			m_raw_pipe = true;
			++optind;
		}
		else
		{
			cerr << "Too many output file names." << endl;
			print_usage();
			exit( EXIT_FAILURE );
		}
	}

	if ( m_dst_file_name == NULL && !m_raw_pipe )
		m_dst_file_name = strdup( "dvgrab-" );
}

void DVgrab::startCapture()
{
	if ( m_dst_file_name )
	{
		pthread_mutex_lock( &capture_mutex );
		switch ( m_file_format )
		{
		case RAW_FORMAT:
			m_writer = new RawHandler();
			break;

		case DIF_FORMAT:
			m_writer = new RawHandler( ".dif" );
			break;

		case AVI_DV1_FORMAT:
			{
				AVIHandler *aviWriter = new AVIHandler( AVI_DV1_FORMAT );
				m_writer = aviWriter;
				break;
			}

		case AVI_DV2_FORMAT:
			{
				AVIHandler *aviWriter = new AVIHandler( AVI_DV2_FORMAT );
				m_writer = aviWriter;
				if ( m_max_file_size == 0 || m_max_file_size > 1000 )
				{
					sendEvent( "Turning on OpenDML to support large file size." );
					m_open_dml = true;
				}
				aviWriter->SetOpenDML( m_open_dml );
				break;
			}

#ifdef HAVE_LIBQUICKTIME
		case QT_FORMAT:
			m_writer = new QtHandler();
			m_writer->SetFilmRate( m_24p );
			m_writer->SetRemove2332( m_24pa );
			break;
#endif

#if defined(HAVE_LIBJPEG) && defined(HAVE_LIBDV)
		case JPEG_FORMAT:
			m_writer = new JPEGHandler( m_jpeg_quality, m_jpeg_deinterlace, m_jpeg_width, m_jpeg_height, m_jpeg_overwrite, m_jpeg_temp, m_jpeg_usetemp );
			break;
#endif

		case MPEG2TS_FORMAT:
			m_writer = new Mpeg2Handler( m_jvc_p25 ? MPEG2_JVC_P25 : 0 );
			break;

		}
		m_writer->SetTimeStamp( m_timestamp );
		m_writer->SetTimeSys( m_timesys );
		m_writer->SetTimeCode( m_timecode );
		m_writer->SetBaseName( m_dst_file_name );
		m_writer->SetMaxFrameCount( m_frame_count );
		m_writer->SetAutoSplit( m_autosplit );
		m_writer->SetTimeSplit ( m_timeSplit );
		m_writer->SetEveryNthFrame( m_frame_every );
		m_writer->SetMaxFileSize( ( off_t ) m_max_file_size * ( off_t ) ( 1024 * 1024 ) );
		if (m_collection_size) {
		  m_sizesplitmode = 1;
		}
		m_writer->SetSizeSplitMode( m_sizesplitmode );
		m_writer->SetMaxColSize( ( off_t ) ( m_collection_size ) * ( off_t ) ( 1024 * 1024 ) );
		m_writer->SetMinColSize( ( off_t ) ( m_collection_size - m_collection_min_cut_file_size ) * ( off_t ) ( 1024 * 1024 ) );
	}

	if ( m_avc )
	{
		if ( m_isRewindFirst && !m_interactive )
		{
			// Stop whatever is happening
			m_avc->Stop( m_node );

			// Wait until it is stopped
			while ( !g_done &&
				AVC1394_MASK_RESPONSE_OPERAND( m_avc->TransportStatus( m_node ), 3 )
				!= AVC1394_VCR_OPERAND_WIND_STOP )
			{
				timespec t = {0, 125000000L};
				nanosleep( &t, NULL );
			}

			// Rewind
			if ( !g_done )
			{
				m_avc->Rewind( m_node );
				timespec t = {0, 125000000L};
				nanosleep( &t, NULL );
			}

			// Wait until is done rewinding
			while ( !g_done &&
			        AVC1394_MASK_RESPONSE_OPERAND( m_avc->TransportStatus( m_node ), 3 )
			            != AVC1394_VCR_OPERAND_WIND_STOP )
			{
				timespec t = {0, 125000000L};
				nanosleep( &t, NULL );
			}
		}

		// Now Play so we can capture something
		if ( !g_done )
			m_avc->Play( m_node );
	}

	sendEvent( "Waiting for %s...", m_hdv ? "HDV" : "DV" );

	// this is a little unclean, checking global g_done from main.cc to allow interruption
	while ( !g_done && m_frame == NULL )
	{
		timespec t = {0, 25000000L};
		nanosleep( &t, NULL );
	}

	if ( !g_done && m_frame )
	{
		// OK, we have data, commence capture
		sendEvent( "Capture Started" );
		m_captureActive = true;
		m_total_frames = 0;

		// parse the SMIL time value duration
		if ( m_timeDuration == NULL && ! m_duration.empty() )
			m_timeDuration = new SMIL::MediaClippingTime( m_duration, m_frame->GetFrameRate() );

		if ( m_dst_file_name )
			pthread_mutex_unlock( &capture_mutex );
	}
	else
	{
		// No data received, throw an error
		if ( m_dst_file_name )
			pthread_mutex_unlock( &capture_mutex );
		const char *err = m_hdv ? "no HDV. Try again before giving up." : "no DV";
		if ( m_hdv )
			reset_bus( m_port );
		throw std::string( err );
	}
}


void DVgrab::stopCapture()
{
	pthread_mutex_lock( &capture_mutex );
	if ( m_writer != NULL )
	{
		std::string filename = m_writer->GetFileName();
		int frames = m_writer->GetFramesWritten();
		float size = ( float ) m_writer->GetFileSize() / 1024 / 1024;

		m_writer->Close();
		delete m_writer;
		m_writer = NULL;
		if ( m_avc && m_interactive )
			m_avc->Pause( m_node );
		if ( m_frame != NULL )
		{
			TimeCode timeCode;
			struct tm recDate;
			m_frame->GetTimeCode( timeCode );
			if ( ! m_frame->GetRecordingDate( recDate ) )
			{
				// If the month is invalid, then report system date/time
				time_t timesys;
				time( &timesys );
				localtime_r( &timesys, &recDate );
			}
			sendEvent( "\"%s\": %8.2f MiB %d frames timecode %2.2d:%2.2d:%2.2d.%2.2d date %4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d",
			           filename.c_str(), size, frames,
			           timeCode.hour, timeCode.min, timeCode.sec, timeCode.frame,
			           recDate.tm_year + 1900, recDate.tm_mon + 1, recDate.tm_mday,
			           recDate.tm_hour, recDate.tm_min, recDate.tm_sec
			         );
		}
		else
			sendEvent( "\"%s\" %8.2f MiB %d frames", filename.c_str(), size, frames );
		sendEvent( "Capture Stopped" );

		if ( m_dropped_frames > 0 )
			sendEvent( "Warning: %d dropped frames.", m_dropped_frames );
		if ( m_bad_frames > 0 )
			sendEvent( "Warning: %d damaged frames.", m_bad_frames );
		m_dropped_frames = 0;
		m_bad_frames = 0;
		m_captureActive = false;
	}
	pthread_mutex_unlock( &capture_mutex );
}


void DVgrab::testCapture( void )
{
	pthread_attr_t thread_attributes;

	sendEvent( "Bus Reset, launching watchdog thread" );

	pthread_attr_init( &thread_attributes );
	pthread_attr_setdetachstate( &thread_attributes, PTHREAD_CREATE_DETACHED );
	pthread_create( &watchdog_thread, NULL, watchdogThreadProxy, this );
}

void* DVgrab::watchdogThreadProxy( void* arg )
{
	DVgrab *self = static_cast< DVgrab* >( arg );
	self->watchdogThread();
	
	return NULL;
}

void DVgrab::watchdogThread()
{
	if ( m_reader )
	{
		if ( ! m_reader->WaitForAction( 1 ) )
		{
			cleanup();
			sendEvent( "Error: timed out waiting for DV after bus reset" );
			throw;
		}
		// Otherwise, reestablish the connection
		if ( m_connection )
		{
			int newChannel = m_connection->Reconnect();
			if ( newChannel != m_channel )
			{
				cleanup();
				sendEvent( "Error: unable to reestablish connection after bus reset" );
				throw;

				// TODO: the following attempt to recreate reader and restart capture
				// does not work
#if 0
				bool restartCapture = m_captureActive;

				if ( m_captureActive )
					stopCapture();
				m_reader_active = false;
				if ( m_reader )
				{
					m_reader->TriggerAction( );
					pthread_join( capture_thread, NULL );
					m_reader->StopThread();
					delete m_reader;
				}		
				sendEvent( "Closed existing reader" );
				m_reader = new iec61883Reader( m_port, m_channel, m_buffers, 
					this->testCaptureProxy, this, m_hdv );
				if ( m_reader )
				{
					sendEvent( "new reader created" );
					pthread_create( &capture_thread, NULL, captureThread, this );
					m_reader->StartThread();
				}
				sendEvent( "restarting capture" );
				if ( restartCapture )
					startCapture();
#endif
			}
		}
	}
}

void DVgrab::testCaptureProxy( void* arg )
{
	DVgrab *self = static_cast< DVgrab * >( arg );
	self->testCapture();
}

void *DVgrab::captureThread( void *arg )
{
	DVgrab * me = ( DVgrab* ) arg;
	me->captureThreadRun();
	return NULL;
}

void DVgrab::sendCaptureStatus( const char *name, float size, int frames, TimeCode *tc, struct tm *rd, bool newline )
{
	char tc_str[64], rd_str[128];

	if ( tc )
		sprintf( tc_str, "%2.2d:%2.2d:%2.2d.%2.2d", 
			tc->hour, tc->min, tc->sec, tc->frame );
	else
		sprintf( tc_str, "??:??:??.??" );

	if ( rd )
		sprintf( rd_str, "%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d",
			rd->tm_year + 1900, rd->tm_mon + 1, rd->tm_mday,
			rd->tm_hour, rd->tm_min, rd->tm_sec );
	else
		sprintf( rd_str, "????.??.?? ??:??:??" );

	sendEventParams( 2, 0, "\"%s\": %8.2f MiB %5d frames timecode %s date %s%s",
		name, size, frames, tc_str, rd_str, newline ? "\n" : "" );
}

void DVgrab::writeFrame()
{
	// All access to the writer is protected
	pthread_mutex_lock( &capture_mutex );

	// see if we have exceeded requested duration
	if ( m_timeDuration && m_timeDuration->isResolved() &&
	     ( ( float )m_total_frames++ / m_frame->GetFrameRate() * 1000.0 + 0.5 ) >=
	     m_timeDuration->getResolvedOffset() )
	{
		pthread_mutex_unlock( &capture_mutex );
		stopCapture();
		m_reader_active = false;
	}
	else if ( m_writer != NULL )
	{
		std::string fileName = m_writer->GetFileName();
		float size = ( float ) m_writer->GetFileSize() / 1024 / 1024;
		int framesWritten = m_writer->GetFramesWritten();
		TimeCode tc, *timeCode = &tc;
		struct tm rd, *recDate = &rd;
		TimeCode *lasttc = m_isLastTimeCodeSet ? &m_lastTimeCode : 0;
		struct tm *lastrd = m_isLastRecDateSet ? &m_lastRecDate : 0;

		if ( !m_frame->GetTimeCode( tc ) )
			timeCode = 0;
		if ( !m_frame->GetRecordingDate( rd ) )
		{
			// If the month is invalid, then report system date/timem_reader_active
			time_t timesys;
			time( &timesys );
			localtime_r( &timesys, recDate );
		}

		if ( m_lockstep && m_lockPending && m_frame_count > 0 && m_frame->CanStartNewStream() )
		{
			// If a lock is pending due to dropped frames, close the file
			if ( m_writer->FileIsOpen() )
			{
				m_writer->CollectionCounterUpdate();
				m_writer->Close();
			}
			
			if ( !m_hdv && timeCode )
			{
				// Convert timecode to #frames
				SMIL::MediaClippingTime mcTime( m_frame->GetFrameRate() );
				std::ostringstream sb;
				sb << setfill( '0' ) << std::setw( 2 ) 
				<< timeCode->hour << ':' << timeCode->min << ':'
				<< timeCode->sec << ':' << timeCode->frame;
				DVFrame *dvframe = static_cast<DVFrame*>( m_frame );
				if ( dvframe->IsPAL() )
					mcTime.parseSmpteValue( sb.str() );
				else
					mcTime.parseSmpteNtscDropValue( sb.str() );
					
				// If lock step point (multiple of frame count) is reached, skip writing
				if ( mcTime.getFrames() % m_frame_count != 0 )
				{
					pthread_mutex_unlock( &capture_mutex );
					return;
				}
			}
			m_lockPending = false;
		}

		if ( ! m_writer->WriteFrame( m_frame ) )
		{
			pthread_mutex_unlock( &capture_mutex );
			stopCapture();
			throw std::string( "writing failed" );
		}

		m_isNewFile |= m_writer->IsNewFile();

		if ( m_writer->IsNewFile() && !m_writer->IsFirstFile() )
		{
			sendCaptureStatus( fileName.c_str(), size, framesWritten, lasttc, lastrd, true );
			if ( m_dropped_frames > 0 )
				sendEvent( "Warning: %d dropped frames.", m_dropped_frames );
			m_dropped_frames = 0;
			if ( m_bad_frames > 0 )
				sendEvent( "Warning: %d damaged frames.", m_bad_frames );
			m_bad_frames = 0;
		}
		else if ( m_showstatus )
		{
			sendCaptureStatus( m_writer->GetFileName().c_str(),
				(float) m_writer->GetFileSize() / 1024 / 1024,
				m_writer->GetFramesWritten(), timeCode, recDate, false );
		}

		if ( timeCode )
		{
			memcpy( &m_lastTimeCode, timeCode, sizeof( m_lastTimeCode ) );
			m_isLastTimeCodeSet = true;
		}
		if ( recDate )
		{
			memcpy( &m_lastRecDate, recDate, sizeof( m_lastRecDate ) );
			m_isLastRecDateSet = true;
			if ( m_srt )
			{
				if ( m_isNewFile )
				{
					subWriter.newFile( m_writer->GetFileName().c_str() );
					m_isNewFile = false;
				}
				if ( !subWriter.hasFrameRate() )
					subWriter.setFrameRate( m_frame->GetFrameRate() );
				subWriter.addRecordingDate( *recDate, tc );
			}
		}
	}
	pthread_mutex_unlock( &capture_mutex );
}

void DVgrab::sendFrameDroppedStatus( const char *reason, const char *meaning )
{
	TimeCode timeCode;
	struct tm recDate;
	char tc[32], rd[32];

	if ( m_frame && m_frame->GetTimeCode( timeCode ) )
		sprintf( tc, "%2.2d:%2.2d:%2.2d.%2.2d",
			timeCode.hour, timeCode.min, timeCode.sec, timeCode.frame );
	else
		sprintf( tc, "??:??:??.??" );

	if ( m_frame && m_frame->GetRecordingDate( recDate ) )
		sprintf( rd, "%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d",
			recDate.tm_year + 1900, recDate.tm_mon + 1, recDate.tm_mday,
			recDate.tm_hour, recDate.tm_min, recDate.tm_sec );
	else
		sprintf( rd, "????.??.?? ??:??:??" );

	sendEvent( "\n\a\"%s\": %s: timecode %s date %s",
		m_writer ? m_writer->GetFileName().c_str() : "", reason, tc, rd );

	sendEvent( meaning );
}

void DVgrab::captureThreadRun()
{
	m_lockPending = true;
	m_reader_active = true;

	// Loop until we're informed otherwise
	while ( m_reader_active )
	{
		pthread_testcancel();
		// Wait for the reader to indicate that something has happened
		m_reader->WaitForAction( );

		int dropped = m_reader->GetDroppedFrames();
		int badFrames = m_reader->GetBadFrames();

		// Get the next frame
		if ( ( m_frame = m_reader->GetFrame() ) == NULL )
			// reader has erred or signaling a stop condition (end of pipe)
			break;

		// Check if the out queue is falling behind
		bool critical_mass = m_reader->GetOutQueueSize( ) > m_reader->GetInQueueSize( );

		// Handle exceptional situations
		if ( dropped > 0 )
		{
			m_dropped_frames += dropped;
			sendFrameDroppedStatus( "buffer underrun near",
				"This error means that the frames could not be written fast enough." );
			
			if ( m_lockstep && m_frame_count > 0 )
			{
				if ( m_writer->FileIsOpen() )
				{
					if ( ( m_lockstep_maxdrops > -1 && dropped > m_lockstep_maxdrops )
					||( m_lockstep_totaldrops > -1 && m_dropped_frames > m_lockstep_totaldrops ) )
					{
						sendEvent( "Warning: closing file early due to too many dropped frames." );
						m_lockPending = true;
					}
					for ( int n = 0; n < dropped; n++ )
						writeFrame();
				}
				else
				{
					m_dropped_frames = 0;
				}
			}
		}
		if ( badFrames > 0 )
		{
			m_bad_frames += badFrames;
			sendFrameDroppedStatus( "damaged frame near",
				"This means that there were missing or invalid FireWire packets." );
		}

		if ( ! m_frame->IsComplete() )
		{
			m_dropped_frames++;
			sendFrameDroppedStatus( "frame dropped",
				"This error means that the ieee1394 driver received an incomplete frame." );

			if ( m_lockstep && m_frame_count > 0 )
			{
				if ( m_writer->FileIsOpen() )
				{
					if ( m_lockstep_totaldrops > -1 && m_dropped_frames > m_lockstep_totaldrops )
					{
						sendEvent( "Warning: closing file early due to too many dropped frames." );
						m_lockPending = true;
					}
					writeFrame();
				}
				else
				{
					m_dropped_frames = 0;
				}
			}
		}
		else
		{
			if ( m_hdv )
			{
				writeFrame();
			}
			else
			{
				DVFrame *dvframe = static_cast<DVFrame*>( m_frame );
				TimeCode timeCode = { 0, 0, 0, 0 };
				dvframe->GetTimeCode( timeCode );
				if ( dvframe->IsNormalSpeed() &&
				     ( m_jpeg_overwrite ||
				       !m_avc ||
				       !m_isRecordMode ||
				       ( m_isRecordMode &&
				         strcmp( avc1394_vcr_decode_status( m_transportStatus ), "Recording" ) == 0 &&
				         !( timeCode.hour == 0 && timeCode.min == 0  && timeCode.sec == 0 && timeCode.frame == 0 )
				       )
				     )
				   )
					writeFrame();
			}

			// drop frame on stdout if getting low on buffers
			if ( !critical_mass && m_raw_pipe )
			{
				fd_set wfds;
				struct timeval tv =
					{
						0, 20000
					};
				FD_ZERO( &wfds );
				FD_SET( fileno( stdout ), &wfds );
				if ( select( fileno( stdout ) + 1, NULL, &wfds, NULL, &tv ) )
				{
					write( fileno( stdout ), m_frame->data, m_frame->GetDataLen() );
				}
			}
		}
		m_reader->DoneWithFrame( m_frame );
	}
	m_reader_active = false;
}


void DVgrab::status( )
{
	char s[ 32 ];
	unsigned int status;
	static unsigned int prevStatus = 0;
	std::string transportStatus( "" );
	std::string timecode( "--:--:--:--" );
	std::string filename( "" );
	std::string duration( "" );

	if ( ! m_avc )
		return ;

	status = m_avc->TransportStatus( m_node );
	if ( ( int ) status >= 0 )
		transportStatus = avc1394_vcr_decode_status( status );
	if ( prevStatus == 0 )
		prevStatus = status;
	if ( status != prevStatus && AVC1394_MASK_RESPONSE_OPERAND( prevStatus, 2 ) == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_WIND )
	{
		quadlet_t resp2 = AVC1394_MASK_RESPONSE_OPERAND( status, 2 );
		quadlet_t resp3 = AVC1394_MASK_RESPONSE_OPERAND( status, 3 );
		if ( resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_WIND && resp3 == AVC1394_VCR_OPERAND_WIND_STOP )
			sendEvent( "Winding Stopped" );
	}
	m_transportStatus = prevStatus = status;

	if ( m_avc->Timecode( m_node, s ) )
		timecode = s;

	if ( m_writer != NULL )
		filename = m_writer->GetFileName();
	else
		filename = "";

	if ( m_frame != NULL && m_writer != NULL )
	{
		sprintf( s, "%8.2f", ( float ) m_writer->GetFramesWritten() / m_frame->GetFrameRate() );
		duration = s;
	}
	else
		duration = "";

	fprintf( stderr, "%-80.80s\r", " " );
	fprintf( stderr, "\"%s\" %s \"%s\" %8s sec\r", transportStatus.c_str(),
	         timecode.c_str(),
	         filename.c_str(),
	         duration.c_str() );
	fflush( stderr );
}

bool DVgrab::execute( const char cmd )
{
	bool result = true;
	switch ( cmd )
	{
	case 'p':
		if ( m_avc )
		{
			m_avc->Play( m_node );
		}
		break;
	case ' ':
		if ( m_avc )
		{
			if ( isPlaying() )
				m_avc->Pause( m_node );
			else
				m_avc->Play( m_node );
		}
		break;
	case 'h':
		if ( m_avc )
		{
			m_avc->Reverse( m_node );
		}
		break;
	case 'j':
		if ( m_avc )
		{
			m_avc->Pause( m_node );
			m_avc->Rewind( m_node );
		}
		break;
	case 'k':
		if ( m_avc )
		{
			m_avc->Pause( m_node );
		}
		break;
	case 'l':
		if ( m_avc )
		{
			m_avc->Pause( m_node );
			m_avc->FastForward( m_node );
		}
		break;
	case 'a':
		if ( m_avc )
		{
			m_avc->Stop( m_node );
			m_avc->Rewind( m_node );
		}
		break;
	case 'z':
		if ( m_avc )
		{
			m_avc->Stop( m_node );
			m_avc->FastForward( m_node );
		}
		break;
	case '1':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, -14 );
		}
		break;
	case '2':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, -11 );
		}
		break;
	case '3':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, -8 );
		}
		break;
	case '4':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, -4 );
		}
		break;
	case '5':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, -1 );
		}
		break;
	case '6':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, 1 );
		}
		break;
	case '7':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, 4 );
		}
		break;
	case '8':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, 8 );
		}
		break;
	case '9':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, 11 );
		}
		break;
	case '0':
		if ( m_avc )
		{
			m_avc->Shuttle( m_node, 14 );
		}
		break;
	case 's':
	case 0x1b:    // Esc
		if ( m_captureActive )
			stopCapture();
		else if ( m_avc )
			m_avc->Stop( m_node );
		break;
	case 'c':
		startCapture();
		break;
	case 'q':
		result = false;
		break;
	case '?':
		cerr << "q=quit, p=play, c=capture, Esc=stop, h=reverse, j=backward scan, k=pause" << endl;
		cerr << "l=forward scan, a=rewind, z=fast forward, 0-9=trickplay, <space>=play/pause" << endl;
		break;
	default:
		//fprintf( stderr, "\nunkown key 0x%2.2x", cmd );
		//result = false;
		break;
	}
	return result;
}

bool DVgrab::isPlaying()
{
	if ( ! m_avc )
		return false;
	quadlet_t resp2 = AVC1394_MASK_RESPONSE_OPERAND( m_transportStatus, 2 );
	quadlet_t resp3 = AVC1394_MASK_RESPONSE_OPERAND( m_transportStatus, 3 );
	return ( ( resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_PLAY && resp3 != AVC1394_VCR_OPERAND_PLAY_FORWARD_PAUSE ) ||
		( resp2 == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_RECORD && resp3 != AVC1394_VCR_OPERAND_RECORD_PAUSE ) );
}

bool DVgrab::done()
{
	if ( m_reader_active )
	{
		// Stop capture at end of tape
		if ( !m_interactive && m_writer && m_writer->GetFileSize() > 0 && m_avc && !m_isRecordMode )
		{
			m_transportStatus = m_avc->TransportStatus( m_node );
			if ( AVC1394_MASK_RESPONSE_OPERAND( m_transportStatus, 3 ) == AVC1394_VCR_OPERAND_WIND_STOP
			     && AVC1394_MASK_OPCODE( m_transportStatus ) == AVC1394_VCR_RESPONSE_TRANSPORT_STATE_WIND )
			return true;
		}
		
		timespec t = {0, 125000000L};
		return ( nanosleep( &t, NULL ) == -1 );
	}
	return true;
}

void DVgrab::cleanup()
{
	stopCapture();
	if ( m_avc && !m_no_stop )
		m_avc->Stop( m_node );
	m_reader_active = false;
	if ( m_reader )
	{
		m_reader->StopThread();
		pthread_join( capture_thread, NULL );
		delete m_reader;
	}
	delete m_avc;
	delete m_connection;
	delete m_timeDuration;
	if (m_dst_file_name)
		free(m_dst_file_name);
}
