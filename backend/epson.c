/* 
   epson.c - SANE library for Epson flatbed scanners.

   based on Kazuhiro Sasayama previous
   Work on epson.[ch] file from the SANE package.

   original code taken from sane-0.71
   Copyright (C) 1997 Hypercore Software Design, Ltd.

   modifications
   Copyright (C) 1998-1999 Christian Bucher <bucher@vernetzt.at>
   Copyright (C) 1998-1999 Kling & Hautzinger GmbH
   Copyright (C) 1999 Norihiko Sawa <sawa@yb3.so-net.ne.jp>
   Copyright (C) 1999-2000 Karl Heinz Kremer <khk@khk.net>
   Copyright (C) 2000 Mike Porter <mike@udel.edu> (mjp)

*/

#define	SANE_EPSON_VERSION	"SANE Epson Backend v0.1.33 - 2000-07-29"

/*
   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.  */

/*
   2000-07-28   Changed #include <...> to #include "..." for the
   		sane/... include files.
   2000-07-26   Fixed problem with Perfection610: The variable
   		s->color_shuffle_line was never correctly initialized
   2000-06-28   When closing the scanner device the data that's	
		still in the scanner, waiting to be transferred
		is flushed. This fixes the problem with scanimage -T
   2000-06-13   Invert image when scanning negative with TPU,
                Show film type only when TPU is selected
   2000-06-13   Initialize optical_res to 0 (Dave Hill)
   2000-06-07   Fix in sane_close() - found by Henning Meier-Geinitz 
   2000-06-01	Threshhold should only be active when scan depth
		is 1 and halftoning is off.  (mjp)
   2000-05-28	Turned on scanner based color correction.
                Dependancies between many options are now
                being enforced.  For instance, auto area seg
                (AAS) should only be on when scan depth == 1.
                Added some routines to active and deactivate
                options.  Routines report if option changed.
                Help prevent extraneous option reloads.  Split
                sane_control_option in getvalue and setvalue.
                Further split up setvalue into several different
                routines. (mjp)                         
   2000-05-21   In sane_close use close_scanner instead of just the 
		SCSI close function.
   2000-05-20   ... finally fixed the problem with the 610
                Added resolution_list to Epson_Device structure in
		epson.h - this fixes a bug that caused problems when 
		more than one EPSON scanner was connected.
   2000-05-13   Fixed the color problem with the Perfection 610. The few
		lines with "garbage" at the beginning of the scan are not
		yet removed. 
   2000-05-06   Added support for multiple EPSON scanners. At this time
		this may not be bug free, but it's a start and it seems
		to work well with just one scanner.
   2000-04-06   Did some cleanup on the gamma correction part. The user
		defined table is now initialized to gamma=1, the gamma
		handling is also no longer depending on platform specific
		tables (handled instead by pointers to the actual tables)
   2000-03-27	Disable request for push button status
   2000-03-22   Removed free() calls to static strings to remove
		compile warnings. These were introduced to apparently
		fix an OS/2 bug. It now turned out that they are not
		necessary. The real fix was in the repository for a
		long time (2000-01-25).
   2000-03-19	Fixed problem with A4 level devices - they use the 
		line mode instead of the block mode. The routine to 
		handle this was screwed up pretty bad. Now I have 
		a solid version that handles all variations of line
		mode (automatically deals with the order the color
		lines are sent).
   2000-03-06   Fixed occasional crash after warm up when the "in warmup
		state" went away in between doing ESC G and getting the
		extended status message.  
   2000-03-02	Code cleanup, disabled ZOOM until I have time to 
		deal with all the side effects.
   2000-03-01   More D1 fixes. In the future I have to come up with
		a more elegant solution to destinguish between different
		function levels. The level > n does not work anymore with
		D1. 
		Added support for "set threshold" and "set zoom".
   2000-02-23   First stab at level D1 support, also added a test
		for valid "set halftone" command to enable OPT_HALFTONE
   2000-02-21   Check for "warming up" in after sane_start. This is
		IMHO a horrible hack, but that's the only way without
		a major redesign that will work. (KHK)
   2000-02-20   Added some cleanup on error conditions in attach()
                Use new sanei_config_read() instead of fgets() for
		compatibility with OS/2 (Yuri Dario)
   2000-02-19   Changed some "int" to "size_t" types
		Removed "Preview Resolution"
		Implemented resolution list as WORD_LIST instead of
		a RANGE (KHK)
   2000-02-11   Default scan source is always "Flatbed", regardless
		of installed options. Corrected some typos. (KHK)
   2000-02-03   Gamma curves now coupled with gamma correction menu.
		Only when "User defined" is selected are the curves
		selected. (Dave Hill)
		Renamed "Contrast" to "Gamma Correction" (KHK)
   2000-02-02   "Brown Paper Bag Release" Put the USB fix finally
		into the CVS repository.
   2000-02-01   Fixed problem with USB scanner not being recognized
		because of hte changes to attach a few days ago. (KHK)
   2000-01-29   fixed core dump with xscanimage by moving the gamma
		curves to the standard interface (no longer advanced)
   		Removed pragma pack() from source code to make it 
		easier to compile on non-gcc compilers (KHK)
   2000-01-26   fixed problem with resolution selection when using the
		resolution list in xsane (KHK)
   2000-01-25	moved the section where the device name is assigned 
		in attach. This avoids the core dump of frontend
		applications when no scanner is found (Dave Hill)
   2000-01-24	reorganization of SCSI related "helper" functions
		started support for user defined color correction -
		this is not yet available via the UI (Christian Bucher)
   2000-01-24	Removed C++ style comments '//' (KHK)
*/


#ifdef  _AIX
#	include  <lalloca.h>		/* MUST come first for AIX! */
#endif

#include  "sane/config.h"

#include  <lalloca.h>

#include  <limits.h>
#include  <stdio.h>
#include  <string.h>
#include  <stdlib.h>
#include  <ctype.h>
#include  <fcntl.h>
#include  <unistd.h>
#include  <errno.h>

#include  "sane/sane.h"
#include  "sane/saneopts.h"
#include  "sane/sanei_scsi.h"

/*
 *  NOTE: try to isolate scsi stuff in own section.
 *
 */

#define  TEST_UNIT_READY_COMMAND	0x00
#define  READ_6_COMMAND			0x08
#define  WRITE_6_COMMAND		0x0a
#define  INQUIRY_COMMAND		0x12
#define  TYPE_PROCESSOR			0x03

/*
 *
 *
 */

static SANE_Status inquiry ( int fd, int page_code, void * buf, size_t * buf_size) {
	u_char cmd [ 6];
	int status;

	memset( cmd, 0, 6);
	cmd[ 0] = INQUIRY_COMMAND;
	cmd[ 2] = page_code;
	cmd[ 4] = *buf_size > 255 ? 255 : *buf_size;
	status = sanei_scsi_cmd( fd, cmd, sizeof cmd, buf, buf_size);

	return status;
}

/*
 *
 *
 */

static int scsi_read ( int fd, void * buf, size_t buf_size, SANE_Status * status) {
	u_char cmd [ 6];

	memset( cmd, 0, 6);
	cmd[ 0] = READ_6_COMMAND;
	cmd[ 2] = buf_size >> 16;
	cmd[ 3] = buf_size >> 8;
	cmd[ 4] = buf_size;

	if( SANE_STATUS_GOOD == ( *status = sanei_scsi_cmd( fd, cmd, sizeof( cmd), buf, &buf_size)))
		return buf_size;

	return 0;
}

/*
 *
 *
 */

static int scsi_write ( int fd, const void * buf, size_t buf_size, SANE_Status * status) {
	u_char * cmd;

	cmd = alloca( 6 + buf_size);
	memset( cmd, 0, 6);
	cmd[ 0] = WRITE_6_COMMAND;
	cmd[ 2] = buf_size >> 16;
	cmd[ 3] = buf_size >> 8;
	cmd[ 4] = buf_size;
	memcpy( cmd + 6, buf, buf_size);

	if( SANE_STATUS_GOOD == ( *status = sanei_scsi_cmd( fd, cmd, 6 + buf_size, NULL, NULL)))
		return buf_size;

	return 0;
}

/*
 *
 *
 */

#include  <sane/sanei_pio.h>
#include  "epson.h"

#define  BACKEND_NAME	epson
#include  <sane/sanei_backend.h>

#include  <sane/sanei_config.h>
#define  EPSON_CONFIG_FILE	"epson.conf"

#ifndef  PATH_MAX
#	define  PATH_MAX	(1024)
#endif

#define  walloc(x)	( x *) malloc( sizeof( x) )
#define  walloca(x)	( x *) alloca( sizeof( x) )

#ifndef  XtNumber
#	define  XtNumber(x)  ( sizeof x / sizeof x [ 0] )
#	define  XtOffset(p_type,field)  ((size_t)&(((p_type)NULL)->field))
#	define  XtOffsetOf(s_type,field)  XtOffset(s_type*,field)
#endif

/* NOTE: you can find these codes with "man ascii". */
#define	 STX	0x02
#define	 ACK	0x06
#define	 NAK	0x15
#define	 CAN	0x18
#define	 ESC	0x1B

#define	 S_ACK	"\006"
#define	 S_CAN	"\030"

#define  STATUS_FER		0x80		/* fatal error */
#define  STATUS_AREA_END	0x20		/* area end */
#define  STATUS_OPTION		0x10		/* option installed */

#define  EXT_STATUS_FER		0x80		/* fatal error */
#define  EXT_STATUS_FBF		0x40		/* flat bed scanner */
#define  EXT_STATUS_WU		0x02		/* warming up */
#define  EXT_STATUS_PB		0x01		/* scanner has a push button */

#define  EXT_STATUS_IST		0x80		/* option detected */
#define  EXT_STATUS_EN		0x40		/* option enabled */
#define  EXT_STATUS_ERR		0x20		/* other error */
#define  EXT_STATUS_PE		0x08		/* no paper */
#define  EXT_STATUS_PJ		0x04		/* paper jam */
#define  EXT_STATUS_OPN		0x02		/* cover open */

#define	 EPSON_LEVEL_A1		 0
#define	 EPSON_LEVEL_A2		 1
#define	 EPSON_LEVEL_B1		 2
#define	 EPSON_LEVEL_B2		 3
#define	 EPSON_LEVEL_B3		 4
#define	 EPSON_LEVEL_B4		 5
#define	 EPSON_LEVEL_B5		 6
#define	 EPSON_LEVEL_B6		 7
#define	 EPSON_LEVEL_B7		 8
#define	 EPSON_LEVEL_B8		 9
#define	 EPSON_LEVEL_F5		10
#define  EPSON_LEVEL_D1		11

/* there is also a function level "A5", which I'm igoring here until somebody can 
   convince me that this is still needed. The A5 level was for the GT-300, which
   was (is) a monochrome only scanner. So if somebody really wants to use this
   scanner with SANE get in touch with me and we can work something out - khk */

#define	 EPSON_LEVEL_DEFAULT	EPSON_LEVEL_B3

