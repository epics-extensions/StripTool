/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripCA.h"

#include <cadef.h>
#include <db_access.h>

typedef struct _StripCAInfo
{
  Strip		strip;
  struct	_ChannelData
  {
    chid		chan_id;
    evid		event_id;
    double		value;
    struct _StripCAInfo	*this;
  } chan_data[STRIP_MAX_CURVES];
} StripCAInfo;


/* ====== Prototypes ====== */
static void	addfd_callback		(void *, int, int);
static void	work_callback		(void *);
static void	connect_callback	(struct connection_handler_args);
static void	info_callback		(struct event_handler_args);
static void	data_callback		(struct event_handler_args);
static double	value_callback		(void *);


/*
 * StripCA_initialize
 */
StripCA		StripCA_initialize	(Strip strip)
{
  StripCAInfo	*sca = NULL;
  int		status;
  int		i;

  
  if ((sca = (StripCAInfo *)calloc (sizeof (StripCAInfo), 1)) != NULL)
    {
      sca->strip = strip;
      status = ca_task_initialize ();
      if (status != ECA_NORMAL)
	{
	  SEVCHK (status, "StripCA: Channel Access initialization error");
	  free (sca);
	  sca = NULL;
	}
      else for (i = 0; i < STRIP_MAX_CURVES; i++)
	{
	  ca_add_fd_registration (addfd_callback, sca);
	  sca->chan_data[i].this 	= sca;
	  sca->chan_data[i].chan_id 	= NULL;
	}
    }

  return (StripCA)sca;
}


/*
 * StripCA_terminate
 */
void		StripCA_terminate	(StripCA the_sca)
{
  ca_task_exit ();
}



/*
 * StripCA_request_connect
 *
 *	Requests connection for the specified curve.
 */
int	StripCA_request_connect	(StripCurve curve, void *the_sca)
{
  StripCAInfo	*sca = (StripCAInfo *)the_sca;
  int		i;
  int		ret_val;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (sca->chan_data[i].chan_id == NULL)
      break;

  if (ret_val = (i < STRIP_MAX_CURVES))
    {
      StripCurve_setattr (curve, STRIPCURVE_SAMPLEDATA, &sca->chan_data[i], 0);
      ret_val = ca_search_and_connect
	((char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME),
	 &sca->chan_data[i].chan_id,
	 connect_callback,
	 curve);
      if (ret_val != ECA_NORMAL)
	{
	  SEVCHK (ret_val, "StripCA: Channel Access unable to connect\n");
	  fprintf
	    (stderr, "channel name: %s\n",
	     (char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME));
	  ret_val = 0;
	}
      else ret_val = 1;
    }

  ca_flush_io();

  return ret_val;
}


/*
 * StripCA_request_disconnect
 *
 *	Requests disconnection for the specified curve.
 */
int	StripCA_request_disconnect	(StripCurve 	curve,
					 void 		*the_sca)
{
  struct _ChannelData	*cd;
  int			i;
  int			ret_val;

  cd = (struct _ChannelData *) StripCurve_getattr_val
    (curve, STRIPCURVE_SAMPLEDATA);

  if (cd->event_id != NULL)
    {
      if ((ret_val = ca_clear_event (cd->event_id)) != ECA_NORMAL)
	{
	  SEVCHK
	    (ret_val, "StripCA_request_disconnect: error in ca_clear_event");
	  ret_val = 0;
	}
      else
	{
	  cd->event_id = NULL;
	  ret_val = 1;
	}
    }
  ca_flush_io();
  
  if (cd->chan_id != NULL)
    {
      /* **** ca_clear_channel() causes info to be printed to stdout **** */
      if ((ret_val = ca_clear_channel (cd->chan_id)) != ECA_NORMAL)
	{
	  SEVCHK
	    (ret_val, "StripCA_request_disconnect: error in ca_clear_channel");
	  ret_val = 0;
	}
      else
	{
	  cd->chan_id = NULL;
	  ret_val = 1;
	}
    }
  else ret_val = 1;

  ca_flush_io();
  return ret_val;
}


/*
 * addfd_callback
 *
 *	Add new file descriptors to select upon.
 *	Remove old file descriptors from selection.
 */
static void	addfd_callback	(void *data, int fd, int active)
{
  StripCAInfo	*strip_ca = (StripCAInfo *)data;
  if (active)
    Strip_addfd (strip_ca->strip, fd, work_callback, strip_ca);
  else
    Strip_clearfd (strip_ca->strip, fd);
}


/*
 * work_callback
 *
 *	Gives control to Channel Access for a while.
 */
static void	work_callback		(void *data)
{
  ca_pend_event (STRIP_CA_PEND_TIMEOUT);
}


/*
 * connect_callback
 */
