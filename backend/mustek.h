/* sane - Scanner Access Now Easy.
   Copyright (C) 1996, 1997 David Mosberger-Tang, 1998 Andreas Bolsch for
   extension to ScanExpress models version 0.5,
   2000 Henning Meier-Geinitz
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
   If you do not wish that, delete this exception notice.  

   This file implements a SANE backend for Mustek and some Trust flatbed
   scanners with SCSI or proprietary interface.  */

#ifndef mustek_h
#define mustek_h

#include <sys/types.h>

/* flag values: */
#define MUSTEK_FLAG_SINGLE_PASS	(1 << 0)   /* single-pass scanner? */
#define MUSTEK_FLAG_ADF		(1 << 1)   /* automatic document feeder */
#define MUSTEK_FLAG_ADF_READY	(1 << 2)   /* paper present */
#define MUSTEK_FLAG_TA		(1 << 3)   /* transparency adapter */
#define MUSTEK_FLAG_USE_EIGHTS	(1 << 4)   /* use 1/8" lengths */
#define MUSTEK_FLAG_LD_FIX	(1 << 5)   /* need line-distance fix? */
#define MUSTEK_FLAG_LD_MFS	(1 << 6)   /* MFS line-distance corr? */
#define MUSTEK_FLAG_LD_NONE	(1 << 7)   /* no line-distance corr */
#define MUSTEK_FLAG_LD_N1	(1 << 8)   /* LD corr for N-type v1 */
#define MUSTEK_FLAG_LD_N2	(1 << 9)   /* LD corr for N-type v2 */
#define MUSTEK_FLAG_LINEART_FIX	(1 << 10)  /* lineart fix/hack */
#define MUSTEK_FLAG_N		(1 << 11)  /* N-type scanner (non SCSI) */
#define MUSTEK_FLAG_SE		(1 << 12)  /* ScanExpress model */
#define MUSTEK_FLAG_DOUBLE_RES  (1 << 13)  /* MSF-06000CZ res. encoding */
#define MUSTEK_FLAG_FORCE_GAMMA (1 << 14)  /* force gamma table upload */
#define MUSTEK_FLAG_ENLARGE_X   (1 << 15)  /* need to enlarge x-res */
#define MUSTEK_FLAG_PRO         (1 << 16)  /* Professional series model */


/* source values: */
#define MUSTEK_SOURCE_FLATBED	0
#define MUSTEK_SOURCE_ADF	1
#define MUSTEK_SOURCE_TA	2

/* mode values: */
#define MUSTEK_MODE_LINEART	(1 << 0)	/* grayscale 1 bit / pixel */
#define MUSTEK_MODE_GRAY	(1 << 1)	/* grayscale 8 bits / pixel */
#define MUSTEK_MODE_COLOR	(1 << 2)	/* color 24 bits / pixel */
#define MUSTEK_MODE_HALFTONE	(1 << 3)	/* use dithering */
#define MUSTEK_MODE_GRAY_FAST   (1 << 4)        /* Pro series fast grayscale */

enum Mustek_Option
  {
    OPT_NUM_OPTS = 0,

    OPT_MODE_GROUP,
    OPT_MODE,
    OPT_RESOLUTION,
    OPT_SPEED,
    OPT_SOURCE,
    OPT_BACKTRACK,
    OPT_PREVIEW,
    OPT_GRAY_PREVIEW,

    OPT_GEOMETRY_GROUP,
    OPT_TL_X,			/* top-left x */
    OPT_TL_Y,			/* top-left y */
    OPT_BR_X,			/* bottom-right x */
    OPT_BR_Y,			/* bottom-right y */

    OPT_ENHANCEMENT_GROUP,
    OPT_GRAIN_SIZE,
    OPT_BRIGHTNESS,
    OPT_CONTRAST,
    OPT_CUSTOM_GAMMA,		/* use custom gamma tables? */
    /* The gamma vectors MUST appear in the order gray, red, green,
       blue.  */
    OPT_GAMMA_VECTOR,
    OPT_GAMMA_VECTOR_R,
    OPT_GAMMA_VECTOR_G,
    OPT_GAMMA_VECTOR_B,
    OPT_QUALITY_CAL,
    OPT_HALFTONE_DIMENSION,
    OPT_HALFTONE_PATTERN,

    /* must come last: */
    NUM_OPTIONS
  };

typedef union
  {
    SANE_Word w;
    SANE_Word *wa;		/* word array */
    SANE_String s;
  }
Option_Value;

typedef struct Mustek_Device
  {
    struct Mustek_Device *next;
    SANE_Device sane;
    SANE_Range dpi_range;
    SANE_Range x_range;
    SANE_Range y_range;
    /* scan area when transparency adapter is used: */
    SANE_Range x_trans_range;
    SANE_Range y_trans_range;
    unsigned flags;
    /* length of gamma table, probably always <= 4096 for the SE */
    int gamma_length;		
    /* values actually used by scanner, not necessarily the desired! */
    int bpl, lines;
    /* what is needed for calibration (ScanExpress and Pro series)*/
    struct
      {
        int bytes;
        int lines;
	unsigned char *buffer;
      }
    cal;    
    /* current and maximum buffer size of the scanner */
    int buffer_size;
    int max_buffer_size;
    /* firmware format: 0 = old, MUSTEK at pos 8; 1 = new, MUSTEK at
       pos 36 */
    int firmware_format;
    /* firmware revision system: 0 = old, x.yz; 1 = new, Vxyz */
    int firmware_revision_system;
  }
Mustek_Device;

typedef struct Mustek_Scanner
  {
    /* all the state needed to define a scan request: */
    struct Mustek_Scanner *next;

    SANE_Option_Descriptor opt[NUM_OPTIONS];
    Option_Value val[NUM_OPTIONS];
    SANE_Int gamma_table[4][256];
    SANE_Int halftone_pattern[64];

    int scanning;
    int cancelled;
    int pass;			/* pass number */
    int line;			/* current line number */
    SANE_Parameters params;

    /* Parsed option values and variables that are valid only during
       actual scanning: */
    int mode;
    int one_pass_color_scan;
    int resolution_code;
    int fd;			/* SCSI filedescriptor */
    pid_t reader_pid;		/* process id of reader */
    int pipe;			/* pipe to reader process */
    long start_time;            /* at this time the scan started */

    /* scanner dependent/low-level state: */
    Mustek_Device *hw;

    /* line-distance correction related state: */
    struct
      {
	int color;	/* first color appearing in read data */ 
	int max_value;
	int peak_res;
	int dist[3];	/* line distance */
	int index[3];	/* index for R/G/B color assignment */
	int quant[3];	/* for resolution correction */
	int saved[3];	/* number of saved color lines */
	/* these are used for SE, MFS and N line-distance correction: */
	unsigned char *buf[3];
	/* these are used for N line-distance correction only: */
	int ld_line;	/* line # currently processed in ld-correction */
	int lmod3;	/* line # modulo 3 */
      }
    ld;
  }
Mustek_Scanner;

#endif /* mustek_h */