static EpsonCmdRec epson_cmd [ ] =
{
/*
 *       request identity
 *       |   request identity2
 *       |   |   request status
 *       |   |   |   request condition
 *       |   |   |   |   set color mode
 *       |   |   |   |   |   start scanning
 *       |   |   |   |   |   |   set data format
 *       |   |   |   |   |   |   |   set resolution
 *       |   |   |   |   |   |   |   |   set zoom
 *       |   |   |   |   |   |   |   |   |   set scan area
 *       |   |   |   |   |   |   |   |   |   |   set brightness
 *       |   |   |   |   |   |   |   |   |   |   |            set gamma
 *       |   |   |   |   |   |   |   |   |   |   |            |   set halftoning
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   set color correction
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   initialize scanner
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   set speed
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   set lcount
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   mirror image
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   set gamma table
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   set outline emphasis
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   set dither
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   set color correction coefficients
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   request extension status
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   control an extension
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    forward feed / eject
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   request push button status
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   control auto area segmentation
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   set film type
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   set exposure time
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   |   set bay
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   |   |   set threshold
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   |   |   |
 *       |   |   |   |   |   |   |   |   |   |   |            |   |   |   |   |   |   |   |   |   |   |   |   |    |   |   |   |   |   |   |
 */
  {"A1",'I', 0 ,'F','S', 0 ,'G', 0 ,'R', 0 ,'A', 0 ,{ 0,0,0}, 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,
  {"A2",'I', 0 ,'F','S', 0 ,'G','D','R','H','A','L',{-3,3,0},'Z','B', 0 ,'@', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B1",'I', 0 ,'F','S','C','G','D','R', 0 ,'A', 0 ,{ 0,0,0}, 0 ,'B', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B2",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B', 0 ,'@', 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 ,  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B3",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@', 0 , 0 , 0 , 0 , 0 , 0 ,'m','f','e',  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B4",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@','g','d', 0 ,'z','Q','b','m','f','e',  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B5",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,
  {"B6",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 , 0 , 0 , 0 , 0 , 0 , 0 }
  ,	      	      	      	      	
  {"B7",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-4,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e','\f','!','s','N', 0 , 0 ,'t'}
  ,	      	      	      	      	
  {"B8",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-4,3,0},'Z','B','M','@','g','d','K','z','Q','b','m','f','e',  0 ,'!','s','N', 0 , 0 , 0 }
  ,	      	      	      	      	
  {"F5",'I', 0 ,'F','S','C','G','D','R','H','A','L',{-3,3,0},'Z', 0 ,'M','@','g','d','K','z','Q', 0 ,'m','f','e','\f', 0 , 0 ,'N','T','P', 0 }
  ,
  {"D1",'I','i','F', 0 ,'C','G','D','R', 0 ,'A', 0 ,{ 0,0,0},'Z', 0 , 0 ,'@','g','d', 0 ,'z', 0 , 0 , 0 ,'f', 0 ,  0 ,'!', 0 , 0 , 0 , 0 , 0 }
};



/*

.) exposure time

- no docs found

- from epson_31101999.c

u_char default_tval[4] = {2, 0x80, 0x80, 0x80};
u_char     neg_tval[4] = {2, 0x8e, 0x86, 0x92};

ESC T + 4 byte

- there are defs in include/sane/saneopts.h

#define SANE_NAME_CAL_EXPOS_TIME	"cal-exposure-time"
#define SANE_NAME_CAL_EXPOS_TIME_R	"cal-exposure-time-r"
#define SANE_NAME_CAL_EXPOS_TIME_G	"cal-exposure-time-g"
#define SANE_NAME_CAL_EXPOS_TIME_B	"cal-exposure-time-b"
#define SANE_NAME_SCAN_EXPOS_TIME	"scan-exposure-time"
#define SANE_NAME_SCAN_EXPOS_TIME_R	"scan-exposure-time-r"
#define SANE_NAME_SCAN_EXPOS_TIME_G	"scan-exposure-time-g"
#define SANE_NAME_SCAN_EXPOS_TIME_B	"scan-exposure-time-b"
#define SANE_NAME_SELECT_EXPOSURE_TIME	"select-exposure-time"

Einmal f�r Calibrierung, andere f�r Scan. lamp-density gibt es auch noch.
One for calibration, other one for scan. There is also lamp-density defined.

- da war noch mal die bl�de mail wo ganz genau drinsteht wie das geht.

.) 12 bit

- there are defs in include/sane/saneopts.h

#define SANE_NAME_TEN_BIT_MODE		"ten-bit-mode"
#define SANE_NAME_TWELVE_BIT_MODE	"twelve-bit-mode"

- not used by other backends.




*/

/*
 *
 *
 */

struct mode_param {
	int color;
	int mode_flags;
	int dropout_mask;
	int depth;
};

static const struct mode_param mode_params [ ] =
	{ { 0, 0x00, 0x30,  1}
	, { 0, 0x00, 0x30,  8}
	, { 1, 0x02, 0x00,  8}
	};

static const struct mode_param mode_params_5 [ ] =
	{ { 0, 0x00, 0x30,  1}
	, { 0, 0x00, 0x30,  8}
	, { 1, 0x03, 0x10,  8}
	};

static const SANE_String_Const mode_list [ ] =
	{ "Binary"
	, "Gray"
	, "Color"
	, NULL
	};

static const SANE_String_Const mode_list_5 [ ] =
	{ "Binary"
	, "Gray"
	, "Color"
	, NULL
	};

#define  FBF_STR	"Flatbed"
#define  TPU_STR	"Transparency Unit"
#define  ADF_STR	"Automatic Document Feeder"

/*
 * source list need one dummy entry (save device settings is crashing).
 * NOTE: no const.
 */

static SANE_String_Const source_list [ ] =
	{ FBF_STR
	, NULL
	, NULL
	, NULL
	};

#define FILM_TYPE_POSITIVE	(0)
#define FILM_TYPE_NEGATIVE	(1)

static const SANE_String_Const film_list [ ] =
	{ "Positive Film"
	, "Negative Film"
	, NULL
	};

/*
 * TODO: add some missing const.
 */

#define HALFTONE_NONE 0x01
#define HALFTONE_TET 0x03

static int halftone_params [ ] =
	{ HALFTONE_NONE
	, 0x00
	, 0x10
	, 0x20
	, 0x80
	, 0x90
	, 0xa0
	, 0xb0
	, HALFTONE_TET
	, 0xc0
	, 0xd0
	};

static const SANE_String_Const halftone_list [ ] =
	{ "None"
	, "Halftone A (Hard Tone)"
	, "Halftone B (Soft Tone)"
	, "Halftone C (Net Screen)"
	, NULL
	};

static const SANE_String_Const halftone_list_4 [ ] =
	{ "None"
	, "Halftone A (Hard Tone)"
	, "Halftone B (Soft Tone)"
	, "Halftone C (Net Screen)"
	, "Dither A (4x4 Bayer)"
	, "Dither B (4x4 Spiral)"
	, "Dither C (4x4 Net Screen)"
	, "Dither D (8x4 Net Screen)"
	, NULL
	};

static const SANE_String_Const halftone_list_7 [ ] =
	{ "None"
	, "Halftone A (Hard Tone)"
	, "Halftone B (Soft Tone)"
	, "Halftone C (Net Screen)"
	, "Dither A (4x4 Bayer)"
	, "Dither B (4x4 Spiral)"
	, "Dither C (4x4 Net Screen)"
	, "Dither D (8x4 Net Screen)"
	, "Text Enhanced Technology"
	, "Download pattern A"
	, "Download pattern B"
	, NULL
	};

static int dropout_params [ ] =
	{ 0x00
	, 0x10
	, 0x20
	, 0x30
	};

static const SANE_String_Const dropout_list [ ] =
	{ "None"
	, "Red"
	, "Green"
	, "Blue"
	, NULL
	};

/*
 * NOTE: if enable "User defined" change also default from 4 to 5.
 */

static int color_params [ ] =
	{ 0x00
	, 0x01
	, 0x10
	, 0x20
	, 0x40
	, 0x80
	};

static SANE_Bool color_userdefined [] =
	{ SANE_FALSE
	, SANE_TRUE
	, SANE_FALSE
	, SANE_FALSE
	, SANE_FALSE
	, SANE_FALSE
	};

static const SANE_String_Const color_list [ ] =
	{ "No Correction"
	, "User defined"
	, "Impact-dot printers"
	, "Thermal printers"
	, "Ink-jet printers"
	, "CRT monitors"
	, NULL
	};

static int gamma_params_ab [ ] =
	{ 0x01
	, 0x03
	, 0x00
	, 0x10
	, 0x20
	};

static const SANE_String_Const gamma_list_ab [ ] =
	{ "Default"
	, "User defined"
	, "High density printing"
	, "Low density printing"
	, "High contrast printing"
	, NULL
	};

static SANE_Bool gamma_userdefined_ab [ ] =
	{
	  SANE_FALSE,
	  SANE_TRUE,
	  SANE_FALSE,
	  SANE_FALSE,
	  SANE_FALSE,
	};

static int gamma_params_d [ ] =
	{ 0x03
	, 0x04
	};

static const SANE_String_Const gamma_list_d [ ] =
	{ "User defined (Gamma=1.0)"
	, "User defined (Gamma=1.8)"
	, NULL
	};

static SANE_Bool gamma_userdefined_d [ ] =
	{
	  SANE_TRUE,
	  SANE_TRUE
	};

static SANE_Bool * gamma_userdefined;
static int * gamma_params;

static const SANE_String_Const bay_list [ ] =
	{ " 1 "
	, " 2 "
	, " 3 "
	, " 4 "
	, " 5 "
	, " 6 "
	, NULL
	};

/*
 *  minimum, maximum, quantization.
 */

static const SANE_Range u8_range = { 0, 255, 0};
static const SANE_Range s8_range = { -127, 127, 0};
static const SANE_Range zoom_range = { 50, 200, 0};

static int mirror_params [ ] =
	{ 0
	, 1
	};

#define  speed_params	mirror_params
#define  film_params	mirror_params

static const SANE_Range outline_emphasis_range = { -2, 2, 0 };
/* static const SANE_Range gamma_range = { -2, 2, 0 }; */

struct qf_param {
	SANE_Word tl_x;
	SANE_Word tl_y;
	SANE_Word br_x;
	SANE_Word br_y;
};

/* gcc don't like to overwrite const field */
static /*const*/ struct qf_param qf_params [ ] =
	{ { 0, 0, SANE_FIX( 120.0), SANE_FIX( 120.0) }
	, { 0, 0, SANE_FIX( 148.5), SANE_FIX( 210.0) }
	, { 0, 0, SANE_FIX( 210.0), SANE_FIX( 148.5) }
	, { 0, 0, SANE_FIX( 215.9), SANE_FIX( 279.4) }		/* 8.5" x 11" */
	, { 0, 0, SANE_FIX( 210.0), SANE_FIX( 297.0) }
	, { 0, 0, 0, 0}
	};

static const SANE_String_Const qf_list [ ] =
	{ "CD"
	, "A5 portrait"
	, "A5 landscape"
	, "Letter"
	, "A4"
	, "max"
	, NULL
	};

/* 
 * this is now stored in the s->hw->resolution_list field

static SANE_Word * resolution_list = NULL;

 */

/*
 *
 *
 */

static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  for (i = 0; strings[i]; i++)
    {
      size = strlen(strings[i]) + 1;
      if (size > max_size)
        max_size = size;
    }
  return max_size;
}

typedef struct {
	u_char	code;
	u_char	status;
	u_short	count;
	u_char	buf [ 1];

} EpsonHdrRec, * EpsonHdr;

typedef struct {
	u_char	code;
	u_char	status;
	u_short	count;

	u_char	type;
	u_char	level;

	u_char	buf [ 1];

} EpsonIdentRec, * EpsonIdent;


typedef struct {
	u_char	code;
	u_char	status;
	u_short	count;

	u_char	buf [ 1];

} EpsonParameterRec, * EpsonParameter;

typedef struct {
	u_char	code;
	u_char	status;

	u_char	buf [ 4];

} EpsonDataRec, * EpsonData;

/*
 *
 *
 */

static EpsonHdr command ( Epson_Scanner * s, const u_char * cmd, size_t cmd_size, SANE_Status * status);
static SANE_Status get_identity_information(SANE_Handle handle);
static SANE_Status get_identity2_information(SANE_Handle handle);
static int send ( Epson_Scanner * s, const void *buf, size_t buf_size, SANE_Status * status);
static ssize_t receive ( Epson_Scanner * s, void *buf, ssize_t buf_size, SANE_Status * status);
static SANE_Status color_shuffle(SANE_Handle handle, int *new_length);

/*
 *
 *
 */

static int send ( Epson_Scanner * s, const void *buf, size_t buf_size, SANE_Status * status) {

	DBG( 3, "send buf, size = %lu\n", ( u_long) buf_size);

#if 1
	{
		size_t k;
		const u_char * s = buf;

		for( k = 0; k < buf_size; k++) {
			DBG( 125, "buf[%u] %02x %c\n", k, s[ k], isprint( s[ k]) ? s[ k] : '.');
		}
	}
#endif

	if( s->hw->connection == SANE_EPSON_SCSI) {
		return scsi_write( s->fd, buf, buf_size, status);
	} else if (s->hw->connection == SANE_EPSON_PIO) {
		size_t n;

		if( buf_size == ( n = sanei_pio_write( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;

		return n;
	} else if (s->hw->connection == SANE_EPSON_USB) {
		size_t n;

		if( buf_size == ( n = write( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;

		return n;
	}

	return SANE_STATUS_INVAL;
	/* never reached */
}

/*
 *
 *
 */

static ssize_t receive ( Epson_Scanner * s, void *buf, ssize_t buf_size, SANE_Status * status) {
	ssize_t n = 0;

	if( s->hw->connection == SANE_EPSON_SCSI) 
	{
		n = scsi_read( s->fd, buf, buf_size, status);
	} 
        else if ( s->hw->connection == SANE_EPSON_PIO) 
	{
		if( buf_size == ( n = sanei_pio_read( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
			*status = SANE_STATUS_INVAL;
	}
	else if (s->hw->connection == SANE_EPSON_USB) 
	{
		/* only report an error if we don't read anything */
		if( 0 < ( n = read( s->fd, buf, buf_size)))
			*status = SANE_STATUS_GOOD;
		else
		{
			if (n < 0)
			{
				DBG(0, "error in receive - status = %d\n", errno);
			}

			*status = SANE_STATUS_INVAL;
		}
	}

	DBG( 7, "receive buf, expected = %lu, got = %d\n", ( u_long) buf_size, n);

#if 1
	if (n > 0)
	{
		ssize_t k;
		const u_char * s = buf;

		for( k = 0; k < n; k++) {
			DBG( 127, "buf[%u] %02x %c\n", k, s[ k], isprint( s[ k]) ? s[ k] : '.');
	 	}
	}
#endif

	return n;
}

/*
 *
 *
 */

static SANE_Status expect_ack ( Epson_Scanner * s) {
	u_char result [ 1];
	size_t len;
	SANE_Status status;

	len = sizeof result;

	receive( s, result, len, &status);

	if( SANE_STATUS_GOOD != status)
		return status;

	if( ACK != result[ 0])
		return SANE_STATUS_INVAL;

	return SANE_STATUS_GOOD;
}

/*
 *
 *
 */

static SANE_Status set_cmd ( Epson_Scanner * s, u_char cmd, int val) {
	SANE_Status status;
	u_char params [ 2];

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = ESC;
	params[ 1] = cmd;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	params[ 0] = val;
	send( s, params, 1, &status);
	status = expect_ack( s);

	return status;
}

/*
 *
 *
 */

#define  set_color_mode(s,v)		set_cmd(s,(s)->hw->cmd->set_color_mode,v)
#define  set_data_format(s,v)		set_cmd( s,(s)->hw->cmd->set_data_format, v)
#define  set_halftoning(s,v)		set_cmd( s,(s)->hw->cmd->set_halftoning, v)
#define  set_gamma(s,v)			set_cmd( s,(s)->hw->cmd->set_gamma, v)
#define  set_color_correction(s,v)	set_cmd( s,(s)->hw->cmd->set_color_correction, v)
#define  set_lcount(s,v)		set_cmd( s,(s)->hw->cmd->set_lcount, v)
#define  set_bright(s,v)		set_cmd( s,(s)->hw->cmd->set_bright, v)
#define  mirror_image(s,v)		set_cmd( s,(s)->hw->cmd->mirror_image, v)
#define  set_speed(s,v)			set_cmd( s,(s)->hw->cmd->set_speed, v)
#define  set_outline_emphasis(s,v)	set_cmd( s,(s)->hw->cmd->set_outline_emphasis, v)
#define  control_auto_area_segmentation(s,v)	set_cmd( s,(s)->hw->cmd->control_auto_area_segmentation, v)
#define  set_film_type(s,v)		set_cmd( s,(s)->hw->cmd->set_film_type, v)
#define  set_exposure_time(s,v)		set_cmd( s,(s)->hw->cmd->set_exposure_time, v)
#define  set_bay(s,v)			set_cmd( s,(s)->hw->cmd->set_bay, v)
#define  set_threshold(s,v)		set_cmd( s,(s)->hw->cmd->set_threshold, v)

/*#define  (s,v)		set_cmd( s,(s)->hw->cmd->, v) */

static SANE_Status set_zoom ( Epson_Scanner * s, int x_zoom, int y_zoom) 
{
	SANE_Status status;
	u_char cmd[2];
	u_char params[2];

	if( ! s->hw->cmd->set_zoom)
		return SANE_STATUS_GOOD;

	cmd[0] = ESC;
	cmd[1] = s->hw->cmd->set_zoom;

	send( s, cmd, 2, &status);
	status = expect_ack( s);

	if( status != SANE_STATUS_GOOD)
		return status;

	params[ 0] = x_zoom;
	params[ 1] = y_zoom;

	send( s, params, 2, &status);
	status = expect_ack( s);

	return status;
}


static SANE_Status set_resolution ( Epson_Scanner * s, int xres, int yres) {
	SANE_Status status;
	u_char params[4];

	if( ! s->hw->cmd->set_resolution)
		return SANE_STATUS_GOOD;

	params[0] = ESC;
	params[1] = s->hw->cmd->set_resolution;

	send( s, params, 2, &status);
	status = expect_ack( s);

	if( status != SANE_STATUS_GOOD)
		return status;

	params[ 0] = xres;
	params[ 1] = xres >> 8;
	params[ 2] = yres;
	params[ 3] = yres >> 8;

	send( s, params, 4, &status);
	status = expect_ack( s);

	return status;
}

/*
 *
 *
 */

static SANE_Status set_scan_area ( Epson_Scanner * s, int x, int y, int width, int height) {
	SANE_Status status;
	u_char params[ 8];

	if( ! s->hw->cmd->set_scan_area) {
		return SANE_STATUS_GOOD;
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->set_scan_area;

	send( s, params, 2, &status);
	status = expect_ack( s);
	if( status != SANE_STATUS_GOOD)
		return status;

	params[ 0] = x;
	params[ 1] = x >> 8;
	params[ 2] = y;
	params[ 3] = y >> 8;
	params[ 4] = width;
	params[ 5] = width >> 8;
	params[ 6] = height;
	params[ 7] = height >> 8;

	DBG( 1, "%p %d %d %d %d\n", ( void *) s, x, y, width, height);

	send( s, params, 8, &status);
	status = expect_ack( s);

	return status;
}

/*
 *
 *
 */

static SANE_Status set_color_correction_coefficients ( Epson_Scanner * s) {
	SANE_Status status;
	u_char cmd = s->hw->cmd->set_color_correction_coefficients;
	u_char params [ 2];
	const int length = 9;
	signed char cct [ 9];

	DBG( 1, "set_color_correction_coefficients: starting.\n" );
	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = ESC;
	params[ 1] = cmd;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	cct[ 0] = s->val[ OPT_CCT_1].w;
	cct[ 1] = s->val[ OPT_CCT_2].w;
	cct[ 2] = s->val[ OPT_CCT_3].w;
	cct[ 3] = s->val[ OPT_CCT_4].w;
	cct[ 4] = s->val[ OPT_CCT_5].w;
	cct[ 5] = s->val[ OPT_CCT_6].w;
	cct[ 6] = s->val[ OPT_CCT_7].w;
	cct[ 7] = s->val[ OPT_CCT_8].w;
	cct[ 8] = s->val[ OPT_CCT_9].w;

	DBG( 1, "set_color_correction_coefficients: %d,%d,%d %d,%d,%d %d,%d,%d.\n",
		cct[0], cct[1], cct[2], cct[3],
		cct[4], cct[5], cct[6], cct[7], cct[8] );

	send( s, cct, length, &status);
	status = expect_ack( s);
	DBG( 1, "set_color_correction_coefficients: ending=%d.\n", status );

	return status;
}

/*
 *
 *
 */

static SANE_Status set_gamma_table ( Epson_Scanner * s) {
	SANE_Status status;
	u_char cmd = s->hw->cmd->set_gamma_table;
	u_char params [ 2];
	const int length = 257;
	u_char gamma [ 257];
	int n;

	DBG( 1, "set_gamma_table: starting.\n" );
	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = ESC;
	params[ 1] = cmd;

/*
	Print the gamma tables before sending them to the scanner.
*/

	if (DBG_LEVEL > 0) {
		int	c, i, j;

		DBG (1, "set_gamma_table()\n");
		for (c=0; c<4; c++) {
			for (i=0; i<256; i+= 16) {
				DBG (1, "Gamma Table[%d][%d] ", c, i);
				for (j=0; j<16; j++) {
					DBG (1, " %02x", s->gamma_table[c][i+j]);
				}
				DBG (1, "\n");
			}
		}
	}


/*
 * TODO: &status in send make no sense like that.
 */

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'm';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 0] [ n];
	}

	send( s, gamma, length, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'r';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 1] [ n];
	}

	send( s, gamma, length, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'g';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 2] [ n];
	}

	send( s, gamma, length, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	send( s, params, 2, &status);
	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) )
		return status;

	gamma[ 0] = 'b';

	for( n = 0; n < 256; ++n) {
		gamma[ n + 1] = s->gamma_table[ 3] [ n];
	}

	send( s, gamma, length, &status);
	status = expect_ack( s);
	DBG( 1, "set_gamma_table: complete = %d.\n", status );

	return status;
}

/*
 *
 *
 */

static SANE_Status check_ext_status ( Epson_Scanner * s) {
	SANE_Status status;
	u_char cmd = s->hw->cmd->request_extension_status;
	u_char params [ 2];
	u_char * buf;
	EpsonHdr head;

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	params[ 0] = ESC;
	params[ 1] = cmd;

	if( NULL == ( head = ( EpsonHdr) command( s, params, 2, &status) ) ) {
		DBG( 0, "Extended status flag request failed\n");
		return status;
	}

	buf = &head->buf[ 0];

	if( buf[ 0] & EXT_STATUS_WU) {
		DBG( 10, "option: warming up\n");
		status = SANE_STATUS_DEVICE_BUSY;
	}

	if( buf[ 0] & EXT_STATUS_FER) {
		DBG( 0, "option: fatal error\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_ERR) {
		DBG( 0, "ADF: other error\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_PE) {
		DBG( 0, "ADF: no paper\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_PJ) {
		DBG( 0, "ADF: paper jam\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 1] & EXT_STATUS_OPN) {
		DBG( 0, "ADF: cover open\n");
		status = SANE_STATUS_INVAL;
	}

	if( buf[ 6] & EXT_STATUS_ERR) {
		DBG( 0, "TPU: other error\n");
		status = SANE_STATUS_INVAL;
	}

	return status;
}

/*
 *
 *
 */

static SANE_Status reset ( Epson_Scanner * s) {
	SANE_Status status;
	u_char param[2];

	if( ! s->hw->cmd->initialize_scanner)
		return SANE_STATUS_GOOD;

	param[0] = ESC;
	param[1] = s->hw->cmd->initialize_scanner;

	send (s, param, 2, &status);
	status = expect_ack( s);
	return status;
}

/*
 *
 *
 */

static void close_scanner ( Epson_Scanner * s) {

	if( s->hw->connection == SANE_EPSON_SCSI)
		sanei_scsi_close( s->fd);
	else if ( s->hw->connection == SANE_EPSON_PIO) 
		sanei_pio_close( s->fd);
	else if ( s->hw->connection == SANE_EPSON_USB)
		close( s->fd);

	return;
}

/*
 *
 *
 */

static SANE_Status open_scanner ( Epson_Scanner * s) {
	SANE_Status status = 0;

	DBG(5, "open_scanner()\n");

	if( s->hw->connection == SANE_EPSON_SCSI) {
		if( SANE_STATUS_GOOD != ( status = sanei_scsi_open( s->hw->sane.name, &s->fd, NULL, NULL))) {
			DBG( 1, "sane_start: %s open failed: %s\n", s->hw->sane.name, sane_strstatus( status));
			return status;
		}
	} else if ( s->hw->connection == SANE_EPSON_PIO) {
		if( SANE_STATUS_GOOD != ( status = sanei_pio_open( s->hw->sane.name, &s->fd))) {
			DBG( 1, "sane_start: %s open failed: %s\n", s->hw->sane.name, sane_strstatus( status));
			return status;
		}
	} else if (s->hw->connection == SANE_EPSON_USB) {
		int flags;

#ifdef _O_RDWR
 flags = _O_RDWR;
#else
 flags = O_RDWR;
#endif
#ifdef _O_EXCL
 flags |= _O_EXCL;
#else
 flags |= O_EXCL;
#endif
#ifdef _O_BINARY
 flags |= _O_BINARY;
#endif
#ifdef O_BINARY
 flags |= O_BINARY;
#endif

		s->fd = open(s->hw->sane.name, flags);
		if (s->fd < 0) {
			DBG( 1, "sane_start: %s open failed: %s\n", 
				s->hw->sane.name, strerror(errno));
			status = (errno == EACCES) ? SANE_STATUS_ACCESS_DENIED 
				: SANE_STATUS_INVAL;
			return status;
		}
		
	}

	return status;
}

/*
 *
 *
 */

static SANE_Status eject ( Epson_Scanner * s) {
	SANE_Status status;
	u_char params [ 2];
	u_char cmd = s->hw->cmd->eject;

	DBG(5, "eject()\n");

	if( ! cmd)
		return SANE_STATUS_UNSUPPORTED;

	if( SANE_STATUS_GOOD != ( status = open_scanner( s)))
		return status;

	params[ 0] = cmd;

	send( s, params, 1, &status);

	if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) ) {
		close_scanner( s);
		return status;
	}

	close_scanner( s);
	return status;
}

/*
 *
 *
 */

#ifdef OBSOLETE
static Epson_Device dummy_dev =
{ { NULL, "Epson", NULL, "flatbed scanner" }
};
#endif

static int num_devices= 0;		/* number of EPSON scanners attached to backend */
static Epson_Device *first_dev = NULL;	/* first EPSON scanner in list */
static Epson_Scanner *first_handle = NULL;


static EpsonHdr command ( Epson_Scanner * s, const u_char * cmd, 
      size_t cmd_size, SANE_Status * status) 
{
	EpsonHdr head;
	u_char * buf;

	if( NULL == ( head = walloc( EpsonHdrRec))) {
		*status = SANE_STATUS_NO_MEM;
		return ( EpsonHdr) 0;
	}

	send( s, cmd, cmd_size, status);

	if( SANE_STATUS_GOOD != *status)
		return ( EpsonHdr) 0;

	buf = ( u_char *) head;

	if( s->hw->connection == SANE_EPSON_SCSI) 
	{
		receive( s, buf, 4, status);
		buf += 4;
	}
	else if (s->hw->connection == SANE_EPSON_USB)
        {
                int     bytes_read;
                bytes_read = receive( s, buf, 4, status);
                buf += bytes_read;
	} 
	else 
	{
		receive( s, buf, 1, status);
		buf += 1;
	}

	if( SANE_STATUS_GOOD != *status)
		return ( EpsonHdr) 0;

	DBG( 4, "code   %02x\n", (int) head->code);

	switch (head->code) 
        {
	  default:
            if( 0 == head->code)
	      DBG( 1, "Incompatible printer port (probably bi/directional)\n");
            else if( cmd[cmd_size - 1] == head->code)
	      DBG( 1, "Incompatible printer port (probably not bi/directional)\n");

            DBG( 2, "Illegal response of scanner for command: %02x\n", head->code);
            break;

          case NAK:
		/* fall through */
          case ACK:
            break;	/* no need to read any more data after ACK or NAK */

          case STX:
	    if(  s->hw->connection == SANE_EPSON_SCSI) 
	    {
		/* nope */
	    } 
	    else if (s->hw->connection == SANE_EPSON_USB)
            {
                /* we've already read the complete data */
            } 
	    else
            {
		receive (s, buf, 3, status);
/*		buf += 3; */
	    }

            if( SANE_STATUS_GOOD != *status)
	      return (EpsonHdr) 0;

	    DBG( 4, "status %02x\n", (int) head->status);
	    DBG( 4, "count  %d\n", (int) head->count);

            if( NULL == (head = realloc (head, sizeof (EpsonHdrRec) + head->count)))
	    {
	      *status = SANE_STATUS_NO_MEM;
	      return (EpsonHdr) 0;
	    }

            buf = head->buf;
            receive (s, buf, head->count, status);

            if( SANE_STATUS_GOOD != *status)
	      return (EpsonHdr) 0;

            break;
        }

        return head;
}


/*
 * static SANE_Status attach()
 *
 * Attach one device with name *dev_name to the backend.
 */

static SANE_Status attach ( const char * dev_name, Epson_Device * * devp) {
	SANE_Status status;
	Epson_Scanner * s = walloca( Epson_Scanner);
	char * str;
	struct Epson_Device * dev;

	DBG(1, "%s\n", SANE_EPSON_VERSION);

	DBG(5, "attach(%s)\n", dev_name);

	for (dev = first_dev; dev; dev = dev->next)
	{
		if (strcmp(dev->sane.name, dev_name) == 0)
		{
			if (devp)
			{
				*devp = dev;
			}
			return SANE_STATUS_GOOD;
		}
	}

	dev = malloc(sizeof(*dev));
	if (!dev)
     	 {
	   return SANE_STATUS_NO_MEM;
	 }

	

/*
 *  set dummy values.
 */

	s->hw = dev;		
	s->hw->sane.name = NULL;
	s->hw->sane.type = "flatbed scanner";
	s->hw->sane.vendor = "Epson";
	s->hw->sane.model = NULL;
	s->hw->optical_res = 0;		/* just to have it initialized */
	s->hw->color_shuffle = SANE_FALSE;
	s->hw->extension = SANE_FALSE;
	s->hw->use_extension = SANE_FALSE;
	
	s->hw->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];	/* use default function level */
        s->hw->connection = SANE_EPSON_NODEV;		/* no device configured yet */

	DBG( 3, "attach: opening %s\n", dev_name);

	s->hw->last_res = s->hw->last_res_preview = 0;	/* set resolution to safe values */

/*
 *  decide if interface is USB, SCSI or parallel.
 */

	/*
	 * if the config file contains a line "usb /dev/usbscanner", then handle this
 	 * here and use the USB device from now on.
	 */
	if (strncmp(dev_name, SANE_EPSON_CONFIG_USB, strlen(SANE_EPSON_CONFIG_USB)) == 0) {
		/* we have a match for the USB string and adjust the device name */
		dev_name += strlen(SANE_EPSON_CONFIG_USB);
		dev_name  = sanei_config_skip_whitespace(dev_name);
		s->hw->connection = SANE_EPSON_USB;
	}
	/*
	 * if the config file contains a line "pio 0xXXX", then handle this case here
	 * and use the PIO (parallel interface) device from now on.
	 */
	else if (strncmp(dev_name, SANE_EPSON_CONFIG_PIO, strlen(SANE_EPSON_CONFIG_PIO)) == 0) {
		/* we have a match for the PIO string and adjust the device name */
		dev_name += strlen(SANE_EPSON_CONFIG_PIO);
		dev_name  = sanei_config_skip_whitespace(dev_name);
		s->hw->connection = SANE_EPSON_PIO;
	}
	else {		/* legacy mode */
		char * end;

		strtol( dev_name, &end, 0);

		if( ( end == dev_name) || *end) {
			s->hw->connection = SANE_EPSON_SCSI;
		} else {
			s->hw->connection = SANE_EPSON_PIO;
	      	}
	}

	if (s->hw->connection == SANE_EPSON_NODEV) 
	{
		/* 
		   With the current code this can neve happen, because 
		   the routine to handle the legacy mode will always 
		   return a SCSI device. If this gets changed however,
		   here is the test to return with an error code.
		*/
		return SANE_STATUS_INVAL;
	}

/*
 *  if interface is SCSI do an inquiry.
 */

	if( s->hw->connection == SANE_EPSON_SCSI) {
#define  INQUIRY_BUF_SIZE	36
		u_char buf[ INQUIRY_BUF_SIZE + 1];
		size_t buf_size = INQUIRY_BUF_SIZE;

		if( SANE_STATUS_GOOD != ( status = sanei_scsi_open( dev_name, &s->fd, NULL, NULL))) {
			DBG( 1, "attach: open failed: %s\n", sane_strstatus( status));
			return status;
		}

		DBG( 3, "attach: sending INQUIRY\n");
/*		buf_size = sizeof buf; */

		if( SANE_STATUS_GOOD != ( status = inquiry( s->fd, 0, buf, &buf_size))) {
			DBG( 1, "attach: inquiry failed: %s\n", sane_strstatus( status));
			close_scanner( s);
			return status;
		}

		buf[ INQUIRY_BUF_SIZE] = 0;
		DBG( 1, ">%s<\n", buf + 8);

		/* 
		 * For USB and PIO scanners this will be done later, once
		 * we have communication established with the device.
		 */

		if( buf[ 0] != TYPE_PROCESSOR
			|| strncmp( buf + 8, "EPSON", 5) != 0
			|| (strncmp( buf + 16, "SCANNER ", 8) != 0
				&& strncmp( buf + 14, "SCANNER ", 8) != 0
				&& strncmp( buf + 14, "Perfection", 10) != 0
				&& strncmp( buf + 16, "Perfection", 10) != 0
				&& strncmp( buf + 16, "Expression", 10) != 0))
		{
			DBG( 1, "attach: device doesn't look like an Epson scanner\n");
			close_scanner( s);
			return SANE_STATUS_INVAL;
		}

/*
 *  else parallel or USB.
 */

	} else if (s->hw->connection == SANE_EPSON_PIO) {
		if( SANE_STATUS_GOOD != ( status = sanei_pio_open( dev_name, &s->fd))) {
			DBG( 1, "dev_open: %s: can't open %s as a parallel-port device\n",
				sane_strstatus( status), dev_name);
			return status;
		}
	} else if (s->hw->connection == SANE_EPSON_USB) {
		int flags;

#ifdef _O_RDWR
		flags = _O_RDWR;
#else
		flags = O_RDWR;
#endif
#ifdef _O_EXCL
		flags |= _O_EXCL;
#else
		flags |= O_EXCL;
#endif
#ifdef _O_BINARY
		flags |= _O_BINARY;
#endif
#ifdef O_BINARY
		flags |= O_BINARY;
#endif

		s->fd = open(dev_name, flags);
		if (s->fd < 0) {
			DBG( 1, "sane_start: %s open (USB) failed: %s\n", 
				dev_name, strerror(errno));
			status = (errno == EACCES) ? SANE_STATUS_ACCESS_DENIED 
				: SANE_STATUS_INVAL;
			return status;
		}
	}

/*
 * Initialize (ESC @).
 */
/* NOTE: disabled cause of batch use of scanimage with ADF. */

#if 1
	{
		void	* ptr;
		u_char	param[2];

		param[0] = ESC;
		param[1] = s->hw->cmd->initialize_scanner;

		ptr = command( s, param, 2, &status);

		free(ptr);
	}
#endif
/*
 *  Identification Request (ESC I).
 */
	if (s->hw->cmd->request_identity != 0)
	{
		status =  get_identity_information(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}	/* request identity */


	/*
	 * Check for "Request Identity 2" command. If this command is available
	 * get the information from the scanner and store it in dev
	 */

	if (s->hw->cmd->request_identity2 != 0)
	{
		 get_identity2_information(s);
		if (status != SANE_STATUS_GOOD)
			return status;
	}	/* request identity 2 */

		

/*
 *  Set defaults for no extension.
 */

	dev->x_range = &dev->fbf_x_range;
	dev->y_range = &dev->fbf_y_range;

/*
 *  Extended status flag request (ESC f).
 *    this also requests the scanner device name from the the scanner
 */
#if 0
	if( SANE_TRUE == dev->extension) 
#endif
	/*
	 * because we are also using the device name from this command, 
	 * we have to run this block even if the scanner does not report
	 * an extension. The extensions are only reported if the ADF or
	 * the TPU are actually detected. 
	 */
	{
		u_char * buf;
		u_char params[2];
		EpsonHdr head;
		SANE_String_Const * source_list_add = source_list;

		params[0] = ESC;
		params[1] = s->hw->cmd->request_extension_status;

		if( NULL == ( head = ( EpsonHdr) command( s, params, 2, &status) ) ) {
			DBG( 0, "Extended status flag request failed\n");
			return status;
		}

		buf = &head->buf[ 0];

/*
 *  FBF
 */

		*source_list_add++ = FBF_STR;

/*
 *  ADF
 */

		if( buf[ 1] & EXT_STATUS_IST) {
			DBG( 1, "ADF detected\n");

			if( buf[ 1] & EXT_STATUS_EN) {
				DBG( 1, "ADF is enabled\n");
				dev->x_range = &dev->adf_x_range;
				dev->y_range = &dev->adf_y_range;
			}

			dev->adf_x_range.min = 0;
			dev->adf_x_range.max = SANE_FIX( ( buf[  3] << 8 | buf[ 2]) * 25.4 / dev->dpi_range.max);
			dev->adf_x_range.quant = 0;

			dev->adf_y_range.min = 0;
			dev->adf_y_range.max = SANE_FIX( ( buf[  5] << 8 | buf[ 4]) * 25.4 / dev->dpi_range.max);
			dev->adf_y_range.quant = 0;

			DBG( 5, "adf tlx %f tly %f brx %f bry %f [mm]\n"
				, SANE_UNFIX( dev->adf_x_range.min)
				, SANE_UNFIX( dev->adf_y_range.min)
				, SANE_UNFIX( dev->adf_x_range.max)
				, SANE_UNFIX( dev->adf_y_range.max)
				);

			*source_list_add++ = ADF_STR;

			dev->ADF = SANE_TRUE;
		}


/*
 *  TPU
 */

		if( buf[ 6] & EXT_STATUS_IST) {
			DBG( 1, "TPU detected\n");

			if( buf[ 6] & EXT_STATUS_EN) {
				DBG( 1, "TPU is enabled\n");
				dev->x_range = &dev->tpu_x_range;
				dev->y_range = &dev->tpu_y_range;
			}

			dev->tpu_x_range.min = 0;
			dev->tpu_x_range.max = SANE_FIX( ( buf[  8] << 8 | buf[ 7]) * 25.4 / dev->dpi_range.max);
			dev->tpu_x_range.quant = 0;

			dev->tpu_y_range.min = 0;
			dev->tpu_y_range.max = SANE_FIX( ( buf[ 10] << 8 | buf[ 9]) * 25.4 / dev->dpi_range.max);
			dev->tpu_y_range.quant = 0;

			DBG( 5, "tpu tlx %f tly %f brx %f bry %f [mm]\n"
				, SANE_UNFIX( dev->tpu_x_range.min)
				, SANE_UNFIX( dev->tpu_y_range.min)
				, SANE_UNFIX( dev->tpu_x_range.max)
				, SANE_UNFIX( dev->tpu_y_range.max)
				);

			*source_list_add++ = TPU_STR;

			dev->TPU = SANE_TRUE;
		}

		*source_list_add = NULL;
/*
 *	Get the device name and copy it to dummy_dev.sane.model
 *	The device name starts at buf[0x1A] and is up to 16 bytes long
 *	We are overwriting whatever was set previously!
 */
 		{
#define DEVICE_NAME_LEN	(16)		
			char device_name[DEVICE_NAME_LEN + 1];
			char *end_ptr;
			int len;

			/* make sure that the end of string is marked */
			device_name[DEVICE_NAME_LEN] = '\0';

			/* copy the string to an area where we can work with it */
			memcpy(device_name, buf + 0x1A, DEVICE_NAME_LEN);
			end_ptr = strchr(device_name, ' ');
			if (end_ptr != NULL)
			{
				*end_ptr = '\0';
			}

			len = strlen(device_name);

			str = malloc( len + 1);
			str[len] = '\0';

			/* finally copy the device name to the structure */
			dev->sane.model = ( char *) memcpy( str, device_name, len);
		}
	}

/*
 *  Set values for quick format "max" entry.
 */

	qf_params[ XtNumber( qf_params) - 1].tl_x = dev->x_range->min;
	qf_params[ XtNumber( qf_params) - 1].tl_y = dev->y_range->min;
	qf_params[ XtNumber( qf_params) - 1].br_x = dev->x_range->max;
	qf_params[ XtNumber( qf_params) - 1].br_y = dev->y_range->max;

/*
 *
 */

#if 0
	{
		u_char * buf;
		EpsonHdr head;

#define  PUSH_BUTTON_STATUS_EN		0x01

		if( NULL == ( head = ( EpsonHdr) command( s, "\033!", 2, &status) ) ) {
			DBG( 0, "Request the push button status failed\n");
			return status;
		}

		buf = &head->buf[ 0];

		if( buf[ 0] & PUSH_BUTTON_STATUS_EN)
			DBG( 1, "Push button was pressed\n");


	}
#endif

/*
 *	now we can finally set the device name
 */
	str = malloc( strlen( dev_name) + 1);
	dev->sane.name = strcpy( str, dev_name);

	close_scanner( s);

	++num_devices;
	dev->next = first_dev;
	first_dev = dev;

	if (devp)
	{
		*devp = dev;
	}

	return SANE_STATUS_GOOD;
}

/*
 *
 */

static SANE_Status attach_one ( const char *dev) {
	return attach( dev, 0);
}

/*
 *
 */

SANE_Status sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize) {
  size_t len;
  FILE *fp;

  authorize = authorize;	/* get rid of the compiler warning */

  DBG_INIT ();
#if defined PACKAGE && defined VERSION
  DBG( 2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if( version_code != NULL)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  /* default to /dev/scanner instead of insisting on config file */
  if( (fp = sanei_config_open (EPSON_CONFIG_FILE)))
    {
      char line[PATH_MAX];

      while (sanei_config_read (line, sizeof (line), fp))
	{
	  DBG( 4, "sane_init, >%s<\n", line);
	  if( line[0] == '#')		/* ignore line comments */
	    continue;
	  len = strlen (line);
	  if( line[len - 1] == '\n')
            line[--len] = '\0';
	  if( !len)
            continue;			/* ignore empty lines */
	  DBG( 4, "sane_init, >%s<\n", line);

          sanei_config_attach_matching_devices (line, attach_one);
	}
      fclose (fp);
    }

  /* read the option section and assign the connection type to the
     scanner structure - which we don't have at this time. So I have
     to come up with something :-) */

  return SANE_STATUS_GOOD;
}

/*
 * void sane_exit(void)
 *
 * Clean up the list of attached scanners. 
 */

void sane_exit ( void) {
	Epson_Device *dev, *next;

	for (dev = first_dev; dev; dev = next)
	{
		next = dev->next;
		free((char *) dev->sane.name);
		free((char *) dev->sane.model);
		free(dev);
	}
}

/*
 *
 *
 */

SANE_Status sane_get_devices ( const SANE_Device * * * device_list, SANE_Bool local_only) 
{

#if 0

	static const SANE_Device *devlist [ 2];
	int i;

	i = 0;
	if( dummy_dev.sane.name != NULL)
		devlist[ i++] = &dummy_dev.sane;

	devlist[ i] = NULL;

	*device_list = devlist;
	return SANE_STATUS_GOOD;
#endif
	static const SANE_Device **devlist = 0;
	Epson_Device *dev;
	int i;

	DBG(5, "sane_get_devices()\n");

	local_only = local_only;	/* just to get rid of the compiler warning */

	if (devlist) 
	{
		free (devlist); 
	}

	devlist = malloc((num_devices + 1) * sizeof(devlist[0]));
	if (!devlist)
	{
		return SANE_STATUS_NO_MEM;
	}

	i = 0;

	for (dev = first_dev; i < num_devices; dev = dev->next)
	{
		devlist[i++] = &dev->sane;
	}

	devlist[i++] = 0;

	*device_list = devlist;

	return SANE_STATUS_GOOD;	
}

/*
 *
 *
 */

static SANE_Status init_options ( Epson_Scanner * s) {
	int i;

	for( i = 0; i < NUM_OPTIONS; ++i) {
		s->opt[ i].size = sizeof( SANE_Word);
		s->opt[ i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
	}

	s->opt[ OPT_NUM_OPTS].title	= SANE_TITLE_NUM_OPTIONS;
	s->opt[ OPT_NUM_OPTS].desc	= SANE_DESC_NUM_OPTIONS;
	s->opt[ OPT_NUM_OPTS].cap	= SANE_CAP_SOFT_DETECT;
	s->val[ OPT_NUM_OPTS].w		= NUM_OPTIONS;

	/* "Scan Mode" group: */

	s->opt[ OPT_MODE_GROUP].title	= "Scan Mode";
	s->opt[ OPT_MODE_GROUP].desc	= "";
	s->opt[ OPT_MODE_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_MODE_GROUP].cap	= 0;

		/* scan mode */
		s->opt[ OPT_MODE].name = SANE_NAME_SCAN_MODE;
		s->opt[ OPT_MODE].title = SANE_TITLE_SCAN_MODE;
		s->opt[ OPT_MODE].desc = SANE_DESC_SCAN_MODE;
		s->opt[ OPT_MODE].type = SANE_TYPE_STRING;
		s->opt[ OPT_MODE].size = max_string_size(mode_list);
		s->opt[ OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_MODE].constraint.string_list = mode_list;
		s->val[ OPT_MODE].w = 0;		/* Binary */

		/* halftone */
		s->opt[ OPT_HALFTONE].name	= SANE_NAME_HALFTONE;
		s->opt[ OPT_HALFTONE].title	= SANE_TITLE_HALFTONE;
		s->opt[ OPT_HALFTONE].desc	= "Selects the halftone.";

		s->opt[ OPT_HALFTONE].type = SANE_TYPE_STRING;
		s->opt[ OPT_HALFTONE].size = max_string_size(halftone_list_7);
		s->opt[ OPT_HALFTONE].constraint_type = SANE_CONSTRAINT_STRING_LIST;

		if( s->hw->level >= 7)
			s->opt[ OPT_HALFTONE].constraint.string_list = halftone_list_7;
		else if( s->hw->level >= 4)
			s->opt[ OPT_HALFTONE].constraint.string_list = halftone_list_4;
		else
			s->opt[ OPT_HALFTONE].constraint.string_list = halftone_list;

		s->val[ OPT_HALFTONE].w = 1;	/* Halftone A */

		if( ! s->hw->cmd->set_halftoning) {
                        s->opt[ OPT_HALFTONE].cap |= SANE_CAP_INACTIVE;
                } 

		/* dropout */
		s->opt[ OPT_DROPOUT].name = "dropout";
		s->opt[ OPT_DROPOUT].title = "Dropout";
		s->opt[ OPT_DROPOUT].desc = "Selects the dropout.";

		s->opt[ OPT_DROPOUT].type = SANE_TYPE_STRING;
		s->opt[ OPT_DROPOUT].size = max_string_size(dropout_list);
		s->opt[ OPT_DROPOUT].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_DROPOUT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_DROPOUT].constraint.string_list = dropout_list;
		s->val[ OPT_DROPOUT].w = 0;	/* None */

		/* brightness */
		s->opt[ OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
		s->opt[ OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
		s->opt[ OPT_BRIGHTNESS].desc = "Selects the brightness.";

		s->opt[ OPT_BRIGHTNESS].type = SANE_TYPE_INT;
		s->opt[ OPT_BRIGHTNESS].unit = SANE_UNIT_NONE;
		s->opt[ OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_BRIGHTNESS].constraint.range = &s->hw->cmd->bright_range;
		s->val[ OPT_BRIGHTNESS].w = 0;	/* Normal */

		if( ! s->hw->cmd->set_bright) {
			s->opt[ OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		}

		/* sharpness */
		s->opt[ OPT_SHARPNESS].name     = "sharpness";
		s->opt[ OPT_SHARPNESS].title    = "Sharpness";
		s->opt[ OPT_SHARPNESS].desc     = "";

		s->opt[ OPT_SHARPNESS].type = SANE_TYPE_INT;
		s->opt[ OPT_SHARPNESS].unit = SANE_UNIT_NONE;
		s->opt[ OPT_SHARPNESS].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_SHARPNESS].constraint.range = &outline_emphasis_range;
		s->val[ OPT_SHARPNESS].w = 0;	/* Normal */

		if( ! s->hw->cmd->set_outline_emphasis) {
			s->opt[ OPT_SHARPNESS].cap |= SANE_CAP_INACTIVE;
		}


		/* gamma */
		s->opt[ OPT_GAMMA_CORRECTION].name     = SANE_NAME_GAMMA_CORRECTION;
		s->opt[ OPT_GAMMA_CORRECTION].title    = SANE_TITLE_GAMMA_CORRECTION;
		s->opt[ OPT_GAMMA_CORRECTION].desc     = SANE_DESC_GAMMA_CORRECTION;

		s->opt[ OPT_GAMMA_CORRECTION].type = SANE_TYPE_STRING;
		s->opt[ OPT_GAMMA_CORRECTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		/* 
		 * special handling for D1 function level - at this time I'm not
		 * testing for D1, I'm just assuming that all D level scanners will
		 * behave the same way. This has to be confirmed with the next D-level
		 * scanner 
		 */
		if (s->hw->cmd->level[0] == 'D')
		{
			s->opt[ OPT_GAMMA_CORRECTION].size = max_string_size(gamma_list_d);
			s->opt[ OPT_GAMMA_CORRECTION].constraint.string_list = gamma_list_d;
			s->val[ OPT_GAMMA_CORRECTION].w = 1;		/* Default */
			gamma_userdefined = gamma_userdefined_d;
			gamma_params = gamma_params_d;
		}
		else
		{
			s->opt[ OPT_GAMMA_CORRECTION].size = max_string_size(gamma_list_ab);
			s->opt[ OPT_GAMMA_CORRECTION].constraint.string_list = gamma_list_ab;
			s->val[ OPT_GAMMA_CORRECTION].w = 0;		/* Default */
			gamma_userdefined = gamma_userdefined_ab;
			gamma_params = gamma_params_ab;
		}

		if( ! s->hw->cmd->set_gamma) {
			s->opt[ OPT_GAMMA_CORRECTION].cap |= SANE_CAP_INACTIVE;
		}


		/* gamma vector */
		s->opt[ OPT_GAMMA_VECTOR].name  = SANE_NAME_GAMMA_VECTOR;
		s->opt[ OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
		s->opt[ OPT_GAMMA_VECTOR].desc  = SANE_DESC_GAMMA_VECTOR;

		s->opt[ OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
		s->opt[ OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR].wa = &s->gamma_table [ 0] [ 0];


		/* red gamma vector */
		s->opt[ OPT_GAMMA_VECTOR_R].name  = SANE_NAME_GAMMA_VECTOR_R;
		s->opt[ OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
		s->opt[ OPT_GAMMA_VECTOR_R].desc  = SANE_DESC_GAMMA_VECTOR_R;

		s->opt[ OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
		s->opt[ OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR_R].wa = &s->gamma_table [ 1] [ 0];


		/* green gamma vector */
		s->opt[ OPT_GAMMA_VECTOR_G].name  = SANE_NAME_GAMMA_VECTOR_G;
		s->opt[ OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
		s->opt[ OPT_GAMMA_VECTOR_G].desc  = SANE_DESC_GAMMA_VECTOR_G;

		s->opt[ OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
		s->opt[ OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR_G].wa = &s->gamma_table [ 2] [ 0];


		/* red gamma vector */
		s->opt[ OPT_GAMMA_VECTOR_B].name  = SANE_NAME_GAMMA_VECTOR_B;
		s->opt[ OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
		s->opt[ OPT_GAMMA_VECTOR_B].desc  = SANE_DESC_GAMMA_VECTOR_B;

		s->opt[ OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
		s->opt[ OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
		s->opt[ OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
		s->opt[ OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
		s->val[ OPT_GAMMA_VECTOR_B].wa = &s->gamma_table [ 3] [ 0];

		if (gamma_userdefined[s->val[ OPT_GAMMA_CORRECTION].w] == SANE_TRUE )
		{
			s->opt[ OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		} else {
			s->opt[ OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
		}

		/* initialize the Gamma tables */
		memset(&s->gamma_table[0], 0, 256 * sizeof(SANE_Word));
		memset(&s->gamma_table[1], 0, 256 * sizeof(SANE_Word));
		memset(&s->gamma_table[2], 0, 256 * sizeof(SANE_Word));
		memset(&s->gamma_table[3], 0, 256 * sizeof(SANE_Word));
		for (i = 0 ; i < 256 ; i++) 
		{
			s->gamma_table[0][i] = i;
			s->gamma_table[1][i] = i;
			s->gamma_table[2][i] = i;
			s->gamma_table[3][i] = i;
		}


		/* color correction */
		s->opt[ OPT_COLOR_CORRECTION].name     = "color-correction";
		s->opt[ OPT_COLOR_CORRECTION].title    = "Color correction";
		s->opt[ OPT_COLOR_CORRECTION].desc     = "Sets the color correction table for the selected output device.";

		s->opt[ OPT_COLOR_CORRECTION].type = SANE_TYPE_STRING;
		s->opt[ OPT_COLOR_CORRECTION].size = 32;
		s->opt[ OPT_COLOR_CORRECTION].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_COLOR_CORRECTION].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_COLOR_CORRECTION].constraint.string_list = color_list;
		s->val[ OPT_COLOR_CORRECTION].w = 5;	/* scanner default: CRT monitors */

		if( ! s->hw->cmd->set_color_correction) {
			s->opt[ OPT_COLOR_CORRECTION].cap |= SANE_CAP_INACTIVE;
		}

		/* resolution */
		s->opt[ OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
		s->opt[ OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
		s->opt[ OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;

		s->opt[ OPT_RESOLUTION].type = SANE_TYPE_INT;
		s->opt[ OPT_RESOLUTION].unit = SANE_UNIT_DPI;
		s->opt[ OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
		s->opt[ OPT_RESOLUTION].constraint.word_list = s->hw->resolution_list;
		s->val[ OPT_RESOLUTION].w = s->hw->dpi_range.min;

		/* threshold */
		s->opt[ OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
		s->opt[ OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
		s->opt[ OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;

		s->opt[ OPT_THRESHOLD].type = SANE_TYPE_INT;
		s->opt[ OPT_THRESHOLD].unit = SANE_UNIT_NONE;
		s->opt[ OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_THRESHOLD].constraint.range = &u8_range;
		s->val[ OPT_THRESHOLD].w = 0x80;

		if( ! s->hw->cmd->set_threshold) {
			s->opt[ OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;
		}


	s->opt[ OPT_CCT_GROUP].title	= "Color correction coefficients";
	s->opt[ OPT_CCT_GROUP].desc	= "Matrix multiplication of RGB";
	s->opt[ OPT_CCT_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_CCT_GROUP].cap	= SANE_CAP_ADVANCED;


		/* color correction coefficients */
		s->opt[ OPT_CCT_1].name  = "cct-1";
		s->opt[ OPT_CCT_2].name  = "cct-2";
		s->opt[ OPT_CCT_3].name  = "cct-3";
		s->opt[ OPT_CCT_4].name  = "cct-4";
		s->opt[ OPT_CCT_5].name  = "cct-5";
		s->opt[ OPT_CCT_6].name  = "cct-6";
		s->opt[ OPT_CCT_7].name  = "cct-7";
		s->opt[ OPT_CCT_8].name  = "cct-8";
		s->opt[ OPT_CCT_9].name  = "cct-9";

		s->opt[ OPT_CCT_1].title = "Green";
		s->opt[ OPT_CCT_2].title = "Shift green to red";
		s->opt[ OPT_CCT_3].title = "Shift green to blue";
		s->opt[ OPT_CCT_4].title = "Shift red to green";
		s->opt[ OPT_CCT_5].title = "Red";
		s->opt[ OPT_CCT_6].title = "Shift red to blue";
		s->opt[ OPT_CCT_7].title = "Shift blue to green";
		s->opt[ OPT_CCT_8].title = "Shift blue to red";
		s->opt[ OPT_CCT_9].title = "Blue";

		s->opt[ OPT_CCT_1].desc  = "Controls green level";
		s->opt[ OPT_CCT_2].desc  = "Adds to red based on green level";
		s->opt[ OPT_CCT_3].desc  = "Adds to blue based on green level";
		s->opt[ OPT_CCT_4].desc  = "Adds to green based on red level";
		s->opt[ OPT_CCT_5].desc  = "Controls red level";
		s->opt[ OPT_CCT_6].desc  = "Adds to blue based on red level";
		s->opt[ OPT_CCT_7].desc  = "Adds to green based on blue level";
		s->opt[ OPT_CCT_8].desc  = "Adds to red based on blue level";
		s->opt[ OPT_CCT_9].desc  = "Control blue level";

		s->opt[ OPT_CCT_1].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_2].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_3].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_4].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_5].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_6].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_7].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_8].type = SANE_TYPE_INT;
		s->opt[ OPT_CCT_9].type = SANE_TYPE_INT;

		s->opt[ OPT_CCT_1].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_2].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_3].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_4].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_5].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_6].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_7].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_8].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_CCT_9].cap |= SANE_CAP_ADVANCED;

		s->opt[ OPT_CCT_1].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_2].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_3].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_4].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_5].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_6].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_7].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_8].unit = SANE_UNIT_NONE;
		s->opt[ OPT_CCT_9].unit = SANE_UNIT_NONE;

		s->opt[ OPT_CCT_1].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_2].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_3].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_4].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_5].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_6].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_7].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_8].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_CCT_9].constraint_type = SANE_CONSTRAINT_RANGE;

		s->opt[ OPT_CCT_1].constraint.range = &s8_range;
		s->opt[ OPT_CCT_2].constraint.range = &s8_range;
		s->opt[ OPT_CCT_3].constraint.range = &s8_range;
		s->opt[ OPT_CCT_4].constraint.range = &s8_range;
		s->opt[ OPT_CCT_5].constraint.range = &s8_range;
		s->opt[ OPT_CCT_6].constraint.range = &s8_range;
		s->opt[ OPT_CCT_7].constraint.range = &s8_range;
		s->opt[ OPT_CCT_8].constraint.range = &s8_range;
		s->opt[ OPT_CCT_9].constraint.range = &s8_range;

		s->val[ OPT_CCT_1].w = 32;
		s->val[ OPT_CCT_2].w = 0;
		s->val[ OPT_CCT_3].w = 0;
		s->val[ OPT_CCT_4].w = 0;
		s->val[ OPT_CCT_5].w = 32;
		s->val[ OPT_CCT_6].w = 0;
		s->val[ OPT_CCT_7].w = 0;
		s->val[ OPT_CCT_8].w = 0;
		s->val[ OPT_CCT_9].w = 32;

		if( ! s->hw->cmd->set_color_correction_coefficients)
		{
			s->opt[ OPT_CCT_1].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_2].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_3].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_4].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_5].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_6].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_7].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_8].cap |= SANE_CAP_INACTIVE;
			s->opt[ OPT_CCT_9].cap |= SANE_CAP_INACTIVE;
		}


	/* "Advanced" group: */
	s->opt[ OPT_ADVANCED_GROUP].title	= "Advanced";
	s->opt[ OPT_ADVANCED_GROUP].desc	= "";
	s->opt[ OPT_ADVANCED_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_ADVANCED_GROUP].cap		= SANE_CAP_ADVANCED;


		/* mirror */
		s->opt[ OPT_MIRROR].name     = "mirror";
		s->opt[ OPT_MIRROR].title    = "Mirror image";
		s->opt[ OPT_MIRROR].desc     = "Mirror the image.";

		s->opt[ OPT_MIRROR].type = SANE_TYPE_BOOL;
		s->val[ OPT_MIRROR].w = SANE_FALSE;

		if( ! s->hw->cmd->mirror_image) {
			s->opt[ OPT_MIRROR].cap |= SANE_CAP_INACTIVE;
		}


		/* speed */
		s->opt[ OPT_SPEED].name     = "speed";
		s->opt[ OPT_SPEED].title    = "Speed";
		s->opt[ OPT_SPEED].desc     = "";

		s->opt[ OPT_SPEED].type = SANE_TYPE_BOOL;
		s->val[ OPT_SPEED].w = SANE_FALSE;

		if( ! s->hw->cmd->set_speed) {
			s->opt[ OPT_SPEED].cap |= SANE_CAP_INACTIVE;
		}

		/* preview speed */
		s->opt[ OPT_PREVIEW_SPEED].name     = "preview-speed";
		s->opt[ OPT_PREVIEW_SPEED].title    = "Speed";
		s->opt[ OPT_PREVIEW_SPEED].desc     = "";

		s->opt[ OPT_PREVIEW_SPEED].type = SANE_TYPE_BOOL;
		s->val[ OPT_PREVIEW_SPEED].w = SANE_FALSE;

		if( ! s->hw->cmd->set_speed) {
			s->opt[ OPT_PREVIEW_SPEED].cap |= SANE_CAP_INACTIVE;
		}

		/* auto area segmentation */
		s->opt[ OPT_AAS].name	= "auto-area-segmentation";
		s->opt[ OPT_AAS].title	= "Auto area segmentation";
		s->opt[ OPT_AAS].desc	= "";

		s->opt[ OPT_AAS].type	= SANE_TYPE_BOOL;
		s->val[ OPT_AAS].w	= SANE_TRUE;

		if( ! s->hw->cmd->control_auto_area_segmentation) {
			s->opt[ OPT_AAS].cap |= SANE_CAP_INACTIVE;
		}


		/* zoom */
		s->opt[ OPT_ZOOM].name	= "zoom";
		s->opt[ OPT_ZOOM].title = "Zoom";
		s->opt[ OPT_ZOOM].desc	= "Defines the zoom factor the scanner will use";

		s->opt[ OPT_ZOOM].type	= SANE_TYPE_INT;
		s->opt[ OPT_ZOOM].unit	= SANE_UNIT_NONE;
		s->opt[ OPT_ZOOM].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_ZOOM].constraint.range = &zoom_range;
		s->val[ OPT_ZOOM].w	= 100;

