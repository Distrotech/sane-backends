/**************************************************************************/

SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback authorize)
{
  char devnam[PATH_MAX] = "/dev/scanner";
  FILE *fp;

  DBG_INIT ();
  DBG (1, ">> sane_init\n");

#if defined PACKAGE && defined VERSION
  DBG (2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if (version_code)
    *version_code = SANE_VERSION_CODE (V_MAJOR, V_MINOR, 0);

  fp = sanei_config_open( CANON_CONFIG_FILE);
  if (fp)
  {
    char line[PATH_MAX];
    size_t len;

    /* read config file */
    while (sanei_config_read (line, sizeof (line), fp))
    {
      if (line[0] == '#')		/* ignore line comments */
	continue;
      len = strlen (line);
  
      if (!len)
	continue;			/* ignore empty lines */
      strcpy (devnam, line);
    }
    fclose (fp);
  }
  sanei_config_attach_matching_devices (devnam, attach_one);
  DBG (1, "<< sane_init\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

void
sane_exit (void)
{
  CANON_Device *dev, *next;
  DBG (1, ">> sane_exit\n");

  for (dev = first_dev; dev; dev = next)
  {
    next = dev->next;
    free ((void *) dev->sane.name);
    free ((void *) dev->sane.model);
    free (dev);
  }

  DBG (1, "<< sane_exit\n");
}

/**************************************************************************/

SANE_Status
sane_get_devices (const SANE_Device *** device_list, SANE_Bool local_only)
{
  static const SANE_Device **devlist = 0;
  CANON_Device *dev;
  int i;
  DBG (1, ">> sane_get_devices\n");

  if (devlist)
    free (devlist);
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return (SANE_STATUS_NO_MEM);

  i = 0;
  for (dev = first_dev; dev; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  DBG (1, "<< sane_get_devices\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

SANE_Status
sane_open (SANE_String_Const devnam, SANE_Handle * handle)
{
  SANE_Status status;
  CANON_Device *dev;
  CANON_Scanner *s;
  int i, j;

  DBG (1, ">> sane_open\n");

  if (devnam[0] == '\0')
  {
    for (dev = first_dev; dev; dev = dev->next)
    {
      if (strcmp (dev->sane.name, devnam) == 0)
	break;
    }

    if (!dev)
    {
      status = attach (devnam, &dev);
      if (status != SANE_STATUS_GOOD)
	return (status);
    }
  }
  else
  {
    dev = first_dev;
  }

  if (!dev)
    return (SANE_STATUS_INVAL);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));

  s->fd = -1;
  s->hw = dev;

  for (i = 0; i < 4; ++i)
    for (j = 0; j < 256; ++j)
      s->gamma_table[i][j] = j;

  init_options (s);

  s->next = first_handle;
  first_handle = s;

  *handle = s;

  DBG (1, "<< sane_open\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

void
sane_close (SANE_Handle handle)
{
  CANON_Scanner *s = (CANON_Scanner *) handle;
  SANE_Status status;

  DBG (1, ">> sane_close\n");

  if (s->val[OPT_EJECT_BEFOREEXIT].w == SANE_TRUE)
  {
    if (s->fd == -1)
    {
      sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
    }
    status = medium_position(s->fd);
    if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: MEDIUM POSTITION failed\n");
      sanei_scsi_close (s->fd);
      s->fd = -1;
    }
    s->AF_NOW = SANE_TRUE;
DBG(1, "sane_close AF_NOW = '%d'\n", s->AF_NOW);
  }

  if (s->fd != -1)
  {
    sanei_scsi_close (s->fd);
  }

  free (s);

  DBG (1, ">> sane_close\n");
}

/**************************************************************************/

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  CANON_Scanner *s = handle;
  DBG (21, ">> sane_get_option_descriptor %s\n", option_name[option]);

  if ((unsigned) option >= NUM_OPTIONS)
    return (0);

  DBG (21, "<< sane_get_option_descriptor %s\n", option_name[option]);
  return (s->opt + option);
}

/**************************************************************************/

SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  SANE_Word w, cap;
  SANE_Byte gbuf[256];
  size_t buf_size;
  int i, neg, gamma_component;

  DBG (21, ">> sane_control_option %s\n", option_name[option]);

  if (info)
    *info = 0;

  if (s->scanning == SANE_TRUE)
  {
    DBG (21, ">> sane_control_option: device is busy scanning\n");
    return (SANE_STATUS_DEVICE_BUSY);
  }
  if (option >= NUM_OPTIONS)
    return (SANE_STATUS_INVAL);

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return (SANE_STATUS_INVAL);

  if (action == SANE_ACTION_GET_VALUE)
  {
    DBG (21, "sane_control_option get value of %s\n", option_name[option]);
    switch (option)
    {
      /* word options: */
    case OPT_FLATBED_ONLY:
    case OPT_TPU_ON:
    case OPT_TPU_PN:
    case OPT_TPU_TRANSPARENCY:
    case OPT_RESOLUTION_BIND:
    case OPT_HW_RESOLUTION_ONLY:   /* 990320, ss */
    case OPT_X_RESOLUTION:
    case OPT_Y_RESOLUTION:
    case OPT_TL_X:
    case OPT_TL_Y:
    case OPT_BR_X:
    case OPT_BR_Y:
    case OPT_NUM_OPTS:
    case OPT_BRIGHTNESS:
    case OPT_CONTRAST:
    case OPT_CUSTOM_GAMMA:
    case OPT_CUSTOM_GAMMA_BIND:
    case OPT_HNEGATIVE:
      /*     case OPT_GRC: */
    case OPT_MIRROR:
    case OPT_AE:
    case OPT_PREVIEW:
    case OPT_BIND_HILO:
    case OPT_HILITE_R:
    case OPT_SHADOW_R:
    case OPT_HILITE_G:
    case OPT_SHADOW_G:
    case OPT_HILITE_B:
    case OPT_SHADOW_B:
    case OPT_EJECT_AFTERSCAN:
    case OPT_EJECT_BEFOREEXIT:
    case OPT_THRESHOLD:
    case OPT_AF:
    case OPT_AF_ONCE:
    case OPT_FOCUS:
      if ( (option >= OPT_NEGATIVE) && (option <= OPT_SHADOW_B) )
      {
	DBG(21, "GET_VALUE for %s: s->val[%s].w = %d\n",
	    option_name[option], option_name[option], s->val[option].w);
      }
      *(SANE_Word *) val = s->val[option].w;
      DBG(21, "value for option %s: %d\n", option_name[option], s->val[option].w);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS;
      return (SANE_STATUS_GOOD);

    case OPT_GAMMA_VECTOR:
    case OPT_GAMMA_VECTOR_R:
    case OPT_GAMMA_VECTOR_G:
    case OPT_GAMMA_VECTOR_B:

      memset (gbuf, 0, sizeof (gbuf));
      buf_size = sizeof (gbuf);
      sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);

      DBG (21, "sending GET_DENSITY_CURVE\n");
      if (s->val[OPT_CUSTOM_GAMMA_BIND].w == SANE_TRUE)
      {
	/* If using bind analog gamma, option will be OPT_GAMMA_VECTOR.
	   In this case, use the curve for green                        */
	gamma_component = 2;
      }
      else
      {
	/* Else use a different index for each curve */
	gamma_component = option - OPT_GAMMA_VECTOR;
      }

      /* Now get the values from the scanner */
      status = 
	get_density_curve (s->fd, gamma_component, gbuf, &buf_size);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      if (status != SANE_STATUS_GOOD)
      {
	DBG (21, "GET_DENSITY_CURVE\n");
	return (SANE_STATUS_INVAL);
      }

      neg = (s->hw->info.model == CS2700) ? 
	s->val[OPT_NEGATIVE].w :
	  s->val[OPT_HNEGATIVE].w;
      for (i=0; i<256; i++)
      {
	if(neg == SANE_FALSE)
	{
	  s->gamma_table[option-OPT_GAMMA_VECTOR][i] = 255.0 / 256.0 * (SANE_Int)gbuf[i];
	}
	else
	{
	  s->gamma_table[option-OPT_GAMMA_VECTOR][i] = 255 - 255.0 / 256.0 * (SANE_Int)gbuf[255-i];;
	}
      }
	
      memcpy (val, s->val[option].wa, s->opt[option].size);
      DBG(21, "value for option %s: %d\n", option_name[option], s->val[option].w);
      return (SANE_STATUS_GOOD);

      /* string options: */
    case OPT_TPU_DCM:
    case OPT_TPU_FILMTYPE:
    case OPT_MODE:
      strcpy (val, s->val[option].s);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS;
      DBG(21, "value for option %s: %s\n", option_name[option], s->val[option].s);
      return (SANE_STATUS_GOOD);

    case OPT_PAGE:
      strcpy (val, s->val[option].s);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      DBG(21, "value for option %s: %s\n", option_name[option], s->val[option].s);
      return (SANE_STATUS_GOOD);

    case OPT_NEGATIVE:
      strcpy (val, s->val[option].s);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      DBG(21, "value for option %s: %s\n", option_name[option], s->val[option].s);
      return (SANE_STATUS_GOOD);

    case OPT_NEGATIVE_TYPE:
      strcpy (val, s->val[option].s);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      DBG(21, "value for option %s: %s\n", option_name[option], s->val[option].s);
      return (SANE_STATUS_GOOD);

    case OPT_SCANNING_SPEED:
      strcpy (val, s->val[option].s);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      DBG(21, "value for option %s: %s\n", option_name[option], s->val[option].s);
      return (SANE_STATUS_GOOD);

    default:
      val = 0;
      return (SANE_STATUS_GOOD);
    }
  }
  else if (action == SANE_ACTION_SET_VALUE)
  {
    DBG (21, "sane_control_option set value for %s\n", option_name[option]);
    if (!SANE_OPTION_IS_SETTABLE (cap))
      return (SANE_STATUS_INVAL);
  
    status = sanei_constrain_value (s->opt + option, val, info);

    if (status != SANE_STATUS_GOOD)
      return status;

    switch (option)
    {
      /* (mostly) side-effect-free word options: */
    case OPT_TPU_PN:
    case OPT_TPU_TRANSPARENCY:
    case OPT_X_RESOLUTION:
    case OPT_Y_RESOLUTION:
    case OPT_TL_X:
    case OPT_TL_Y:
    case OPT_BR_X:
    case OPT_BR_Y:
      if (info && s->val[option].w != *(SANE_Word *) val)
	*info |= SANE_INFO_RELOAD_PARAMS;
      /* fall through */
    case OPT_NUM_OPTS:
    case OPT_BRIGHTNESS:
    case OPT_CONTRAST:
    case OPT_THRESHOLD:
    case OPT_HNEGATIVE:
      /*     case OPT_GRC: */
    case OPT_MIRROR:
    case OPT_AE:
    case OPT_PREVIEW:
    case OPT_HILITE_R:
    case OPT_SHADOW_R:
    case OPT_HILITE_G:
    case OPT_SHADOW_G:
    case OPT_HILITE_B:
    case OPT_SHADOW_B:
    case OPT_AF_ONCE:
    case OPT_FOCUS:
    case OPT_EJECT_AFTERSCAN:
    case OPT_EJECT_BEFOREEXIT:
      s->val[option].w = *(SANE_Word *) val;
      DBG(21, "SET_VALUE for %s: s->val[%s].w = %d\n",
	  option_name[option], option_name[option], s->val[option].w);
      return (SANE_STATUS_GOOD);

    case OPT_RESOLUTION_BIND:
      if (s->val[option].w != *(SANE_Word *) val)
      {
	s->val[option].w = *(SANE_Word *) val;
	
	if (info) { *info |= SANE_INFO_RELOAD_OPTIONS; }
	if (s->val[option].w == SANE_FALSE)
	{ /* don't bind */
	  s->opt[OPT_Y_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
	  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_X_RESOLUTION;
	  s->opt[OPT_X_RESOLUTION].name  = SANE_NAME_SCAN_X_RESOLUTION;
	  s->opt[OPT_X_RESOLUTION].desc  = SANE_DESC_SCAN_X_RESOLUTION;
	}
	else
	{ /* bind */
	  s->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
	  s->opt[OPT_X_RESOLUTION].name  = SANE_NAME_SCAN_RESOLUTION;
	  s->opt[OPT_X_RESOLUTION].desc  = SANE_DESC_SCAN_RESOLUTION;
	}
      }
      return SANE_STATUS_GOOD;

    /* 990320, ss: switch between slider and option menue for resolution */
    case OPT_HW_RESOLUTION_ONLY:
      if (s->val[option].w != *(SANE_Word *) val)
        {
        int iPos, xres, yres;

	s->val[option].w = *(SANE_Word *) val;
	
	if (info) { *info |= SANE_INFO_RELOAD_OPTIONS; }
	if (s->val[option].w == SANE_FALSE)
	  {
          /* use complete range */
          s->opt[OPT_X_RESOLUTION].constraint_type  = SANE_CONSTRAINT_RANGE;
          s->opt[OPT_X_RESOLUTION].constraint.range = &s->hw->info.xres_range;
          s->opt[OPT_Y_RESOLUTION].constraint_type  = SANE_CONSTRAINT_RANGE;
          s->opt[OPT_Y_RESOLUTION].constraint.range = &s->hw->info.yres_range;
	  }
	else
	  {
          /* use only hardware resolutions */
          s->opt[OPT_X_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
          s->opt[OPT_X_RESOLUTION].constraint.word_list = s->xres_word_list;
          s->opt[OPT_Y_RESOLUTION].constraint_type = SANE_CONSTRAINT_WORD_LIST;
          s->opt[OPT_Y_RESOLUTION].constraint.word_list = s->yres_word_list;
          
          /* adjust resolutions */
          xres = s->xres_word_list[1];
          for (iPos = 0; iPos < s->xres_word_list[0]; iPos++)
            {
            if (s->val[OPT_X_RESOLUTION].w >= s->xres_word_list[iPos+1])
              {
              xres = s->xres_word_list[iPos+1];
              }
            }
          s->val[OPT_X_RESOLUTION].w = xres;

          yres = s->yres_word_list[1];
          for (iPos = 0; iPos < s->yres_word_list[0]; iPos++)
            {
            if (s->val[OPT_Y_RESOLUTION].w >= s->yres_word_list[iPos+1])
              {
              yres = s->yres_word_list[iPos+1];
              }
            }
          s->val[OPT_Y_RESOLUTION].w = yres;
          }
        }
      return (SANE_STATUS_GOOD);

    case OPT_BIND_HILO:
      if (s->val[option].w != *(SANE_Word *) val)
      {
	s->val[option].w = *(SANE_Word *) val;
	
	if (info) { *info |= SANE_INFO_RELOAD_OPTIONS; }
	if (s->val[option].w == SANE_FALSE)
	{ /* don't bind */
	  s->opt[OPT_HILITE_R].cap &= ~SANE_CAP_INACTIVE;
	  s->opt[OPT_SHADOW_R].cap &= ~SANE_CAP_INACTIVE;
	  s->opt[OPT_HILITE_B].cap &= ~SANE_CAP_INACTIVE;
	  s->opt[OPT_SHADOW_B].cap &= ~SANE_CAP_INACTIVE;

	  s->opt[OPT_HILITE_G].title = SANE_TITLE_HIGHLIGHT_G;
	  s->opt[OPT_HILITE_G].name  = SANE_NAME_HIGHLIGHT_G;
	  s->opt[OPT_HILITE_G].desc  = SANE_DESC_HIGHLIGHT_G;

	  s->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW_G;
	  s->opt[OPT_SHADOW_G].name  = SANE_NAME_SHADOW_G;
	  s->opt[OPT_SHADOW_G].desc  = SANE_DESC_SHADOW_G;
	}
	else
	{ /* bind */
	  s->opt[OPT_HILITE_R].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_HILITE_B].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;

	  s->opt[OPT_HILITE_G].title = SANE_TITLE_HIGHLIGHT;
	  s->opt[OPT_HILITE_G].name  = SANE_NAME_HIGHLIGHT;
	  s->opt[OPT_HILITE_G].desc  = SANE_DESC_HIGHLIGHT;

	  s->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW;
	  s->opt[OPT_SHADOW_G].name  = SANE_NAME_SHADOW;
	  s->opt[OPT_SHADOW_G].desc  = SANE_DESC_SHADOW;
	}
      }
      return SANE_STATUS_GOOD;

    case OPT_AF:
      if (info && s->val[option].w != *(SANE_Word *) val)
	*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
      s->val[option].w = *(SANE_Word *) val;
      w = *(SANE_Word *) val;
      if (w)
      {
	s->opt[OPT_AF_ONCE].cap &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_FOCUS].cap |= SANE_CAP_INACTIVE;
      }
      else
      {
	s->opt[OPT_AF_ONCE].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_FOCUS].cap &= ~SANE_CAP_INACTIVE;
      }
      return (SANE_STATUS_GOOD);

    case OPT_FLATBED_ONLY:
      switch (action)
      {
      case SANE_ACTION_SET_VALUE:
        s->val[option].w = *(SANE_Word *) val;
        if (s->hw->adf.Status != ADF_STAT_NONE &&
          s->val[option].w == SANE_TRUE) /* switch on */
        {
          s->hw->adf.Priority |= 0x03; /* flatbed mode */
          s->hw->adf.Feeder   &= 0x00; /* no autofeed mode (default)*/
          s->hw->adf.Status    = ADF_STAT_DISABLED;
          s->val[option].w = SANE_TRUE;
        } /* if it isn't connected, don't bother fixing */
        break;
      case SANE_ACTION_GET_VALUE:
        val = &s->val[option].w;
        break;
      default:
        break;
      }
      return SANE_STATUS_GOOD;

    case OPT_TPU_ON:
      if ( s->val[OPT_TPU_ON].w == TPU_STAT_INACTIVE ) /* switch on */
      {
        s->val[OPT_TPU_ON].w = TPU_STAT_ACTIVE;
        s->opt[OPT_TPU_ON].title = "Turn Off the Transparency Unit";
        s->opt[OPT_TPU_TRANSPARENCY].cap &= 
          (s->hw->tpu.ControlMode == 3) ? ~SANE_CAP_INACTIVE : ~0;
        s->opt[OPT_TPU_FILMTYPE].cap &= 
          (s->hw->tpu.ControlMode == 1) ? ~SANE_CAP_INACTIVE : ~0;
      }
      else /* switch off */
      {
        s->val[OPT_TPU_ON].w = TPU_STAT_INACTIVE;
        s->opt[OPT_TPU_ON].title = "Turn On Transparency Unit";
        s->opt[OPT_TPU_TRANSPARENCY].cap |= SANE_CAP_INACTIVE;
        s->opt[OPT_TPU_FILMTYPE].cap |= SANE_CAP_INACTIVE;
      }
      s->opt[OPT_TPU_PN].cap ^= SANE_CAP_INACTIVE;
      s->opt[OPT_TPU_DCM].cap ^= SANE_CAP_INACTIVE;
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      return SANE_STATUS_GOOD;
      
    case OPT_TPU_DCM:
      if (s->val[OPT_TPU_DCM].s)
        free (s->val[OPT_TPU_DCM].s);
      s->val[OPT_TPU_DCM].s = strdup (val);
      
      s->opt[OPT_TPU_TRANSPARENCY].cap |= SANE_CAP_INACTIVE;
      s->opt[OPT_TPU_FILMTYPE].cap |= SANE_CAP_INACTIVE;
      if (!strcmp(s->val[OPT_TPU_DCM].s, 
                  "Correction according to Transparency Ratio"))
      {
        s->hw->tpu.ControlMode = 3;
        s->opt[OPT_TPU_TRANSPARENCY].cap &= ~SANE_CAP_INACTIVE;
      }
      else if (!strcmp(s->val[OPT_TPU_DCM].s,
                       "Correction according to Film type"))
      {
        s->hw->tpu.ControlMode = 1;
        s->opt[OPT_TPU_FILMTYPE].cap &= ~SANE_CAP_INACTIVE;
      }
      else s->hw->tpu.ControlMode = 0;
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      return SANE_STATUS_GOOD;
      
    case OPT_TPU_FILMTYPE:
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      return SANE_STATUS_GOOD;
      
    case OPT_MODE:
      if (info && strcmp (s->val[option].s, (SANE_String) val))
	*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      if (!strcmp(val, "Lineart") || !strcmp(val, "Halftone"))
      {
	/* For Lineart and Halftone: */
	/* Enable "threshold" */
	s->opt[OPT_THRESHOLD].cap      &= ~SANE_CAP_INACTIVE;

	/* Disable "custom gamma" and "brightness & contrast" */
	s->opt[OPT_CUSTOM_GAMMA].cap        |= SANE_CAP_INACTIVE;
	s->opt[OPT_CUSTOM_GAMMA_BIND].cap   |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR].cap        |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_R].cap      |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_G].cap      |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_B].cap      |= SANE_CAP_INACTIVE;

	s->opt[OPT_BRIGHTNESS].cap     |= SANE_CAP_INACTIVE ;
	s->opt[OPT_CONTRAST].cap       |= SANE_CAP_INACTIVE ;
      }
      else
      {
	/* For Gray and Color modes: */
	/* Disable "threshold" */
	s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

	/* Enable "custom gamma" and "brightness & contrast" */
	s->opt[OPT_CUSTOM_GAMMA].cap      &= ~SANE_CAP_INACTIVE;
	if (s->val[OPT_CUSTOM_GAMMA].w == SANE_TRUE)
	{
	  if (!strcmp(val, "Color"))
	  {
	    s->opt[OPT_CUSTOM_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
	    if (s->val[OPT_CUSTOM_GAMMA_BIND].w == SANE_TRUE)
	    {
	      s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    }
	    else
	    {
	      s->opt[OPT_GAMMA_VECTOR].cap   |=  SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	    }
	  }
	  else
	  {
	    s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	  }
	}
	else
	{
	  s->opt[OPT_BRIGHTNESS].cap   &= ~SANE_CAP_INACTIVE ;
	  s->opt[OPT_CONTRAST].cap     &= ~SANE_CAP_INACTIVE ;
	}
      }
      return (SANE_STATUS_GOOD);

    case OPT_NEGATIVE:
      if (info && strcmp (s->val[option].s, (SANE_String) val))
	*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      if (!strcmp(val, "Negatives"))
      {
	s->RIF = 0;
	s->opt[OPT_NEGATIVE_TYPE].cap &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_SCANNING_SPEED].cap &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_AE].cap |= SANE_CAP_INACTIVE;
      }
      else
      {
	s->RIF = 1;
	s->opt[OPT_NEGATIVE_TYPE].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_SCANNING_SPEED].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_AE].cap &= ~SANE_CAP_INACTIVE;
      }
      return (SANE_STATUS_GOOD);

    case OPT_NEGATIVE_TYPE:
      if (info && strcmp (s->val[option].s, (SANE_String) val))
	*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      for(i = 0; strcmp(val, negative_filmtype_list[i]); i++);
      s->negative_filmtype = i;
      return (SANE_STATUS_GOOD);

    case OPT_SCANNING_SPEED:
      if (info && strcmp (s->val[option].s, (SANE_String) val))
	*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      for(i = 0; strcmp(val, scanning_speed_list[i]); i++);
      s->scanning_speed = i;
      return (SANE_STATUS_GOOD);

    case OPT_PAGE:
      if (info && strcmp (s->val[option].s, (SANE_String) val))
	*info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
      if (s->val[option].s)
	free (s->val[option].s);
      s->val[option].s = strdup (val);
      if (info)
	*info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
      DBG(21, "value for option %s: %s\n", option_name[option], s->val[option].s);
      if (!strcmp(val, "Show normal options"))
      {
	DBG(21,"setting OPT_PAGE to 'Normal options'\n");
	s->opt[OPT_MODE_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_RESOLUTION_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_ENHANCEMENT_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_EJECT_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_ADF_GROUP].cap &= ~SANE_CAP_ADVANCED;

	s->opt[OPT_FOCUS_GROUP].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_MARGINS_GROUP].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_COLORS_GROUP].cap |= SANE_CAP_ADVANCED;
      }
      else if (!strcmp(val, "Show advanced options"))
      {
	DBG(21,"setting OPT_PAGE to 'Advanced options'\n");
	s->opt[OPT_MODE_GROUP].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_RESOLUTION_GROUP].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_ENHANCEMENT_GROUP].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_EJECT_GROUP].cap |= SANE_CAP_ADVANCED;
	s->opt[OPT_ADF_GROUP].cap |= SANE_CAP_ADVANCED;

	s->opt[OPT_FOCUS_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_MARGINS_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_COLORS_GROUP].cap &= ~SANE_CAP_ADVANCED;
      }
      else
      {
	s->opt[OPT_MODE_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_RESOLUTION_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_ENHANCEMENT_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_EJECT_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_ADF_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_FOCUS_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_MARGINS_GROUP].cap &= ~SANE_CAP_ADVANCED;
	s->opt[OPT_COLORS_GROUP].cap &= ~SANE_CAP_ADVANCED;
      }
      return (SANE_STATUS_GOOD);

    case OPT_EJECT_NOW:
      sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
      status = medium_position(s->fd);
      if (status != SANE_STATUS_GOOD)
      {
	DBG (21, "MEDIUM POSTITION failed\n");
	sanei_scsi_close (s->fd);
	s->fd = -1;
	return (SANE_STATUS_INVAL);
      }
