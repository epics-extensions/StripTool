/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#include "StripDefines.h"
#include "StripMisc.h"
#include "Axis.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_TIC_LABEL_LEN	16
#define TIC_LEN			5
#define TICLABEL_OFFSET		3
#define TICLABEL_PAD		2
#define ONEDAY_NSECS		86400
#define MAX_DATE_LEN		16
#define DEF_MIN			0
#define DEF_MAX			10000
#define MAX_YLABEL_LEN		12

/* private data */
typedef struct
{
  char			*label;
  AxisOrientation	orient;
  AxisValueType		type;
  StripConfig		*config;
  void			*minval, *maxval;

  void			*value_buf; 		/* memory for tic values */
  struct		_tics
  {
    char	label[MAX_TIC_LABEL_LEN+1];	/* label string */
    int		width;				/* of string, in pixels */
    int		offset;				/* from origin */
    void	*value;				/* label value */
  }			tics[MAX_TICS+1];

  int			ticlabel_max;
  int			x0, y0, x1, y1;
  int			recalc;		/* true if need tic info, etc. */
  int			resized; 	/* true if axis must be resized */
  int			new_range;	/* true if new min or max */
  int			recent_ntics;	/* most recent number of tics */
  XFontStruct		*font_info;
  Pixel			val_pixel;
  int			precision;
  double		granularity;	/* number of axis units which comprise
					 * a noticable delta in the displayed
					 * value */
} AxisInfo;

size_t	AxisValueType_size[NUM_AXIS_VALTYPES] =
{
  sizeof (double),
  sizeof (struct timeval)
};

static int		numtics;
static struct timeval	tv;
struct tm		*tm;
static time_t		tt;

/*
 * private function prototypes
 */
static void 	Axis_copyval		(AxisInfo *axis, void *to, void *from);
static void 	Axis_recalc		(AxisInfo *axis, Display *display);
static void 	Axis_config_callback	(StripConfigMask, void *);


/**********************/
Axis	Axis_init
(
  AxisOrientation 	orient,
  AxisValueType 	type,
  StripConfig		*config)
{
  AxisInfo	*axis;
  int		mem_error = 0;
  int		i;

  if ((axis = (AxisInfo *)malloc (sizeof(AxisInfo))) != NULL)
    {
      axis->orient 	= orient;
      axis->type 	= type;
      axis->config	= config;
      axis->label 	= NULL;
      
      if ((axis->minval = (void *)malloc (AxisValueType_size[axis->type]))
	  == NULL)
	mem_error = 1;
      else if ((axis->maxval = (void *)malloc (AxisValueType_size[axis->type]))
	       == NULL)
	mem_error = 1;
      else
	{
	  switch (axis->type)
	    {
	    case AxisReal:
	      *(double *)axis->minval = (double)DEF_MIN;
	      *(double *)axis->maxval = (double)DEF_MAX;
	      break;
	    case AxisTime:
	      ((struct timeval *)axis->minval)->tv_sec = DEF_MIN;
	      ((struct timeval *)axis->minval)->tv_usec = 0;
	      ((struct timeval *)axis->maxval)->tv_sec = DEF_MAX;
	      ((struct timeval *)axis->maxval)->tv_usec = 0;
	      break;
	    default:
	      printf ("\nAxis: bad axis type\n");
	      mem_error = 1;
	    }
	  axis->value_buf = (void *)malloc
	    ((MAX_TICS+1) * AxisValueType_size[axis->type]);
	  if (axis->value_buf == NULL)
	    mem_error = 1;
	  else
	    {
	      for (i = 0; i < MAX_TICS + 1; i++)
		axis->tics[i].value = (void *)
		  ((char *)axis->value_buf +
		   i * AxisValueType_size[axis->type]);
	      
	      axis->x0 = axis->y0 = axis->x1 = axis->y1 = -1;
	      axis->recalc = axis->resized = 1;
	      axis->granularity = 0;
	      axis->precision = DEF_AXIS_PRECISION;
	      axis->val_pixel = 0;
	      axis->recent_ntics = 0;
	    }
	}
    }

  if (mem_error)
    {
      Axis_delete (axis);
      axis = NULL;
    }
  return axis;
}


