/* scanimage -- command line scanning utility
   Uses the SANE library.
   Copyright (C) 1996, 1997, 1998 Andreas Beck and David Mosberger

   You can contact me at becka@sunserver1.rz.uni-duesseldorf.de

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef _AIX
# include <lalloca.h>	/* MUST come first for AIX! */
#endif

#include <sane/config.h>
#include <lalloca.h>

#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#include <sane/sane.h>
#include <sane/sanei.h>
#include <sane/saneopts.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef HAVE_ATEXIT
# define atexit(func)	on_exit(func, 0)	/* works for SunOS, at least */
#endif

typedef struct
  {
    u_char *data;
    int Bpp;		/* bytes/pixel */
    int width;
    int height;
    int x;
    int y;
  }
Image;

static struct option basic_options[] =
{
  {"device-name", required_argument, NULL, 'd'},
  {"list-devices", no_argument, NULL, 'L'},
  {"help", no_argument, NULL, 'h'},
  {"verbose", no_argument, NULL, 'v'},
  {"test", no_argument, NULL, 'T'},
  {"version", no_argument, NULL, 'V'},
  {"batch", optional_argument, NULL, 'b'},
  {0, 0, NULL, 0}
};

#define BASE_OPTSTRING	"d:hLvVT"
#define STRIP_HEIGHT	256	/* # lines we increment image height */

static struct option * all_options;
static int option_number_len;
static int * option_number;
static SANE_Handle device;
static int verbose;
static int test;
static int help;
static const char * prog_name;
static SANE_Option_Descriptor window_option[2];
static int window[4];
static SANE_Word window_val[2];
static int window_val_user[2];	/* is width/height user-specified? */

static RETSIGTYPE
sighandler (int signum)
{
  if (device)
    {
      fprintf (stderr, "%s: stopping scanner...\n", prog_name);
      sane_cancel (device);
    }
}

static void
print_unit (SANE_Unit unit)
{
  switch (unit)
    {
    case SANE_UNIT_NONE:	break;
    case SANE_UNIT_PIXEL:	fputs ("pel", stdout); break;
    case SANE_UNIT_BIT:		fputs ("bit", stdout); break;
    case SANE_UNIT_MM:		fputs ("mm", stdout); break;
    case SANE_UNIT_DPI:		fputs ("dpi", stdout); break;
    case SANE_UNIT_PERCENT:	fputc ('%', stdout); break;
    case SANE_UNIT_MICROSECOND:	fputs ("us", stdout); break;
    }
}

static void
print_option (SANE_Device *device, int opt_num, char short_name)
{
  const char *str, *last_break, *start;
  const SANE_Option_Descriptor *opt;
  SANE_Bool not_first = SANE_FALSE;
  int i, column;

  opt = sane_get_option_descriptor (device, opt_num);

  if (short_name)
    printf ("    -%c", short_name);
  else
    printf ("    --%s", opt->name);

  if (opt->type == SANE_TYPE_BOOL)
    {
      fputs ("[=(", stdout);
      if (opt->cap & SANE_CAP_AUTOMATIC)
	fputs ("auto|", stdout);
      fputs ("yes|no)]", stdout);
    }
  else if (opt->type != SANE_TYPE_BUTTON)
    {
      fputc (' ', stdout);
      if (opt->cap & SANE_CAP_AUTOMATIC)
	{
	  fputs ("auto|", stdout);
	  not_first = SANE_TRUE;
	}
      switch (opt->constraint_type)
	{
	case SANE_CONSTRAINT_NONE:
	  switch (opt->type)
	    {
	    case SANE_TYPE_INT:		fputs ("<int>", stdout); break;
	    case SANE_TYPE_FIXED:	fputs ("<float>", stdout); break;
	    case SANE_TYPE_STRING:	fputs ("<string>", stdout); break;
	    default:
	      break;
	    }
	  if (opt->type != SANE_TYPE_STRING && opt->size > sizeof (SANE_Word))
	    fputs (",...", stdout);
	  break;

	case SANE_CONSTRAINT_RANGE:
	  if (opt->type == SANE_TYPE_INT)
	    {
	      printf ("%d..%d",
		      opt->constraint.range->min, opt->constraint.range->max);
	      print_unit (opt->unit);
	      if (opt->size > sizeof (SANE_Word))
		fputs (",...", stdout);
	      if (opt->constraint.range->quant)
		printf (" (in steps of %d)",
			opt->constraint.range->quant);
	    }
	  else
	    {
	      printf ("%g..%g",
		    SANE_UNFIX(opt->constraint.range->min),
		    SANE_UNFIX(opt->constraint.range->max));
	      print_unit (opt->unit);
	      if (opt->size > sizeof (SANE_Word))
		fputs (",...", stdout);
	      if (opt->constraint.range->quant)
		printf (" (in steps of %g)",
			SANE_UNFIX(opt->constraint.range->quant));
	    }
	  break;

	case SANE_CONSTRAINT_WORD_LIST:
	  for (i = 0; i < opt->constraint.word_list[0]; ++i)
	    {
	      if (not_first)
		fputc ('|', stdout);

	      not_first = SANE_TRUE;

	      if (opt->type == SANE_TYPE_INT)
		printf ("%d", opt->constraint.word_list[i + 1]);
	      else
		printf ("%g",
			SANE_UNFIX(opt->constraint.word_list[i + 1]));
	    }
	  print_unit (opt->unit);
	  if (opt->size > sizeof (SANE_Word))
	    fputs (",...", stdout);
	  break;

	case SANE_CONSTRAINT_STRING_LIST:
	  for (i = 0; opt->constraint.string_list[i]; ++i)
	    {
	      if (i > 0)
		fputc ('|', stdout);

	      fputs (opt->constraint.string_list[i], stdout);
	    }
	  break;
	}
    }
  if (opt->type == SANE_TYPE_STRING || opt->size == sizeof (SANE_Word))
    {
      /* print current option value */
      fputs (" [", stdout);
      if (SANE_OPTION_IS_ACTIVE(opt->cap))
	{
	  void * val = alloca (opt->size);
	  sane_control_option (device, opt_num, SANE_ACTION_GET_VALUE, val, 0);
	  switch (opt->type)
	    {
	    case SANE_TYPE_BOOL:
	      fputs (*(SANE_Bool *)val ? "yes" : "no", stdout);
	      break;

	    case SANE_TYPE_INT:
	      printf ("%d", *(SANE_Int *)val);
	      break;

	    case SANE_TYPE_FIXED:
	      printf ("%g", SANE_UNFIX(*(SANE_Fixed *)val));
	      break;

	    case SANE_TYPE_STRING:
	      fputs ((char *) val, stdout);
	      break;

	    default:
	      break;
	    }
	}
      else
	fputs ("inactive", stdout);
      fputc (']', stdout);
    }
  fputs ("\n        ", stdout);

  switch (short_name)
    {
    case 'x':
      fputs ("Width of scan-area.", stdout);
      break;

    case 'y':
      fputs ("Height of scan-area.", stdout);
      break;

    default:
      column = 8; last_break = 0; start = opt->desc;
      for (str = opt->desc; *str; ++str)
	{
	  ++column;
	  if (*str == ' ')
	    last_break = str;
	  if (column >= 79 && last_break)
	    {
	      while (start < last_break)
		fputc (*start++, stdout);
	      start = last_break + 1;	/* skip blank */
	      fputs ("\n        ", stdout);
	      column = 8 + (str - start);
	    }
	}
      while (*start)
	fputc (*start++, stdout);
    }
  fputc ('\n', stdout);
}

