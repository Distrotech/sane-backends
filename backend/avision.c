/*******************************************************************************
 * SANE - Scanner Access Now Easy.

   avision.c

   This file (C) 1999, 2000 Meino Christian Cramer and Rene Rebe

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

   *****************************************************************************

   This file implements a SANE backend for the Avision AV 630CS scanner with
   SCSI-2 command set.

   (feedback to:  mccramer@s.netic.de and rene.rebe@myokay.net)

   Very much thanks to:
     Avision INC for the documentation we got! ;-)
     Gunter Wagner for some fixes and the transparency option

********************************************************************************/

#include <sane/config.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/time.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>
#include <sane/sanei_scsi.h>
#include <sane/sanei_config.h>
#include <sane/sanei_backend.h>

#include <avision.h>

/* For timeval... */
#ifdef DEBUG
#include <sys/time.h>
#endif


#define BACKEND_NAME	avision

#ifndef PATH_MAX
# define PATH_MAX	1024
#endif

#define AVISION_CONFIG_FILE "avision.conf"

#define MM_PER_INCH	(254.0 / 10.0)

static int num_devices;
static Avision_Device *first_dev;
static Avision_Scanner *first_handle;

static const SANE_String_Const mode_list[] =
{
    "Line Art", "Dithered", "Gray", "Color", 0
};

/* avision_res will be overwritten in init_options() !!! */

static const SANE_Range u8_range =
  {
      0,		/* minimum */
    255,		/* maximum */
      0			/* quantization */
  };

static const SANE_Range percentage_range =
  {
    SANE_FIX(-100),	/* minimum */
    SANE_FIX( 100),	/* maximum */
    SANE_FIX( 1  )	/* quantization */
  };

static const SANE_Range abs_percentage_range =
  {
    SANE_FIX( 0),	/* minimum */
    SANE_FIX( 100),	/* maximum */
    SANE_FIX( 1  )	/* quantization */
  };


#define INQ_LEN	0x60

static const u_int8_t inquiry[] =
{
  AVISION_SCSI_INQUIRY, 0x00, 0x00, 0x00, INQ_LEN, 0x00
};