/**********************/
void Axis_delete (Axis the_axis)
{
  AxisInfo	*axis = (AxisInfo *)the_axis;

  if (axis != NULL)
    {
      if (axis->label != NULL) free (axis->label);
      if (axis->minval != NULL) free (axis->minval);
      if (axis->maxval != NULL) free (axis->maxval);
      if (axis->value_buf != NULL) free (axis->value_buf);
      free (axis);
    }
}


/**********************/
int Axis_setattr (Axis the_axis, ...)
{
  va_list		ap;
  int			attrib;
  AxisInfo		*axis = (AxisInfo *)the_axis;
  int 			ret_val = 1;
  int			tmp;
  double		r;
  struct timeval	*t;
  

  va_start (ap, the_axis);
  for (attrib = va_arg (ap, AxisAttribute);
       (attrib != 0);
       attrib = va_arg (ap, AxisAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < AXIS_NUM_ATTRIBUTES))))
	switch (attrib)
	  {
	  case AXIS_LABEL:
	    free (axis->label);
	    axis->label = strdup (va_arg (ap, char *));
	    axis->recalc = 1;
	    break;
	    
	  case AXIS_MIN:
	    switch (axis->type)
	      {
	      case AxisReal:
		r = va_arg (ap, double);
		if (r != *(double *)axis->minval)
		  {
		    axis->recalc =  axis->new_range = 1;
		    *(double *)axis->minval = r;
		  }
		break;
	      case AxisTime:
		t = va_arg (ap, struct timeval *);
		if (compare_times (t, (struct timeval *)axis->minval) != 0)
		  {
		    axis->recalc = axis->new_range = 1;
		    Axis_copyval (axis, axis->minval, t);
		  }
		break;
	      default:
		printf ("Axis: bad axis type\n");
	      }
	    break;
	    
	  case AXIS_MAX:
	    switch (axis->type)
	      {
	      case AxisReal:
		r = va_arg (ap, double);
		if (r != *(double *)axis->maxval)
		  {
		    axis->recalc = axis->new_range = 1;
		    *(double *)axis->maxval = r;
		  }
		break;
	      case AxisTime:
		t = va_arg (ap, struct timeval *);
		if (compare_times (t, (struct timeval *)axis->maxval) != 0)
		  {
		    axis->recalc = axis->new_range = 1;
		    Axis_copyval (axis, axis->maxval, t);
		  }
		break;
	      default:
		printf ("Axis: bad axis type\n");
	      }
	    break;

	  case AXIS_PRECISION:
	    tmp = va_arg (ap, int);
	    if (tmp != axis->precision)
	      {
		axis->precision = tmp;
		axis->recalc = 1;
	      }
	    break;
	    
	  case AXIS_VALCOLOR:
	    axis->val_pixel = va_arg (ap, Pixel);
	    break;
	    
	  default:
	    fprintf (stderr, "Axis_setattr: cannot set read-only attribute\n");
	    ret_val = 0;
	  }
      else break;
    }
  va_end (ap);

  return ret_val;
}