/*		if( ! s->hw->cmd->set_zoom) */ {
			s->opt[ OPT_ZOOM].cap |= SANE_CAP_INACTIVE;
		}


	/* "Preview settings" group: */
	s->opt[ OPT_PREVIEW_GROUP].title = SANE_TITLE_PREVIEW;
	s->opt[ OPT_PREVIEW_GROUP].desc = "";
	s->opt[ OPT_PREVIEW_GROUP].type = SANE_TYPE_GROUP;
	s->opt[ OPT_PREVIEW_GROUP].cap = SANE_CAP_ADVANCED;

		/* preview */
		s->opt[ OPT_PREVIEW].name     = SANE_NAME_PREVIEW;
		s->opt[ OPT_PREVIEW].title    = SANE_TITLE_PREVIEW;
		s->opt[ OPT_PREVIEW].desc     = SANE_DESC_PREVIEW;

		s->opt[ OPT_PREVIEW].type = SANE_TYPE_BOOL;
		s->val[ OPT_PREVIEW].w = SANE_FALSE;

	/* "Geometry" group: */
	s->opt[ OPT_GEOMETRY_GROUP].title = "Geometry";
	s->opt[ OPT_GEOMETRY_GROUP].desc = "";
	s->opt[ OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
	s->opt[ OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;

		/* top-left x */
		s->opt[ OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
		s->opt[ OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
		s->opt[ OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;

		s->opt[ OPT_TL_X].type = SANE_TYPE_FIXED;
		s->opt[ OPT_TL_X].unit = SANE_UNIT_MM;
		s->opt[ OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_TL_X].constraint.range = s->hw->x_range;
		s->val[ OPT_TL_X].w = 0;

		/* top-left y */
		s->opt[ OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
		s->opt[ OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
		s->opt[ OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;

		s->opt[ OPT_TL_Y].type = SANE_TYPE_FIXED;
		s->opt[ OPT_TL_Y].unit = SANE_UNIT_MM;
		s->opt[ OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_TL_Y].constraint.range = s->hw->y_range;
		s->val[ OPT_TL_Y].w = 0;

		/* bottom-right x */
		s->opt[ OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
		s->opt[ OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
		s->opt[ OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;

		s->opt[ OPT_BR_X].type = SANE_TYPE_FIXED;
		s->opt[ OPT_BR_X].unit = SANE_UNIT_MM;
		s->opt[ OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_BR_X].constraint.range = s->hw->x_range;
		s->val[ OPT_BR_X].w = s->hw->x_range->max;

		/* bottom-right y */
		s->opt[ OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
		s->opt[ OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
		s->opt[ OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;

		s->opt[ OPT_BR_Y].type = SANE_TYPE_FIXED;
		s->opt[ OPT_BR_Y].unit = SANE_UNIT_MM;
		s->opt[ OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
		s->opt[ OPT_BR_Y].constraint.range = s->hw->y_range;
		s->val[ OPT_BR_Y].w = s->hw->y_range->max;

		/* Quick format */
		s->opt[ OPT_QUICK_FORMAT].name     = "quick-format";
		s->opt[ OPT_QUICK_FORMAT].title    = "Quick format";
		s->opt[ OPT_QUICK_FORMAT].desc     = "";

		s->opt[ OPT_QUICK_FORMAT].type = SANE_TYPE_STRING;
		s->opt[ OPT_QUICK_FORMAT].size = max_string_size(qf_list);
		s->opt[ OPT_QUICK_FORMAT].cap |= SANE_CAP_ADVANCED;
		s->opt[ OPT_QUICK_FORMAT].constraint_type = SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_QUICK_FORMAT].constraint.string_list = qf_list;
		s->val[ OPT_QUICK_FORMAT].w = XtNumber( qf_params) - 1;		/* max */

	/* "Optional equipment" group: */
	s->opt[ OPT_EQU_GROUP].title	= "Optional equipment";
	s->opt[ OPT_EQU_GROUP].desc	= "";
	s->opt[ OPT_EQU_GROUP].type	= SANE_TYPE_GROUP;
	s->opt[ OPT_EQU_GROUP].cap	= SANE_CAP_ADVANCED;


		/* source */
		s->opt[ OPT_SOURCE].name	= SANE_NAME_SCAN_SOURCE;
		s->opt[ OPT_SOURCE].title	= SANE_TITLE_SCAN_SOURCE;
		s->opt[ OPT_SOURCE].desc	= SANE_DESC_SCAN_SOURCE;

		s->opt[ OPT_SOURCE].type	= SANE_TYPE_STRING;
		s->opt[ OPT_SOURCE].size	= max_string_size(source_list);

		s->opt[ OPT_SOURCE].constraint_type		= SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_SOURCE].constraint.string_list	= source_list;

		if( ! s->hw->extension) {
			s->opt[ OPT_SOURCE].cap |= SANE_CAP_INACTIVE;
		}
		s->val[ OPT_SOURCE].w	= 0;	/* always use Flatbed as default */


		/* film type */
		s->opt[ OPT_FILM_TYPE].name	= "film-type";
		s->opt[ OPT_FILM_TYPE].title	= "Film type";
		s->opt[ OPT_FILM_TYPE].desc	= "";

		s->opt[ OPT_FILM_TYPE].type	= SANE_TYPE_STRING;
		s->opt[ OPT_FILM_TYPE].size	= max_string_size(film_list);

		s->opt[ OPT_FILM_TYPE].constraint_type		= SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_FILM_TYPE].constraint.string_list	= film_list;

		s->val[ OPT_FILM_TYPE].w	= 0;

		s->opt[ OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;		/* default is inactive */

#if 0
		if( ( ! s->hw->TPU) && ( ! s->hw->cmd->set_bay) ) {		/* Hack: Using set_bay to indicate. */
			s->opt[ OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;
		}
#endif


		/* forward feed / eject */
		s->opt[ OPT_EJECT].name		= "eject";
		s->opt[ OPT_EJECT].title	= "Eject";
		s->opt[ OPT_EJECT].desc		= "";

		s->opt[ OPT_EJECT].type	= SANE_TYPE_BUTTON;

		if( ( ! s->hw->ADF) && ( ! s->hw->cmd->set_bay) ) {		/* Hack: Using set_bay to indicate. */
			s->opt[ OPT_EJECT].cap |= SANE_CAP_INACTIVE;
		}


		/* auto forward feed / eject */
		s->opt[ OPT_AUTO_EJECT].name	= "auto-eject";
		s->opt[ OPT_AUTO_EJECT].title	= "Auto eject";
		s->opt[ OPT_AUTO_EJECT].desc	= "Eject document after scanning";

		s->opt[ OPT_AUTO_EJECT].type	= SANE_TYPE_BOOL;
		s->val[ OPT_AUTO_EJECT].w	= SANE_FALSE;

		if( ! s->hw->ADF) {		/* Hack: Using set_bay to indicate. */
			s->opt[ OPT_AUTO_EJECT].cap |= SANE_CAP_INACTIVE;
		}


		/* select bay */
		s->opt[ OPT_BAY].name		= "bay";
		s->opt[ OPT_BAY].title		= "Bay";
		s->opt[ OPT_BAY].desc		= "select bay to scan";

		s->opt[ OPT_BAY].type		= SANE_TYPE_STRING;
		s->opt[ OPT_BAY].size		= max_string_size(bay_list);
		s->opt[ OPT_BAY].constraint_type		= SANE_CONSTRAINT_STRING_LIST;
		s->opt[ OPT_BAY].constraint.string_list		= bay_list;
		s->val[ OPT_BAY].w		= 0;					/* Bay 1 */

		if( ! s->hw->cmd->set_bay) {
			s->opt[ OPT_BAY].cap |= SANE_CAP_INACTIVE;
		}


#if 0
		/* button test */
		s->opt[ OPT_FORMAT_MAX].name     = "max.";
		s->opt[ OPT_FORMAT_MAX].title    = "max.";
		s->opt[ OPT_FORMAT_MAX].desc     = "";

		s->opt[ OPT_FORMAT_MAX].type = SANE_TYPE_BUTTON;
		s->opt[ OPT_FORMAT_MAX].unit = SANE_UNIT_NONE;
		s->opt[ OPT_FORMAT_MAX].constraint_type = SANE_CONSTRAINT_NONE;
		s->opt[ OPT_FORMAT_MAX].constraint.range = NULL;
#endif






	return SANE_STATUS_GOOD;
}

/*
 *
 *
 */

SANE_Status sane_open ( SANE_String_Const devicename, SANE_Handle * handle) 
{
	Epson_Device	*dev;
	Epson_Scanner	*s;
	SANE_Status	status;

	DBG(5, "sane_open(%s)\n", devicename);

	/* search for device */
	if (devicename[0])
	{
		for (dev = first_dev; dev; dev = dev->next)
		{
			if (strcmp(dev->sane.name, devicename) == 0)
			{
				break;
			}
		}

		if (!dev)
		{
			status = attach(devicename, &dev);
			if (status != SANE_STATUS_GOOD)
			{
				return status;
			}
		}
	}
	else
	{
		dev = first_dev;
	}

	if (!dev)
	{
		return SANE_STATUS_INVAL;
	}

	s = calloc(sizeof( Epson_Scanner), 1);
	if (!s)
	{
		return SANE_STATUS_NO_MEM;
	}

	s->fd = -1;
	s->hw = dev;

	init_options( s);

	/* insert newly opened handle into list of open handles */
	s->next = first_handle;	
	first_handle = s;

	*handle = ( SANE_Handle) s;
	return SANE_STATUS_GOOD;
}

/*
 *
 *
 */

void sane_close ( SANE_Handle handle) 
{
	Epson_Scanner *s, *prev;

	/*
	 * Test if there is still data pending from 
	 * the scanner. If so, then do a cancel
	 */

	s = (Epson_Scanner *) handle;

	/*
	 * If the s->ptr pointer is not NULL, then a scan operation
	 * was started and if s->eof is FALSE, it was not finished.
	 */

	if (!s->eof && s->ptr != NULL)
	{
		u_char * dummy;
		int len;
		SANE_Status status;

		/* malloc one line */
		dummy = malloc (s->params.bytes_per_line);
		if (dummy == NULL)
		{
			DBG (0, "Out of memory\n");
	        	return;
		}
		else
		{

			/* there is still data to read from the scanner */

			s->canceling = SANE_TRUE;
			status = sane_read(s, dummy, s->params.bytes_per_line, &len);

			while (!s->eof && 
				SANE_STATUS_CANCELLED != sane_read(s, dummy, s->params.bytes_per_line, &len))
			{
				/* empty body, the while condition does the processing */
			}
		}
	}



	/* remove handle from list of open handles */
	prev = 0;
	for (s = first_handle; s; s = s->next)
	{
		if (s == handle)
			break;
		prev = s;
	}

	if (!s)
	{
		DBG(1, "close: invalid handle (0x%p)\n", handle);
		return;
	}

	if (prev)
		prev->next = s->next;
	else
		first_handle = s->next;

	if (s->fd != -1)
		close_scanner(s);

	free(s);
}

/*
 *
 *
 */

const SANE_Option_Descriptor *
sane_get_option_descriptor ( SANE_Handle handle, SANE_Int option)

{
	Epson_Scanner *s = (Epson_Scanner *) handle;

	if( option < 0 || option >= NUM_OPTIONS)
		return NULL;

	return( s->opt + option );
}

/*
 *
 *
 */

static const SANE_String_Const *
search_string_list (const SANE_String_Const * list, SANE_String value)

{
	while( *list != NULL && strcmp( value, *list) != 0) {
		++list;
	}

	return( (*list == NULL) ? NULL : list );
}

/*
 *
 *
 */

/*
    Activate, deactivate an option.  Subroutines so we can add
    debugging info if we want.  The change flag is set to TRUE
    if we changed an option.  If we did not change an option,
    then the value of the changed flag is not modified.
*/

static void sane_activate( Epson_Scanner * s, SANE_Int option,
	SANE_Bool * change )

{
	if (!SANE_OPTION_IS_ACTIVE( s->opt[ option ].cap )) {
		s->opt[ option ].cap &= ~SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void sane_deactivate( Epson_Scanner * s, SANE_Int option,
	SANE_Bool * change )
{
	if (SANE_OPTION_IS_ACTIVE(s->opt[ option ].cap)) {
		s->opt[ option ].cap |= SANE_CAP_INACTIVE;
		*change = SANE_TRUE;
	}
}

static void sane_optstate( SANE_Bool state, Epson_Scanner * s,
	SANE_Int option, SANE_Bool * change )

{
	if (state) {
		sane_activate( s, option, change );
	} else {
		sane_deactivate( s, option, change );
	}
}

/**
    End of sane_activate, sane_deactivate, sane_optstate.
**/

static SANE_Status getvalue( SANE_Handle handle,
	SANE_Int option,
	void * value)

{
	Epson_Scanner          * s    = (Epson_Scanner *) handle;
	SANE_Option_Descriptor * sopt = &(s->opt[ option ]);
	Option_Value           * sval = &(s->val[ option ]);

	switch (option) {
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
		memcpy( value, sval->wa, sopt->size );
		break;

	case OPT_NUM_OPTS:
	case OPT_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_MIRROR:
	case OPT_SPEED:
	case OPT_PREVIEW_SPEED:
	case OPT_AAS:
	case OPT_PREVIEW:
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_CCT_1:
	case OPT_CCT_2:
	case OPT_CCT_3:
	case OPT_CCT_4:
	case OPT_CCT_5:
	case OPT_CCT_6:
	case OPT_CCT_7:
	case OPT_CCT_8:
	case OPT_CCT_9:
	case OPT_THRESHOLD:
	case OPT_ZOOM:
		*((SANE_Word *) value) = sval->w;
		break;

	case OPT_MODE:
	case OPT_HALFTONE:
	case OPT_DROPOUT:
	case OPT_QUICK_FORMAT:
	case OPT_SOURCE:
	case OPT_FILM_TYPE:
	case OPT_GAMMA_CORRECTION:
	case OPT_COLOR_CORRECTION:
	case OPT_BAY:
		strcpy( (char *) value, sopt->constraint.string_list[sval->w]);
		break;
#if 0
	case OPT_MODEL:
		strcpy( value, sval->s);
		break;
#endif


	default:
		return SANE_STATUS_INVAL;

	}

	return SANE_STATUS_GOOD;
}


/**
    End of getvalue.
**/

static void handle_depth_halftone( Epson_Scanner * s, SANE_Bool * reload)

/*
    This routine handles common options between OPT_MODE and
    OPT_HALFTONE.  These options are TET (a HALFTONE mode), AAS
    - auto area segmentation, and threshold.  Apparently AAS
    is some method to differentiate between text and photos.
    Or something like that.

    AAS is available when the scan color depth is 1 and the
    halftone method is not TET.

    Threshold is available when halftone is NONE, and depth is 1.
*/

{
	int hti = s->val[ OPT_HALFTONE ].w;
	int mdi = s->val[ OPT_MODE ].w;
	SANE_Bool aas	= SANE_FALSE;
	SANE_Bool thresh= SANE_FALSE;

	if (!s->hw->cmd->control_auto_area_segmentation) return;

	if (mode_params[ mdi ].depth == 1) {
		if (halftone_params[ hti ] != HALFTONE_TET) {
			aas = SANE_TRUE;
		}
		if (halftone_params[ hti ] == HALFTONE_NONE) {
			thresh = SANE_TRUE;
		}
	}
	sane_optstate( aas, s, OPT_AAS, reload );
	sane_optstate( thresh, s, OPT_THRESHOLD, reload );
}

/**
    End of handle_depth_halftone.
**/

static void handle_resolution( Epson_Scanner * s,
	SANE_Int option, void * value )

{
	int n, k = 0, f;
	int min_d = s->hw->res_list[ s->hw->res_list_size - 1];
	SANE_Int v = *(SANE_Word *) value;
	SANE_Int best = v;
	int * last = ( OPT_RESOLUTION == option) ?
				&s->hw->last_res :
				&s->hw->last_res_preview;

/*
    We don't assume the list is sorted.  Search the list of
    resolutions, looking for the value closest to the one we want.
*/

	for( n = 0; n < s->hw->res_list_size; n++) {
		int d = abs( v - s->hw->res_list[ n]);

		if( d < min_d) {
			min_d = d;
			k = n;
			best = s->hw->res_list[ n];
		}
	}

/*
    Problem: does not reach all values cause of scroll bar resolution.

    If the requested resolution is not actually available, search
    search the list of resolutions to see if the last resolution we
    used is in the list.  If the last resolution is in the list, and
    it is not near the to the closest possible value to the user's
    request, then use the one next to the last one - taking into
    account the direction the user appears to be going.  Or something
    like that.  I did not write this code.
*/

	if( (v != best) && *last) {
		for( f = 0; f < s->hw->res_list_size; f++)
			if( *last == s->hw->res_list[ f])
				break;

		if( f != k && f != k - 1 && f != k + 1) {
			if( k > f) {
				best = s->hw->res_list[ f + 1];
			} else if ( k < f) {
				best = s->hw->res_list[ f - 1];
			}
		}
	}

	*last = best;
	s->val[ option ].w = (SANE_Word) best;

	DBG(3, "Selected resolution %d dpi\n", best);
}

/**
    End of handle_resolution.
**/

static void handle_source( Epson_Scanner * s, SANE_Int optindex,
	char * value )

/*
    Handles setting the source (flatbed, transparency adapter (TPU),
    or auto document feeder (ADF)).
*/

{
	int force_max = SANE_FALSE;
	SANE_Bool dummy;

	if (s->val[ OPT_SOURCE ].w == optindex) return;
	s->val[ OPT_SOURCE ].w = optindex;

	if(  s->val[ OPT_TL_X ].w == s->hw->x_range->min
	  && s->val[ OPT_TL_Y ].w == s->hw->y_range->min
	  && s->val[ OPT_BR_X ].w == s->hw->x_range->max
	  && s->val[ OPT_BR_Y ].w == s->hw->y_range->max
	  ) {
	    force_max = SANE_TRUE;
	}
	if( ! strcmp( ADF_STR, value) ) {
		s->hw->x_range = &s->hw->adf_x_range;
		s->hw->y_range = &s->hw->adf_y_range;
		s->hw->use_extension = SANE_TRUE;
		/* disable film type option */
		s->opt[ OPT_FILM_TYPE].cap &= ~SANE_CAP_INACTIVE;
	} else if( ! strcmp( TPU_STR, value) ) {
		s->hw->x_range = &s->hw->tpu_x_range;
		s->hw->y_range = &s->hw->tpu_y_range;
		s->hw->use_extension = SANE_TRUE;
		/* enable film type option */
		s->opt[ OPT_FILM_TYPE].cap |= SANE_CAP_INACTIVE;
	} else {
		s->hw->x_range = &s->hw->fbf_x_range;
		s->hw->y_range = &s->hw->fbf_y_range;
		s->hw->use_extension = SANE_FALSE;
		/* disable film type option */
		s->opt[ OPT_FILM_TYPE].cap &= ~SANE_CAP_INACTIVE;
	}

	qf_params[ XtNumber(qf_params)-1 ].tl_x = s->hw->x_range->min;
	qf_params[ XtNumber(qf_params)-1 ].tl_y = s->hw->y_range->min;
	qf_params[ XtNumber(qf_params)-1 ].br_x = s->hw->x_range->max;
	qf_params[ XtNumber(qf_params)-1 ].br_y = s->hw->y_range->max;

	s->opt[ OPT_BR_X ].constraint.range = s->hw->x_range;
	s->opt[ OPT_BR_Y ].constraint.range = s->hw->y_range;

	if( s->val[ OPT_TL_X].w < s->hw->x_range->min || force_max)
		s->val[ OPT_TL_X].w = s->hw->x_range->min;

	if( s->val[ OPT_TL_Y].w < s->hw->y_range->min || force_max)
		s->val[ OPT_TL_Y].w = s->hw->y_range->min;

	if( s->val[ OPT_BR_X].w > s->hw->x_range->max || force_max)
		s->val[ OPT_BR_X].w = s->hw->x_range->max;

	if( s->val[ OPT_BR_Y].w > s->hw->y_range->max || force_max)
		s->val[ OPT_BR_Y].w = s->hw->y_range->max;

	sane_optstate( s->hw->TPU && s->hw->use_extension,
		s, OPT_FILM_TYPE, &dummy );
	sane_optstate( s->hw->ADF && s->hw->use_extension,
		s, OPT_AUTO_EJECT, &dummy );
	sane_optstate( s->hw->ADF && s->hw->use_extension,
		s, OPT_EJECT, &dummy );

#if 0
BAY is part  of the filmscan device.  We are not sure
if we are really going to support this device in this
code.  Is there an online manual for it?

	sane_optstate( s->hw->ADF && s->hw->use_extension,
		s, OPT_BAY, &reload );
#endif
}

/**
    End of handle_source.
**/

static SANE_Status setvalue( SANE_Handle handle,
	SANE_Int option,
	void * value,
	SANE_Int * info)

{
	Epson_Scanner          * s    = ( Epson_Scanner *) handle;
	SANE_Option_Descriptor * sopt = &(s->opt[ option ]);
	Option_Value           * sval = &(s->val[ option ]);

	SANE_Status status;
	const SANE_String_Const * optval;
	int optindex;
	SANE_Bool reload = SANE_FALSE;	

	status = sanei_constrain_value( sopt, value, info);

	if( status != SANE_STATUS_GOOD) return status;

	optval = NULL;
	optindex = 0;

	if( sopt->constraint_type == SANE_CONSTRAINT_STRING_LIST) {
		optval = search_string_list( sopt->constraint.string_list,
					     (char *) value);

		if( optval == NULL) return SANE_STATUS_INVAL;
		optindex = optval - sopt->constraint.string_list;
	}

	switch (option) {
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
		memcpy( sval->wa, value, sopt->size);	/* Word arrays */
		break;

	case OPT_CCT_1:
	case OPT_CCT_2:
	case OPT_CCT_3:
	case OPT_CCT_4:
	case OPT_CCT_5:
	case OPT_CCT_6:
	case OPT_CCT_7:
	case OPT_CCT_8:
	case OPT_CCT_9:
		sval->w = *((SANE_Word *) value);	/* Simple values */
		break;

	case OPT_DROPOUT:
	case OPT_FILM_TYPE:
	case OPT_BAY:
		sval->w = optindex;			/* Simple lists */
		break;

	case OPT_EJECT:
/*		return eject( s ); */
		eject( s );
		break;
			
	case OPT_RESOLUTION:
		handle_resolution( s, option, value );
		break;

	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
		sval->w = *((SANE_Word *) value);
		DBG( 1, "set = %f\n", SANE_UNFIX( sval->w));
		if (NULL != info) *info |= SANE_INFO_RELOAD_PARAMS;
		break;

	case OPT_SOURCE:
		handle_source( s, optindex, (char *) value );
		reload = SANE_TRUE;
		break;

	case OPT_MODE:
	{
		SANE_Bool ic = mode_params[ optindex ].color; /* IsColor? */
		SANE_Bool iccu = 	/* Is color correction a user type? */
			color_userdefined[ s->val[ OPT_COLOR_CORRECTION ].w ];

		sval->w = optindex;

		if (s->hw->cmd->set_halftoning != 0) {
			sane_optstate( mode_params[ optindex ].depth == 1,
				s, OPT_HALFTONE, &reload );
		}

		sane_optstate( !ic, s, OPT_DROPOUT, &reload );
		if (s->hw->cmd->set_color_correction) {
		    sane_optstate( ic, s, OPT_COLOR_CORRECTION, &reload );
		}
		if (s->hw->cmd->set_color_correction_coefficients) {
			sane_optstate( ic && iccu, s, OPT_CCT_1, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_2, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_3, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_4, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_5, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_6, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_7, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_8, &reload );
			sane_optstate( ic && iccu, s, OPT_CCT_9, &reload );
		}

		handle_depth_halftone( s, &reload );
		break;
	}

	case OPT_HALFTONE:
		sval->w = optindex;
		handle_depth_halftone( s, &reload );
		break;

		sval->w = optindex;
		break;

	case OPT_COLOR_CORRECTION:
	{
		SANE_Bool f = color_userdefined[ optindex ];

		sval->w = optindex;
		sane_optstate( f, s, OPT_CCT_1, &reload );
		sane_optstate( f, s, OPT_CCT_2, &reload );
		sane_optstate( f, s, OPT_CCT_3, &reload );
		sane_optstate( f, s, OPT_CCT_4, &reload );
		sane_optstate( f, s, OPT_CCT_5, &reload );
		sane_optstate( f, s, OPT_CCT_6, &reload );
		sane_optstate( f, s, OPT_CCT_7, &reload );
		sane_optstate( f, s, OPT_CCT_8, &reload );
		sane_optstate( f, s, OPT_CCT_9, &reload );

		break;
	}

	case OPT_GAMMA_CORRECTION:
	{
		SANE_Bool f = gamma_userdefined[ optindex ];

		sval->w = optindex;
		sane_optstate( f, s, OPT_GAMMA_VECTOR, &reload );
		sane_optstate( f, s, OPT_GAMMA_VECTOR_R, &reload );
		sane_optstate( f, s, OPT_GAMMA_VECTOR_G, &reload );
		sane_optstate( f, s, OPT_GAMMA_VECTOR_B, &reload );
		sane_optstate( !f, s, OPT_BRIGHTNESS, &reload ); /* Note... */

		break;
	}

	case OPT_MIRROR:
	case OPT_SPEED:
	case OPT_PREVIEW_SPEED:
	case OPT_AAS:
	case OPT_PREVIEW:							/* needed? */
	case OPT_BRIGHTNESS:
	case OPT_SHARPNESS:
	case OPT_AUTO_EJECT:
	case OPT_THRESHOLD:
	case OPT_ZOOM:
		sval->w = *(( SANE_Word *) value);
		break;

	case OPT_QUICK_FORMAT:
		sval->w = optindex;

		s->val[ OPT_TL_X ].w = qf_params[ sval->w ].tl_x;
		s->val[ OPT_TL_Y ].w = qf_params[ sval->w ].tl_y;
		s->val[ OPT_BR_X ].w = qf_params[ sval->w ].br_x;
		s->val[ OPT_BR_Y ].w = qf_params[ sval->w ].br_y;

		if( s->val[ OPT_TL_X ].w < s->hw->x_range->min)
			s->val[ OPT_TL_X ].w = s->hw->x_range->min;

		if( s->val[ OPT_TL_Y ].w < s->hw->y_range->min)
			s->val[ OPT_TL_Y ].w = s->hw->y_range->min;

		if( s->val[ OPT_BR_X ].w > s->hw->x_range->max)
			s->val[ OPT_BR_X ].w = s->hw->x_range->max;

		if( s->val[ OPT_BR_Y ].w > s->hw->y_range->max)
			s->val[ OPT_BR_Y ].w = s->hw->y_range->max;

		reload = SANE_TRUE;
		break;

	default:
		return SANE_STATUS_INVAL;
	}

	if (reload && info != NULL) {
		*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	}

	return SANE_STATUS_GOOD;
}

/**
    End of setvalue.
**/

SANE_Status sane_control_option ( SANE_Handle handle,
	SANE_Int option,
	SANE_Action action,
	void * value, SANE_Int * info)

{
	if( option < 0 || option >= NUM_OPTIONS)
		return SANE_STATUS_INVAL;

	if( info != NULL) *info = 0;

	switch (action) {
		case SANE_ACTION_GET_VALUE:
			return( getvalue( handle, option, value ) );

		case SANE_ACTION_SET_VALUE:
			return( setvalue( handle, option, value, info ) );
		default:
      			return SANE_STATUS_INVAL;
	}

	return SANE_STATUS_GOOD;
}

/*
 *
 *
 */

SANE_Status sane_get_parameters ( SANE_Handle handle, SANE_Parameters * params) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	int ndpi;

	DBG(5, "sane_get_parameters()\n");

	memset( &s->params, 0, sizeof( SANE_Parameters));

	ndpi = s->val[ OPT_RESOLUTION].w;

	s->params.pixels_per_line = SANE_UNFIX( s->val[ OPT_BR_X].w - s->val[ OPT_TL_X].w) / 25.4 * ndpi;
	s->params.lines = SANE_UNFIX( s->val[ OPT_BR_Y].w - s->val[ OPT_TL_Y].w) / 25.4 * ndpi;

		DBG( 3, "Preview = %d\n", s->val[OPT_PREVIEW].w);
		DBG( 3, "Resolution = %d\n", s->val[OPT_RESOLUTION].w);

		DBG( 1, "get para %p %p tlx %f tly %f brx %f bry %f [mm]\n"
			, ( void * ) s
			, ( void * ) s->val
			, SANE_UNFIX( s->val[ OPT_TL_X].w)
			, SANE_UNFIX( s->val[ OPT_TL_Y].w)
			, SANE_UNFIX( s->val[ OPT_BR_X].w)
			, SANE_UNFIX( s->val[ OPT_BR_Y].w)
			);

	/* pixels_per_line seems to be 8 * n.  */
	s->params.pixels_per_line = s->params.pixels_per_line & ~7;

	s->params.last_frame = SANE_TRUE;
	s->params.depth = mode_params[ s->val[ OPT_MODE].w].depth;

	if( mode_params[ s->val[ OPT_MODE].w].color) {
		s->params.format = SANE_FRAME_RGB;
		s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	} else {
		s->params.format = SANE_FRAME_GRAY;
		s->params.bytes_per_line = s->params.pixels_per_line * s->params.depth / 8;
	}

	if( NULL != params)
		*params = s->params;

	return SANE_STATUS_GOOD;
}

/*
 *
 *
 */

SANE_Status sane_start ( SANE_Handle handle) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Status status;
	const struct mode_param * mparam;
	u_char params[4];
	int ndpi;
	int left, top;
	int lcount;
	int i, j;	/* loop counter */

	DBG( 1, "preview %d\n", s->val[ OPT_PREVIEW].w);

#if 0
	status = sane_get_parameters( handle, NULL);
	if( status != SANE_STATUS_GOOD)
		return status;
#endif

	open_scanner( s);
/*
 * NOTE: added cause there was error reported for some scanner.
 * NOTE: disabled cause of batch use scanimage and ADF.
 *	reset( s);
 */

/*
 *
 */

/*
 *  There is some undocumented special with TPU enable/disable.
 *      TPU power	ESC e		status
 *	on		0		NAK
 *	on		1		ACK
 *	off		0		ACK
 *	off		1		NAK
 *
 * probably it make no sense to scan with TPU powered on and source flatbed, cause light
 * will come from both sides.
 */


#if 0
	if( s->hw->TPU) {

/*
 * NOTE: should remove source TPU, and switch automatically.
 * NOTE: not shure if that is a good idea.
 */




	} else
#endif

/*
 *
 */

	if( s->hw->extension) {
		u_char * buf;
		EpsonHdr head;

		DBG( 1, "use extension = %d\n", s->hw->use_extension);

		params[0] = ESC;
		params[1] = s->hw->cmd->control_an_extension;

		if( NULL == ( head = ( EpsonHdr) command( s, params, 2, &status) ) ) {
			DBG( 0, "control of an extension failed\n");
			return status;
		}

		params[ 0] = s->hw->use_extension;	/* 1: effective, 0: ineffective */
		send( s, params, 1, &status); /* to make (in)effective an extension unit*/
		status = expect_ack ( s);

		if( SANE_STATUS_GOOD != status) {
			DBG( 0, "Probably you have to power %s your TPU\n"
				, s->hw->use_extension ? "on" : "off");

			DBG( 0, "Also you have to restart sane, cause it gives a ....\n");
			DBG( 0, "about the return code I'm sending.\n");

			return status;
		}

		params[0] = ESC;
		params[1] = s->hw->cmd->request_extension_status;

		if( NULL == ( head = ( EpsonHdr) command( s, params, 2, &status) ) ) {
			DBG( 0, "Extended status flag request failed\n");
			return status;
		}

		buf = &head->buf[ 0];

		if( buf[ 0] & EXT_STATUS_FER) {
			DBG( 0, "option: fatal error\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_ERR) {
			DBG( 0, "ADF: other error\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_PE) {
			DBG( 0, "ADF: no paper\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_PJ) {
			DBG( 0, "ADF: paper jam\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 1] & EXT_STATUS_OPN) {
			DBG( 0, "ADF: cover open\n");
			status = SANE_STATUS_INVAL;
		}

		if( buf[ 6] & EXT_STATUS_ERR) {
			DBG( 0, "TPU: other error\n");
			status = SANE_STATUS_INVAL;
		}

		if( SANE_STATUS_GOOD != status) {
			close_scanner( s);
			return status;
		}
	}

/*
 *
 */

	mparam = mode_params + s->val[ OPT_MODE].w;
	status = set_data_format( s, mparam->depth);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_data_format failed: %s\n", sane_strstatus( status));
		return status;
	}

	/*
	 * The byte sequence mode was introduced in B5, for B34 we need line sequence mode 
	 
	 */

	if( (s->hw->cmd->level[0] == 'D' || 
            (s->hw->cmd->level[0] == 'B' && s->hw->level >= 5)) && mparam->mode_flags == 0x02)
	{
		status = set_color_mode( s, 0x13);
	}
	else
	{
		status = set_color_mode( s , mparam->mode_flags
			  | ( mparam->dropout_mask
			    & dropout_params[ s->val[ OPT_DROPOUT].w]
			    )
			);
	}

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_color_mode failed: %s\n", sane_strstatus( status));
		return status;
	}



	if( s->hw->cmd->set_halftoning && SANE_OPTION_IS_ACTIVE( s->opt[ OPT_HALFTONE].cap) ) {
		status = set_halftoning( s, halftone_params[ s->val[ OPT_HALFTONE].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_halftoning failed: %s\n", sane_strstatus( status));
			return status;
		}
	}


	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_BRIGHTNESS].cap) ) {
		status = set_bright( s, s->val[OPT_BRIGHTNESS].w);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_bright failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_MIRROR].cap) ) {

		status = mirror_image( s, mirror_params[ s->val[ OPT_MIRROR].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: mirror_image failed: %s\n", sane_strstatus( status));
			return status;
		}

	}

#if 1
	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_SPEED].cap) ) {

		if( s->val[ OPT_PREVIEW].w)
			status = set_speed( s, speed_params[ s->val[ OPT_PREVIEW_SPEED].w]);
		else
			status = set_speed( s, speed_params[ s->val[ OPT_SPEED].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_speed failed: %s\n", sane_strstatus( status));
			return status;
		}

	}
#else
	status = set_speed( s, mode_params[ s->val[ OPT_MODE]].depth == 1 ? 1 : 0);
#endif

/*
 *  use of speed_params is ok here since they are false and true.
 *  NOTE: I think I should throw that "params" stuff as long w is already the value.
 */

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_AAS].cap) ) {

		status = control_auto_area_segmentation( s, speed_params[ s->val[ OPT_AAS].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: control_auto_area_segmentation failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	s->invert_image = SANE_FALSE;	/* default to not inverting the image */

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_FILM_TYPE].cap) ) {

		s->invert_image = (s->val[ OPT_FILM_TYPE].w == FILM_TYPE_NEGATIVE);

		status = set_film_type( s, film_params[ s->val[ OPT_FILM_TYPE].w]);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_film_type failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_BAY].cap) ) {

		status = set_bay( s, s->val[ OPT_BAY].w);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_bay: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_SHARPNESS].cap) ) {

		status = set_outline_emphasis( s, s->val[ OPT_SHARPNESS].w);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_outline_emphasis failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_GAMMA_CORRECTION].cap) ) {
		int val;
		if (s->hw->cmd->level[0] == 'D')
		{
			/*
			 * The D1 level has only the two user defined gamma 
			 * settings.
			 */
			val = gamma_params[ s->val[ OPT_GAMMA_CORRECTION].w];
		}
		else
		{
			val = gamma_params[ s->val[ OPT_GAMMA_CORRECTION].w];

			/*
			 * If "Default" is selected then determine the actual value
			 * to send to the scanner: If bilevel mode, just send the 
			 * value from the table (0x01), for grayscale or color mode 
			 * add one and send 0x02.
			 */
			if( s->val[ OPT_GAMMA_CORRECTION].w <= 1) {
				val += mparam->depth == 1 ? 0 : 1;
			}
		}

		DBG( 1, "sane_start: set_gamma( s, 0x%x ).\n", val);
		status = set_gamma( s, val);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_gamma failed: %s\n", sane_strstatus( status));
			return status;
		}
	}

	if (gamma_userdefined[s->val[ OPT_GAMMA_CORRECTION].w])
	{	/* user defined. */
		status = set_gamma_table( s);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_gamma_table failed: %s\n", sane_strstatus (status));
			return status;
		}
	}