static const u_int8_t test_unit_ready[] =
{
  AVISION_SCSI_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t stop[] =
{
  AVISION_SCSI_START_STOP, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const u_int8_t get_status[] =
{
  AVISION_SCSI_GET_DATA_STATUS, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                      0x00, 0x00, 0x0c, 0x00
};

static int make_mode (char *mode)
{
    DBG(3, "make_mode\n" );
    
    if (strcmp (mode, "Line Art") == 0)
	return THRESHOLDED;
    if (strcmp (mode, "Dithered") == 0)
	return DITHERED;
    else if (strcmp (mode, "Gray") == 0)
	return GREYSCALE;
    else if (strcmp (mode, "Color") == 0)
	return TRUECOLOR;
    
    return -1;
}

static SANE_Status
wait_ready (int fd)
{
  SANE_Status status;
  int i;

  for (i = 0; i < 1000; ++i)
    {
      DBG(3, "wait_ready: sending TEST_UNIT_READY\n");
      status = sanei_scsi_cmd (fd, test_unit_ready, sizeof (test_unit_ready),
			       0, 0);
      switch (status)
	{
	default:
	  /* Ignore errors while waiting for scanner to become ready.
	     Some SCSI drivers return EIO while the scanner is
	     returning to the home position.  */
	  DBG(1, "wait_ready: test unit ready failed (%s)\n",
	      sane_strstatus (status));
	  /* fall through */
	case SANE_STATUS_DEVICE_BUSY:
	  usleep (100000);	/* retry after 100ms */
	  break;

	case SANE_STATUS_GOOD:
	  return status;
	}
    }
  DBG(1, "wait_ready: timed out after %d attempts\n", i);
  return SANE_STATUS_INVAL;
}


static SANE_Status
sense_handler (int fd, u_char *sense, void *arg)
{
    /*MCC*/
    
    int i;
    
    SANE_Status status;
    
    SANE_Bool ASC_switch;
    
    DBG(3, "sense_handler\n");
    
    ASC_switch = SANE_FALSE;
    
    switch (sense[0])
	{
	case 0x00:
	    status = SANE_STATUS_GOOD;
	    
	default:
	    DBG(1, "sense_handler: got unknown sense code %02x\n", sense[0]);
	    status = SANE_STATUS_IO_ERROR;
	}
    
    for (i = 0; i < 21; i++)
	{
	    DBG(1, "%d:[%x]\n", i, sense[i]);
	}
    
    if (sense[VALID_BYTE] & VALID)
	{
	    switch (sense[ERRCODE_BYTE] & ERRCODEMASK)
		{
		case ERRCODESTND:
		    {
			DBG (5, "SENSE: STANDARD ERROR CODE\n" );
			break;
		    }
		case ERRCODEAV:
		    {
			DBG (5, "SENSE: AVISION SPECIFIC ERROR CODE\n" );
			break;
		    }
		}
	    
	    switch (sense[SENSEKEY_BYTE] & SENSEKEY_MASK)
		{
		    
		case NOSENSE          :
		    {
			DBG (5, "SENSE: NO SENSE\n" );
			break;
		    }
		case NOTREADY         :
		    {
			DBG (5, "SENSE: NOT READY\n" );
			break;
		    }
		case MEDIUMERROR      :
		    {
			DBG (5, "SENSE: MEDIUM ERROR\n" );
			break;
		    }
		case HARDWAREERROR    :
		    {
			DBG (5, "SENSE: HARDWARE FAILURE\n" );
			break;
		    }
		case ILLEGALREQUEST   :
		    {
			DBG (5, "SENSE: ILLEGAL REQUEST\n" );
			ASC_switch = SANE_TRUE;                   
			break;
		    }
		case UNIT_ATTENTION   :
		    {
			DBG (5, "SENSE: UNIT ATTENTION\n" );
			break;
		    }
		case VENDORSPEC       :
		    {
			DBG (5, "SENSE: VENDOR SPECIFIC\n" );
			break;
		    }
		case ABORTEDCOMMAND   :
		    {
			DBG (5, "SENSE: COMMAND ABORTED\n" );
			break;
		    }
		}

	    if (sense[EOS_BYTE] & EOSMASK)
		{
		    DBG (5, "SENSE: END OF SCAN\n" );
		}
	    else
		{
		    DBG (5, "SENSE: SCAN HAS NOT YET BEEN COMPLETED\n" );
		}

	    if (sense[ILI_BYTE] & INVALIDLOGICLEN)
		{
		    DBG (5, "SENSE: INVALID LOGICAL LENGTH\n" );
		}


	    if ((sense[ASC_BYTE] != 0) && (sense[ASCQ_BYTE] != 0))
		{
		    if (sense[ASC_BYTE] == ASCFILTERPOSERR)
			{
			    DBG (5, "X\n");
			}

		    if (sense[ASCQ_BYTE] == ASCQFILTERPOSERR)
			{
			    DBG (5, "X\n");
			}

		    if (sense[ASCQ_BYTE] == ASCQFILTERPOSERR)
			{
			    DBG (5, "SENSE: FILTER POSITIONING ERROR\n" );
			}

		    if ((sense[ASC_BYTE] == ASCFILTERPOSERR) && 
			(sense[ASCQ_BYTE] == ASCQFILTERPOSERR))
			{
			    DBG (5, "SENSE: FILTER POSITIONING ERROR\n" );
			}

		    if ((sense[ASC_BYTE] == ASCADFPAPERJAM) && 
			(sense[ASCQ_BYTE] == ASCQADFPAPERJAM ))
			{
			    DBG(5, "ADF Paper Jam\n" );
			}
		    if ((sense[ASC_BYTE] == ASCADFCOVEROPEN) &&
			(sense[ASCQ_BYTE] == ASCQADFCOVEROPEN))
			{
			    DBG(5, "ADF Cover Open\n" );
			}
		    if ((sense[ASC_BYTE] == ASCADFPAPERCHUTEEMPTY) &&
			(sense[ASCQ_BYTE] == ASCQADFPAPERCHUTEEMPTY))
			{
			    DBG(5, "ADF Paper Chute Empty\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINTERNALTARGETFAIL) &&
			(sense[ASCQ_BYTE] == ASCQINTERNALTARGETFAIL))
			{
			    DBG(5, "Internal Target Failure\n" );
			}
		    if ((sense[ASC_BYTE] == ASCSCSIPARITYERROR) &&
			(sense[ASCQ_BYTE] == ASCQSCSIPARITYERROR))
			{
			    DBG(5, "SCSI Parity Error\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINVALIDCOMMANDOPCODE) &&
			(sense[ASCQ_BYTE] == ASCQINVALIDCOMMANDOPCODE))
			{
			    DBG(5, "Invalid Command Operation Code\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINVALIDFIELDCDB) &&
			(sense[ASCQ_BYTE] == ASCQINVALIDFIELDCDB))
			{
			    DBG(5, "Invalid Field in CDB\n" );
			}
		    if ((sense[ASC_BYTE] == ASCLUNNOTSUPPORTED) &&
			(sense[ASCQ_BYTE] == ASCQLUNNOTSUPPORTED))
			{
			    DBG(5, "Logical Unit Not Supported\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINVALIDFIELDPARMLIST) &&
			(sense[ASCQ_BYTE] == ASCQINVALIDFIELDPARMLIST))
			{
			    DBG(5, "Invalid Field in parameter List\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINVALIDCOMBINATIONWIN) &&
			(sense[ASCQ_BYTE] == ASCQINVALIDCOMBINATIONWIN))
			{
			    DBG(5, "Invalid  Combination  of  Window  Specified\n" );
			}
		    if ((sense[ASC_BYTE] == ASCMSGERROR) &&
			(sense[ASCQ_BYTE] == ASCQMSGERROR))
			{
			    DBG(5, "Message Error\n" );
			}
		    if ((sense[ASC_BYTE] == ASCCOMMCLREDANOTHINITIATOR) &&
			(sense[ASCQ_BYTE] == ASCQCOMMCLREDANOTHINITIATOR))
			{
			    DBG(5, "Command Cleared By Another Initiator.\n" );
			}
		    if ((sense[ASC_BYTE] == ASCIOPROCTERMINATED) &&
			(sense[ASCQ_BYTE] == ASCQIOPROCTERMINATED))
			{
			    DBG(5, "I/O process Terminated.\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINVBITIDMSG) &&
			(sense[ASCQ_BYTE] == ASCQINVBITIDMSG))
			{
			    DBG(5, "Invalid Bit in Identify Message\n" );
			}
		    if ((sense[ASC_BYTE] == ASCINVMSGERROR) &&
			(sense[ASCQ_BYTE] == ASCQINVMSGERROR))
			{
			    DBG(5, "Invalid Message Error\n" );
			}
		    if ((sense[ASC_BYTE] == ASCLAMPFAILURE) &&
			(sense[ASCQ_BYTE] == ASCQLAMPFAILURE))
			{
			    DBG(5, "Lamp Failure\n" );
			}
		    if ((sense[ASC_BYTE] == ASCMECHPOSERROR) &&
			(sense[ASCQ_BYTE] == ASCQMECHPOSERROR))
			{
			    DBG(5, "Mechanical Positioning Error\n" );
			}
		    if ((sense[ASC_BYTE] == ASCPARAMLISTLENERROR) &&
			(sense[ASCQ_BYTE] == ASCQPARAMLISTLENERROR))
			{
			    DBG(5, "Parameter List Length Error\n" );
			}
		    if ((sense[ASC_BYTE] == ASCPARAMNOTSUPPORTED) &&
			(sense[ASCQ_BYTE] == ASCQPARAMNOTSUPPORTED))
			{
			    DBG(5, "Parameter Not Supported\n" );
			}
		    if ((sense[ASC_BYTE]  == ASCPARAMVALINVALID  ) &&
			(sense[ASCQ_BYTE] == ASCQPARAMVALINVALID )  )
			{
			    DBG(5, "Parameter Value Invalid\n" );
			}
		    if ((sense[ASC_BYTE]  == ASCPOWERONRESET) &&
			(sense[ASCQ_BYTE] == ASCQPOWERONRESET)) 
			{
			    DBG(5, "Power-on, Reset, or Bus Device Reset Occurred\n" );
			}
		    if ((sense[ASC_BYTE] == ASCSCANHEADPOSERROR) &&
			(sense[ASCQ_BYTE] == ASCQSCANHEADPOSERROR))
			{
			    DBG (5, "SENSE: FILTER POSITIONING ERROR\n" );
			}

		}
	    else
		{
		    DBG(5, "No Additional Sense Information\n" );
		}

	    if (ASC_switch == SANE_TRUE)
		{
		    if (sense[SKSV_BYTE] & SKSVMASK)
			{
			    if (sense[CD_BYTE] & CDMASK)
				{
				    DBG (5, "SENSE: ERROR IN COMMAND PARAMETER...\n" );
				}
			    else
				{
				    DBG (5, "SENSE: ERROR IN DATA PARAMETER...\n" );
				}
                    
			    if (sense[BPV_BYTE] & BPVMASK)
				{
				    DBG(5, "BIT %d ERRORNOUS OF\n", (int)sense[BITPOINTER_BYTE] & BITPOINTERMASK);
				}
                    
			    DBG(5, "ERRORNOUS BYTE %d \n", (int)sense[BYTEPOINTER_BYTE1] );
                    
			}
               
		}
          
          
	}
    return status;
}


static SANE_Status
attach (const char *devname, Avision_Device **devp)
{
  char result[INQ_LEN];
  int fd;
  Avision_Device *dev;
  SANE_Status status;
  size_t size;
  char *mfg, *model;
  char *p;

  DBG(3, "attach\n" );

  for (dev = first_dev; dev; dev = dev->next)
    if (strcmp (dev->sane.name, devname) == 0) {
      if (devp)
	*devp = dev;
      return SANE_STATUS_GOOD;
    }

  DBG(3, "attach: opening %s\n", devname);
  status = sanei_scsi_open (devname, &fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD) {
    DBG(1, "attach: open failed (%s)\n", sane_strstatus (status));
    return SANE_STATUS_INVAL;
  }

  DBG(3, "attach: sending INQUIRY\n");
  size = sizeof (result);
  status = sanei_scsi_cmd (fd, inquiry, sizeof (inquiry), result, &size);
  if (status != SANE_STATUS_GOOD || size != INQ_LEN) {
    DBG(1, "attach: inquiry failed (%s)\n", sane_strstatus (status));
    sanei_scsi_close (fd);
    return status;
  }

  status = wait_ready (fd);
  sanei_scsi_close (fd);
  if (status != SANE_STATUS_GOOD)
    return status;
  
  /* DEBUG CODE!! To test new Scanenr Output...
  printf ("RAW.Result: ");
  for (i = 0; i < sizeof (result); i++)
  {
    printf ("%d:[%c-%x], ", i, result[i], (unsigned char) result[i]);
  }
  printf ("\n");
  */
  
  result[33]= '\0';
  p = strchr(result+16,' ');
  if (p) *p = '\0';
  model = strdup (result+16);

  result[16]= '\0';
  p = strchr(result+8,' ');
  if (p) *p = '\0';
  mfg = strdup (result+8);

  DBG(1, "attach: Inquiry gives mfg=%s, model=%s.\n", mfg, model);
  
  if (strcmp (mfg, "AVISION") != 0) {
    DBG(1, "attach: device doesn't look like a AVISION scanner "
	   "(result[0]=%#02x)\n", result[0]);
    return SANE_STATUS_INVAL;
  }
  
  dev = malloc (sizeof (*dev));
  if (!dev)
    return SANE_STATUS_NO_MEM;
  
  memset (dev, 0, sizeof (*dev));

  dev->sane.name   = strdup (devname);
  dev->sane.vendor = "AVISION";
  dev->sane.model  = model;
  dev->sane.type   = "flatbed scanner";

  dev->x_range.min = 0;
  dev->y_range.min = 0;
  /* Getting max X and max > ...*/
  /* Doesn't work! Avision doesn't return the information! ...
  dev->x_range.max = SANE_FIX ( (int) ((((int) result[81] << 8) + result[82]) / 300) * MM_PER_INCH);
  dev->y_range.max = SANE_FIX ( (int) ((((int) result[83] << 8) + result[84]) / 300) * MM_PER_INCH);
  */  
  dev->x_range.max = SANE_FIX ( 8.5 * MM_PER_INCH);
  dev->y_range.max = SANE_FIX (11.8 * MM_PER_INCH);

  dev->x_range.quant = 0;
  dev->y_range.quant = 0;
  
  /* Maybe an other Avision Scanner returns this ... and I like test it...
  printf ("X-Range: %d inch\n", dev->x_range.max);
  printf ("Y-Range: %d inch\n", dev->y_range.max);

  printf ("Raw-Range: %d, %d, %d, %d\n", (int)result[81], (int)result[82], (int)result[83], (int)result[84]);
  */
  
  dev->dpi_range.min = 50;
  dev->dpi_range.quant = 1;
  dev->dpi_range.max = 1200;

   DBG(3, "attach: found AVISION scanner model %s (%s)\n",
      dev->sane.model, dev->sane.type);

  ++num_devices;
  dev->next = first_dev;
  first_dev = dev;

  if (devp)
    *devp = dev;
  return SANE_STATUS_GOOD;
}


static size_t
max_string_size (const SANE_String_Const strings[])
{
  size_t size, max_size = 0;
  int i;

  DBG(3, "max_string_size\n" );

  for (i = 0; strings[i]; ++i)
    {
      size = strlen (strings[i]) + 1;
      if (size > max_size)
	max_size = size;
    }
  return max_size;
}


static SANE_Status
constrain_value (Avision_Scanner *s, SANE_Int option, void *value,
		 SANE_Int *info)
{
     DBG(3, "constrain_value\n" );
  return sanei_constrain_value (s->opt + option, value, info);
}


/* Will we need this ever??? Clean up in next release Meino???
static unsigned char sign_mag (double val)
{
  if (val >  100) val =  100;
  if (val < -100) val = -100;
  if (val >= 0) return ( val);
  else          return ((unsigned char)(-val)) | 0x80;
}
*/

static SANE_Status
scan_area_and_windows (Avision_Scanner *s)
{
    SANE_Status status;
    struct def_win_par dwp;
   
    DBG(3, "scan_area_and_windows\n" );

    /* wipe out anything
     */
    memset (&dwp,'\0',sizeof (dwp));

    /* command setup
     */
    dwp.dwph.opc = AVISION_SCSI_AREA_AND_WINDOWS;
    set_triple (dwp.dwph.len,sizeof (dwp.wdh) + sizeof (dwp.wdb)); 
    set_double (dwp.wdh.wpll, sizeof (dwp.wdb));

    /* resolution parameters
     */
    set_double (dwp.wdb.xres, s->avdimen.res);
    set_double (dwp.wdb.yres, s->avdimen.res);
     
    /* upper left corner coordinates
     */
    set_quad (dwp.wdb.ulx, s->avdimen.tlx);
    set_quad (dwp.wdb.uly, s->avdimen.tly);

    /* width and length in inch/1200
     */
    set_quad (dwp.wdb.width, s->avdimen.wid);
    set_quad (dwp.wdb.length, s->avdimen.len);

    /* width and length in bytes
     */
    set_double (dwp.wdb.linewidth, s->params.bytes_per_line );
    set_double (dwp.wdb.linecount, s->params.lines );

    dwp.wdb.bitset1        = 0x60;
    dwp.wdb.bitset1       |= s->val[OPT_SPEED].w;

    dwp.wdb.bitset2        = 0x00;

    /* quality scan option switch
     */
    if( s->val[OPT_QSCAN].w == SANE_TRUE )
	{
	    dwp.wdb.bitset2  |= AV_QSCAN_ON; /* Q_SCAN  ON */
	}

    /* quality calibration option switch
     */
    if( s->val[OPT_QCALIB].w == SANE_TRUE )
	{
	    dwp.wdb.bitset2  |= AV_QCALIB_ON;  /* Q_CALIB ON */
	}
    /* transparency switch
     */
    if( s->val[OPT_TRANS].w == SANE_TRUE )
        {
            dwp.wdb.bitset2 |= AV_TRANS_ON; /* Set to transparency mode */
        }
    
    /* fixed value
     */
    dwp.wdb.pad_type       = 3;
    dwp.wdb.vendor_specid  = 0xFF;
    dwp.wdb.paralen        = 9;

    /* currently also fixed
       (and unsopported by all Avision scanner I know ...)
    */
    dwp.wdb.highlight      = 0xFF;
    dwp.wdb.shadow         = 0x00;
    
    /* mode dependant settings
     */
    switch (s->mode) {

    case THRESHOLDED:
	dwp.wdb.bpp = 1;
	dwp.wdb.image_comp = 0;
	dwp.wdb.bitset1        &= 0xC7;
	dwp.wdb.thresh     = 1 + 2.55 *   (SANE_UNFIX (s->val[OPT_THRESHOLD].w)); 
	dwp.wdb.brightness = 128 + 1.28 * (SANE_UNFIX (s->val[OPT_BRIGHTNESS].w)); 
	dwp.wdb.contrast   = 128 + 1.28 * (SANE_UNFIX (s->val[OPT_CONTRAST].w)); 
	break;

    case DITHERED:
	dwp.wdb.bpp = 1;
	dwp.wdb.image_comp = 1;
	dwp.wdb.bitset1        &= 0xC7;
	dwp.wdb.thresh     = 1 + 2.55 *   (SANE_UNFIX (s->val[OPT_THRESHOLD].w)); 
	dwp.wdb.brightness = 128 + 1.28 * (SANE_UNFIX (s->val[OPT_BRIGHTNESS].w)); 
	dwp.wdb.contrast   = 128 + 1.28 * (SANE_UNFIX (s->val[OPT_CONTRAST].w)); 
	break;

    case GREYSCALE:
	dwp.wdb.bpp = 8;
	dwp.wdb.image_comp = 2;
	dwp.wdb.bitset1        &= 0xC7;
	/*dwp.wdb.bitset1        |= 0x30; *//* thanks Gunter */
	break;

    case TRUECOLOR:
	dwp.wdb.bpp = 8;
	dwp.wdb.image_comp = 5;
	break;

    default:
	DBG(3, "Invalid mode. %d\n", s->mode);
	exit (1);
    }

    /* set window command
     */
    status = sanei_scsi_cmd (s->fd, &dwp, sizeof (dwp), 0, 0);

    /* back to caller 
     */
    return status;
}

static SANE_Status
start_scan (Avision_Scanner *s)
{
    struct command_scan cmd;
    
    DBG(3, "start_scan\n" );

    memset (&cmd,'\0',sizeof (cmd));
    cmd.opc = AVISION_SCSI_START_STOP;
    cmd.transferlen = 1;

    if (s->val[OPT_PREVIEW].w == SANE_TRUE) {
	cmd.bitset1 |= 0x01<<6;
    }
    else {
	cmd.bitset1 &= ~(0x01<<6);
    }

    if (s->val[OPT_QSCAN].w == SANE_TRUE) {
	cmd.bitset1 |= 0x01<<7;
    }
    else {
	cmd.bitset1 &= ~(0x01<<7);
    }
    
    return sanei_scsi_cmd (s->fd, &cmd, sizeof (cmd), 0, 0);
}


static SANE_Status
stop_scan (Avision_Scanner *s)
{ 
  /* XXX I don't think a AVISION can stop in mid-scan. Just stop
     sending it requests for data.... 
   */
  DBG(3, "stop_scan\n" );

  return sanei_scsi_cmd (s->fd, stop, sizeof (stop), 0, 0);
}


static SANE_Status
do_eof (Avision_Scanner *s)
{
  int childstat;

  DBG(3, "do_eof\n" );


  if (s->pipe >= 0)
    {
      close (s->pipe);
      s->pipe = -1;
    }
  wait (&childstat); /* added: mcc, without a wait()-call you will produce
                        zombie childs */

  return SANE_STATUS_EOF;
}


static SANE_Status
do_cancel (Avision_Scanner *s)
{

  DBG(3, "do_cancel\n" );

  s->scanning = SANE_FALSE;
  s->pass = 0;

  do_eof (s);

  if (s->reader_pid > 0)
    {
      int exit_status;

      /* ensure child knows it's time to stop: */
      kill (s->reader_pid, SIGTERM);
      while (wait (&exit_status) != s->reader_pid);
      s->reader_pid = 0;
    }

  if (s->fd >= 0)
    {
      stop_scan (s);
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }

  return SANE_STATUS_CANCELLED;
}

static SANE_Status
read_data (Avision_Scanner *s, SANE_Byte *buf, int lines, int bpl)
{
  struct command_read rcmd;
  size_t nbytes;
  SANE_Status status;

  DBG(3, "read_data\n" );

  nbytes = bpl * lines;
  memset (&rcmd,'\0',sizeof (rcmd));
  rcmd.opc = 0x28;
  set_triple (rcmd.transferlen,nbytes);
  rcmd.datatypequal[0]=0x0d;
  rcmd.datatypequal[1]=0x0a;
  
  DBG(3, "read_data: bytes %d\n", nbytes );

  status = sanei_scsi_cmd (s->fd, &rcmd, sizeof (rcmd), buf, &nbytes);
  
  return status;
}



static SANE_Status
init_options (Avision_Scanner *s)
{
  int i;

  DBG(3, "init_options\n" );

  memset (s->opt, 0, sizeof (s->opt));
  memset (s->val, 0, sizeof (s->val));

  for (i = 0; i < NUM_OPTIONS; ++i) {
    s->opt[i].size = sizeof (SANE_Word);
    s->opt[i].cap = SANE_CAP_SOFT_SELECT | SANE_CAP_SOFT_DETECT;
  }

  s->opt[OPT_NUM_OPTS].title = SANE_TITLE_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].desc = SANE_DESC_NUM_OPTIONS;
  s->opt[OPT_NUM_OPTS].cap = SANE_CAP_SOFT_DETECT;
  s->val[OPT_NUM_OPTS].w = NUM_OPTIONS;

  /* "Mode" group: */
  s->opt[OPT_MODE_GROUP].title = "Scan Mode";
  s->opt[OPT_MODE_GROUP].desc = "";
  s->opt[OPT_MODE_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_MODE_GROUP].cap = 0;
  s->opt[OPT_MODE_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* scan mode */
  s->opt[OPT_MODE].name = SANE_NAME_SCAN_MODE;
  s->opt[OPT_MODE].title = SANE_TITLE_SCAN_MODE;
  s->opt[OPT_MODE].desc = "Select the scan mode";
  s->opt[OPT_MODE].type = SANE_TYPE_STRING;
  s->opt[OPT_MODE].size = max_string_size (mode_list);
  s->opt[OPT_MODE].constraint_type = SANE_CONSTRAINT_STRING_LIST;
  s->opt[OPT_MODE].constraint.string_list = mode_list;
  s->val[OPT_MODE].s = strdup (mode_list[OPT_MODE_DEFAULT]);
  
  /* set the internal mode int */
  s->mode = make_mode (s->val[OPT_MODE].s);
    
  /* resolution */
  s->opt[OPT_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
  s->opt[OPT_RESOLUTION].type = SANE_TYPE_INT;
  s->opt[OPT_RESOLUTION].unit = SANE_UNIT_DPI;
  s->opt[OPT_RESOLUTION].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_RESOLUTION].constraint.range = &s->hw->dpi_range;
  s->val[OPT_RESOLUTION].w = OPT_RESOLUTION_DEFAULT;

  /* preview */
  s->opt[OPT_PREVIEW].name = SANE_NAME_PREVIEW;
  s->opt[OPT_PREVIEW].title = SANE_TITLE_PREVIEW;
  s->opt[OPT_PREVIEW].desc = SANE_DESC_PREVIEW;
  s->opt[OPT_PREVIEW].cap = SANE_CAP_SOFT_DETECT | SANE_CAP_SOFT_SELECT;
  s->val[OPT_PREVIEW].w = 0;

  /* speed option */
  s->hw->speed_range.min = (SANE_Int)0;
  s->hw->speed_range.max = (SANE_Int)4;
  s->hw->speed_range.quant = (SANE_Int)1;

  s->opt[OPT_SPEED].name  = SANE_NAME_SCAN_SPEED;
  s->opt[OPT_SPEED].title = SANE_TITLE_SCAN_SPEED;
  s->opt[OPT_SPEED].desc  = SANE_DESC_SCAN_SPEED;
  s->opt[OPT_SPEED].type  = SANE_TYPE_INT;
  s->opt[OPT_SPEED].constraint_type  = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_SPEED].constraint.range = &s->hw->speed_range; 
  s->val[OPT_SPEED].w = 0;

  /* "Geometry" group: */
  s->opt[OPT_GEOMETRY_GROUP].title = "Geometry";
  s->opt[OPT_GEOMETRY_GROUP].desc = "";
  s->opt[OPT_GEOMETRY_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_GEOMETRY_GROUP].cap = SANE_CAP_ADVANCED;
  s->opt[OPT_GEOMETRY_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* top-left x */
  s->opt[OPT_TL_X].name = SANE_NAME_SCAN_TL_X;
  s->opt[OPT_TL_X].title = SANE_TITLE_SCAN_TL_X;
  s->opt[OPT_TL_X].desc = SANE_DESC_SCAN_TL_X;
  s->opt[OPT_TL_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_X].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_X].constraint.range = &s->hw->x_range;
  s->val[OPT_TL_X].w = 0;

  /* top-left y */
  s->opt[OPT_TL_Y].name = SANE_NAME_SCAN_TL_Y;
  s->opt[OPT_TL_Y].title = SANE_TITLE_SCAN_TL_Y;
  s->opt[OPT_TL_Y].desc = SANE_DESC_SCAN_TL_Y;
  s->opt[OPT_TL_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_TL_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_TL_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_TL_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_TL_Y].w = 0;

  /* bottom-right x */
  s->opt[OPT_BR_X].name = SANE_NAME_SCAN_BR_X;
  s->opt[OPT_BR_X].title = SANE_TITLE_SCAN_BR_X;
  s->opt[OPT_BR_X].desc = SANE_DESC_SCAN_BR_X;
  s->opt[OPT_BR_X].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_X].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_X].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_X].constraint.range = &s->hw->x_range;
  s->val[OPT_BR_X].w = s->hw->x_range.max;

  /* bottom-right y */
  s->opt[OPT_BR_Y].name = SANE_NAME_SCAN_BR_Y;
  s->opt[OPT_BR_Y].title = SANE_TITLE_SCAN_BR_Y;
  s->opt[OPT_BR_Y].desc = SANE_DESC_SCAN_BR_Y;
  s->opt[OPT_BR_Y].type = SANE_TYPE_FIXED;
  s->opt[OPT_BR_Y].unit = SANE_UNIT_MM;
  s->opt[OPT_BR_Y].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BR_Y].constraint.range = &s->hw->y_range;
  s->val[OPT_BR_Y].w = s->hw->y_range.max;

  /* "Enhancement" group: */
  s->opt[OPT_ENHANCEMENT_GROUP].title = "Enhancement";
  s->opt[OPT_ENHANCEMENT_GROUP].desc = "";
  s->opt[OPT_ENHANCEMENT_GROUP].type = SANE_TYPE_GROUP;
  s->opt[OPT_ENHANCEMENT_GROUP].cap = 0;
  s->opt[OPT_ENHANCEMENT_GROUP].constraint_type = SANE_CONSTRAINT_NONE;

  /* transparency adapter. */
  s->opt[OPT_TRANS].name = "transparency";
  s->opt[OPT_TRANS].title = "transparency";
  s->opt[OPT_TRANS].desc = "Switch transparency mode on.";
  s->opt[OPT_TRANS].type = SANE_TYPE_BOOL;
  s->opt[OPT_TRANS].unit = SANE_UNIT_NONE;
  s->val[OPT_TRANS].w = SANE_FALSE;
  
  /* brightness */
  s->opt[OPT_BRIGHTNESS].name = SANE_NAME_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].title = SANE_TITLE_BRIGHTNESS;
  s->opt[OPT_BRIGHTNESS].desc = SANE_DESC_BRIGHTNESS
    "  This option is active for lineart/halftone modes only.  "
    "For multibit modes (grey/color) use the gamma-table(s).";
  s->opt[OPT_BRIGHTNESS].type = SANE_TYPE_FIXED;
  s->opt[OPT_BRIGHTNESS].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_BRIGHTNESS].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_BRIGHTNESS].constraint.range = &percentage_range;
  s->val[OPT_BRIGHTNESS].w = SANE_FIX(0);

  /* contrast */
  s->opt[OPT_CONTRAST].name = SANE_NAME_CONTRAST;
  s->opt[OPT_CONTRAST].title = SANE_TITLE_CONTRAST;
  s->opt[OPT_CONTRAST].desc = SANE_DESC_CONTRAST
    "  This option is active for lineart/halftone modes only.  "
    "For multibit modes (grey/color) use the gamma-table(s).";
  s->opt[OPT_CONTRAST].type = SANE_TYPE_FIXED;
  s->opt[OPT_CONTRAST].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_CONTRAST].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_CONTRAST].constraint.range = &percentage_range;
  s->val[OPT_CONTRAST].w = SANE_FIX(0);

  /* Threshold */
  s->opt[OPT_THRESHOLD].name = SANE_NAME_THRESHOLD;
  s->opt[OPT_THRESHOLD].title = SANE_TITLE_THRESHOLD;
  s->opt[OPT_THRESHOLD].desc = SANE_DESC_THRESHOLD;
  s->opt[OPT_THRESHOLD].type = SANE_TYPE_FIXED;
  s->opt[OPT_THRESHOLD].unit = SANE_UNIT_PERCENT;
  s->opt[OPT_THRESHOLD].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_THRESHOLD].constraint.range = &abs_percentage_range;
  s->opt[OPT_THRESHOLD].cap = SANE_CAP_INACTIVE;
  s->val[OPT_THRESHOLD].w = SANE_FIX(50);

  /* Quality Scan */
  s->opt[OPT_QSCAN].name   = "Qualitiy Scan";
  s->opt[OPT_QSCAN].title  = "Quality Scan";
  s->opt[OPT_QSCAN].desc   = "Turn on quality scanning (slower & better)";
  s->opt[OPT_QSCAN].type   = SANE_TYPE_BOOL;
  s->opt[OPT_QSCAN].unit   = SANE_UNIT_NONE;
  s->val[OPT_QSCAN].w      = SANE_TRUE;

  /* Quality Calibration */
  s->opt[OPT_QCALIB].name  = SANE_NAME_QUALITY_CAL;
  s->opt[OPT_QCALIB].title = SANE_TITLE_QUALITY_CAL;
  s->opt[OPT_QCALIB].desc  = SANE_DESC_QUALITY_CAL;
  s->opt[OPT_QCALIB].type  = SANE_TYPE_BOOL;
  s->opt[OPT_QCALIB].unit  = SANE_UNIT_NONE;
  s->val[OPT_QCALIB].w     = SANE_TRUE;

