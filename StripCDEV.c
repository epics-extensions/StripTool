/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripCDEV.h"

#include <cdev.h>
#include <math.h>

#define MAX_BUF_LEN	256
#define DEFAULT_ATTR	"VAL"

#define DEF_DEVATTR_UNITS	"undefined"
#define DEF_DEVATTR_PRECISION	4
#define DEF_DEVATTR_DISPLO	-1.0
#define DEF_DEVATTR_DISPHI	1.0

typedef enum
{
  DEVATTR_VALUE = 0,
  DEVATTR_UNITS,
  DEVATTR_PRECISION,
  DEVATTR_DISPLO,
  DEVATTR_DISPHI,
  DEVATTR_COUNT
}
DevAttr;

static char	*DevAttrStr[DEVATTR_COUNT] =
{
  "value",
  "units",
  "precision",
  "displayLow",
  "displayHigh"
};

typedef struct _StripCDEVInfo
{
  Strip		strip;
  struct	_DeviceData
  {
    cdev_cbk_t			cb_id;
    char			buf[MAX_BUF_LEN];
    char			*dev, *attr;
    int				tag;
    double			value;
    struct _StripCDEVInfo	*this;
  } dev_data[STRIP_MAX_CURVES];
} StripCDEVInfo;
      

/* ====== Prototypes ====== */
static void	fd_callback		(int, int, void *);
static void	work_callback		(void *);
static void	info_callback		(int,
					 void *,
					 cdev_request_t,
					 cdev_data_t);
static void	monitor_callback	(int,
					 void *,
					 cdev_request_t,
					 cdev_data_t);
static double	value_callback		(void *);

/*
 * StripCDEV_initialize
 */
StripCDEV	StripCDEV_initialize	(Strip strip)
{
  StripCDEVInfo	*scd = NULL;
  int		status;
  int		fd[MAX_BUF_LEN];
  int		i;

  if ((scd = (StripCDEVInfo *)calloc (sizeof (StripCDEVInfo), 1)) != NULL)
    {
      scd->strip = strip;
      for (i = 0; i < STRIP_MAX_CURVES; i++)
	{
	  scd->dev_data[i].cb_id = 0;
	  scd->dev_data[i].tag = -1;
	  scd->dev_data[i].this = scd;
	}

      cdevSetThreshold (CDEV_SEVERITY_ERROR);

      i = MAX_BUF_LEN;
      status = cdevGetFds (fd, &i);
      if (status == CDEV_SUCCESS)
	{
	  for (i--; i >= 0; i--)
	    Strip_addfd (scd->strip, fd[i], work_callback, scd);
	  status = addFdChangedCallback (fd_callback, scd);
	  if (status = CDEV_SUCCESS)
	    cdevFlush();
	}
      
     if (status != CDEV_SUCCESS)
	{
	  free (scd);
	  scd = NULL;
	}
     }
  
  return (StripCDEV)scd;
}


/*
 * StripCDEV_terminate
 */
void	StripCDEV_terminate	(StripCDEV the_scd)
{
  StripCDEVInfo	*scd = (StripCDEVInfo *)the_scd;
  int		i;
  
  for (i = 0; i < STRIP_MAX_CURVES; i++)
    {
      if (scd->dev_data[i].cb_id != 0)
	cdevCbkFree (scd->dev_data[i].cb_id);
    }
  free (scd);
}


/*
 * StripCDEV_request_connect
 */