/* A scalar has the following syntax:

     V [ U ]

   V is the value of the scalar.  It is either an integer or a
   floating point number, depending on the option type.

   U is an optional unit.  If not specified, the default unit is used.
   The following table lists which units are supported depending on
   what the option's default unit is:

     Option's unit:	Allowed units:

     SANE_UNIT_NONE:
     SANE_UNIT_PIXEL:	pel
     SANE_UNIT_BIT:	b (bit), B (byte)
     SANE_UNIT_MM:	mm (millimeter), cm (centimeter), in or " (inches),
     SANE_UNIT_DPI:	dpi
     SANE_UNIT_PERCENT:	%
     SANE_UNIT_PERCENT:	us
 */
static const char *
parse_scalar (const SANE_Option_Descriptor * opt, const char * str,
	      SANE_Word * value)
{
  char * end;
  double v;

  if (opt->type == SANE_TYPE_FIXED)
    v = strtod (str, &end) * (1 << SANE_FIXED_SCALE_SHIFT);
  else
    v = strtol (str, &end, 10);

  if (str == end)
    {
      fprintf (stderr,
	       "%s: option --%s: bad option value (rest of option: %s)\n",
	       prog_name, opt->name, str);
      exit (1);
    }
  str = end;

  switch (opt->unit)
    {
    case SANE_UNIT_NONE:
    case SANE_UNIT_PIXEL:
      break;

    case SANE_UNIT_BIT:
      if (*str == 'b' || *str == 'B')
	{
	  if (*str++ == 'B')
	    v *= 8;
	}
      break;

    case SANE_UNIT_MM:
      if (str[0] == '\0')
	v *= 1.0;			/* default to mm */
      else if (strcmp (str, "mm") == 0)
	str += sizeof ("mm") - 1;
      else if (strcmp (str, "cm") == 0)
	{
	  str += sizeof ("cm") - 1;
	  v *= 10.0;
	}
      else if (strcmp (str, "in") == 0 || *str == '"')
	{
	  if (*str++ != '"')
	    ++str;
	  v *= 25.4;	/* 25.4 mm/inch */
	}
      else
	{
	  fprintf (stderr,
		   "%s: option --%s: illegal unit (rest of option: %s)\n",
		   prog_name, opt->name, str);
	  return 0;
	}
      break;

    case SANE_UNIT_DPI:
      if (strcmp (str, "dpi") == 0)
	str += sizeof ("dpi") - 1;
      break;

    case SANE_UNIT_PERCENT:
      if (*str == '%')
	++str;
      break;

    case SANE_UNIT_MICROSECOND:
      if (strcmp (str, "us") == 0)
	str += sizeof ("us") - 1;
      break;
    }
  *value = v + 0.5;
  return str;
}