#if 0
  /* custom-gamma table */
  s->opt[OPT_CUSTOM_GAMMA].name = SANE_NAME_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].title = SANE_TITLE_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].desc = SANE_DESC_CUSTOM_GAMMA;
  s->opt[OPT_CUSTOM_GAMMA].type = SANE_TYPE_BOOL;
  s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
  s->val[OPT_CUSTOM_GAMMA].w = SANE_FALSE;

  /* grayscale gamma vector */
  s->opt[OPT_GAMMA_VECTOR].name = SANE_NAME_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].title = SANE_TITLE_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].desc = SANE_DESC_GAMMA_VECTOR;
  s->opt[OPT_GAMMA_VECTOR].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR].wa = &s->gamma_table[0][0];

  /* red gamma vector */
  s->opt[OPT_GAMMA_VECTOR_R].name = SANE_NAME_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].title = SANE_TITLE_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].desc = SANE_DESC_GAMMA_VECTOR_R;
  s->opt[OPT_GAMMA_VECTOR_R].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_R].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_R].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_R].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_R].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_R].wa = &s->gamma_table[1][0];

  /* green gamma vector */
  s->opt[OPT_GAMMA_VECTOR_G].name = SANE_NAME_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].title = SANE_TITLE_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].desc = SANE_DESC_GAMMA_VECTOR_G;
  s->opt[OPT_GAMMA_VECTOR_G].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_G].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_G].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_G].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_G].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_G].wa = &s->gamma_table[2][0];

  /* blue gamma vector */
  s->opt[OPT_GAMMA_VECTOR_B].name = SANE_NAME_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].title = SANE_TITLE_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].desc = SANE_DESC_GAMMA_VECTOR_B;
  s->opt[OPT_GAMMA_VECTOR_B].type = SANE_TYPE_INT;
  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
  s->opt[OPT_GAMMA_VECTOR_B].unit = SANE_UNIT_NONE;
  s->opt[OPT_GAMMA_VECTOR_B].size = 256 * sizeof (SANE_Word);
  s->opt[OPT_GAMMA_VECTOR_B].constraint_type = SANE_CONSTRAINT_RANGE;
  s->opt[OPT_GAMMA_VECTOR_B].constraint.range = &u8_range;
  s->val[OPT_GAMMA_VECTOR_B].wa = &s->gamma_table[3][0];