#if 0
	status = set_color_correction( s, 0x80);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_color_correction failed: %s\n", sane_strstatus (status));
		return status;
	}
#else
/*
 * TODO: think about if SANE_OPTION_IS_ACTIVE is a good criteria to send commands.
 */

	if( SANE_OPTION_IS_ACTIVE( s->opt[ OPT_COLOR_CORRECTION].cap) ) {
		int val = color_params[ s->val[ OPT_COLOR_CORRECTION].w];

		DBG( 1, "sane_start: set_color_correction( s, 0x%x )\n", val );
		status = set_color_correction( s, val);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_color_correction failed: %s\n", sane_strstatus( status));
			return status;
		}
	}
	if( 1 == s->val[ OPT_COLOR_CORRECTION].w) {	/* user defined. */
		status = set_color_correction_coefficients( s);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_color_correction_coefficients failed: %s\n", sane_strstatus( status));
			return status;
		}
	}
#endif

	if (s->hw->cmd->set_threshold != 0 && SANE_OPTION_IS_ACTIVE( s->opt[ OPT_THRESHOLD].cap))
	{
		status = set_threshold(s, s->val[ OPT_THRESHOLD].w);
		
		if (SANE_STATUS_GOOD != status)
		{
			DBG(1, "sane_start: set_threshold(%d) failed: %s\n", 
				s->val[ OPT_THRESHOLD].w, sane_strstatus(status));
			return status;
		}
	}

	ndpi = s->val[ OPT_RESOLUTION].w;

	status = set_resolution( s, ndpi, ndpi);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_resolution(%d, %d) failed: %s\n", 
			ndpi, ndpi, sane_strstatus( status));
		return status;
    	}

	status = sane_get_parameters( handle, NULL);

	if( status != SANE_STATUS_GOOD)
		return status;

	/* set the zoom */
	if (s->hw->cmd->set_zoom != 0 && SANE_OPTION_IS_ACTIVE( s->opt[ OPT_ZOOM].cap))
	{
		status = set_zoom(s, s->val[ OPT_ZOOM].w, s->val[ OPT_ZOOM].w);
		if (status != SANE_STATUS_GOOD)
		{
			DBG(1, "sane_start: set_zoom(%d) failed: %s\n", 
				s->val[ OPT_ZOOM].w, sane_strstatus(status));
			return status;
		}
	}