/* A vector has the following syntax:

     [ '[' I ']' ] S { [','|'-'] [ '[' I ']' S }

   The number in brackets (I), if present, determines the index of the
   vector element to be set next.  If I is not present, the value of
   last index used plus 1 is used.  The first index value used is 0
   unless I is present.

   S is a scalar value as defined by parse_scalar().

   If two consecutive value specs are separated by a comma (,) their
   values are set independently.  If they are separated by a dash (-),
   they define the endpoints of a line and all vector values between
   the two endpoints are set according to the value of the
   interpolated line.  For example, [0]15-[255]15 defines a vector of
   256 elements whose value is 15.  Similarly, [0]0-[255]255 defines a
   vector of 256 elements whose value starts at 0 and increases to
   255.  */
static void
parse_vector (const SANE_Option_Descriptor * opt, const char * str,
	      SANE_Word * vector, size_t vector_length)
{
  SANE_Word value, prev_value = 0;
  int index = -1, prev_index = 0;
  char * end, separator = '\0';

  /* initialize vector to all zeroes: */
  memset (vector, 0, vector_length * sizeof (SANE_Word));

  do {
    if (*str == '[')
      {
	/* read index */
	index = strtol (++str, &end, 10);
	if (str == end || *end != ']')
	  {
	    fprintf (stderr, "%s: option --%s: closing bracket missing "
		     "(rest of option: %s)\n", prog_name, opt->name, str);
	    exit (1);
	  }
	str = end + 1;
      }
    else
      ++index;

    if (index < 0 || index >= vector_length)
      {
	fprintf (stderr, "%s: option --%s: index %d out of range [0..%ld]\n",
		 prog_name, opt->name, index, (long) vector_length - 1);
	exit (1);
      }

    /* read value */
    str = parse_scalar (opt, str, &value);
    if (!str)
      exit (1);

    if (*str && *str != '-' && *str != ',')
      {
	fprintf (stderr,
		 "%s: option --%s: illegal separator (rest of option: %s)\n",
		 prog_name, opt->name, str);
	exit (1);
      }

    /* store value: */
    vector[index] = value;
    if (separator == '-')
      {
	/* interpolate */
	double v, slope;
	int i;

	v = (double) prev_value;
	slope = ((double) value - v) / (index - prev_index);

	for (i = prev_index + 1; i < index; ++i)
	  {
	    v += slope;
	    vector[i] = (SANE_Word) v;
	  }
      }

    prev_index = index;
    prev_value = value;
    separator = *str++;
  } while (separator == ',' || separator == '-');

  if (verbose > 2)
    {
      int i;

      fprintf (stderr, "%s: value for --%s is: ", prog_name, opt->name);
      for (i = 0; i < vector_length; ++i)
	if (opt->type == SANE_TYPE_FIXED)
	  fprintf (stderr, "%g ", SANE_UNFIX(vector[i]));
	else
	  fprintf (stderr, "%d ", vector[i]);
      fputc ('\n', stderr);
    }
}

void
fetch_options (SANE_Device * device)
{
  const SANE_Option_Descriptor * opt;
  SANE_Int num_dev_options;
  int i, option_count;

  /* and now build the full table of long options: */

  sane_control_option (device, 0, SANE_ACTION_GET_VALUE, &num_dev_options, 0);

  option_count = 0;
  for (i = 0; i < num_dev_options; ++i)
    {
      opt = sane_get_option_descriptor (device, i);

      if (!SANE_OPTION_IS_SETTABLE (opt->cap))
	continue;

      option_number[option_count] = i;

      all_options[option_count].name = (char *) opt->name;
      all_options[option_count].flag = 0;
      all_options[option_count].val = 0;

      if (opt->type == SANE_TYPE_BOOL)
	all_options[option_count].has_arg = optional_argument;
      else if (opt->type == SANE_TYPE_BUTTON)
	all_options[option_count].has_arg = no_argument;
      else
	all_options[option_count].has_arg = required_argument;

      /* Keep track of top-left corner options (if they exist at
	 all) and replace the bottom-right corner options by a
	 width/height option (if they exist at all).  */
      if ((opt->type == SANE_TYPE_FIXED || opt->type == SANE_TYPE_INT)
	  && opt->size == sizeof (SANE_Int)
	  && (opt->unit == SANE_UNIT_MM || opt->unit == SANE_UNIT_PIXEL))
	{
	  if (strcmp (opt->name, SANE_NAME_SCAN_TL_X) == 0)
	    {
	      window[2] = i;
	      all_options[option_count].val = 'l';
	    }
	  else if (strcmp (opt->name, SANE_NAME_SCAN_TL_Y) == 0)
	    {
	      window[3] = i;
	      all_options[option_count].val = 't';
	    }
	  else if (strcmp (opt->name, SANE_NAME_SCAN_BR_X) == 0)
	    {
	      window[0] = i;
	      all_options[option_count].name = "width";
	      all_options[option_count].val  = 'x';
	      window_option[0] = *opt;
	      window_option[0].title = "Scan width";
	      window_option[0].desc = "Width of scanning area.";
	      if (!window_val_user[0])
		sane_control_option (device, i, SANE_ACTION_GET_VALUE,
				     &window_val[0], 0);
	    }
	  else if (strcmp (opt->name, SANE_NAME_SCAN_BR_Y) == 0)
	    {
	      window[1] = i;
	      all_options[option_count].name = "height";
	      all_options[option_count].val  = 'y';
	      window_option[1] = *opt;
	      window_option[1].title = "Scan height";
	      window_option[1].desc = "Height of scanning area.";
	      if (!window_val_user[1])
		sane_control_option (device, i, SANE_ACTION_GET_VALUE,
				     &window_val[1], 0);
	    }
	}
      ++option_count;
    }
  memcpy (all_options + option_count, basic_options, sizeof (basic_options));
  option_count += NELEMS(basic_options);
  memset (all_options + option_count, 0, sizeof (all_options[0]));

  /* Initialize width & height options based on backend default
     values for top-left x/y and bottom-right x/y: */
  for (i = 0; i < 2; ++i)
    {
      if (window[i] && window[i + 2] && !window_val_user[i])
	{
	  SANE_Word pos;
	  sane_control_option (device, window[i + 2],
			       SANE_ACTION_GET_VALUE, &pos, 0);
	  window_val[i] = window_val[i] - pos + 1;
	}
    }
}

