/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _Axis
#define _Axis

#include "StripConfig.h"

#include <X11/Xlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#define	MAX_TICS		32
#define DEF_AXIS_PRECISION	6

typedef enum
{
  AxisHorizontal, AxisVertical
}
AxisOrientation;

typedef enum
{
  AxisReal, AxisTime, NUM_AXIS_VALTYPES
}
AxisValueType;

typedef enum
{
  AXIS_LABEL = 1,	/* (char *)					rw */
  AXIS_ORIENTATION,	/* (AxisOrientation)				r  */
  AXIS_VALTYPE,		/* (AxisValueType)				r  */
  AXIS_TICOFFSETS,	/* (int *)					r  */
  AXIS_GRANULARITY,	/* (?)						   */
  AXIS_MIN,		/* (double/struct timeval *)			rw */
  AXIS_MAX,		/* (double/struct timeval *)			rw */
  AXIS_PRECISION,	/* (int)					rw */
  AXIS_VALCOLOR,	/* (Pixel)					rw */
  AXIS_NEWTICS,		/* (int)					r  */
  AXIS_NUM_ATTRIBUTES
}
AxisAttribute;

typedef void *	Axis;

/*
 * Axis_init: creates a new Axis object
 */
Axis	Axis_init
(
  AxisOrientation	orient,
  AxisValueType		type,
  StripConfig		*config);	/* axis label */

/*
 * Axis_delete: destroys the Axis object
 */
void	Axis_delete	(Axis the_axis);

/*
 * Axis_set/getattr:	set or get the specified attribute.
 *                      Return true on success.
 */
int	Axis_setattr	(Axis the_axis, ...);
int	Axis_getattr	(Axis the_axis, ...);

/*
 * Axis_draw: draws the axis
 */
void	Axis_draw	(Axis the_axis, Display *display,
			 Pixmap pixmap, GC gc,
			 int x0, int y0, 	/* top left point */
			 int x1, int y1); 	/* bottom right point */

#endif