/**********************/
int Axis_getattr (Axis the_axis, ...)
{
  va_list	ap;
  int		attrib;
  AxisInfo	*axis = (AxisInfo *)the_axis;
  int		ret_val = 1;
  int		*array;
  int		i;
  int		tmp;

  va_start (ap, the_axis);
  for (attrib = va_arg (ap, AxisAttribute);
       attrib != 0;
       attrib = va_arg (ap, AxisAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < AXIS_NUM_ATTRIBUTES))))
	switch (attrib)
	  {
	  case AXIS_LABEL:
	    *(va_arg (ap, char **)) = axis->label;
	    break;
	    
	  case AXIS_ORIENTATION:
	    *(va_arg (ap, AxisOrientation *)) = axis->orient;
	    break;
	    
	  case AXIS_VALTYPE:
	    *(va_arg (ap, AxisValueType *)) = axis->type;
	    break;
	    
	  case AXIS_TICOFFSETS:
	    array = (va_arg (ap, int *));
	    
	    if (axis->orient == AxisHorizontal)
	      tmp = axis->config->Option.axis_xnumtics;
	    else tmp = axis->config->Option.axis_ynumtics;
	    
	    for (i = 0; i < tmp; i++)
		array[i] = axis->tics[i+1].offset;
	    break;
	    
	  case AXIS_GRANULARITY:
	    *(va_arg (ap, double *)) = axis->granularity;
	    break;
	    
	  case AXIS_MIN:
	    switch (axis->type)
	      {
	      case AxisReal:
		Axis_copyval (axis, va_arg (ap, double *), axis->minval);
		break;
	      case AxisTime:
		Axis_copyval
		  (axis, va_arg (ap, struct timeval *), axis->minval);
		break;
	      default:
		printf ("Axis: bad axis type\n");
	      }
	    break;
	    
	  case AXIS_MAX:
	    switch (axis->type)
	      {
	      case AxisReal:
		Axis_copyval (axis, va_arg (ap, double *), axis->maxval);
		break;
	      case AxisTime:
		Axis_copyval
		  (axis, va_arg (ap, struct timeval *), axis->maxval);
		break;
	      default:
		printf ("Axis: bad axis type\n");
	      }
	    break;
	    
	  case AXIS_PRECISION:
	    *(va_arg (ap, int *)) = axis->precision;
	    break;
	    
	  case AXIS_VALCOLOR:
	    *(va_arg (ap, Pixel *)) = axis->val_pixel;
	    break;
	  }
      else break;
    }
  va_end (ap);
  return ret_val;
}


/**********************/
/*
 * Axis_draw
 *
 * This function is responsible for putting the graphical representation of
 * the specified axis onto the specified pixmap.  In order to achieve this
 * it must perform several tasks:
 *
 * 1. draw the axis lines
 * 2. draw the tic marks, and label them appropriately
 */