static void
set_option (SANE_Handle device, int optnum, void * valuep)
{
  const SANE_Option_Descriptor * opt;
  SANE_Status status;
  SANE_Word orig = 0;
  SANE_Int info;

  opt = sane_get_option_descriptor (device, optnum);

  if (opt->size == sizeof (SANE_Word) && opt->type != SANE_TYPE_STRING)
    orig = *(SANE_Word *) valuep;

  status = sane_control_option (device, optnum, SANE_ACTION_SET_VALUE,
				valuep, &info);
  if (status != SANE_STATUS_GOOD)
    {
      fprintf (stderr, "%s: setting of option --%s failed (%s)\n",
	       prog_name, opt->name, sane_strstatus (status));
      exit (1);
    }

  if ((info & SANE_INFO_INEXACT) && opt->size == sizeof (SANE_Word))
    {
      if (opt->type == SANE_TYPE_INT)
	fprintf (stderr, "%s: rounded value of %s from %d to %d\n",
		 prog_name, opt->name, orig, *(SANE_Word *) valuep);
      else if (opt->type == SANE_TYPE_FIXED)
	fprintf (stderr, "%s: rounded value of %s from %g to %g\n",
		 prog_name, opt->name,
		 SANE_UNFIX(orig), SANE_UNFIX(*(SANE_Word *) valuep));
    }

  if (info & SANE_INFO_RELOAD_OPTIONS)
    fetch_options (device);
}

static void
process_backend_option (SANE_Handle device, int optnum, const char * optarg)
{
  static SANE_Word * vector = 0;
  static size_t vector_size = 0;
  const SANE_Option_Descriptor * opt;
  size_t vector_length;
  SANE_Status status;
  SANE_Word value;
  void * valuep;

  opt = sane_get_option_descriptor (device, optnum);

  if (!SANE_OPTION_IS_ACTIVE(opt->cap))
    {
      fprintf (stderr, "%s: attempted to set inactive option %s\n",
	       prog_name, opt->name);
      exit (1);
    }

  if ((opt->cap & SANE_CAP_AUTOMATIC) && strncasecmp (optarg, "auto", 4) == 0)
    {
      status = sane_control_option (device, optnum, SANE_ACTION_SET_AUTO,
				    0, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  fprintf (stderr, "%s: failed to set option --%s to automatic (%s)\n",
		   prog_name, opt->name, sane_strstatus (status));
	  exit (1);
	}
      return;
    }

  valuep = &value;
  switch (opt->type)
    {
    case SANE_TYPE_BOOL:
      value = 1;	/* no argument means option is set */
      if (optarg)
	{
	  if (strncasecmp (optarg, "yes", strlen (optarg)) == 0)
	    value = 1;
	  else if (strncasecmp (optarg, "no", strlen (optarg)) == 0)
	    value = 0;
	  else
	    {
	      fprintf (stderr, "%s: option --%s: bad option value `%s'\n",
		       prog_name, opt->name, optarg);
	      exit (1);
	    }
	}
      break;

    case SANE_TYPE_INT:
    case SANE_TYPE_FIXED:
      /* ensure vector is long enough: */
      vector_length = opt->size / sizeof (SANE_Word);
      if (vector_size < vector_length)
	{
	  vector_size = vector_length;
	  vector = realloc (vector, vector_length * sizeof (SANE_Word));
	  if (!vector)
	    {
	      fprintf (stderr, "%s: out of memory\n", prog_name);
	      exit (1);
	    }
	}
      parse_vector (opt, optarg, vector, vector_length);
      valuep = vector;
      break;

    case SANE_TYPE_STRING:
      valuep = (void *) optarg;
      break;

    case SANE_TYPE_BUTTON:
      value = 0;	/* value doesn't matter */
      break;

    default:
      fprintf (stderr, "%s: duh, got unknown option type %d\n",
	       prog_name, opt->type);
      return;
    }
  set_option (device, optnum, valuep);
}

