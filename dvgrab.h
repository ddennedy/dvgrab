/*
* dvgrab.h -- DVGrab control class
* Copyright (C) 2003-2008 Dan Dennedy <dan@dennedy.org>
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
*
*/


#ifndef _DVGRAB_H
#define _DVGRAB_H 1

#include <string>

#include <libraw1394/raw1394.h>
#include <pthread.h>
#include <getopt.h>

#include "ieee1394io.h"
#include "filehandler.h"
#include "dvframe.h"
#include "hdvframe.h"
#include "smiltime.h"

#include <stdint.h>

#define DEFAULT_FORMAT (RAW_FORMAT)
#define DEFAULT_FORMAT_STR "raw"
#define DEFAULT_FRAMES 0
#define DEFAULT_LOCKSTEP_MAXDROPS -1
#define DEFAULT_LOCKSTEP_TOTALDROPS -1
#define DEFAULT_SIZE 1000
#define DEFAULT_CSIZE 0
#define DEFAULT_CMINCUTSIZE 0
#define DEFAULT_EVERY 1
#define DEFAULT_CHANNEL 63
#define DEFAULT_BUFFERS 100
#define DEFAULT_V4L2_DEVICE "/dev/video"

extern int g_debug;

class DVgrab
{
private:
	/// the interface card to use (typically == 0)
	int	m_port;
	int m_node;

	char *m_program_name;

	int m_showstatus;
	bool m_hdv;
	bool m_reader_active;
	const char *m_input_file_name;
	char *m_dst_file_name;
	int m_autosplit;
	int m_timestamp;
	int m_channel;
	int m_frame_count;
	int m_max_file_size;
	int m_collection_size;
	int m_collection_min_cut_file_size;
	int m_sizesplitmode;
	int m_file_format;
	int m_open_dml;
	int m_frame_every;
	int m_jpeg_quality;
	int m_jpeg_deinterlace;
	int m_jpeg_width;
	int m_jpeg_height;
	std::string m_jpeg_temp;
	int m_jpeg_usetemp;
	int m_jpeg_overwrite;
	int m_dropped_frames;
	int m_bad_frames;
	bool m_interactive;
	int m_buffers;
	int m_total_frames;
	std::string m_duration;
	SMIL::MediaClippingTime* m_timeDuration;
	int m_noavc;
	uint64_t m_guid;
	int m_timesys;
	iec61883Connection* m_connection;
	bool m_raw_pipe;
	int m_no_stop;
	int m_timecode;
	int m_lockstep;
	int m_lockstep_maxdrops;
	int m_lockstep_totaldrops;
	bool m_lockPending;
	TimeCode m_lastTimeCode;
	bool m_isLastTimeCodeSet;
	struct tm m_lastRecDate;
	bool m_isLastRecDateSet;
	bool m_v4l2;
	int m_jvc_p25;
	int m_24p;
	int m_24pa;
	int m_timeSplit;
	int m_srt;
	bool m_isNewFile;
	bool m_isRecordMode;
	int m_isRewindFirst;

	static FileHandler *m_writer;
	bool m_captureActive;

	static pthread_mutex_t capture_mutex;
	static pthread_t capture_thread;
	static pthread_t watchdog_thread;

	AVC *m_avc;
	IEEE1394Reader *m_reader;
	static Frame *m_frame;

	unsigned int m_transportStatus;

	static void *captureThread( void* );
	static void *watchdogThreadProxy( void* );

public:
	DVgrab( int argc, char *argv[] );
	~DVgrab();

	void getargs( int argc, char *argv[] );
	void startCapture();
	void stopCapture();
	void status();
	void watchdogThread();
	void captureThreadRun();
	bool execute( const char );
	bool isPlaying();
	bool isInteractive()
	{
		return m_interactive;
	}
	bool done();
	void testCapture( void );
	static void testCaptureProxy( BusResetHandlerData );

private:
	void sendCaptureStatus( const char *name, float size, int frames, TimeCode *tc, struct tm *rd, bool newline );
	void sendFrameDroppedStatus( const char *reason, const char *meaning );
	void writeFrame();
	void cleanup();

	void print_usage();
	void print_help();
	void print_version();
	void set_file_format( char *format );
	void set_format_from_name( void );
};


#endif
