/* epson.h - SANE library for Epson flatbed scanners.

   based on Kazuhiro Sasayama previous
   Work on epson.[ch] file from the SANE package.

   original code taken from sane-0.71
   Copyright (C) 1997 Hypercore Software Design, Ltd.

   modifications
   Copyright (C) 1998-1999 Christian Bucher <bucher@vernetzt.at>
   Copyright (C) 1998-1999 Kling & Hautzinger GmbH
   Copyright (C) 1999 Norihiko Sawa <sawa@yb3.so-net.ne.jp>
   Copyright (C) 2000 Karl Heinz Kremer <khk@khk.net>

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

#ifndef epson_h
#define epson_h 1



#define SANE_EPSON_CONFIG_USB "usb"
#define SANE_EPSON_CONFIG_PIO "pio"

#define SANE_NAME_GAMMA_CORRECTION "gamma-correction"
#define SANE_TITLE_GAMMA_CORRECTION "Gamma Correction"
#define SANE_DESC_GAMMA_CORRECTION "Selectes the gamma correction value from a list of pre-defined devices or the user defined table, which can be downloaded to the scanner"

typedef struct {
	unsigned char * level;

	unsigned char	request_identity;
	unsigned char	request_status;
	unsigned char	request_condition;
	unsigned char	set_color_mode;
	unsigned char	start_scanning;
	unsigned char	set_data_format;
	unsigned char	set_resolution;
	unsigned char	set_zoom;
	unsigned char	set_scan_area;
	unsigned char	set_bright;
	SANE_Range	bright_range;
	unsigned char	set_gamma;
	unsigned char	set_halftoning;
	unsigned char	set_color_correction;
	unsigned char	initialize_scanner;
	unsigned char	set_speed;				/* B4 upper */
	unsigned char	set_lcount;
	unsigned char	mirror_image;				/* B5 upper */
	unsigned char	set_gamma_table;			/* B4 upper */
	unsigned char	set_outline_emphasis;			/* B4 upper */
	unsigned char	set_dither;				/* B4 upper */
	unsigned char	set_color_correction_coefficients;	/* B3 upper */
	unsigned char	request_extention_status;		/* EXT */
	unsigned char	control_an_extention;			/* EXT */
	unsigned char	eject;					/* EXT */
	unsigned char	request_push_button_status;
	unsigned char	control_auto_area_segmentation;
	unsigned char	set_film_type;				/* EXT */
	unsigned char	set_exposure_time;			/* F5 */
	unsigned char	set_bay;				/* F5 */
} EpsonCmdRec, * EpsonCmd;

enum
	{ OPT_NUM_OPTS = 0
	, OPT_MODE_GROUP
		, OPT_MODE
		, OPT_HALFTONE
		, OPT_DROPOUT
		, OPT_BRIGHTNESS
		, OPT_SHARPNESS
		, OPT_GAMMA_CORRECTION
		, OPT_COLOR_CORRECTION
		, OPT_RESOLUTION
	, OPT_ADVANCED_GROUP
		, OPT_MIRROR
		, OPT_SPEED
		, OPT_AAS
		, OPT_GAMMA_VECTOR
		, OPT_GAMMA_VECTOR_R
		, OPT_GAMMA_VECTOR_G
		, OPT_GAMMA_VECTOR_B
	, OPT_CCT_GROUP
		, OPT_CCT_1
		, OPT_CCT_2
		, OPT_CCT_3
		, OPT_CCT_4
		, OPT_CCT_5
		, OPT_CCT_6
		, OPT_CCT_7
		, OPT_CCT_8
		, OPT_CCT_9
	, OPT_PREVIEW_GROUP
		, OPT_PREVIEW
		, OPT_PREVIEW_SPEED
	, OPT_GEOMETRY_GROUP
		, OPT_TL_X
		, OPT_TL_Y
		, OPT_BR_X
		, OPT_BR_Y
		, OPT_QUICK_FORMAT
	, OPT_EQU_GROUP
		, OPT_SOURCE
		, OPT_AUTO_EJECT
		, OPT_FILM_TYPE
		, OPT_BAY
		, OPT_EJECT
	, NUM_OPTIONS
	};

typedef enum {				/* hardware connection to the scanner */
        SANE_EPSON_NODEV,		/* default, no HW specified yet */
	SANE_EPSON_SCSI,		/* SCSI interface */
	SANE_EPSON_PIO,			/* parallel interface */
	SANE_EPSON_USB			/* USB interface */
} Epson_Connection_Type;

struct Epson_Device {
	SANE_Device sane;
	SANE_Int level;
	SANE_Range dpi_range;

	SANE_Range * x_range;		/* x range w/out extension */
	SANE_Range * y_range;		/* y range w/out extension */

	SANE_Range fbf_x_range;		/* flattbed x range */
	SANE_Range fbf_y_range;		/* flattbed y range */
	SANE_Range adf_x_range;		/* autom. document feeder x range */
	SANE_Range adf_y_range;		/* autom. document feeder y range */
	SANE_Range tpu_x_range;		/* transparency unit x range */
	SANE_Range tpu_y_range;		/* transparency unit y range */

	Epson_Connection_Type connection;
					/* hardware interface type */

	SANE_Int *res_list;		/* list of resolutions */
	SANE_Int res_list_size;		/* number of entries in this list */
	SANE_Int last_res;		/* last selected resolution */
	SANE_Int last_res_preview;	/* last selected preview resolution */

	SANE_Bool extension;		/* extension is installed */
	SANE_Bool use_extension;	/* use the installed extension */
	SANE_Bool TPU;			/* TPU is installed */
	SANE_Bool ADF;			/* ADF is installed */

	EpsonCmd cmd;
};

typedef struct Epson_Device Epson_Device;

typedef union {
	SANE_Word w;
	SANE_Word * wa;		/* word array */
	SANE_String s;
} Option_Value;

struct Epson_Scanner {
	int fd;
	Epson_Device * hw;
	SANE_Option_Descriptor opt [ NUM_OPTIONS];
	Option_Value val [ NUM_OPTIONS];
	SANE_Parameters params;
	SANE_Bool block;
	SANE_Bool eof;
	SANE_Byte * buf, * end, * ptr;
	SANE_Bool canceling;
	SANE_Word gamma_table [ 4] [ 256];
};

typedef struct Epson_Scanner Epson_Scanner;

#endif /* not epson_h */