static void
write_pnm_header (SANE_Frame format, int width, int height, int depth)
{
  switch (format)
    {
    case SANE_FRAME_RED:
    case SANE_FRAME_GREEN:
    case SANE_FRAME_BLUE:
    case SANE_FRAME_RGB:
      printf ("P6\n# SANE data follows\n%d %d\n255\n", width, height);
      break;

    default:
      if (depth == 1)
	printf ("P4\n# SANE data follows\n%d %d\n", width, height);
      else
	printf ("P5\n# SANE data follows\n%d %d\n255\n", width, height);
      break;
    }
#ifdef __EMX__	/* OS2 - write in binary mode. */
  _fsetmode(stdout, "b");
#endif
}

static void *
advance (Image *image)
{
  if (++image->x >= image->width)
    {
      image->x = 0;
      if (++image->y >= image->height || !image->data)
	{
	  size_t old_size = 0, new_size;

	  if (image->data)
	    old_size = image->height * image->width * image->Bpp;

	  image->height += STRIP_HEIGHT;
	  new_size = image->height * image->width * image->Bpp;

	  if (image->data)
	    image->data = realloc (image->data, new_size);
	  else
	    image->data = malloc (new_size);
	  if (image->data)
	    memset (image->data + old_size, 0, new_size - old_size);
	}
    }
  if (!image->data)
      fprintf (stderr, "%s: can't allocate image buffer (%dx%d)\n",
	       prog_name, image->width, image->height);
  return image->data;
}

static SANE_Status
scan_it (void)
{
  int i, len, first_frame = 1, offset = 0, must_buffer = 0;
  SANE_Byte buffer[32*1024], min = 0xff, max = 0;
  SANE_Parameters parm;
  SANE_Status status;
  Image image = {0, };
  static const char *format_name[] =
    {
      "gray", "RGB", "red", "green", "blue"
    };

  do
    {
      status = sane_start (device);
      if (status != SANE_STATUS_GOOD)
	{
	  fprintf (stderr, "%s: sane_start: %s\n",
		   prog_name, sane_strstatus (status));
	  goto cleanup;
	}

      status = sane_get_parameters (device, &parm);
      if (status != SANE_STATUS_GOOD)
	{
	  fprintf (stderr, "%s: sane_get_parameters: %s\n",
		   prog_name, sane_strstatus (status));
	  goto cleanup;
	}

      if (verbose)
	{
	  if (first_frame)
	    {
	      if (parm.lines >= 0)
		fprintf (stderr, "%s: scanning image of size %dx%d pixels at "
			 "%d bits/pixel\n",
			 prog_name, parm.pixels_per_line, parm.lines,
			 8 * parm.bytes_per_line / parm.pixels_per_line);
	      else
		fprintf (stderr, "%s: scanning image %d pixels wide and "
			 "variable height at %d bits/pixel\n",
			 prog_name, parm.pixels_per_line,
			 8 * parm.bytes_per_line / parm.pixels_per_line);
	    }
	  fprintf (stderr, "%s: acquiring %s frame\n", prog_name,
		   format_name[parm.format]);
	}

      if (first_frame)
	{
	  switch (parm.format)
	    {
	    case SANE_FRAME_RED:
	    case SANE_FRAME_GREEN:
	    case SANE_FRAME_BLUE:
	      assert (parm.depth == 8);
	      must_buffer = 1;
	      offset = parm.format - SANE_FRAME_RED;
	      break;

	    case SANE_FRAME_RGB:
	      assert (parm.depth == 8);
	    case SANE_FRAME_GRAY:
	      assert (parm.depth == 1 || parm.depth == 8);
	      if (parm.lines < 0)
		{
		  must_buffer = 1;
		  offset = 0;
		}
	      else
		write_pnm_header (parm.format, parm.pixels_per_line,
				  parm.lines, parm.depth);
	      break;
	    }

	  if (must_buffer)
	    {
	      /* We're either scanning a multi-frame image or the
                 scanner doesn't know what the eventual image height
                 will be (common for hand-held scanners).  In either
                 case, we need to buffer all data before we can write
                 the image.  */
	      image.width  = parm.pixels_per_line;
	      if (parm.lines >= 0)
		/* See advance(); we allocate one extra line so we
		   don't end up realloc'ing in when the image has been
		   filled in.  */
		image.height = parm.lines - STRIP_HEIGHT + 1;
	      else
		image.height = 0;
	      image.Bpp = 3;
	      if (parm.format == SANE_FRAME_GRAY)
		image.Bpp = 1;
	      image.x = image.width - 1;
	      image.y = -1;
	      if (!advance (&image))
		status = SANE_STATUS_NO_MEM;
		goto cleanup;
	    }
	}
      else
	{
	  assert (parm.format >= SANE_FRAME_RED
		  && parm.format <= SANE_FRAME_BLUE);
	  offset = parm.format - SANE_FRAME_RED;
	  image.x = image.y = 0;
	}
	
      while (1)
	{
	  status = sane_read (device, buffer, sizeof (buffer), &len);
	  if (status != SANE_STATUS_GOOD)
	    {
	      if (verbose && parm.depth == 8)
		fprintf (stderr, "%s: min/max graylevel value = %d/%d\n",
			 prog_name, min, max);
	      if (status != SANE_STATUS_EOF)
		{
		  fprintf (stderr, "%s: sane_read: %s\n",
			   prog_name, sane_strstatus (status));
		  return status;
		}
	      break;
	    }
	  if (must_buffer)
	    {
	      switch (parm.format)
		{
		case SANE_FRAME_RED:
		case SANE_FRAME_GREEN:
		case SANE_FRAME_BLUE:
		  for (i = 0; i < len; ++i)
		    {
		      image.data[offset + 3*i] = buffer[i];
		      if (!advance (&image))
			status = SANE_STATUS_NO_MEM;
			goto cleanup;
		    }
		  offset += 3*len;
		  break;

		case SANE_FRAME_RGB:
		  for (i = 0; i < len; ++i)
		    {
		      image.data[offset + i] = buffer[i];
		      if ((offset + i) % 3 == 0 && !advance (&image))
			status = SANE_STATUS_NO_MEM;
			goto cleanup;
		    }
		  offset += len;
		  break;

		case SANE_FRAME_GRAY:
		  for (i = 0; i < len; ++i)
		    {
		      image.data[offset + i] = buffer[i];
		      if (!advance (&image))
			status = SANE_STATUS_NO_MEM;
			goto cleanup;
		    }
		  offset += len;
		  break;
		}
	    }
	  else
	    fwrite (buffer, 1, len, stdout);

	  if (verbose && parm.depth == 8)
	    {
	      for (i = 0; i < len; ++i)
		if (buffer[i] >= max)
		  max = buffer[i];
		else if (buffer[i] < min)
		  min = buffer[i];
	    }
	}
      first_frame = 0;
    }
  while (!parm.last_frame);

  if (must_buffer)
    {
      image.height = image.y;
      write_pnm_header (parm.format, image.width, image.height, parm.depth);
      fwrite (image.data, image.Bpp, image.height * image.width, stdout);
    }

cleanup:
  sane_cancel (device);
  if (image.data)
    free (image.data);

  return status;
}