DBG(21, "AF_NOW before = '%d'\n", s->AF_NOW);
      s->AF_NOW = SANE_TRUE;
DBG(21, "AF_NOW after = '%d'\n", s->AF_NOW);
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return status;

    case OPT_CUSTOM_GAMMA:
      w = *(SANE_Word *) val;

      if (w == s->val[OPT_CUSTOM_GAMMA].w)
	return SANE_STATUS_GOOD;		/* no change */

      if (info)
	*info |= SANE_INFO_RELOAD_OPTIONS;

      s->val[OPT_CUSTOM_GAMMA].w = w;
      if (w)
      {
	const char *mode = s->val[OPT_MODE].s;
	  
	if (!strcmp (mode, "Gray"))
	  s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	else if (!strcmp (mode, "Color"))
	{
	  s->opt[OPT_CUSTOM_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
	  if (s->val[OPT_CUSTOM_GAMMA_BIND].w == SANE_TRUE)
	  {
	    s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	  }
	  else
	  {
	    s->opt[OPT_GAMMA_VECTOR].cap   |=  SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
	    s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	  }
	}
	s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE ;
	s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE ;
      }
      else
      {
	s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR].cap   |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
      }

      return SANE_STATUS_GOOD;

    case OPT_CUSTOM_GAMMA_BIND:
      w = *(SANE_Word *) val;

      if (w == s->val[OPT_CUSTOM_GAMMA_BIND].w)
	return SANE_STATUS_GOOD;		/* no change */

      if (info)
	*info |= SANE_INFO_RELOAD_OPTIONS;

      s->val[OPT_CUSTOM_GAMMA_BIND].w = w;
      if (w)
      {
	s->opt[OPT_GAMMA_VECTOR].cap   &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
      }
      else
      {
	s->opt[OPT_GAMMA_VECTOR].cap   |=  SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
	s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
      }

      return SANE_STATUS_GOOD;

    case OPT_GAMMA_VECTOR:
    case OPT_GAMMA_VECTOR_R:
    case OPT_GAMMA_VECTOR_G:
    case OPT_GAMMA_VECTOR_B:
      memcpy (s->val[option].wa, val, s->opt[option].size);
      DBG(21, "setting gamma vector\n");