#endif

  return SANE_STATUS_GOOD;
}


/* This function is executed as a child process.  The reason this is
   executed as a subprocess is because some (most?) generic SCSI
   interfaces block a SCSI request until it has completed.  With a
   subprocess, we can let it block waiting for the request to finish
   while the main process can go about to do more important things
   (such as recognizing when the user presses a cancel button).

   WARNING: Since this is executed as a subprocess, it's NOT possible
   to update any of the variables in the main process (in particular
   the scanner state cannot be updated).  */
static int
reader_process (Avision_Scanner *s, int fd)
{
     SANE_Byte *data;
     int lines_per_buffer, bpl;
     SANE_Status status;
     sigset_t sigterm_set;
     FILE *fp;

     DBG(3, "reader_process\n" );

     sigemptyset (&sigterm_set);
     sigaddset (&sigterm_set, SIGTERM);

     fp = fdopen (fd, "w");
     if (!fp)
          return 1;

     bpl = s->params.bytes_per_line;
     /* the "/2" is a test if scanning gets a bit faster ... ?!? ;-) 
	(see related discussions on sane-ml)
      */
     lines_per_buffer = sanei_scsi_max_request_size / bpl / 2;
     if (!lines_per_buffer)
          return 2;			/* resolution is too high */

     /* Limit the size of a single transfer to one inch. 
        XXX Add a stripsize option. */
     if (lines_per_buffer > s->val[OPT_RESOLUTION].w )
          lines_per_buffer = s->val[OPT_RESOLUTION].w;

     DBG(3, "lines_per_buffer=%d, bytes_per_line=%d\n", lines_per_buffer, bpl);

     data = malloc (lines_per_buffer * bpl);

     for (s->line = 0; s->line < s->params.lines; s->line += lines_per_buffer) {
	 if (s->line + lines_per_buffer > s->params.lines)
	     /* do the last few lines: */
	     lines_per_buffer = s->params.lines - s->line;
	 
          sigprocmask (SIG_BLOCK, &sigterm_set, 0);
          status = read_data (s, data, lines_per_buffer, bpl);
          sigprocmask (SIG_UNBLOCK, &sigterm_set, 0);
          if (status != SANE_STATUS_GOOD) {
               DBG(1, "reader_process: read_data failed with status=%d\n", status);
               return 3;
          }
          DBG(3, "reader_process: read %d lines\n", lines_per_buffer);
    
          if ((s->mode == TRUECOLOR) || (s->mode == GREYSCALE)) 
          {
               fwrite (data, lines_per_buffer, bpl, fp);
          } 
          else 
          {
	      /* in singlebit mode, the scanner returns 1 for black. ;-( --DM */
               int i;
      
               for (i = 0; i < lines_per_buffer * bpl; ++i)
               {
                    fputc (~data[i], fp);
               }
          }
     }
     fclose (fp);

     {
          char cmd[] =
          {0x17, 0, 0, 0, 0, 0};
          SANE_Status status;
       
          status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), NULL, NULL);
          if (status != SANE_STATUS_GOOD)
          {
               DBG (1, "release_unit failed\n" );
          }
       
     }

     return 0;
}