static void	connect_callback	(struct connection_handler_args args)
{
  StripCurve		curve;
  struct _ChannelData	*cd;
  int			status;

  curve = (StripCurve)(ca_puser (args.chid));
  cd = (struct _ChannelData *)StripCurve_getattr_val
    (curve, STRIPCURVE_SAMPLEDATA);

  switch (ca_state (args.chid))
    {
    case cs_never_conn:
      fprintf (stderr, "StripCA connect_callback: ioc not found\n");
      cd->chan_id = NULL;
      cd->event_id = NULL;
      Strip_freecurve (cd->this->strip, curve);
      break;
      
    case cs_prev_conn:
      fprintf
	(stderr,
	 "StripCA connect_callback: IOC unavailable for %s\n",
	 ca_name (args.chid));
      StripCurve_setstat
	(curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
      break;
      
    case cs_conn:
      /* now connected, so get the control info if this is first time */
      if (cd->event_id == 0)
	{
	  status = ca_get_callback
	    (DBR_CTRL_DOUBLE, cd->chan_id, info_callback, curve);
	  if (status != ECA_NORMAL)
	    {
	      SEVCHK
		(status, "StripCA connect_callback: error in ca_get_callback");
	      Strip_freecurve (cd->this->strip, curve);
	    }
	}
      break;
      
    case cs_closed:
      fprintf (stderr, "StripCA connect_callback: invalid chid\n");
      break;
    }

  fflush (stderr);

  ca_flush_io();
}


/*
 * info_callback
 */
static void	info_callback		(struct event_handler_args args)
{
  StripCurve			curve;
  struct _ChannelData		*cd;
  struct dbr_ctrl_double	*ctrl;
  int				status;
  double			low, hi;

  curve = (StripCurve)(ca_puser (args.chid));
  cd = (struct _ChannelData *)StripCurve_getattr_val
    (curve, STRIPCURVE_SAMPLEDATA);

  if (args.status != ECA_NORMAL)
    {
      fprintf
	(stderr,
	 "StripCA info_callback:\n"
	 "  [%s] get: %s\n",
	 ca_name(cd->chan_id),
	 ca_message_text[CA_EXTRACT_MSG_NO(args.status)]);
    }
  else
    {
      ctrl = (struct dbr_ctrl_double *)args.dbr;

      low = ctrl->lower_disp_limit;
      hi = ctrl->upper_disp_limit;

      if (hi <= low)
	{
	  low = ctrl->lower_ctrl_limit;
	  hi = ctrl->upper_ctrl_limit;
	  if (hi <= low)
	    {
	      if (ctrl->value == 0)
		{
		  hi = 100;
		  low = -hi;
		}
	      else
		{
		  low = ctrl->value - (ctrl->value / 10.0);
		  hi = ctrl->value + (ctrl->value / 10.0);
		}
	    }
	}

      if (!StripCurve_getstat (curve, STRIPCURVE_EGU_SET))
	StripCurve_setattr (curve, STRIPCURVE_EGU, ctrl->units, 0);
      if (!StripCurve_getstat (curve, STRIPCURVE_PRECISION_SET))
	StripCurve_setattr (curve, STRIPCURVE_PRECISION, ctrl->precision, 0);
      if (!StripCurve_getstat (curve, STRIPCURVE_MIN_SET))
	StripCurve_setattr (curve, STRIPCURVE_MIN, low, 0);
      if (!StripCurve_getstat (curve, STRIPCURVE_MAX_SET))
	StripCurve_setattr (curve, STRIPCURVE_MAX, hi, 0);

      status = ca_add_event
	(DBR_STS_DOUBLE, cd->chan_id, data_callback, curve, &cd->event_id);
      if (status != ECA_NORMAL)
	{
	  SEVCHK
	    (status, "StripCA info_callback: error in ca_get_callback");
	  Strip_freecurve (cd->this->strip, curve);
	}
    }
  
  ca_flush_io();
}


/*
 * data_callback
 */
static void	data_callback		(struct event_handler_args args)
{
  StripCurve			curve;
  struct _ChannelData		*cd;
  struct dbr_sts_double		*sts;
  int				status;

  curve = (StripCurve)ca_puser (args.chid);
  cd = (struct _ChannelData *)StripCurve_getattr_val
    (curve, STRIPCURVE_SAMPLEDATA);

  if (args.status != ECA_NORMAL)
    {
      fprintf
	(stderr,
	 "StripCA data_callback:\n"
	 "  [%s] get: %s\n",
	 ca_name(cd->chan_id),
	 ca_message_text[CA_EXTRACT_MSG_NO(args.status)]);
      StripCurve_setstat
	(curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
    }
  else
    {
      if (StripCurve_getstat (curve, STRIPCURVE_WAITING))
	{
	  StripCurve_setattr (curve, STRIPCURVE_SAMPLEFUNC, value_callback, 0);
	  Strip_setconnected (cd->this->strip, curve);
	}
      sts = (struct dbr_sts_double *)args.dbr;
      cd->value = sts->value;
    }
}


/*
 * value_callback
 *
 *	Returns the current value specified by the CurveData passed in.
 */
static double	value_callback	(void *data)
{
  struct _ChannelData	*cd = (struct _ChannelData *)data;

  return cd->value;
}