/*       if (info) */
/* 	*info |= SANE_INFO_RELOAD_OPTIONS; */
      return (SANE_STATUS_GOOD);

    }
  }

  DBG (1, "<< sane_control_option %s\n", option_name[option]);
  return (SANE_STATUS_INVAL);
}

/**************************************************************************/

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters * params)
{
  CANON_Scanner *s = handle;
  DBG (1, ">> sane_get_parameters\n");

  if (!s->scanning)
  {
    int width, length, xres, yres;
    const char *mode;

    memset (&s->params, 0, sizeof (s->params));

    width = SANE_UNFIX(s->val[OPT_BR_X].w - s->val[OPT_TL_X].w)
      * s->hw->info.mud / MM_PER_INCH;
    length = SANE_UNFIX(s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
      * s->hw->info.mud / MM_PER_INCH;

    xres  = s->val[OPT_X_RESOLUTION].w;
    yres  = s->val[OPT_Y_RESOLUTION].w;
    if ( (s->val[OPT_RESOLUTION_BIND].w == SANE_TRUE) 
	 || (s->val[OPT_PREVIEW].w == SANE_TRUE) )
    {
      yres = xres;
    }

    /* make best-effort guess at what parameters will look like once
       scanning starts.  */
    if (xres > 0 && yres > 0 && width > 0 && length > 0)
    {
      DBG(11, "sane_get_parameters: width='%d', xres='%d', mud='%d'\n",
	  width, xres, s->hw->info.mud);
      s->params.pixels_per_line = width * xres / s->hw->info.mud;
      DBG(11, "sane_get_parameters: length='%d', yres='%d', mud='%d'\n",
	  length, yres, s->hw->info.mud);
      s->params.lines = length * yres / s->hw->info.mud;
      DBG(11, "sane_get_parameters: pixels_per_line='%d', lines='%d'\n",
	  s->params.pixels_per_line, s->params.lines);
    }

    mode = s->val[OPT_MODE].s;
    if (strcmp (mode, "Lineart") == 0 || strcmp (mode, "Halftone") == 0)
    {
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line / 8;
      /* workaround rounding problems */
      s->params.pixels_per_line = s->params.bytes_per_line * 8;
      s->params.depth = 1;
    }
    else if (strcmp (mode, "Gray") == 0)
    {
      s->params.format = SANE_FRAME_GRAY;
      s->params.bytes_per_line = s->params.pixels_per_line;
      s->params.depth = 8;
    }
    else
    {
      s->params.format = SANE_FRAME_RGB;
      s->params.bytes_per_line = 3 * s->params.pixels_per_line;
      s->params.depth = 8;
    }
    s->params.last_frame = SANE_TRUE;
  }

  DBG(11, "sane_get_parameters: xres='%d', yres='%d', pixels_per_line='%d', bytes_per_line='%d', lines='%d'\n", s->xres, s->yres, s->params.pixels_per_line, s->params.bytes_per_line, s->params.lines);

  if (params)
    *params = s->params;

  DBG (1, "<< sane_get_parameters\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

SANE_Status
sane_start (SANE_Handle handle)
{
  int mode;
  char *mode_str;
  CANON_Scanner *s = handle;
  SANE_Status status;
  u_char wbuf[72], dbuf[28], ebuf[64];
  size_t buf_size;
  int i;

  DBG (1, ">> sane_start\n");

  s->scanning = SANE_FALSE;

  if( (s->hw->adf.Status == SANE_TRUE)
      && (s->val[OPT_FLATBED_ONLY].w != SANE_TRUE)
      && (s->hw->adf.Problem != 0))
  {
    DBG (3, "SCANNER ADF HAS A PROBLEM\n");
    if (s->hw->adf.Problem & 0x08)
    {
      status = SANE_STATUS_COVER_OPEN;
      DBG (3, "ADF Cover Open\n");
    }
    else if (s->hw->adf.Problem & 0x04)
    {
      status = SANE_STATUS_JAMMED;
      DBG (3, "ADF Paper Jam\n");
    }
    else /* adf.Problem = 0x02 */
    {
      status = SANE_STATUS_NO_DOCS;
      DBG (3, "ADF No More Documents\n");
    }
    return status;
  }
  else if( (s->hw->adf.Status == SANE_TRUE)
      && (s->val[OPT_FLATBED_ONLY].w == SANE_TRUE))
  {
    set_adf_mode(s->fd, s->hw->adf.Priority);
    /* 2.23 define ADF Mode */
  }
  else if( (s->val[OPT_AE].w == SANE_TRUE)
      && (!strcmp(s->val[OPT_NEGATIVE].s, "Slides"))
      && (s->val[OPT_PREVIEW].w == SANE_FALSE))
  {
    DBG (1, ">> going to adjust_hilo_points\n"); 
    adjust_hilo_points(s);
    DBG (1, "<< returned from adjust_hilo_points\n"); 
  }

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, 0);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "open of %s failed: %s\n",
	 s->hw->sane.name, sane_strstatus (status));
    return (status);
  }


  /* Do focus, but not for the preview */
 if( (s->val[OPT_PREVIEW].w == SANE_FALSE) &&
     (s->AF_NOW == SANE_TRUE))
  {
    do_focus(s);
    if(s->val[OPT_AF_ONCE].w == SANE_TRUE)
    {
      s->AF_NOW = SANE_FALSE;
    }
  }

  if (s->val[OPT_CUSTOM_GAMMA].w == 1)
  {
    do_gamma(s);
  }

#if 1
  DBG (3, "attach: sending GET SCAN MODE for scan control conditions\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 20;
  status = get_scan_mode (s->fd, (u_char)SCAN_CONTROL_CONDITIONS, 
			  ebuf, &buf_size);
/*   if (status != SANE_STATUS_GOOD) */
/*   { */
/*     DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n"); */
/*     sanei_scsi_close (s->fd); */
/*     return (SANE_STATUS_INVAL); */
/*   } */
  for (i=0; i<buf_size; i++)
  {
    DBG(3, "scan mode control byte[%d] = %d\n", i, ebuf[i]);
  }

  DBG (3, "attach: sending GET SCAN MODE for transparency unit\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 12;
  status = get_scan_mode (s->fd, (u_char)TRANSPARENCY_UNIT, 
			  ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "attach: GET SCAN MODE for transparency unit failed\n");
    sanei_scsi_close (s->fd);
    return (SANE_STATUS_INVAL);
  }
  for (i=0; i<buf_size; i++)
  {
    DBG(3, "scan mode control byte[%d] = %d\n", i, ebuf[i]);
  }

#endif


  mode_str = s->val[OPT_MODE].s;
  s->xres = s->val[OPT_X_RESOLUTION].w;
  s->yres = s->val[OPT_Y_RESOLUTION].w;
  
  if ( (s->val[OPT_RESOLUTION_BIND].w == SANE_TRUE) 
       || (s->val[OPT_PREVIEW].w == SANE_TRUE) )
  {
    s->yres = s->xres;
  }

  s->ulx = SANE_UNFIX(s->val[OPT_TL_X].w) * s->hw->info.mud / MM_PER_INCH;
  s->uly = SANE_UNFIX(s->val[OPT_TL_Y].w) * s->hw->info.mud / MM_PER_INCH;

  s->width = SANE_UNFIX(s->val[OPT_BR_X].w - s->val[OPT_TL_X].w)
    * s->hw->info.mud / MM_PER_INCH;
  s->length = SANE_UNFIX(s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
    * s->hw->info.mud / MM_PER_INCH;

  DBG(11, "s->width='%d', s->length='%d'\n", s->width, s->length);

  s->RIF = (s->hw->info.model == CS2700) ? 
    s->RIF :
    ((strcmp (mode_str, "Lineart") == 0) || 
     (strcmp (mode_str, "Halftone") == 0)) 
    ? s->val[OPT_HNEGATIVE].w : !s->val[OPT_HNEGATIVE].w;

/*   s->brightness = (s->RIF == 0) ?  */
/*     s->val[OPT_BRIGHTNESS].w : (255 - s->val[OPT_BRIGHTNESS].w); */
  s->brightness = s->val[OPT_BRIGHTNESS].w;
  s->contrast = s->val[OPT_CONTRAST].w;
  s->threshold = s->val[OPT_THRESHOLD].w;
  s->bpp = s->params.depth;


  s->GRC = s->val[OPT_CUSTOM_GAMMA].w;
  s->Mirror = s->val[OPT_MIRROR].w;
  s->AE  = s->val[OPT_AE].w;

  
  s->HiliteG = s->val[OPT_HILITE_G].w;
  s->ShadowG = s->val[OPT_SHADOW_G].w;
  if (s->val[OPT_BIND_HILO].w == SANE_TRUE)
  {
    s->HiliteR = s->val[OPT_HILITE_G].w;
    s->ShadowR = s->val[OPT_SHADOW_G].w;
    s->HiliteB = s->val[OPT_HILITE_G].w;
    s->ShadowB = s->val[OPT_SHADOW_G].w;
  }
  else
  {
    s->HiliteR = s->val[OPT_HILITE_R].w;
    s->ShadowR = s->val[OPT_SHADOW_R].w;
    s->HiliteB = s->val[OPT_HILITE_B].w;
    s->ShadowB = s->val[OPT_SHADOW_B].w;
  }

  if (strcmp (mode_str, "Lineart") == 0)
  {
    mode = 4;
    s->image_composition = 0;
  }
  else if (strcmp (mode_str, "Halftone") == 0)
  {
    mode = 4;
    s->image_composition = 1;
  }
  else if (strcmp (mode_str, "Gray") == 0)
  {
    mode = 5;
    s->image_composition = 2;
  }
  else if (strcmp (mode_str, "Color") == 0)
  {
    mode = 6;
    s->image_composition = 5;
  }

  memset (wbuf, 0, sizeof (wbuf));
  wbuf[7] = 64;
  wbuf[10] = s->xres >> 8;
  wbuf[11] = s->xres;
  wbuf[12] = s->yres >> 8;
  wbuf[13] = s->yres;
  wbuf[14] = s->ulx >> 24;
  wbuf[15] = s->ulx >> 16;
  wbuf[16] = s->ulx >> 8;
  wbuf[17] = s->ulx;
  wbuf[18] = s->uly >> 24;
  wbuf[19] = s->uly >> 16;
  wbuf[20] = s->uly >> 8;
  wbuf[21] = s->uly;
  wbuf[22] = s->width >> 24;
  wbuf[23] = s->width >> 16;
  wbuf[24] = s->width >> 8;
  wbuf[25] = s->width;
  wbuf[26] = s->length >> 24;
  wbuf[27] = s->length >> 16;
  wbuf[28] = s->length >> 8;
  wbuf[29] = s->length;
  wbuf[30] = s->brightness;
  wbuf[31] = s->threshold;
  wbuf[32] = s->contrast;
  wbuf[33] = s->image_composition;
  wbuf[34] = s->bpp;
  wbuf[36] = 1;
/*   wbuf[37] = (s->RIF << 7) + 3; */
  wbuf[37] = (1 << 7) + 3;
  wbuf[50] = (s->GRC << 3) | (s->Mirror << 2) | (s->AE);
/*   wbuf[50] = (s->GRC << 3) | (s->Mirror << 2) ; */
/*   if(s->RIF == 1) */
/*   { */
/*     wbuf[50] |= s->AE; */
/*   } */

  wbuf[54] = 2;
  wbuf[57] = 1;
  wbuf[58] = 1;
  wbuf[59] = s->HiliteR;
  wbuf[60] = s->ShadowR;
  wbuf[62] = s->HiliteG;
  wbuf[64] = s->ShadowG;
  wbuf[70] = s->HiliteB;
  wbuf[71] = s->ShadowB;

  DBG(7, "RIF=%d, GRC=%d, Mirror=%d, AE=%d\n",
      s->RIF, s->GRC, s->Mirror, s->AE);
  DBG(7, "HR=%d, SR=%d, HG=%d, SG=%d, HB=%d, SB=%d\n",
      s->HiliteR, s->ShadowR,
      s->HiliteG, s->ShadowG,
      s->HiliteB, s->ShadowB);


  buf_size = sizeof (wbuf);
  status = set_window (s->fd, wbuf);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "SET WINDOW failed: %s\n", sane_strstatus (status));
    return (status);
  }

  buf_size = sizeof (wbuf);
  memset (wbuf, 0, buf_size);
  status = get_window (s->fd, wbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "GET WINDOW failed: %s\n", sane_strstatus (status));
    return (status);
  }
  DBG (5, "xres=%d\n", (wbuf[10] * 256) + wbuf[11]);
  DBG (5, "yres=%d\n", (wbuf[12] * 256) + wbuf[13]);
  DBG (5, "ulx=%d\n", (wbuf[14] * 256 * 256 * 256)
       + (wbuf[15] * 256 * 256) + (wbuf[16] * 256) + wbuf[17]);
  DBG (5, "uly=%d\n", (wbuf[18] * 256 * 256 * 256)
       + (wbuf[19] * 256 * 256) + (wbuf[20] * 256) + wbuf[21]);
  DBG (5, "width=%d\n", (wbuf[22] * 256 * 256 * 256)
       + (wbuf[23] * 256 * 256) + (wbuf[24] * 256) + wbuf[25]);
  DBG (5, "length=%d\n", (wbuf[26] * 256 * 256 * 256)
       + (wbuf[27] * 256 * 256) + (wbuf[28] * 256) + wbuf[29]);