void Axis_draw (Axis the_axis, Display *display, Pixmap pixmap, GC gc,
		int x0, int y0, int x1, int y1)
{
  AxisInfo	*axis = (AxisInfo *)the_axis;
  XGCValues	gc_attr;
  Pixel		fg;
  int		i;
  int		write_date;
  int		prev_day, cur_day;
  char		date[MAX_DATE_LEN];

  prev_day = cur_day = 0;

  axis->resized = (((x1 - x0) != (axis->x1 - axis->x0))
		   || ((y1 - y0) != (axis->y1 - axis->y0)));

  axis->x0 = x0;
  axis->y0 = y0;
  axis->x1 = x1;
  axis->y1 = y1;

  axis->recalc = (axis->recent_ntics != axis->config->Option.axis_ynumtics);

  if (axis->recalc || axis->resized) Axis_recalc (axis, display);

  XSetFont (display, gc, axis->font_info->fid);

  if (axis->config->Option.axis_ycolorstat)
    {
      /* get the current foreground color */
      XGetGCValues (display, gc, GCForeground, &gc_attr);
      if (axis->val_pixel == 0) axis->val_pixel = gc_attr.foreground;
      fg = gc_attr.foreground;
    }
  
  /* first of all, draw the axis line on the innermost edge of the axis
   * real estate */
  if (axis->orient == AxisVertical)
    XDrawLine (display, pixmap, gc, x1, y0, x1, y1);
  else XDrawLine (display, pixmap, gc, x0, y0, x1, y0);

  /* now draw the tic marks and place the labels */
  numtics =
    (axis->orient == AxisVertical?
     axis->config->Option.axis_ynumtics : axis->config->Option.axis_xnumtics);

  for (i = 0; i < numtics + 1; i++)
    if (axis->orient == AxisVertical)
      {
	XDrawLine
	  (display, pixmap, gc,
	   x1, y1 - axis->tics[i].offset,
	   x1 - TIC_LEN, y1 - axis->tics[i].offset);

	if (axis->config->Option.axis_ycolorstat)
	  XSetForeground (display, gc, axis->val_pixel);
	XDrawString
	  (display, pixmap, gc,
	   x1 - TIC_LEN - TICLABEL_OFFSET - axis->ticlabel_max,
	   y1 - axis->tics[i].offset + axis->font_info->ascent / 3,
	   axis->tics[i].label,
	   strlen (axis->tics[i].label));
	if (axis->config->Option.axis_ycolorstat)
	  XSetForeground (display, gc, fg);
      }
    else	/* horizontal axis */
      {
	XDrawLine
	  (display, pixmap, gc,
	   x0 + axis->tics[i].offset, y0,
	   x0 + axis->tics[i].offset, y0 + TIC_LEN);

	/* don't draw a label for the origin tic */
	if ((i == 0) && (axis->type == AxisTime)) continue;
	
	if (axis->config->Option.axis_ycolorstat)
	  XSetForeground (display, gc, axis->val_pixel);
	XDrawString
	  (display, pixmap, gc,
	   x0 + axis->tics[i].offset - axis->tics[i].width + 1,
	   y0 + TIC_LEN + TICLABEL_OFFSET + axis->font_info->ascent,
	   axis->tics[i].label,
	   strlen (axis->tics[i].label));
	if (axis->config->Option.axis_ycolorstat)
	  XSetForeground (display, gc, fg);

	if (axis->type == AxisTime)
	  {
	    /* if this is the first tic label or if the date is different
	     * from that of the previous tic, then write the date under
	     * the time.
	     */
	    tt = (time_t)((struct timeval *)axis->tics[i].value)->tv_sec;
	    tm = localtime(&tt);
	    if ((write_date = (i == 0)))
	      cur_day = tm->tm_mday;
	    
	    if (!write_date)
	      {
		/* if here then we're not on the first tic mark, so compare
		 * this tic's date with the previous tic's. */
		 cur_day = tm->tm_mday;
		 write_date = (cur_day != prev_day);
	      }
	    
	    if (write_date)
	      {
		prev_day = cur_day;
		strftime (date, MAX_DATE_LEN, "%b %d, %y", tm);
		
		if (axis->config->Option.axis_ycolorstat)
		  XSetForeground (display, gc, axis->val_pixel);
		XDrawString
		  (display, pixmap, gc,
		   x0 + axis->tics[i].offset -
		   XTextWidth (axis->font_info, date, strlen (date)) + 1,
		   y0 + TIC_LEN + TICLABEL_OFFSET + TICLABEL_PAD +
		   (axis->font_info->ascent*2) + axis->font_info->descent,
		   date, strlen (date));
		if (axis->config->Option.axis_ycolorstat)
		  XSetForeground (display, gc, fg);
	      }
	  }
      }
}


/**********************/
/*
 * Axis_recalc
 *
 * This function updates the tics array, and selects an appropriate font for
 * the current axis real estate, 
 */