int	StripCDEV_request_connect	(StripCurve curve, void *the_scd)
{
  StripCDEVInfo		*scd = (StripCDEVInfo *)the_scd;
  struct _DeviceData	*dd;
  int			i;
  int			status;
  int			ret_val;
  char			msg_buf[MAX_BUF_LEN];
  int			tag;
  int			one = 1;
  cdev_data_t		data_id;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (scd->dev_data[i].cb_id == 0)
      break;

  if (ret_val = (i < STRIP_MAX_CURVES))
    {
      dd = &scd->dev_data[i];
      StripCurve_setattr (curve, STRIPCURVE_SAMPLEDATA, dd, 0);
      
      /* parse string designating the requested value for device/attribute */
      strcpy(dd->buf, (char *)StripCurve_getattr_val (curve, STRIPCURVE_NAME));
      for (dd->dev = dd->attr = dd->buf;
	   (*dd->attr != '\0') && (*dd->attr != '.');
	   dd->attr++);
      if (*dd->attr == '.')
	{
	  *dd->attr = '\0';	/* separate the device from the attribute */
	  dd->attr++;		/* point at the attribute string */
	}
      else dd->attr = DEFAULT_ATTR;

      /* set the device context to return useful attributes */
      cdevDataAllocate (&data_id);
      for (i = 0; i < DEVATTR_COUNT; i++)
	{
	  status = cdevDataTagC2I (DevAttrStr[i], &tag);
	  if (status == CDEV_SUCCESS)
	    cdevDataInsert (data_id, tag, CDEV_INT_32, &one);
	}
      cdevSetContext (dd->dev, data_id);
      
      /* set up the monitor */
      cdevCbkAllocate (info_callback, curve, &dd->cb_id);
      status = cdevSendCallback (dd->dev, "get", 0, dd->cb_id);
      ret_val = (status == CDEV_SUCCESS);
      cdevFlush();
      cdevDataFree (data_id);
    }

  return ret_val;
}


/*
 * StripCDEV_request_disconnect
 */
int	StripCDEV_request_disconnect	(StripCurve curve, void *the_scd)
{
  struct _DeviceData	*dd;
  int			i;
  int			status;
  int			ret_val;
  char			msg_buf[MAX_BUF_LEN];

  dd = (struct _DeviceData *)StripCurve_getattr_val
    (curve, STRIPCURVE_SAMPLEDATA);

  if (dd->cb_id != 0)
    {
      sprintf (msg_buf, "monitorOff %s", dd->attr);
      status = cdevSendCallback (dd->dev, msg_buf, 0, dd->cb_id);
      cdevFlush();
      cdevCbkFree (dd->cb_id);
      dd->cb_id = 0;
      dd->tag = -1;
      ret_val = 1;
    }
  else ret_val = 1;

  return ret_val;
}


/*
 * fd_callback
 *
 *	Add new file descriptors to select upon.
 *	Remove old file descriptors from selection.
 */
static void	fd_callback	(int fd, int opened, void *data)
{
  StripCDEVInfo	*scd = (StripCDEVInfo *)data;
  if (opened)
    Strip_addfd (scd->strip, fd, work_callback, scd);
  else Strip_clearfd (scd->strip, fd);
}


/*
 * work_callback
 *
 *	Gives control to cdev for a while.
 */
static void	work_callback		(void *data)
{
  cdevPoll();
}


/*
 * info_callback
 */