/*
 *  Now s->params is initialized.
 */

/*
 *  in file:frontend/preview.c
 *
 *  The preview strategy is as follows:
 *
 *   1) A preview always acquires an image that covers the entire
 *      scan surface.  This is necessary so the user can see not
 *      only what is, but also what isn't selected.
 */

	left = SANE_UNFIX( s->val[OPT_TL_X].w) / 25.4 * ndpi + 0.5;
	top = SANE_UNFIX( s->val[OPT_TL_Y].w) / 25.4 * ndpi + 0.5;

	/*
	 * Calculate correction for line_distance in D1 scanner:
	 * Start line_distance lines earlier and add line_distance lines at the end
	 *
	 * Because the actual line_distance is not yet calculated we have to do this
	 * first. !!!
	 */

	s->hw->color_shuffle = SANE_FALSE;
	s->current_output_line = 0;
	s->lines_written = 0;
	s->color_shuffle_line = 0;

	if ((s->hw->optical_res != 0) && (mparam->depth == 8) && (mparam->mode_flags != 0))
	 {

	  s->line_distance = s->hw->max_line_distance * ndpi / s->hw->optical_res;
	  if (s->line_distance != 0)
	  {
		  s->hw->color_shuffle = SANE_TRUE;
	  }
	  else
		  s->hw->color_shuffle = SANE_FALSE;
	 }


	/* modify the scan area */
	if (s->hw->color_shuffle == SANE_TRUE) 
	 {
#if 0
	   top -= s->line_distance;
	   if (top < 0)
	    {
	      top = 0;
	    }
#endif

           s->params.lines += 2* s->line_distance;
	 }

	if (SANE_UNFIX( s->val[ OPT_BR_Y].w) / 25.4 * ndpi < s->params.lines)
	{
	 	s->params.lines = (int) SANE_UNFIX(s->val[OPT_BR_Y].w) / 25.4 * ndpi + 0.5;
	}


	status = set_scan_area( s, left, top, s->params.pixels_per_line, s->params.lines);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: set_scan_area failed: %s\n", sane_strstatus( status));
		return status;
	}

	s->block = SANE_FALSE;
	lcount = 1;

	/*
	 * The set line count commands needs to be sent for certain scanners in 
	 * color mode. The D1 level requires it, we are however only testing for
	 * 'D' and not for the actual numeric level.
	 */

	if( ( (s->hw->cmd->level[0] == 'B') && 
			( (s->hw->level >= 5) || ( (s->hw->level >= 4) && (! mode_params[s->val[ OPT_MODE].w].color))))
		|| ( s->hw->cmd->level[0] == 'D') )
	{
		s->block = SANE_TRUE;
		lcount = sanei_scsi_max_request_size / s->params.bytes_per_line;

		if( lcount > 255)
			lcount = 255;

		if( lcount == 0)
			return SANE_STATUS_NO_MEM;

		status = set_lcount( s, lcount);

		if( SANE_STATUS_GOOD != status) {
			DBG( 1, "sane_start: set_lcount(%d) failed: %s\n", 
				lcount, sane_strstatus (status));
			return status;
		}
    	}

	if( SANE_TRUE == s->hw->extension) { /* make sure if any errors */

/* TODO	*/

		u_char result[ 4];		/* with an extension */
		u_char * buf;
		size_t len;

		params[0] = ESC;
		params[1] = s->hw->cmd->request_extension_status;

		send( s, params, 2, &status);		/* send ESC f (request extension status) */

		if( SANE_STATUS_GOOD != status)
			return status;

		len = 4;				/* receive header */

		receive( s, result, len, &status);
    		if( SANE_STATUS_GOOD != status)
      			return status;

		len = result[ 3] << 8 | result[ 2];
		buf = alloca( len);

		receive( s, buf, len, &status);		/* reveive actual status data */

		if( buf[ 0] & 0x80) {
			close_scanner( s);
			return SANE_STATUS_INVAL;
		}
	}