#define clean_buffer(buf,size)	memset ((buf), 0x23, size)

static void
pass_fail (int max, int len, SANE_Byte *buffer, SANE_Status status)
{
  if (status != SANE_STATUS_GOOD)
    fprintf (stderr, "FAIL Error: %s\n", sane_strstatus (status));
  else if (buffer[len] != 0x23)
    {
      while (buffer[len] != 0x23) ++len;
      fprintf (stderr, "FAIL Cheat: %d bytes\n", len);
    }
  else if (len > max)
    fprintf (stderr, "FAIL Overflow: %d bytes\n", len);
  else if (len == 0)
    fprintf (stderr, "FAIL No data\n");
  else
    fprintf (stderr, "PASS\n");
}

static SANE_Status
test_it (void)
{
  int i, len;
  SANE_Parameters parm;
  SANE_Status status;
  Image image = {0, };
  static const char *format_name[] =
    { "gray", "RGB", "red", "green", "blue" };

  status = sane_start (device);
  if (status != SANE_STATUS_GOOD)
    {
      fprintf (stderr, "%s: sane_start: %s\n",
	       prog_name, sane_strstatus (status));
      goto cleanup;
    }

  status = sane_get_parameters (device, &parm);
  if (status != SANE_STATUS_GOOD)
    {
      fprintf (stderr, "%s: sane_get_parameters: %s\n",
	       prog_name, sane_strstatus (status));
      goto cleanup;
    }

  if (parm.lines >= 0)
    fprintf (stderr, "%s: scanning image of size %dx%d pixels at "
	     "%d bits/pixel\n", prog_name, parm.pixels_per_line, parm.lines,
             8 * parm.bytes_per_line / parm.pixels_per_line);
  else
    fprintf (stderr, "%s: scanning image %d pixels wide and "
             "variable height at %d bits/pixel\n",
	     prog_name, parm.pixels_per_line,
             8 * parm.bytes_per_line / parm.pixels_per_line);
  fprintf (stderr, "%s: acquiring %s frame, %d bits/sample\n",
           prog_name, format_name[parm.format], parm.depth);

  image.data= malloc (parm.bytes_per_line * 2);

  clean_buffer (image.data, parm.bytes_per_line * 2);
  fprintf (stderr, "%s: reading one scanline, %d bytes...\t", prog_name,
           parm.bytes_per_line);
  status = sane_read (device, image.data, parm.bytes_per_line, &len);
  pass_fail (parm.bytes_per_line, len, image.data, status);
  if (status != SANE_STATUS_GOOD)
    goto cleanup;

  clean_buffer (image.data, parm.bytes_per_line * 2);
  fprintf (stderr, "%s: reading one byte...\t\t", prog_name);
  status = sane_read (device, image.data, 1, &len);
  pass_fail (1, len, image.data, status);
  if (status != SANE_STATUS_GOOD)
    goto cleanup;

  for (i = 2; i < parm.bytes_per_line * 2; i*=2)
    {
      clean_buffer (image.data, parm.bytes_per_line * 2);
      fprintf (stderr, "%s: stepped read, %d bytes... \t", prog_name, i);
      status = sane_read (device, image.data, i, &len);
      pass_fail (i, len, image.data, status);
      if (status != SANE_STATUS_GOOD)
	goto cleanup;
    }

  for (i/=2; i > 2; i/=2)
    {
      clean_buffer (image.data, parm.bytes_per_line * 2);
      fprintf (stderr, "%s: stepped read, %d bytes... \t", prog_name, i - 1);
      status = sane_read (device, image.data, i - 1, &len);
      pass_fail (i - 1, len, image.data, status);
      if (status != SANE_STATUS_GOOD)
	goto cleanup;
    }
 
 cleanup:
  sane_cancel (device);
  if (image.data)
    free (image.data);
  return status;
}