static SANE_Status
attach_one (const char *dev)
{
  attach (dev, 0);
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_init (SANE_Int *version_code, SANE_Auth_Callback authorize)
{
  char dev_name[PATH_MAX];
  size_t len;
  FILE *fp;

  DBG(3, "sane_init\n" );

  DBG_INIT();

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (AVISION_CONFIG_FILE);
  if (!fp) {
    /* default to /dev/scanner instead of insisting on config file */
    attach ("/dev/scanner", 0);
    return SANE_STATUS_GOOD;
  }

  while (sanei_config_read  (dev_name, sizeof (dev_name), fp) ) 
      {
	  if (dev_name[0] == '#')		/* ignore line comments */
	      continue;
	  len = strlen (dev_name);
	  if (dev_name[len - 1] == '\n')
	      dev_name[--len] = '\0';
	  
	  if (!len)
	      continue;			/* ignore empty lines */
	  
	  sanei_config_attach_matching_devices (dev_name, attach_one);
      }
  fclose (fp);
  return SANE_STATUS_GOOD;
}


void
sane_exit (void)
{
  Avision_Device *dev, *next;

  DBG(3, "sane_exit\n" );

  for (dev = first_dev; dev; dev = next) {
    next = dev->next;
    free ((void *) dev->sane.name);
    free ((void *) dev->sane.model);
    free (dev);
  }
}

SANE_Status
sane_get_devices (const SANE_Device ***device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  Avision_Device *dev;
  int i;

  DBG(3, "sane_get_devices\n" );

  if (devlist)
    free (devlist);

  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return SANE_STATUS_NO_MEM;

  i = 0;
  for (dev = first_dev; i < num_devices; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;
  return SANE_STATUS_GOOD;
}


SANE_Status
sane_open (SANE_String_Const devicename, SANE_Handle *handle)
{
  Avision_Device *dev;
  SANE_Status status;
  Avision_Scanner *s;
  int i, j;

  DBG(3, "sane_open:\n");

  if (devicename[0]) {
    for (dev = first_dev; dev; dev = dev->next)
      if (strcmp (dev->sane.name, devicename) == 0)
	break;

    if (!dev) {
      status = attach (devicename, &dev);
      if (status != SANE_STATUS_GOOD)
	return status;
    }
  } else {
    /* empty devicname -> use first device */
    dev = first_dev;
  }

  if (!dev)
    return SANE_STATUS_INVAL;

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));
  s->fd = -1;
  s->pipe = -1;
  s->hw = dev;
  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;


  init_options (s);

  /* insert newly opened handle into list of open handles: */
  s->next = first_handle;
  first_handle = s;

  *handle = s;
  
  return SANE_STATUS_GOOD;
}