static void Axis_recalc (AxisInfo *axis, Display *display)
{
  int		i, p;
  int		max_strlen = 0;
  int		bound_w, bound_h;
  int		numtics;
  int		dummy1, dummy2;
  double	diff, delta;
  double	r;
  time_t	t;

  numtics =
    (axis->orient == AxisVertical?
     axis->config->Option.axis_ynumtics : axis->config->Option.axis_xnumtics);
  
  switch (axis->type)
    {
    case AxisReal:
      diff = *(double *)axis->maxval - *(double *)axis->minval;
      if ((delta = fabs (diff / (double)numtics)) != 0)
	{
	  for (p = 0; delta < 1; p++) delta *= 10;
	  p =  (p > axis->precision? p : axis->precision);
	}
      else p = axis->precision;
      break;
    case AxisTime:
      diff = subtract_times
	(&tv,
	 (struct timeval *)axis->minval,
	 (struct timeval *)axis->maxval);
      break;
    default:
      printf ("\nAxis: bad axis type\n");
      diff = 1.0;
    }
  delta = 1.0 / (double)numtics;

  /* calculate tic positioning and labeling -- treat the axis proper as
   * a tic just like the others (hence the "numtics + 1") */
  for (i = 0; i < numtics + 1; i++)
    {
      /* first determine the tic's pixel offset */
      if (axis->orient == AxisVertical)
	axis->tics[i].offset = (axis->y1 - axis->y0) * (delta * i);
      else axis->tics[i].offset = (axis->x1 - axis->x0) * (delta * i);

      /* now store the tic value and label */
      switch (axis->type)
	{
	case AxisReal:
	  r = *(double *)axis->minval + (delta * i) * diff;
	  Axis_copyval (axis, axis->tics[i].value, &r);
	  dbl2str (r, p, axis->tics[i].label, MAX_YLABEL_LEN);
	  break;
	case AxisTime:
	  dbl2time (&tv, (delta * i) * diff);
	  add_times
	    (axis->tics[i].value, (struct timeval *)axis->minval, &tv);
	  t = (time_t)(((struct timeval *)axis->tics[i].value)->tv_sec);
	  strftime (axis->tics[i].label, MAX_TIC_LABEL_LEN, "%H:%M:%S",
		    localtime (&t));
	  break;
	default:
	  printf ("\nAxis: bad axis type\n");
	}
      max_strlen = max (strlen (axis->tics[i].label), max_strlen);
    }

  /* Now find a good font size for the current real estate.
   * This is potentially time consuming, but will only occur on resizes or
   * when the range changes (possibly introducing longer or smaller tic
   * label strings) */
  if (axis->resized || ((axis->new_range && (axis->type == AxisReal))))
    {
      /* Calculate the size of the bounding box surrounding the tic label,
       * then determine the best-fitting font, given the length of the
       * longest label.
       *
       * Use get_font() from utils.c to get find the best font based on
       * height and width of max_strlen.
       */

      if (axis->orient == AxisVertical)
	{
	  bound_w = axis->x1 - axis->x0 -
	    TIC_LEN - TICLABEL_OFFSET - TICLABEL_PAD;
	  bound_h = (axis->y1 - axis->y0) * delta - TICLABEL_PAD;
	}
      else
	{
	  bound_w = (axis->x1 - axis->x0) * delta - TICLABEL_PAD;
	  bound_h = axis->y1 - axis->y0 -
	    TIC_LEN - TICLABEL_OFFSET - (TICLABEL_PAD * 2);
	  bound_h /= 2;		/* need space for date underneath time */
	}

      axis->font_info = get_font
	(display, bound_h, NULL, bound_w, max_strlen,
	 axis->type == AxisReal? STRIPCHARSET_REALNUM : STRIPCHARSET_TIME);
    }
  
  /* finally, calculate the tic label widths, and accumulate the max */
  axis->ticlabel_max = 0;
  for (i = 0; i < numtics + 1; i++)
    {
      axis->tics[i].width = XTextWidth
	(axis->font_info, axis->tics[i].label, strlen (axis->tics[i].label));
      axis->ticlabel_max = max (axis->tics[i].width, axis->ticlabel_max);
    }
}


/**********************/
static void Axis_copyval (AxisInfo *axis, void *to, void *from)
{
  switch (axis->type)
    {
    case AxisReal:
      *(double *)to = *(double *)from;
      break;
    case AxisTime:
      ((struct timeval *)to)->tv_sec = ((struct timeval *)from)->tv_sec;
      ((struct timeval *)to)->tv_usec = ((struct timeval *)from)->tv_usec;
      break;
    default:
      printf ("\nAxis: bad axis type\n");
    }
}