int
main (int argc, char **argv)
{
  int ch, i, index, all_options_len;
  const SANE_Option_Descriptor * opt;
  const SANE_Device ** device_list;
  SANE_Int num_dev_options = 0;
  const char * devname = 0;
  const char * format = 0;
  int batch = 0;
  SANE_Status status;
  char *full_optstring;

  atexit (sane_exit);

  prog_name = strrchr (argv[0], '/');
  if (prog_name)
    ++prog_name;
  else
    prog_name = argv[0];

  sane_init (0, 0);

  /* make a first pass through the options with error printing and argument
     permutation disabled: */
  opterr = 0;
  while ((ch = getopt_long (argc, argv, "-" BASE_OPTSTRING, basic_options,
			    &index))
	 != EOF)
    {
      switch (ch)
	{
	case ':':
	case '?':
	  break;	/* may be an option that we'll parse later on */

	case 'd': devname = optarg; break;
	case 'b': batch = 1; format = optarg; break;
	case 'h': help = 1; break;
	case 'v': ++verbose; break;
	case 'T': test= 1; break;
	case 'L':
	  {
	    int i;

	    status = sane_get_devices (&device_list, SANE_FALSE);
	    if (status != SANE_STATUS_GOOD)
	      {
		fprintf (stderr, "%s: sane_get_devices() failed: %s\n",
			 prog_name, sane_strstatus (status));
		exit (1);
	      }

	    for (i = 0; device_list[i]; ++i)
	      {
		printf ("device `%s' is a %s %s %s\n",
			 device_list[i]->name, device_list[i]->vendor,
			 device_list[i]->model, device_list[i]->type);
	      }
	    exit (0);
	  }

	case 'V':
	  printf ("scanimage (%s) %s\n", PACKAGE, VERSION);
	  exit (0);

	default:
	  break;	/* ignore device specific options for now */
	}
    }

  if (help)
    printf ("Usage: %s [OPTION]...\n\
\n\
Start image acquisition on a scanner device and write PNM image data to\n\
standard output.\n\
\n\
-b, --batch=FORMAT         working in batch mode\n\
-d, --device-name=DEVICE   use a given scanner device\n\
-h, --help                 display this help message and exit\n\
-L, --list-devices         show available scanner devices\n\
-T, --test                 test backend thoroughly\n\
-v, --verbose              give even more status messages\n\
-V, --version              print version information\n",
	    prog_name);

  if (!devname)
    {
      /* If no device name was specified explicitly, we open the first
	 device we find (if any): */

      status = sane_get_devices (&device_list, SANE_FALSE);
      if (status != SANE_STATUS_GOOD)
	{
	  fprintf (stderr, "%s: sane_get_devices() failed: %s\n",
		   prog_name, sane_strstatus (status));
	  exit (1);
	}
      if (!device_list[0])
	{
	  fprintf (stderr, "%s: no SANE devices found\n", prog_name);
	  exit (1);
	}
      devname = device_list[0]->name;
    }

  status = sane_open (devname, &device);
  if (status != SANE_STATUS_GOOD)
    {
      fprintf (stderr, "%s: open of device %s failed: %s\n",
	       prog_name, devname, sane_strstatus (status));
      if (help)
	device = 0;
      else
	exit (1);
    }

  if (device)
    {
      /* We got a device, find out how many options it has: */
      status = sane_control_option (device, 0, SANE_ACTION_GET_VALUE,
				    &num_dev_options, 0);
      if (status != SANE_STATUS_GOOD)
	{
	  fprintf (stderr, "%s: unable to determine option count\n",
		   prog_name);
	  exit (1);
	}

      all_options_len = num_dev_options + NELEMS(basic_options) + 1;
      all_options = malloc (all_options_len * sizeof (all_options[0]));
      option_number_len = num_dev_options;
      option_number = malloc (option_number_len * sizeof (option_number[0]));
      if (!all_options || !option_number)
	{
	  fprintf (stderr, "%s: out of memory in fetch_options()\n",
		   prog_name);
	  exit (1);
	}

      fetch_options (device);

      {
	char *larg, *targ, *xarg, *yarg;
	larg = targ = xarg = yarg = "";

	/* Maybe accept t, l, x, and y options. */
	if (window[0])
	  xarg = "x:";

	if (window[1])
	  yarg = "y:";

	if (window[2])
	  larg = "l:";

	if (window[3])
	  targ = "t:";

	/* Now allocate the full option list. */
	full_optstring = malloc (strlen (BASE_OPTSTRING)
				 + strlen (larg) + strlen (targ)
				 + strlen (xarg) + strlen (yarg) + 1);

	if (!full_optstring)
	  {
	    fprintf (stderr, "%s: out of memory\n", prog_name);
	    exit (1);
	  }

	strcpy (full_optstring, BASE_OPTSTRING);
	strcat (full_optstring, larg);
	strcat (full_optstring, targ);
	strcat (full_optstring, xarg);
	strcat (full_optstring, yarg);
      }

      optind = 0;
      opterr = 1;	/* re-enable error printing and arg permutation */
      while ((ch = getopt_long (argc, argv, full_optstring, all_options,
				&index))
	     != EOF)
	{
	  switch (ch)
	    {
	    case ':':
	    case '?':
	      exit (1);	/* error message is printed by getopt_long() */

	    case 'd': case 'h': case 'v': case 'V': case 'T':
	      /* previously handled options */
	      break;

	    case 'x':
	      window_val_user[0] = 1;
	      parse_vector (&window_option[0], optarg, &window_val[0], 1);
	      break;

	    case 'y':
	      window_val_user[1] = 1;
	      parse_vector (&window_option[1], optarg, &window_val[1], 1);
	      break;

	    case 'l':	/* tl-x */
	      process_backend_option (device, window[2], optarg);
	      break;

	    case 't':	/* tl-y */
	      process_backend_option (device, window[3], optarg);
	      break;

	    case 0:
	      process_backend_option (device, option_number[index], optarg);
	      break;
	    }
	}

      free (full_optstring);

      for (index = 0; index < 2; ++index)
	if (window[index])
	  {
	    SANE_Word val, pos;

	    if (window[index + 2])
	      sane_control_option (device, window[index + 2],
				   SANE_ACTION_GET_VALUE, &pos, 0);
	    val = pos + window_val[index] - 1;
	    set_option (device, window[index], &val);
	  }
      if (help)
	{
	  printf ("\nOptions specific to device `%s':\n", devname);

	  for (i = 0; i < num_dev_options; ++i)
	    {
	      char short_name = '\0';
	      int j;

	      opt = 0;
	      for (j = 0; j < 4; ++j)
		if (i == window[j])
		  {
		    short_name = "xylt"[j];
		    if (j < 2)
		      opt = window_option + j;
		  }
	      if (!opt)
		opt = sane_get_option_descriptor (device, i);

	      if (opt->type == SANE_TYPE_GROUP)
		printf ("  %s:\n", opt->title);

	      if (!SANE_OPTION_IS_SETTABLE (opt->cap))
		continue;

	      print_option (device, i, short_name);
	    }
	  if (num_dev_options)
	    fputc ('\n', stdout);
	}
    }

  if (help)
    {
      printf ("\
Type ``%s --help -d DEVICE'' to get list of all options for DEVICE.\n\
\n\
List of available devices:", prog_name);
      status = sane_get_devices (&device_list, SANE_FALSE);
      if (status == SANE_STATUS_GOOD)
	{
	  int column = 80;

	  for (i = 0; device_list[i]; ++i)
	    {
	      if (column + strlen (device_list[i]->name) + 1 >= 80)
		{
		  printf ("\n    ");
		  column = 4;
		}
	      if (column > 4)
		{
		  fputc (' ', stdout);
		  column += 1;
		}
	      fputs (device_list[i]->name, stdout);
	      column += strlen (device_list[i]->name);
	    }
	}
      fputc ('\n', stdout);
      exit (0);
    }

  signal (SIGHUP,  sighandler);
  signal (SIGINT,  sighandler);
  signal (SIGPIPE, sighandler);
  signal (SIGTERM, sighandler);

  if (test == 0) 
    {
      int n = 1;

      if (batch && NULL == format)
	format = "out%d.pnm";

      do
	{
	  char path [PATH_MAX];
	  if (batch) /* format is NULL unless batch mode */
	    sprintf (path, format, n);	/* love --(C++) */

	  if (batch && NULL == freopen (path, "w", stdout) )
	    {
	      fprintf (stderr, "cannot open %s\n", path);
	      return SANE_STATUS_ACCESS_DENIED;
	    }

	  status = scan_it ();
	  if (batch)
	    fprintf( stderr, "status = %d\n", status);

	  switch (status)
	    {
	      case SANE_STATUS_GOOD:
	        break;
	      case SANE_STATUS_EOF:
	        status = SANE_STATUS_GOOD;
	        break;
	      default:
	        if (batch)
	          {
		    fclose(stdout);
		    unlink(path);
		  }
	        break;
	    }	/* switch */
	  n++;
	}
      while (batch && SANE_STATUS_GOOD == status);
    }
  else
    status = test_it ();

  sane_close (device);

  return status;
}