/*
 *  for debug purpose
 *  check scanner conditions
 */
#if 1
	if (s->hw->cmd->request_condition != 0)
	  {
	    u_char result [ 4];
	    u_char * buf;
	    size_t len;

	    params[0] = ESC;
	    params[1] = s->hw->cmd->request_condition;

	    send( s, params, 2, &status);	/* send request condition */

	    if( SANE_STATUS_GOOD != status)
	      return status;

	    len = 4;
	    receive( s, result, len, &status);

	    if( SANE_STATUS_GOOD != status)
	      return status;

	    len = result[ 3] << 8 | result[ 2];
	    buf = alloca( len);
	    receive( s, buf, len, &status);

	    if( SANE_STATUS_GOOD != status)
	      return status;

#if 0
	    DBG(10, "SANE_START: length=%d\n", len);
	    for (i = 1; i <= len; i++) { 
	      DBG(10, "SANE_START: %d: %c\n", i, buf[i-1]); 
	    }
#endif

	    DBG( 5, "SANE_START: color: %d\n", (int) buf[1]);
	    DBG( 5, "SANE_START: resolution (x, y): (%d, %d)\n", 
		 (int) (buf[4]<<8|buf[3]), (int) (buf[6]<<8|buf[5]));
	    DBG( 5, "SANE_START: area[dots] (x-offset, y-offset), (x-range, y-range): (%d, %d), (%d, %d)\n",
		 (int) (buf[9]<<8|buf[8]), (int) (buf[11]<<8|buf[10]), 
		 (int) (buf[13]<<8|buf[12]), (int) (buf[15]<<8|buf[14]));
	    DBG( 5, "SANE_START: data format: %d\n", (int) buf[17]);
	    DBG( 5, "SANE_START: halftone: %d\n", (int) buf[19]);
	    DBG( 5, "SANE_START: brightness: %d\n", (int) buf[21]);
	    DBG( 5, "SANE_START: gamma: %d\n", (int) buf[23]);
	    DBG( 5, "SANE_START: zoom[percentage] (x, y): (%d, %d)\n", (int) buf[26], (int) buf[25]);
	    DBG( 5, "SANE_START: color correction: %d\n", (int) buf[28]);
	    DBG( 5, "SANE_START: outline emphasis: %d\n", (int) buf[30]);
	    DBG( 5, "SANE_START: read mode: %d\n", (int) buf[32]);
	    DBG( 5, "SANE_START: mirror image: %d\n", (int) buf[34]);
	    DBG( 5, "SANE_START: (new B6 or B7 command ESC s): %d\n", (int) buf[36]);
	    DBG( 5, "SANE_START: (new B6 or B7 command ESC t): %d\n", (int) buf[38]);
	    DBG( 5, "SANE_START: line counter: %d\n", (int) buf[40]);
	    DBG( 5, "SANE_START: extension control: %d\n", (int) buf[42]);
	    DBG( 5, "SANE_START: (new B6 or B7 command ESC N): %d\n", (int) buf[44]);
	  }