void
sane_close (SANE_Handle handle)
{
  Avision_Scanner *prev, *s;

  DBG(3, "sane_close\n" );
  DBG(3, " \n" );

  /* remove handle from list of open handles: */
  prev = 0;
  for (s = first_handle; s; s = s->next) {
    if (s == handle)
      break;
    prev = s;
  }

  if (!s) {
    DBG(1, "close: invalid handle %p\n", handle);
    return;		/* oops, not a handle we know about */
  }

  if (s->scanning)
    do_cancel (handle);

  if (prev)
    prev->next = s->next;
  else
    first_handle = s->next;

  free (handle);
}


const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  Avision_Scanner *s = handle;
  
  DBG(3, "sane_get_option_descriptor\n" );

  if ((unsigned) option >= NUM_OPTIONS)
    return 0;
  return s->opt + option;
}

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int *info)
{
    Avision_Scanner *s = handle;
    SANE_Status status;
    SANE_Word cap;

    DBG(3, "sane_control_option\n" );

    if (info)
	*info = 0;

    if (s->scanning)
	return SANE_STATUS_DEVICE_BUSY;

    if (option >= NUM_OPTIONS)
	return SANE_STATUS_INVAL;

    cap = s->opt[option].cap;

    if (!SANE_OPTION_IS_ACTIVE (cap))
	return SANE_STATUS_INVAL;

    if (action == SANE_ACTION_GET_VALUE) {
	switch (option) {
	    /* word options: */
	case OPT_PREVIEW:
	     
	case OPT_RESOLUTION:
	case OPT_SPEED:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	     
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	case OPT_QSCAN:    
	case OPT_QCALIB:
	case OPT_TRANS:
#if 0
	case OPT_CUSTOM_GAMMA:
#endif
	    *(SANE_Word *) val = s->val[option].w;
	    return SANE_STATUS_GOOD;
	     
#if 0
	    /* word-array options: */
	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	    memcpy (val, s->val[option].wa, s->opt[option].size);
	    return SANE_STATUS_GOOD;
#endif
	     
	    /* string options: */
	case OPT_MODE:
	    strcpy (val, s->val[option].s);
	    return SANE_STATUS_GOOD;
	}
    } else if (action == SANE_ACTION_SET_VALUE) {
	if (!SANE_OPTION_IS_SETTABLE (cap))
	    return SANE_STATUS_INVAL;
	 
	status = constrain_value (s, option, val, info);
	if (status != SANE_STATUS_GOOD)
	    return status;
	 
	switch (option)
	    {
		/* (mostly) side-effect-free word options: */
	    case OPT_RESOLUTION:
	    case OPT_SPEED:
	    case OPT_TL_X:
	    case OPT_TL_Y:
	    case OPT_BR_X:
	    case OPT_BR_Y:
		if (info)
		    *info |= SANE_INFO_RELOAD_PARAMS;
		/* fall through */
	    case OPT_PREVIEW:
		 
	    case OPT_BRIGHTNESS:
	    case OPT_CONTRAST:
	    case OPT_THRESHOLD:
		 
	    case OPT_QSCAN:    
	    case OPT_QCALIB:
	    case OPT_TRANS:
		s->val[option].w = *(SANE_Word *) val;
		return SANE_STATUS_GOOD;
		
#if 0
		/* side-effect-free word-array options: */
	    case OPT_GAMMA_VECTOR:
	    case OPT_GAMMA_VECTOR_R:
	    case OPT_GAMMA_VECTOR_G:
	    case OPT_GAMMA_VECTOR_B:
		memcpy (s->val[option].wa, val, s->opt[option].size);
		return SANE_STATUS_GOOD;
		 
		/* options with side-effects: */
		 
	    case OPT_CUSTOM_GAMMA:
		w = *(SANE_Word *) val;
		if (w == s->val[OPT_CUSTOM_GAMMA].w)
		    return SANE_STATUS_GOOD;		/* no change */
		 
		s->val[OPT_CUSTOM_GAMMA].w = w;
		if (w) {
		    s->mode = make_mode (s->val[OPT_MODE].s);
		     
		    if (s->mode == GREYSCALE) {
			s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		    } else if (s->mode == TRUECOLOR) {
			s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
			s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
			s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
			s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		    }
		} else {
		    s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
		}
		if (info)
		    *info |= SANE_INFO_RELOAD_OPTIONS;
		return SANE_STATUS_GOOD;
#endif
		 
	    case OPT_MODE:
		{
		    if (s->val[option].s)
			free (s->val[option].s);
		    s->val[option].s = strdup (val);
		     
		    s->mode = make_mode (s->val[OPT_MODE].s);
		     
		    if (info)
			*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
		     
		    s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_CONTRAST].cap   |= SANE_CAP_INACTIVE;
		    s->opt[OPT_THRESHOLD].cap  |= SANE_CAP_INACTIVE;
#if 0
		    s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
#endif
		     
		     
		    if (strcmp (val, "Thresholded") == 0) 
			s->opt[OPT_THRESHOLD].cap  &= ~SANE_CAP_INACTIVE;
		    else {
			s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
			s->opt[OPT_CONTRAST].cap   &= ~SANE_CAP_INACTIVE;
		    }
#if 0
		    if (!binary)
			s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
		     
		    if (s->val[OPT_CUSTOM_GAMMA].w) {
			if (strcmp (val, "Gray") == 0)
			    s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
			else if (strcmp (val, "Color") == 0) {
			    s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
			    s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
			    s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
			    s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
			}
		    }
#endif
		    return SANE_STATUS_GOOD;
		}
	    }
    }
    return SANE_STATUS_INVAL;
}