#if 1
  DBG (3, "sane_start: sending DEFINE SCAN MODE for transparency unit, NP=%d, Negative film type=%d\n", !s->RIF, s->negative_filmtype);
  memset (wbuf, 0, sizeof (wbuf));
  wbuf[0] = 0x02;
  wbuf[1] = 6;
  wbuf[2] = 0x80;
  wbuf[3] = 0x05;
  wbuf[4] = 39;
  wbuf[5] = 16;
  wbuf[6] = !s->RIF;
  wbuf[7] = s->negative_filmtype;
  status = define_scan_mode (s->fd, TRANSPARENCY_UNIT, wbuf);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "define scan mode failed: %s\n", sane_strstatus (status));
    return (status);
  }
#endif

#if 1
  DBG (3, "sane_start: sending DEFINE SCAN MODE for scan control conditions\n");
  memset (wbuf, 0, sizeof (wbuf));
  wbuf[0] = 0x20;
  wbuf[1] = 14;
  wbuf[11] = s->scanning_speed;
  status = define_scan_mode (s->fd, SCAN_CONTROL_CONDITIONS, wbuf);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "define scan mode failed: %s\n", sane_strstatus (status));
    return (status);
  }

  DBG (3, "sane_start: sending GET SCAN MODE for scan control conditions\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  status = get_scan_mode (s->fd, SCAN_CONTROL_CONDITIONS, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "sane_start: GET SCAN MODE for scan control conditions failed\n");
    sanei_scsi_close (s->fd);
    return (SANE_STATUS_INVAL);
  }
  for (i=0; i<buf_size; i++)
  {
    DBG(3, "scan mode byte[%d] = %d\n", i, ebuf[i]);
  }