static void	info_callback		(int		status,
					 void 		*arg,
					 cdev_request_t	req_id,
					 cdev_data_t	data_id)
{
  StripCurve		curve = (StripCurve)arg;
  struct _DeviceData	*dd;
  double		lo, hi;
  int			tag;
  int			one = 1;
  char			msg_buf[MAX_BUF_LEN];
  cdev_data_t		temp_dat;

  union _val {
    int		i;
    double	d;
    char	*s;
  } val;

  if (status != CDEV_SUCCESS)
    {
      fprintf (stdout, "StripCDEV:monitor_callback() status = %d", status);
      fflush (stdout);
    }
  else
    {
      /* first, get the DeviceData structure from the curve */
      dd = (struct _DeviceData *)StripCurve_getattr_val
	(curve, STRIPCURVE_SAMPLEDATA);

/*      cdevDataAsciiDump (data_id, stdout); */

      /* store the value tag, if not already known */
      if (dd->tag < 0)
	cdevDataTagC2I (DevAttrStr[DEVATTR_VALUE], &dd->tag);
      
      /* get the value */
      cdevDataGet (data_id, dd->tag, CDEV_DBL, &dd->value);
      /*
      fprintf (stdout, "%s.%s = %.8lf\n", dd->dev, dd->attr, dd->value);
      */
      
      /* get context info for curve */
      /* units */
      if (!StripCurve_getstat (curve, STRIPCURVE_EGU_SET))
	{
	  status = cdevDataTagC2I (DevAttrStr[DEVATTR_UNITS], &tag);
	  if (status == CDEV_SUCCESS)
	    status = cdevDataGet (data_id, tag, CDEV_STR, &val.s);
	  if (status != CDEV_SUCCESS)
	    StripCurve_setattr (curve, STRIPCURVE_EGU, DEF_DEVATTR_UNITS, 0);
	  else
	    {
	      StripCurve_setattr (curve, STRIPCURVE_EGU, val.s, 0);
	      free (val.s);
	    }
	}
      
      /* precision */
      if (!StripCurve_getstat (curve, STRIPCURVE_PRECISION_SET))
	{
	  status = cdevDataTagC2I (DevAttrStr[DEVATTR_PRECISION], &tag);
	  if (status == CDEV_SUCCESS)
	    status = cdevDataGet (data_id, tag, CDEV_INT_32, &val.i);
	  if (status != CDEV_SUCCESS)
	    val.i = DEF_DEVATTR_PRECISION;
	  StripCurve_setattr (curve, STRIPCURVE_PRECISION, val.i, 0);
	}
      
      
      lo = floor (dd->value - fabs (dd->value));
      hi = ceil (dd->value + fabs (dd->value));
      if (lo == hi)
	{
	  lo = DEF_DEVATTR_DISPLO;
	  hi = DEF_DEVATTR_DISPHI;
	}
      
      /* display low */
      if (!StripCurve_getstat (curve, STRIPCURVE_MIN_SET))
	{
	  status = cdevDataTagC2I (DevAttrStr[DEVATTR_DISPLO], &tag);
	  if (status == CDEV_SUCCESS)
	    status = cdevDataGet (data_id, tag, CDEV_DBL, &val.d);
	  if (status != CDEV_SUCCESS)
	    val.d = lo;
	  StripCurve_setattr (curve, STRIPCURVE_MIN, val.d, 0);
	}
      
      /* display high */
      if (!StripCurve_getstat (curve, STRIPCURVE_MAX_SET))
	{
	  status = cdevDataTagC2I (DevAttrStr[DEVATTR_DISPHI], &tag);
	  if (status == CDEV_SUCCESS)
	    status = cdevDataGet (data_id, tag, CDEV_DBL, &val.d);
	  if (status != CDEV_SUCCESS)
	    val.d = (hi > lo? hi : ceil (lo + 2*lo));
	  StripCurve_setattr (curve, STRIPCURVE_MAX, val.d, 0);
	}

      /* set the device context to return useful attributes */
      cdevDataAllocate (&temp_dat);
      cdevDataTagC2I (DevAttrStr[DEVATTR_VALUE], &tag);
      cdevDataInsert (temp_dat, tag, CDEV_INT_32, &one);
      cdevSetContext (dd->dev, temp_dat);
      
      /* register the monitor callback */
      cdevCbkFree (dd->cb_id);
      cdevCbkAllocate (monitor_callback, curve, &dd->cb_id);
      sprintf (msg_buf, "monitorOn %s", dd->attr);
      cdevSendCallback (dd->dev, msg_buf, 0, dd->cb_id);
      cdevFlush();
      cdevDataFree (temp_dat);
    }
}


/*
 * monitor_callback
 */
static void	monitor_callback	(int		status,
					 void 		*arg,
					 cdev_request_t	req_id,
					 cdev_data_t	data_id)
{
  StripCurve		curve = (StripCurve)arg;
  struct _DeviceData	*dd;

  if (status != CDEV_SUCCESS)
    {
      fprintf (stdout, "StripCDEV:monitor_callback() status = %d", status);
      fflush (stdout);
    }
  else
    {
      /* first, get the DeviceData structure ffrom the curve */
      dd = (struct _DeviceData *)StripCurve_getattr_val
	(curve, STRIPCURVE_SAMPLEDATA);

      /* get the value */
      cdevDataGet (data_id, dd->tag, CDEV_DBL, &dd->value);
      /*
      fprintf (stdout, "%s.%s = %.8lf\n", dd->dev, dd->attr, dd->value);
      */
      
      if (StripCurve_getstat (curve, STRIPCURVE_WAITING))
	{
	  StripCurve_setattr
	    (curve, STRIPCURVE_SAMPLEFUNC, value_callback, 0);
	  Strip_setconnected (dd->this->strip, curve);
	}
    }
}


/*
 * value_callback
 *
 *	Returns the current value specified by the CurveData passed in.
 */
static double	value_callback	(void *data)
{
  struct _DeviceData	*dd = (struct _DeviceData *)data;

  return dd->value;
}