SANE_Status      
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
     Avision_Scanner *s = handle;
     
     DBG(3, "sane_get_parameters\n" );
          
     if (!s->scanning) 
     {
          double tlx,tly,brx,bry,res;

          tlx = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_TL_X].w)/254.0;
          tly = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_TL_Y].w)/254.0;
          brx = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_BR_X].w)/254.0;
          bry = 1200 * 10.0 * SANE_UNFIX(s->val[OPT_BR_Y].w)/254.0;
          res = s->val[OPT_RESOLUTION].w;
          
          s->avdimen.tlx = tlx;
          s->avdimen.tly = tly;
          s->avdimen.brx = brx;
          s->avdimen.bry = bry;
          s->avdimen.res = res;
          s->avdimen.wid = ((s->avdimen.brx-s->avdimen.tlx)/4)*4;  
          s->avdimen.len = ((s->avdimen.bry-s->avdimen.tly)/4)*4;  
          s->avdimen.pixelnum = (( s->avdimen.res * s->avdimen.wid ) / 4800)*4;
          s->avdimen.linenum  = (( s->avdimen.res * s->avdimen.len ) / 4800)*4;

          if( s->avdimen.tlx == 0 )
          {
               s->avdimen.tlx +=  4;
               s->avdimen.wid -=  4;
          }
          s->avdimen.tlx =  (s->avdimen.tlx/4)*4;

          if( s->avdimen.tly == 0 )
          {
               s->avdimen.tly +=  4;
          }

          s->params.pixels_per_line = s->avdimen.pixelnum;
          s->params.lines           = s->avdimen.linenum;

          memset (&s->params, 0, sizeof (s->params));
  
          if (s->avdimen.res > 0 && s->avdimen.wid > 0 && s->avdimen.len > 0) 
          {
               s->params.pixels_per_line = s->avdimen.pixelnum;
               s->params.lines = s->avdimen.linenum;

          }
          switch (s->mode)
          {
              case THRESHOLDED:
              {
                   s->params.format = SANE_FRAME_GRAY;                   
                   s->avdimen.pixelnum = (s->avdimen.pixelnum / 32) * 32;
                   s->params.pixels_per_line = s->avdimen.pixelnum;
                   s->params.bytes_per_line = s->avdimen.pixelnum /8;
                   s->params.depth = 1;
                   s->pass=0;
                   break;
              }
              case DITHERED:
              {
                   s->params.format = SANE_FRAME_GRAY;
                   s->avdimen.pixelnum = (s->avdimen.pixelnum / 32) * 32;
                   s->params.pixels_per_line = s->avdimen.pixelnum;
                   s->params.bytes_per_line = s->avdimen.pixelnum /8;
                   s->params.depth = 1;
                   s->pass=0;
                   break;
              }
              case GREYSCALE:
              {
                   s->params.format = SANE_FRAME_GRAY;
                   s->params.bytes_per_line  = s->avdimen.pixelnum;
                   s->params.pixels_per_line = s->avdimen.pixelnum;
                   s->params.depth = 8;
                   s->pass=0;
                   break;             
              }
              case TRUECOLOR:
              {
                   s->params.format = SANE_FRAME_RGB;
                   s->params.bytes_per_line = s->avdimen.pixelnum * 3;
                   s->params.pixels_per_line = s->avdimen.pixelnum;
                   s->params.depth = 8;
                   s->pass=0;
                   break;
              }
         }
     }
     
     s->params.last_frame =  SANE_TRUE;
     
     if (params)
     {
          *params = s->params;
     }

     s->pass=0;
     return SANE_STATUS_GOOD;
}
     