#endif

  status = scan (s->fd);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "start of scan failed: %s\n", sane_strstatus (status));
    return (status);
  }

  buf_size = sizeof (dbuf);
  memset (dbuf, 0, buf_size);
  status = get_data_status (s->fd, dbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
  {
    DBG (1, "GET DATA STATUS failed: %s\n", sane_strstatus (status));
    return (status);
  }
  DBG (5, "Magnified Width=%d\n", (dbuf[12] * 256 * 256 * 256)
       + (dbuf[13] * 256 * 256) + (dbuf[14] * 256) + dbuf[15]);
  DBG (5, "Magnified Length=%d\n", (dbuf[16] * 256 * 256 * 256)
       + (dbuf[17] * 256 * 256) + (dbuf[18] * 256) + dbuf[19]);
  DBG (5, "Data=%d bytes\n", (dbuf[20] * 256 * 256 * 256)
       + (dbuf[21] * 256 * 256) + (dbuf[22] * 256) + dbuf[23]);

  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;

  DBG (1, "%d pixels per line, %d bytes, %d lines high, total %lu bytes, "
       "dpi=%d\n", s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (u_long) s->bytes_to_read, s->val[OPT_X_RESOLUTION].w);

  s->scanning = SANE_TRUE;

  DBG (1, "<< sane_start\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

SANE_Status
sane_read (SANE_Handle handle, SANE_Byte * buf, SANE_Int max_len,
	   SANE_Int * len)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  size_t nread;

  DBG (21, ">> sane_read\n");

  *len = 0;

  DBG(21, "   sane_read: nread=%d, bytes_to_read=%d\n", nread, s->bytes_to_read);
  if (s->bytes_to_read == 0)
  {
    do_cancel (s);
    return (SANE_STATUS_EOF);
  }

  if (!s->scanning)
    return (do_cancel (s));

  nread = max_len;
  if (nread > s->bytes_to_read)
    nread = s->bytes_to_read;

  status = read_data (s->fd, buf, &nread);
  if (status != SANE_STATUS_GOOD)
  {
    do_cancel (s);
    return (SANE_STATUS_IO_ERROR);
  }
  *len = nread;
  s->bytes_to_read -= nread;

  DBG(21, "   sane_read: nread=%d, bytes_to_read=%d\n", nread, s->bytes_to_read);

  DBG (21, "<< sane_read\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

void
sane_cancel (SANE_Handle handle)
{
  CANON_Scanner *s = handle;
  DBG (1, ">> sane_cancel\n");
  
  s->scanning = SANE_FALSE;

  DBG (1, "<< sane_cancel\n");
}

/**************************************************************************/

SANE_Status
sane_set_io_mode (SANE_Handle handle, SANE_Bool non_blocking)
{
  DBG (1, ">> sane_set_io_mode\n");
  DBG (1, "<< sane_set_io_mode\n");

  return SANE_STATUS_UNSUPPORTED;
}

/**************************************************************************/

SANE_Status
sane_get_select_fd (SANE_Handle handle, SANE_Int * fd)
{
  DBG (1, ">> sane_get_select_fd\n");
  DBG (1, "<< sane_get_select_fd\n");

  return SANE_STATUS_UNSUPPORTED;
}

/**************************************************************************/