#endif


	/* set the retry count to 0 */
	s->retry_count = 0;

	if (s->hw->color_shuffle == SANE_TRUE)
	{

	/* initialize the line buffers */
	  for (i = 0; i < s->line_distance * 2 + 1; i++)
	   {
	     if (s->line_buffer[i] != NULL)
	       free(s->line_buffer[i]);

	     s->line_buffer[i] = malloc(s->params.bytes_per_line);  
	     if (s->line_buffer[i] == NULL)
	      {
	        /* free the memory we've malloced so far */
	        for (j = 0; j < i; j++)
	         {
	           free(s->line_buffer[j]);
		   s->line_buffer[j] = NULL;
	         }
	        return SANE_STATUS_NO_MEM;
	      }
	   }
	}

	params[0] = ESC;
	params[1] = s->hw->cmd->start_scanning;

	send( s, params, 2, &status);

	if( SANE_STATUS_GOOD != status) {
		DBG( 1, "sane_start: start failed: %s\n", sane_strstatus( status));
		return status;
    	}

	s->eof = SANE_FALSE;
	s->buf = realloc( s->buf, lcount * s->params.bytes_per_line);
	s->ptr = s->end = s->buf;
	s->canceling = SANE_FALSE;

	return SANE_STATUS_GOOD;
} /* sane_start */

/*
 *
 * TODO: clean up the eject and direct cmd mess.
 */

SANE_Status sane_auto_eject ( Epson_Scanner * s) {

	if( s->hw->ADF && s->hw->use_extension && s->val[ OPT_AUTO_EJECT].w) {		/* sequence! */
		SANE_Status status;

		u_char params [ 1];
		u_char cmd = s->hw->cmd->eject;

		if( ! cmd)
			return SANE_STATUS_UNSUPPORTED;

		params[ 0] = cmd;

		send( s, params, 1, &status);

		if( SANE_STATUS_GOOD != ( status = expect_ack( s) ) ) {
			return status;
		}
	}

	return SANE_STATUS_GOOD;
}

/*
 *
 *
 */

static SANE_Status read_data_block ( Epson_Scanner * s, EpsonDataRec * result) {
	SANE_Status status;
        u_char param[3];

	receive( s, result, s->block ? 6 : 4, &status);

	if( SANE_STATUS_GOOD != status)
		return status;

	if( STX != result->code) {
		DBG( 1, "code   %02x\n", ( int) result->code);
		DBG( 1, "error, expected STX\n");

		return SANE_STATUS_INVAL;
	}

	if( result->status & STATUS_FER) {
		DBG( 1, "fatal error - Status = %02x\n", result->status);

#if 0
		/* check extended status if the option bit in status is set */
		if( result->status & STATUS_OPTION) {
			status = check_ext_status( s);
		} else
			status = SANE_STATUS_INVAL;
#else
		status = check_ext_status( s);
#endif

		/*
		 * Hack Alert!!!
		 * If the status is SANE_STATUS_DEVICE_BUSY then we need to 
		 * re-issue the command again. We can assume that the command that
		 * caused this problem was ESC G, so in a loop with a sleep 1 we
		 * are testing this over and over and over again, until the lamp
		 * "thinks" it is ready.
		 *
		 * TODO: Store the last command and execute what was actually used
		 *       as the last command. For all situations this error may occur
		 *       ESC G is very very likely to be the command in question, but
		 *       we better make sure that this is the case.
		 *
		 */

		/*
		 * let's safe some stack space: If this is not the first go around,
		 * then just return the status and let the loop handle this - otherwise
		 * we would run this function recursively.
		 */

		if ((status == SANE_STATUS_DEVICE_BUSY && s->retry_count > 0) ||
		    (status == SANE_STATUS_GOOD && s->retry_count > 0))
		{
			return SANE_STATUS_DEVICE_BUSY;	/* return busy even if we just read OK
							   so that the following loop can end
							   gracefully */
		}
		
#define	SANE_EPSON_MAX_RETRIES	(61)

		while (status == SANE_STATUS_DEVICE_BUSY) {
			if (s->retry_count > SANE_EPSON_MAX_RETRIES)
			{
				DBG(0, "Max retry count exceeded (%d)\n", s->retry_count);
				return SANE_STATUS_INVAL;
			}

			sleep(1);	/* wait one second for the next attempt */

			DBG(1, "retrying ESC G - %d\n", ++(s->retry_count)); 

			param[0] = ESC;
			param[1] = s->hw->cmd->start_scanning;

			send( s, param, 2, &status);

			if( SANE_STATUS_GOOD != status) {
				DBG( 1, "read_data_block: start failed: %s\n", sane_strstatus( status));
				return status;
			}

			status = read_data_block(s, result);
    		}
		
	}

	return status;
}

/*
 *
 *
 */

#define GET_COLOR(x)	((x.status>>2) & 0x03)

SANE_Status sane_read ( SANE_Handle handle, SANE_Byte * data, SANE_Int max_length, SANE_Int * length) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Status status;
	int index = 0;
	SANE_Bool reorder = SANE_FALSE;
	int i;	/* loop counter */
	int bytes_to_process = 0;