SANE_Status
sane_start (SANE_Handle handle)
{
     Avision_Scanner *s = handle;
     SANE_Status status;
     int fds[2];
     

     DBG(3, "sane_start\n" );

     /* Fisrt make sure there is no scan running!!! */

     if (s->scanning)
	 return SANE_STATUS_DEVICE_BUSY;

     /* Second make sure we have a current parameter set.  Some of the
        parameters will be overwritten below, but that's OK.  */
     status = sane_get_parameters (s, 0);

     if (status != SANE_STATUS_GOOD)
     {
          return status;
     }

     if (s->fd < 0) 
     {
          status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
          if (status != SANE_STATUS_GOOD) 
          {
               DBG(1, "open: open of %s failed: %s\n",
                   s->hw->sane.name, sane_strstatus (status));
               return status;
          }
     }

     {  /*MCC*/
          char cmd[] =
          {0x16, 0, 0, 0, 0, 0};
          SANE_Status status;
          
          status = sanei_scsi_cmd (s->fd, cmd, sizeof (cmd), NULL, NULL);
          if (status != SANE_STATUS_GOOD)
          {
               DBG (1, "reserve_unit failed\n" );
          }
     }

     status = wait_ready (s->fd);
     if (status != SANE_STATUS_GOOD) 
     {
          DBG(1, "open: wait_ready() failed: %s\n", sane_strstatus (status));
          goto stop_scanner_and_return;
     }
  
     status = scan_area_and_windows (s);
     if (status != SANE_STATUS_GOOD) 
     {
          DBG(1, "open: set scan area command failed: %s\n",
              sane_strstatus (status));
          goto stop_scanner_and_return;
     }

     s->scanning = SANE_TRUE;

     status = start_scan (s);
     if (status != SANE_STATUS_GOOD)
     {
          goto stop_scanner_and_return;
     }

     s->line = 0;

     if (pipe (fds) < 0)
     {
          return SANE_STATUS_IO_ERROR;
     }

     s->reader_pid = fork ();
     if (s->reader_pid == 0) 
     {
          sigset_t ignore_set;
          struct SIGACTION act;
    
          close (fds[0]);
    
          sigfillset (&ignore_set);
          sigdelset (&ignore_set, SIGTERM);
          sigprocmask (SIG_SETMASK, &ignore_set, 0);
    
          memset (&act, 0, sizeof (act));
          sigaction (SIGTERM, &act, 0);
    
          /* don't use exit() since that would run the atexit() handlers... */
          _exit (reader_process (s, fds[1]));
     }
     close (fds[1]);
     s->pipe = fds[0];

     return SANE_STATUS_GOOD;

stop_scanner_and_return:
     do_cancel (s);
     return status;
}


SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len, SANE_Int *len)
{
  Avision_Scanner *s = handle;
  ssize_t nread;

  DBG(3, "sane_read\n" );

  *len = 0;

  nread = read (s->pipe, buf, max_len);
  DBG(3, "sane_read:read %ld bytes\n", (long) nread);

  if (!s->scanning)
    return do_cancel (s);
  
  if (nread < 0) {
    if (errno == EAGAIN) {
      return SANE_STATUS_GOOD;
    } else {
      do_cancel (s);
      return SANE_STATUS_IO_ERROR;
    }
  }

  *len = nread;

  if (nread == 0) {
    s->pass++;
    return do_eof (s);
  }
  return SANE_STATUS_GOOD;
}


void
sane_cancel (SANE_Handle handle)
{
  Avision_Scanner *s = handle;

  DBG(3, "sane_cancel\n" );

  if (s->reader_pid > 0)
    kill (s->reader_pid, SIGTERM);
  s->scanning = SANE_FALSE;
}


SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  Avision_Scanner *s = handle;

  DBG(3, "sane_set_io_mode\n" );
  if (!s->scanning)
    return SANE_STATUS_INVAL;

  if (fcntl (s->pipe, F_SETFL, non_blocking ? O_NONBLOCK : 0) < 0)
    return SANE_STATUS_IO_ERROR;

  return SANE_STATUS_GOOD;
}


SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int *fd)
{
    Avision_Scanner *s = handle;

  DBG(3, "sane_get_select_fd\n" );

  if (!s->scanning)
    return SANE_STATUS_INVAL;

  *fd = s->pipe;
  return SANE_STATUS_GOOD;
}
