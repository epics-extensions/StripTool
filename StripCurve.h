/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripCurve
#define _StripCurve

#include "StripDefines.h" 
#include "StripConfig.h"

#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#define STRIPCURVE_PENDOWN	1
#define STRIPCURVE_PLOTTED	1

/* ======= Data Types ======= */
typedef void *		StripCurve;
typedef double 		(*StripCurveSampleFunc)	(void *);

/* ======= Attributes ======= */
typedef enum
{
  STRIPCURVE_NAME = 1,		/* (char *)				rw */
  STRIPCURVE_EGU,		/* (char *)				rw */
  STRIPCURVE_PRECISION,		/* (int)				rw */
  STRIPCURVE_MIN,		/* (double)				rw */
  STRIPCURVE_MAX,		/* (double)				rw */
  STRIPCURVE_PENSTAT,		/* (int)   plot new data?		rw */
  STRIPCURVE_PLOTSTAT,		/* (int)   curve data plotted?		rw */
  STRIPCURVE_WAITSTAT,		/* (int)   waiting for connection?	rw */
  STRIPCURVE_CONNECTSTAT,	/* (int)   curve connected?		rw */
  STRIPCURVE_COLOR,		/* (Pixel)				r  */
  STRIPCURVE_SAMPLEFUNC,	/* (StripCurveSampleFunc)		rw */
  STRIPCURVE_SAMPLEDATA,	/* (void *)				rw */
  STRIPCURVE_LAST_ATTRIBUTE,
}
StripCurveAttribute;


typedef enum
{
  STRIPCURVE_WAITING 		= (1 << 0),
  STRIPCURVE_CHECK_CONNECT	= (1 << 1),
  STRIPCURVE_CONNECTED		= (1 << 2),
  STRIPCURVE_EGU_SET		= STRIPCFGMASK_CURVE_EGU,
  STRIPCURVE_PRECISION_SET	= STRIPCFGMASK_CURVE_PRECISION,
  STRIPCURVE_MIN_SET		= STRIPCFGMASK_CURVE_MIN,
  STRIPCURVE_MAX_SET		= STRIPCFGMASK_CURVE_MAX
}
StripCurveStatus;



/* ======= Functions ======= */
/* StripCurve_set/getattr
 *
 *	Sets or gets the specified attributes, returning true on success,
 *	false otherwise.
 */
int	StripCurve_setattr	(StripCurve, ...);
int	StripCurve_getattr	(StripCurve, ...);


/*
 * StripCurve_set/getattr_val
 *
 *	Gets the specified attribute, returning a void pointer to it (must
 *	be casted before being accessed).
 */
void	*StripCurve_getattr_val	(StripCurve, StripCurveAttribute);


/*
 * StripCurve_set/get/clearstat
 *
 *	Set:	sets the specified status bit high.
 *	Get: 	returns true iff the specified status bit is high.
 *	Clear: 	sets the specified status bit low.
 */
StripCurveStatus	StripCurve_setstat	(StripCurve, StripCurveStatus);
StripCurveStatus	StripCurve_getstat	(StripCurve, StripCurveStatus);
StripCurveStatus	StripCurve_clearstat	(StripCurve, StripCurveStatus);



/* ======= Private Data (not for client program use) ======= */
typedef struct
{
  StripConfig		*scfg;
  StripCurveDetail	*details;
  void			*id;
  struct timeval	connect_request;
  void			*func_data;
  StripCurveSampleFunc	get_value;
  int			status;
}
StripCurveInfo;

#endif