START_READ:
	DBG( 5, "sane_read: begin\n");

	if( s->ptr == s->end) 
	{
		EpsonDataRec result;
		size_t buf_len;

		if( s->eof) {
			if (s->hw->color_shuffle) 
			{
				DBG(1, "Written %d lines after color shuffle\n", s->lines_written);
				DBG(1, "Lines requested: %d\n", s->params.lines);
			}
			free( s->buf);
			s->buf = NULL;
			sane_auto_eject( s);
			close_scanner( s);
			s->fd = -1;
			*length = 0;

			/*
			 * free the line-buffers
			 */

			for (i = 0; i< s->line_distance; i++)
			{
				if (s->line_buffer[i] != NULL)
				{
					free(s->line_buffer[i]);
					s->line_buffer[i] = NULL;
				}
			}

			return SANE_STATUS_EOF;
		}


		DBG( 5, "sane_read: begin scan1\n");

		if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
			return status;

		buf_len = result.buf[ 1] << 8 | result.buf[ 0];

		DBG( 5, "sane_read: buf len = %lu\n", (u_long) buf_len);

		if( s->block)
		{
			buf_len *= ( result.buf[ 3] << 8 | result.buf[ 2]);
			DBG( 5, "sane_read: buf len (adjusted) = %lu\n", (u_long) buf_len);
		}

		if( !s->block && SANE_FRAME_RGB == s->params.format) {
			/*
			 * Read color data in line mode
			 */


			/* 
			 * read the first color line - the number of bytes to read
			 * is already known (from last call to read_data_block()
			 * We determine where to write the line from the color information
			 * in the data block. At the end we want the order RGB, but the
			 * way the data is delivered does not guarantee this - actually it's
			 * most likely that the order is GRB if it's not RGB!
			 */
			switch (GET_COLOR(result)) 
			{
			  case 1 :	index = 1;
			      		break;
			  case 2 :	index = 0;
			      		break;
			  case 3 : 	index = 2;
			      		break;
			}
			  
			receive( s, s->buf + index * s->params.pixels_per_line, buf_len, &status);

			if( SANE_STATUS_GOOD != status)
				return status;
			/* 
			 * send the ACK signal to the scanner in order to make
			 * it ready for the next data block.
			 */
			send( s, S_ACK, 1, &status);

			/*
			 * ... and request the next data block
			 */
			if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
				return status;

			buf_len = result.buf[ 1] << 8 | result.buf[ 0];
			/*
			 * this should never happen, because we are already in
			 * line mode, but it does not hurt to check ...
			 */
			if( s->block)
				buf_len *= ( result.buf[ 3] << 8 | result.buf[ 2]);

			DBG( 5, "sane_read: buf len2 = %lu\n", (u_long) buf_len);

			switch (GET_COLOR(result)) 
			{
			  case 1 :	index = 1;
			     		break;
			  case 2 :	index = 0;
			     		break;
			  case 3 :	index = 2;
			     		break;
			}
			  
			receive( s, s->buf + index * s->params.pixels_per_line, buf_len, &status);

			if( SANE_STATUS_GOOD != status)
				return status;

			send( s, S_ACK, 1, &status);

			/*
			 * ... and the last data block
			 */
			if( SANE_STATUS_GOOD != ( status = read_data_block( s, &result)))
				return status;

			buf_len = result.buf[ 1] << 8 | result.buf[ 0];

			if( s->block)
				buf_len *= ( result.buf[ 3] << 8 | result.buf[ 2]);

			DBG( 5, "sane_read: buf len3 = %lu\n", (u_long) buf_len);

			switch (GET_COLOR(result)) 
			{
			  case 1 :	index = 1;
			      		break;
			  case 2 :	index = 0;
			      		break;
			  case 3 :	index = 2;
			      		break;
			}
			  
			receive( s, s->buf + index * s->params.pixels_per_line, buf_len, &status);

			if( SANE_STATUS_GOOD != status)
				return status;
		} else {
			/*
			 * Read data in block mode
			 */

			/* do we have to reorder the data ? */
			if (GET_COLOR(result) == 0x01)
			{
				reorder = SANE_TRUE;
			}

			receive( s, s->buf, buf_len, &status);

			bytes_to_process = buf_len;

			if( SANE_STATUS_GOOD != status)
				return status;
		}

		if( result.status & STATUS_AREA_END)
			s->eof = SANE_TRUE;
		else {
			if( s->canceling) {
				send( s, S_CAN, 1, &status);
				expect_ack( s);
				free( s->buf);
				s->buf = NULL;
				sane_auto_eject( s);
				close_scanner( s);
				s->fd = -1;
				*length = 0;
				/*
				 * free the line-buffers 
				 */

				for (i = 0; i< s->line_distance; i++)
				{
					if (s->line_buffer[i] != NULL)
					{
						free(s->line_buffer[i]);
						s->line_buffer[i] = NULL;
					}
				}

				return SANE_STATUS_CANCELLED;
			} else
				send( s, S_ACK, 1, &status);
		}

		s->end = s->buf + buf_len;
		s->ptr = s->buf;

		/*
		 * if we have to re-order the color components (GRB->RGB) we
		 * are doing this here:
		 */

		if (reorder)
		{
			SANE_Byte *ptr;

			for (ptr = s->buf; ptr < s->end; ptr++)
			{
				/* R->G G->R */
				SANE_Byte tmp;

				tmp = *ptr;
				*ptr = *(ptr+1);	/* G */
				*(ptr+1) = tmp;		/* R */
							/* B stays the same */
				ptr += 3;		/* go to next */
			}
		}

		/* 
		 * Do the color_shuffle if everything else is correct - at this time
		 * most of the stuff is hardcoded for the Perfection 610
		 */

		if (s->hw->cmd->level[0] == 'D' && SANE_FRAME_RGB == s->params.format && s->block)
		{
			int new_length = 0;

			status = color_shuffle(s, &new_length);

			/* !!!
			 * If no bytes are returned, check if the scanner is already done, if so, 
			 * we'll probably just return, but if there is more data to process get
			 * the next batch.
			 */

			if (new_length == 0 && s->end != s->ptr)
			{
				goto START_READ;
			}

			s->end = s->buf + new_length;
			s->ptr = s->buf;

		}


		DBG( 5, "sane_read: begin scan2\n");
	}



	/* 
	 * copy the image data to the data memory area
	 */

	if( ! s->block && SANE_FRAME_RGB == s->params.format) {

		max_length /= 3;

		if( max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = 3 * max_length;

		if (s->invert_image == SANE_TRUE)
		{
			while( max_length-- != 0) {
				/* invert the three values */
				*data++ = (u_char) ~(s->ptr[ 0]);
				*data++ = (u_char) ~(s->ptr[ s->params.pixels_per_line]);
				*data++ = (u_char) ~(s->ptr[ 2 * s->params.pixels_per_line]);
				++s->ptr;
			}
		}
		else
		{
			while( max_length-- != 0) {
				*data++ = s->ptr[ 0];
				*data++ = s->ptr[ s->params.pixels_per_line];
				*data++ = s->ptr[ 2 * s->params.pixels_per_line];
				++s->ptr;
			}
		}
	} else {

		if( max_length > s->end - s->ptr)
			max_length = s->end - s->ptr;

		*length = max_length;

		if( 1 == s->params.depth) {
			if (s->invert_image == SANE_TRUE)
			{
				while( max_length-- != 0)
					*data++ = *s->ptr++;
			}
			else
			{
				while( max_length-- != 0)
					*data++ = ~*s->ptr++;
			}
		} else {

			if (s->invert_image == SANE_TRUE)
			{
				int i;

				for (i = 0 ; i < max_length; i++)
				{
					data[i] = (u_char) ~(s->ptr[i]);
				}
			}
			else
			{
				memcpy( data, s->ptr, max_length);
			}
			s->ptr += max_length;
		}
	}

	DBG( 5, "sane_read: end\n");

	return SANE_STATUS_GOOD;
}


static SANE_Status
color_shuffle(SANE_Handle handle, int *new_length)
{
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Byte	*buf = s->buf;
	int	length = s->end - s->buf;

	if (s->hw->color_shuffle == SANE_TRUE)
	{
		SANE_Byte * data_ptr;		/* ptr to data to process */
		SANE_Byte * data_end;		/* ptr to end of processed data */
		SANE_Byte * out_data_ptr;	/* ptr to memory when writing data */
		int	i;			/* loop counter */

		/*
		 * It looks like we are dealing with a scanner that has an odd way
		 * of dealing with colors... The red and blue scan lines are shifted
		 * up or down by a certain number of lines relative to the green line.
		 */
		DBG( 5, "sane_read: color_shuffle\n");


		/*
		 * Initialize the variables we are going to use for the 
		 * copying of the data. data_ptr is the pointer to
		 * the currently worked on scan line. data_end is the
		 * end of the data area as calculated from adding *length 
		 * to the start of data.
		 * out_data_ptr is used when writing out the processed data
		 * and always points to the beginning of the next line to
		 * write.
		 */

		data_ptr = out_data_ptr = buf;
		data_end = data_ptr + length;

		/*
		 * The image data is in *buf, we know that the buffer contains s->end - s->buf ( = length)
		 * bytes of data. The width of one line is in s->params.bytes_per_line
		 */

		/*
		 * The buffer area is supposed to have a number of full scan
		 * lines, let's test if this is the case. 
		 */

		if (length % s->params.bytes_per_line != 0)
		{
			DBG(0, "ERROR in size of buffer: %d / %d\n", 
				length, s->params.bytes_per_line);
			return SANE_STATUS_INVAL;
		}

		while (data_ptr < data_end)
		{
			SANE_Byte * source_ptr, *dest_ptr;
			int	loop;

			/* copy the green information into the current line */

			source_ptr = data_ptr + 1;
			dest_ptr = s->line_buffer[s->color_shuffle_line] +1;

			for (i=0; i< s->params.bytes_per_line /3; i++)
			{
				*dest_ptr = *source_ptr;
				dest_ptr += 3;
				source_ptr += 3;
			}

			/* copy the red information n lines back */

			if (s->color_shuffle_line >= s->line_distance)
			{
				source_ptr = data_ptr + 2;
				dest_ptr = s->line_buffer[s->color_shuffle_line - s->line_distance] + 2;

/*				while (source_ptr < s->line_buffer[s->color_shuffle_line] + s->params.bytes_per_line) */
				for (loop=0; loop < s->params.bytes_per_line / 3 ; loop++)
	
				{
					*dest_ptr = *source_ptr;
					dest_ptr += 3;
					source_ptr += 3;
				}
			}

			/* copy the blue information n lines forward */

			source_ptr = data_ptr;
			dest_ptr = s->line_buffer[s->color_shuffle_line + s->line_distance];

/*			while (source_ptr < s->line_buffer[s->color_shuffle_line] + s->params.bytes_per_line) */
			for (loop=0; loop < s->params.bytes_per_line / 3 ; loop++)
			{
				*dest_ptr = *source_ptr;
				dest_ptr += 3;
				source_ptr += 3;
			}

			data_ptr += s->params.bytes_per_line;

			if (s->color_shuffle_line == s->line_distance)
			{
				/*
				 * we just finished the line in line_buffer[0] - write it to the
				 * output buffer and continue.
				 */


				/*
				 * The ouput buffer ist still "buf", but because we are
				 * only overwriting from the beginning of the memory area
				 * we are not interfering with the "still to shuffle" data
				 * in the same area.
				 */

				/*
				 * Strip the first and last n lines and limit to 
				 */
				if ((s->current_output_line >= s->line_distance) &&
				    (s->current_output_line < s->params.lines + s->line_distance))
				{
					memcpy(out_data_ptr, s->line_buffer[0], s->params.bytes_per_line);
					out_data_ptr += s->params.bytes_per_line;

					s->lines_written++;
				}

				s->current_output_line++;


				/*
				 * Now remove the 0-entry and move all other 
				 * lines up by one. There are 2*line_distance + 1 
				 * buffers, * therefore the loop has to run from 0 
				 * to * 2*line_distance, and because we want to
				 * copy every n+1st entry to n the loop runs
				 * from - to 2*line_distance-1!
				 */

				free(s->line_buffer[0]);

				for (i = 0; i < s->line_distance*2; i++)
				{
					s->line_buffer[i] = s->line_buffer[i+1];
				}

				/*
				 * and create one new buffer at the end
				 */

				s->line_buffer[s->line_distance*2] = malloc(s->params.bytes_per_line);
				if (s->line_buffer[s->line_distance*2] == NULL)
				{
					int i;
					for (i=0; i<s->line_distance*2; i++)
					{
						free(s->line_buffer[i]);
						s->line_buffer[i] = NULL;
					}
					return SANE_STATUS_NO_MEM;
				}
			} else {
				s->color_shuffle_line++;		/* increase the buffer number */
			}
		}

		/*
		 * At this time we've used up all the new data from the scanner, some of
		 * it is still in the line_buffers, but we are ready to return some of it
		 * to the front end software. To do so we have to adjust the size of the
		 * data area and the *new_length variable.
		 */

		*new_length = out_data_ptr - buf;
	}

	return SANE_STATUS_GOOD;

}




/*
 * static SANE_Status get_identity_information ( SANE_Handle handle)
 *
 * Request Identity information from scanner and fill in information
 * into dev and/or scanner structures.
 */
static SANE_Status
get_identity_information(SANE_Handle handle)
{
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	Epson_Device  * dev = s->hw;
	EpsonIdent ident;
	u_char param[3];
	SANE_Status	status;
	u_char * buf;

	DBG(5, "get_identity_information()\n");

	if( ! s->hw->cmd->request_identity)
		return SANE_STATUS_INVAL;


	param[0] = ESC;
	param[1] = s->hw->cmd->request_identity;
	param[2] = '\0';

	if( NULL == ( ident = ( EpsonIdent) command( s, param, 2, &status) ) ) {
		DBG( 0, "ident failed\n");
		return SANE_STATUS_INVAL;
	}

	DBG( 1, "type  %3c 0x%02x\n", ident->type, ident->type);
	DBG( 1, "level %3c 0x%02x\n", ident->level, ident->level);

	{
		char * force = getenv( "SANE_EPSON_CMD_LVL");

		if( force) {
			ident->type = force[ 0];
			ident->level = force[ 1];

			DBG( 1, "type  %3c 0x%02x\n", ident->type, ident->type);
			DBG( 1, "level %3c 0x%02x\n", ident->level, ident->level);

			DBG( 1, "forced\n");
		}
	}

/*
 *  check if option equipment is installed.
 */

	if( ident->status & STATUS_OPTION) {
		DBG( 1, "option equipment is installed\n");
		dev->extension = SANE_TRUE;
	} else {
		DBG( 1, "no option equipment installed\n");
		dev->extension = SANE_FALSE;
	}

	dev->TPU = SANE_FALSE;
	dev->ADF = SANE_FALSE;

/*
 *  set command type and level.
 */

    {
	int n;

	for( n = 0; n < NELEMS( epson_cmd); n++)
		if( ! strncmp( &ident->type, epson_cmd[ n].level, 2) )
			break;

	if( n < NELEMS( epson_cmd) ) {
		dev->cmd = &epson_cmd[ n];
	} else {
		dev->cmd = &epson_cmd[EPSON_LEVEL_DEFAULT];
		DBG( 0, "Unknown type %c or level %c, using %s\n", 
			ident->buf[0], ident->buf[1], dev->cmd->level);
	}

	s->hw->level = dev->cmd->level[ 1] - '0';
    }	/* set comand type and level */ 

/*
 *  Setting available resolutions and xy ranges for sane frontend.
 */

	s->hw->res_list_size = 0;
	s->hw->res_list = ( SANE_Int *) calloc( s->hw->res_list_size, sizeof( SANE_Int));

	if( NULL == s->hw->res_list) {
		DBG( 0, "out of memory\n");
		return SANE_STATUS_NO_MEM;
	}

	{
		int n, k;
		int x = 0, y = 0;

		for( n = ident->count, buf = ident->buf; n; n -= k, buf += k) {
			switch (*buf) {
			case 'R':
			{
				int val = buf[ 2] << 8 | buf[ 1];

				s->hw->res_list_size++;
				s->hw->res_list = ( SANE_Int *) realloc( s->hw->res_list, s->hw->res_list_size * sizeof( SANE_Int));

				if( NULL == s->hw->res_list) {
					DBG( 0, "out of memory\n");
					return SANE_STATUS_NO_MEM;
				}

				s->hw->res_list[ s->hw->res_list_size - 1] = ( SANE_Int) val;

				DBG( 1, "resolution (dpi): %d\n", val);
				k = 3;
				continue;
	    		}
			case 'A':
			{
				x = buf[ 2] << 8 | buf[ 1];
				y = buf[ 4] << 8 | buf[ 3];

				DBG( 1, "maximum scan area: x %d y %d\n", x, y);
				k = 5;
				continue;
	    		}
			default:
				break;
			} /* case */

			break;
		} /* for */

		dev->dpi_range.min = s->hw->res_list[ 0];
		dev->dpi_range.max = s->hw->res_list[ s->hw->res_list_size - 1];
		dev->dpi_range.quant = 0;

		dev->fbf_x_range.min = 0;
		dev->fbf_x_range.max = SANE_FIX( x * 25.4 / dev->dpi_range.max);
		dev->fbf_x_range.quant = 0;

		dev->fbf_y_range.min = 0;
		dev->fbf_y_range.max = SANE_FIX( y * 25.4 / dev->dpi_range.max);
		dev->fbf_y_range.quant = 0;

		DBG( 5, "fbf tlx %f tly %f brx %f bry %f [mm]\n"
			, SANE_UNFIX( dev->fbf_x_range.min)
			, SANE_UNFIX( dev->fbf_y_range.min)
			, SANE_UNFIX( dev->fbf_x_range.max)
			, SANE_UNFIX( dev->fbf_y_range.max)
			);

	}
	
	/*
	 * Copy the resolution list to the resolution_list array so that the frontend can
	 * display the correct values
	 */

	s->hw->resolution_list = malloc( (s->hw->res_list_size +1) * sizeof(SANE_Word));

	if (s->hw->resolution_list == NULL)
	{
		DBG( 0, "out of memory\n");
		return SANE_STATUS_NO_MEM;
	}
	*(s->hw->resolution_list) = s->hw->res_list_size;
	memcpy(&(s->hw->resolution_list[1]), s->hw->res_list, s->hw->res_list_size * sizeof(SANE_Word));
	
	return SANE_STATUS_GOOD;

} /* request identity */


/*
 * static SANE_Status get_identity2_information ( SANE_Handle handle)
 *
 * Request Identity2 information from scanner and fill in information
 * into dev and/or scanner structures.
 */
static SANE_Status
get_identity2_information(SANE_Handle handle)
{
	Epson_Scanner * s = ( Epson_Scanner *) handle;
	SANE_Status	status;
	int len;
	u_char param[3];
	u_char result[4];
	u_char *buf;

	DBG(5, "get_identity2_information()\n");

	if (s->hw->cmd->request_identity2 == 0)
		return SANE_STATUS_UNSUPPORTED;

	param[0] = ESC;
	param[1] = s->hw->cmd->request_identity2;
	param[2] = '\0';

	send( s, param, 2, &status);

	if( SANE_STATUS_GOOD != status)
		return status;

	len = 4;				/* receive header */

	receive( s, result, len, &status);
    	if( SANE_STATUS_GOOD != status)
      		return status;

	len = result[ 3] << 8 | result[ 2];
	buf = alloca( len);

	receive( s, buf, len, &status);		/* reveive actual status data */

	if( buf[ 0] & 0x80) {
		close_scanner( s);
		return SANE_STATUS_INVAL;
	}

	/* replace this with the values from the scanner !!! */
	s->hw->max_line_distance = 8;
	s->hw->optical_res = 600;

	return SANE_STATUS_GOOD;
}

/*
 * void sane_cancel(SANE_Handle handle)
 * 
 * Set the cancel flag to true. The next time the backend requests data
 * from the scanner the CAN message will be sent.
 */

void sane_cancel ( SANE_Handle handle) {
	Epson_Scanner * s = ( Epson_Scanner *) handle;

	if( s->buf != NULL)
		s->canceling = SANE_TRUE;
}

/*
 * SANE_Status sane_set_io_mode()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status sane_set_io_mode ( SANE_Handle handle, SANE_Bool non_blocking) 
{
	/* get rid of compiler warning */
	handle = handle;
	non_blocking = non_blocking;

	return SANE_STATUS_UNSUPPORTED;
}

/*
 * SANE_Status sane_get_select_fd()
 *
 * not supported - for asynchronous I/O
 */

SANE_Status sane_get_select_fd ( SANE_Handle handle, SANE_Int * fd) 
{
	/* get rid of compiler warnings */
	handle = handle;
	fd = fd;

	return SANE_STATUS_UNSUPPORTED;
}
