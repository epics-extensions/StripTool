/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include <X11/Xlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>

#include "StripGraph.h"
#include "StripDefines.h"
#include "StripMisc.h"
#include "Axis.h"
#include "Legend.h"

#define MAX_AXIS_MARGIN		18	/* max margin threshold in mm */
#define MIN_AXIS_MARGIN		8	/* min margin threshold in mm */
#define MAX_TITLE_MARGIN	8	/* max title margin threshold in mm */
#define MIN_TITLE_MARGIN	4	/* min title margin threshold in mm */
#define MAX_LEGEND_MARGIN	35	/* max legend margin threshold in mm */
#define MIN_LEGEND_MARGIN	15	/* min legend margin threshold in mm */

#define STRIP_MARGIN            0.18    /* window fraction for text, &c */

#define BORDERLINEWIDTH 	1	/* line width (Pixels) */


#define SG_DUMP_MATRIX_FIELDWIDTH	30
#define SG_DUMP_MATRIX_NUMWIDTH		20
#define SG_DUMP_MATRIX_BADVALUESTR	"???"

/* must define the filename of the temporary xwd file for printing */
#ifndef XWD_TEMP_FILE_STRING
#  define XWD_TEMP_FILE_STRING    "/tmp/strip.xwd"
#endif

static char	*SGComponentStr[SGCOMP_COUNT] =
{
  "X-Axis",
  "Y-Axis",
  "Legend",
  "Data",
  "Title",
  "Grid"
};

typedef struct
{
  /* === X Stuff === */
  Display	   	*display;
  Window	    	window;
  GC		    	gc;
  Pixmap	    	pixmap;
  Pixmap		plotpix;
  int		    	screen;

  /* === time stuff === */
  struct timeval	t0, t1;
  struct timeval	plotted_t0, plotted_t1;
  struct timeval	marker_t;
  int			marker_x;
  struct timeval	latest_plotted_t;
  int			latest_plotted_x;

  /* === layout info === */
  XRectangle		window_rect;
  XRectangle		rect[SGCOMP_COUNT];
  
  /* === graph components === */
  StripConfig		*config;
  StripCurveInfo	*curves[STRIP_MAX_CURVES];
  StripCurveInfo	*selected_curve;
  StripDataBuffer	data;
  Axis			xAxis;
  Axis			yAxis;
  Legend		legend;
  LegendItem		*legend_items;

  struct _grid
  {
    XSegment	h_seg[MAX_TICS+1];
    XSegment	v_seg[MAX_TICS+1];
  } grid;
  
  struct _monochrome
  {
    XPoint	shape_pos[MAX_SHAPES_PER_LINE];
    int		shape_drawn[MAX_SHAPES_PER_LINE];
  } monochrome[STRIP_MAX_CURVES];
  
  char			*title;
  SGComponentMask	draw_mask;
  unsigned		status;

  void			*user_data;
}
StripGraphInfo;

/* prototypes for internal static functions */
static void 	StripGraph_manage_geometry 	(StripGraphInfo *sgi);
static void 	StripGraph_plotdata		(StripGraphInfo *strip);

/* static variables */
static struct timezone 	tz;
static struct timeval	tv;
static struct timeval	*ptv;


/*
 * StripGraph_init
 */
StripGraph StripGraph_init (Display *display, Window window, StripConfig *cfg)
{
  StripGraphInfo	*sgi;
  XWindowAttributes 	win_attrib;
  int			i;

  if ((sgi = (StripGraphInfo *)malloc (sizeof(StripGraphInfo))) != NULL)
    {
      /* initialize some useful fields */
      sgi->display 	= display;
      sgi->screen 	= DefaultScreen(display);
      sgi->window 	= window;
      sgi->gc 	= XCreateGC
	  (sgi->display, DefaultRootWindow(sgi->display), 0, NULL);

      /* zero out these fields which are determined by calling Strip_resize */
      sgi->pixmap	= 0;
      sgi->plotpix 	= 0;
  
      /* default values */
      sgi->latest_plotted_t.tv_sec	= 0;
      sgi->latest_plotted_t.tv_usec	= 0;
      sgi->marker_t.tv_sec		= 0;
      sgi->marker_t.tv_usec		= 0;
      sgi->legend_items 		= NULL;
      sgi->title 			= NULL;

      /* other initializations */
      sgi->data 		= 0;
      sgi->config		= cfg;

      for (i = 0; i < STRIP_MAX_CURVES; i++)
	sgi->curves[i] = NULL;
      sgi->selected_curve 	= NULL;

      gettimeofday (&sgi->t1, &tz);
      dbl2time (&tv, sgi->config->Time.timespan);
      subtract_times (&sgi->t0, &tv, &sgi->t1);
      sgi->plotted_t0.tv_sec 	= 0;
      sgi->plotted_t0.tv_usec 	= 0;
      sgi->plotted_t1.tv_sec 	= 0;
      sgi->plotted_t1.tv_usec 	= 0;
      
      sgi->yAxis= Axis_init
	(AxisVertical, AxisReal, sgi->config);
      sgi->xAxis = Axis_init
	(AxisHorizontal, AxisTime, sgi->config);
      sgi->legend = Legend_init
	(sgi->display, sgi->screen, sgi->window, sgi->gc, sgi->config);

      /* default area is entire window */
      XGetWindowAttributes (sgi->display, sgi->window, &win_attrib);
      XSetForeground(sgi->display, sgi->gc, sgi->config->Color.background);
      sgi->window_rect.x = 0;
      sgi->window_rect.y = 0;
      sgi->window_rect.width = win_attrib.width;
      sgi->window_rect.height = win_attrib.height;

      sgi->draw_mask = SGCOMPMASK_ALL;
      sgi->status =
	SGSTAT_GRIDX_RECALC |
	SGSTAT_GRIDY_RECALC |
	SGSTAT_GRAPH_REFRESH |
	SGSTAT_MANAGE_GEOMETRY;

      sgi->user_data = NULL;
    }
  
  return (StripGraph)sgi;
}


/*
 * StripGraph_delete
 */
void StripGraph_delete (StripGraph the_graph)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_graph;
  
  if (sgi->pixmap)
    XFreePixmap (sgi->display, sgi->pixmap);
  if (sgi->plotpix)
    XFreePixmap (sgi->display, sgi->plotpix);
  if (sgi->gc)
  XFreeGC (sgi->display, sgi->gc);
  
  Axis_delete (sgi->xAxis);
  Axis_delete (sgi->yAxis);
  
  Legend_delete (sgi->legend);

  free (sgi);
}


/*
 * StripGraph_setattr
 */
int	StripGraph_setattr	(StripGraph the_sgi, ...)
{
  va_list		ap;
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  int			attrib;
  int			ret_val = 1;
  int			tmp_int;

  
  va_start (ap, the_sgi);
  for (attrib = va_arg (ap, StripGraphAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripGraphAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPGRAPH_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPGRAPH_WINDOW:
	    sgi->window = va_arg (ap, Window);
	    StripGraph_setstat (sgi, SGSTAT_MANAGE_GEOMETRY);
	    break;

	  case STRIPGRAPH_XPOS:
	    sgi->window_rect.x = va_arg (ap, int);
	    break;

	  case STRIPGRAPH_YPOS:
	    sgi->window_rect.y = va_arg (ap, int);
	    break;

	  case STRIPGRAPH_WIDTH:
	    tmp_int = va_arg (ap, int);
	    if (tmp_int != sgi->window_rect.width)
	      {
		sgi->window_rect.width = tmp_int;
		StripGraph_setstat (sgi, SGSTAT_MANAGE_GEOMETRY);
	      }
	    break;

	  case STRIPGRAPH_HEIGHT:
	    tmp_int = va_arg (ap, int);
	    if (tmp_int != sgi->window_rect.height)
	      {
		sgi->window_rect.height = tmp_int;
		StripGraph_setstat (sgi, SGSTAT_MANAGE_GEOMETRY);
	      }
	    break;

	  case STRIPGRAPH_TITLE:
	    sgi->title = va_arg (ap, char *);
	    break;

	  case STRIPGRAPH_DATA_BUFFER:
	    sgi->data = va_arg (ap, StripDataBuffer);
	    break;

	  case STRIPGRAPH_BEGIN_TIME:
	    ptv = va_arg (ap, struct timeval *);
	    sgi->t0.tv_sec = ptv->tv_sec;
	    sgi->t0.tv_usec = ptv->tv_usec;
	    break;

	  case STRIPGRAPH_END_TIME:
	    ptv = va_arg (ap, struct timeval *);
	    sgi->t1.tv_sec = ptv->tv_sec;
	    sgi->t1.tv_usec = ptv->tv_usec;
	    break;

	  case STRIPGRAPH_USER_DATA:
	    sgi->user_data = va_arg (ap, char *);
	    break;
	  }
    }

  va_end (ap);
  return ret_val;
}


/*
 * StripGraph_getattr
 */
int	StripGraph_getattr	(StripGraph the_sgi, ...)
{
  va_list		ap;
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  int			attrib;
  int			ret_val = 1;

  
  va_start (ap, the_sgi);
  for (attrib = va_arg (ap, StripGraphAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripGraphAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPGRAPH_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPGRAPH_WINDOW:
	    *(va_arg (ap, Window *)) = sgi->window;
	    break;

	  case STRIPGRAPH_XPOS:
	    *(va_arg (ap, int *)) = sgi->window_rect.x;
	    break;

	  case STRIPGRAPH_YPOS:
	    *(va_arg (ap, int *)) = sgi->window_rect.y;
	    break;

	  case STRIPGRAPH_WIDTH:
	    *(va_arg (ap, int *)) = sgi->window_rect.width;
	    break;

	  case STRIPGRAPH_HEIGHT:
	    *(va_arg (ap, int *)) = sgi->window_rect.height;
	    break;

	  case STRIPGRAPH_TITLE:
	    *(va_arg (ap, char **)) = sgi->title;
	    break;

	  case STRIPGRAPH_DATA_BUFFER:
	    *(va_arg (ap, StripDataBuffer *)) = sgi->data;
	    break;

	  case STRIPGRAPH_BEGIN_TIME:
	    *(va_arg (ap, struct timeval **)) = &sgi->t0;
	    break;

	  case STRIPGRAPH_END_TIME:
	    *(va_arg (ap, struct timeval **)) = &sgi->t1;
	    break;

	  case STRIPGRAPH_USER_DATA:
	    *(va_arg (ap, char **)) = sgi->user_data;
	    break;
	  }
    }

  va_end (ap);
  return ret_val;
}


static void StripGraph_manage_geometry (StripGraphInfo *sgi)
{
  double		pixels_per_mm;
  double		num_pixels;

/*  
  int i;
  fprintf
    (stdout,
     ">>--------------------<<\n"
     "sgi                = %p\n"
     "window_rect.x      = %d\n"
     "window_rect.y      = %d\n"
     "window_rect.width  = %d\n"
     "window_rect.height = %d\n",
     sgi,
     sgi->window_rect.x, sgi->window_rect.y,
     sgi->window_rect.width, sgi->window_rect.height);
*/

  /* pixels per horizontal millimeter */
  pixels_per_mm = (double)DisplayWidth (sgi->display, sgi->screen) /
    (double)DisplayWidthMM (sgi->display, sgi->screen);
  
  /* legend dimensions */
  if (sgi->config->Option.legend_visible)
    {
      num_pixels = STRIP_MARGIN * sgi->window_rect.width;
      if ((num_pixels / pixels_per_mm) > MAX_LEGEND_MARGIN)
	num_pixels = (double)MAX_LEGEND_MARGIN * pixels_per_mm;
      else if ((num_pixels / pixels_per_mm) < MIN_LEGEND_MARGIN)
	num_pixels = (double)MIN_LEGEND_MARGIN * pixels_per_mm;
      
      sgi->rect[SGCOMP_LEGEND].width 	= num_pixels;
      sgi->rect[SGCOMP_LEGEND].height 	= sgi->window_rect.height;
      sgi->rect[SGCOMP_LEGEND].x 	= sgi->window_rect.width -
	sgi->rect[SGCOMP_LEGEND].width;
      sgi->rect[SGCOMP_LEGEND].y 	= 0;
      
      Legend_setattr
	(sgi->legend,
	 LEGEND_WIDTH,	sgi->rect[SGCOMP_LEGEND].width,
	 LEGEND_HEIGHT, sgi->rect[SGCOMP_LEGEND].height,
	 LEGEND_XPOS, 	sgi->rect[SGCOMP_LEGEND].x,
	 LEGEND_YPOS, 	sgi->rect[SGCOMP_LEGEND].y,
	 0);
    }
  else
    {
      sgi->rect[SGCOMP_LEGEND].width 	= 0;
      sgi->rect[SGCOMP_LEGEND].height 	= 0;
      sgi->rect[SGCOMP_LEGEND].x 	= 0;
      sgi->rect[SGCOMP_LEGEND].y 	= 0;
    }

  /* data area dimensions --horizontal */
  num_pixels = STRIP_MARGIN *
    (sgi->window_rect.width - sgi->rect[SGCOMP_LEGEND].width);
  if ((num_pixels / pixels_per_mm) > MAX_AXIS_MARGIN)
    num_pixels = (double)MAX_AXIS_MARGIN * pixels_per_mm;
  else if ((num_pixels / pixels_per_mm) < MIN_AXIS_MARGIN)
    num_pixels = (double)MIN_AXIS_MARGIN * pixels_per_mm;
    
  sgi->rect[SGCOMP_DATA].x 	= num_pixels;
  sgi->rect[SGCOMP_DATA].width 	=  
    (sgi->window_rect.width - num_pixels - sgi->rect[SGCOMP_LEGEND].width);

  /* data area dimensions --vertical */
  pixels_per_mm = (double)DisplayHeight (sgi->display, sgi->screen) /
    (double)DisplayHeightMM (sgi->display, sgi->screen);
  
  /* x-axis height */
  num_pixels = STRIP_MARGIN * sgi->window_rect.height;
  if ((num_pixels / pixels_per_mm) > MAX_AXIS_MARGIN)
    num_pixels = (double)MAX_AXIS_MARGIN * pixels_per_mm;
  else if ((num_pixels / pixels_per_mm) < MIN_AXIS_MARGIN)
    num_pixels = (double)MIN_AXIS_MARGIN * pixels_per_mm;
  sgi->rect[SGCOMP_XAXIS].height = num_pixels;

  /* title height */
  num_pixels = STRIP_MARGIN * sgi->window_rect.height;
  if ((num_pixels / pixels_per_mm) > MAX_TITLE_MARGIN)
    num_pixels = (double)MAX_TITLE_MARGIN * pixels_per_mm;
  else if ((num_pixels / pixels_per_mm) < MIN_TITLE_MARGIN)
    num_pixels = (double)MIN_TITLE_MARGIN * pixels_per_mm;
  sgi->rect[SGCOMP_TITLE].height = num_pixels;

  sgi->rect[SGCOMP_DATA].y = sgi->rect[SGCOMP_TITLE].height;
  sgi->rect[SGCOMP_DATA].height = sgi->window_rect.height -
    sgi->rect[SGCOMP_TITLE].height - sgi->rect[SGCOMP_XAXIS].height;

  /* x-axis dimensions */
  sgi->rect[SGCOMP_XAXIS].x = sgi->rect[SGCOMP_DATA].x;
  sgi->rect[SGCOMP_XAXIS].y = sgi->rect[SGCOMP_DATA].y +
    sgi->rect[SGCOMP_DATA].height;
  sgi->rect[SGCOMP_XAXIS].width = sgi->rect[SGCOMP_DATA].width;

  /* y-axis dimensions */
  sgi->rect[SGCOMP_YAXIS].x = 0;
  sgi->rect[SGCOMP_YAXIS].y = sgi->rect[SGCOMP_DATA].y;
  sgi->rect[SGCOMP_YAXIS].width = sgi->rect[SGCOMP_DATA].x -
    sgi->rect[SGCOMP_YAXIS].x;
  sgi->rect[SGCOMP_YAXIS].height = sgi->rect[SGCOMP_DATA].height;

  /* title area dimensions */
  sgi->rect[SGCOMP_TITLE].x = sgi->rect[SGCOMP_DATA].x;
  sgi->rect[SGCOMP_TITLE].y = 0;
  sgi->rect[SGCOMP_TITLE].width = sgi->rect[SGCOMP_DATA].width;

  /* grid rectangle is same as data rectangle */
  sgi->rect[SGCOMP_GRID].x = sgi->rect[SGCOMP_DATA].x;
  sgi->rect[SGCOMP_GRID].y = sgi->rect[SGCOMP_DATA].y;
  sgi->rect[SGCOMP_GRID].width = sgi->rect[SGCOMP_DATA].width;
  sgi->rect[SGCOMP_GRID].height = sgi->rect[SGCOMP_DATA].height;

/*
  for (i = SGCOMP_XAXIS; i < SGCOMP_COUNT; i++)
    fprintf
      (stdout,
       "-- %0.10s --\n"
       "  x      = %d\n"
       "  y      = %d\n"
       "  width  = %d\n"
       "  height = %d\n",
       SGComponentStr[i],
       sgi->rect[i].x,
       sgi->rect[i].y,
       sgi->rect[i].width,
       sgi->rect[i].height);
  fprintf (stdout, ">>----------------------<<\n");
  fflush (stdout);
*/
  
  /* graph window pixmap */
  if (sgi->pixmap) XFreePixmap (sgi->display, sgi->pixmap);
  sgi->pixmap = XCreatePixmap
    (sgi->display, sgi->window,
     sgi->window_rect.width, sgi->window_rect.height,
     DefaultDepth (sgi->display, sgi->screen));
  XSetForeground (sgi->display, sgi->gc, sgi->config->Color.background);
  XFillRectangle
    (sgi->display, sgi->pixmap, sgi->gc, 
     0, 0, sgi->window_rect.width, sgi->window_rect.height);

  /* plot area pixmap */
  if (sgi->plotpix)
    XFreePixmap (sgi->display, sgi->plotpix);
  sgi->plotpix = XCreatePixmap
    (sgi->display, sgi->window,
     sgi->rect[SGCOMP_DATA].width, sgi->rect[SGCOMP_DATA].height,
     DefaultDepth (sgi->display, sgi->screen));
  XSetForeground (sgi->display, sgi->gc, sgi->config->Color.background);
  XFillRectangle
    (sgi->display, sgi->plotpix, sgi->gc, 
     0, 0, sgi->rect[SGCOMP_DATA].width, sgi->rect[SGCOMP_DATA].height);

  sgi->draw_mask = SGCOMPMASK_ALL;
  StripGraph_setstat
    (sgi,
     SGSTAT_GRIDX_RECALC |
     SGSTAT_GRIDY_RECALC |
     SGSTAT_GRAPH_REFRESH |
     SGSTAT_MANAGE_GEOMETRY);
}


/*
 * StripGraph_draw
 */ 
void StripGraph_draw	(StripGraph 		the_graph,
			 SGComponentMask 	components,
			 Region			*area)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_graph;
  int 			i, w, dummy1, dummy2;
  int			tic_offset[MAX_TICS+1];
  Region		clip_region;
  XRectangle		rectangle;
  XFontStruct		*font;

  /* draw components specified as well as those which need to be drawn */
  sgi->draw_mask |= components;

  if (StripGraph_getstat (sgi, SGSTAT_MANAGE_GEOMETRY))
    {
      StripGraph_manage_geometry (sgi);
      StripGraph_clearstat (sgi, SGSTAT_MANAGE_GEOMETRY);
    }
  
  /* only draw if the window is viewable */
  if (!window_isviewable (sgi->display, sgi->window) ||
      !window_ismapped (sgi->display, sgi->window))
    return;

  /* set the clip mask according to which components are to be drawn */
  clip_region = XCreateRegion ();
  for (i = 0; i < SGCOMP_COUNT; i++)
    if (sgi->draw_mask & (1 << i))
      {
	if (i == SGCOMP_YAXIS)
	  {
	    rectangle.width = sgi->rect[SGCOMP_YAXIS].width;
	    rectangle.height = sgi->window_rect.height;
	    rectangle.x = sgi->rect[SGCOMP_YAXIS].x;
	    rectangle.y = 0;
	    XUnionRectWithRegion (&rectangle, clip_region, clip_region);
	  }
	else XUnionRectWithRegion (&sgi->rect[i], clip_region, clip_region);
      }

  /* if something is to be re-drawn, then erase it from the pixmap */
  if (!XEmptyRegion(clip_region))
    {
      XSetRegion (sgi->display, sgi->gc, clip_region);
      
      /* clear the pixmap */
      XSetForeground (sgi->display, sgi->gc, sgi->config->Color.background);
      XFillRectangle
	(sgi->display, sgi->pixmap, sgi->gc,
	 0, 0, sgi->window_rect.width, sgi->window_rect.height);

      XSetForeground (sgi->display, sgi->gc, sgi->config->Color.foreground);
      XSetLineAttributes
	(sgi->display, sgi->gc,
	 BORDERLINEWIDTH, LineSolid,
	 CapButt, JoinMiter);

      if (sgi->draw_mask & SGCOMPMASK_XAXIS)
	{
	  Axis_setattr (sgi->xAxis, AXIS_MIN, &sgi->t0, AXIS_MAX,& sgi->t1, 0);
	  Axis_draw
	    (sgi->xAxis,
	     sgi->display, sgi->pixmap, sgi->gc,
	     sgi->rect[SGCOMP_XAXIS].x, sgi->rect[SGCOMP_XAXIS].y,
	     sgi->rect[SGCOMP_XAXIS].x + sgi->rect[SGCOMP_XAXIS].width - 1,
	     sgi->rect[SGCOMP_XAXIS].y + sgi->rect[SGCOMP_XAXIS].height - 1);
	  sgi->draw_mask &= ~SGCOMPMASK_XAXIS;
	}
      
      /* draw y axis if not already drawn */
      if (sgi->draw_mask & SGCOMPMASK_YAXIS)
	{
	  if (sgi->selected_curve == NULL)
	    {
	      for (i = 0; i < STRIP_MAX_CURVES; i++)
		if (sgi->curves[i] != NULL) break;
	      if (i < STRIP_MAX_CURVES)
		sgi->selected_curve = sgi->curves[i];
	    }
	  if (sgi->selected_curve != NULL)
	    Axis_setattr
	      (sgi->yAxis,
	       AXIS_MIN, 	(double)sgi->selected_curve->details->min,
	       AXIS_MAX, 	(double)sgi->selected_curve->details->max,
	       AXIS_PRECISION,	sgi->selected_curve->details->precision,
	       AXIS_VALCOLOR, 	sgi->selected_curve->details->pixel,
	       0);
	  else
	    Axis_setattr
	      (sgi->yAxis,
	       AXIS_MIN, 		(double)0,
	       AXIS_MAX, 		(double)1,
	       AXIS_PRECISION,		(int)DEF_AXIS_PRECISION,
	       AXIS_VALCOLOR, 		sgi->config->Color.foreground,
	       0);
	  
	  Axis_draw
	    (sgi->yAxis,
	     sgi->display, sgi->pixmap, sgi->gc,
	     sgi->rect[SGCOMP_YAXIS].x, sgi->rect[SGCOMP_YAXIS].y,
	     sgi->rect[SGCOMP_YAXIS].x + sgi->rect[SGCOMP_YAXIS].width - 1,
	     sgi->rect[SGCOMP_YAXIS].y + sgi->rect[SGCOMP_YAXIS].height - 1);
	  
	  sgi->draw_mask &= ~SGCOMPMASK_YAXIS;
	}
      
      if ((sgi->draw_mask & SGCOMPMASK_LEGEND) &&
	  sgi->config->Option.legend_visible)
	{
	  if (StripGraph_getstat (sgi, SGSTAT_LEGEND_REFRESH))
	    {
	      Legend_update (sgi->legend);
	      StripGraph_clearstat (sgi, SGSTAT_LEGEND_REFRESH);
	    }
	  Legend_show (sgi->legend, sgi->pixmap);
	  sgi->draw_mask &= ~SGCOMPMASK_LEGEND;
	}
      
      /* update the plotpix pixmap to reflect current graph state */
      if (sgi->draw_mask & SGCOMPMASK_DATA)
	{
	  XSetClipMask (sgi->display, sgi->gc, None);
	  StripGraph_plotdata (sgi);
	  XSetRegion (sgi->display, sgi->gc, clip_region);
	}
      
      if (sgi->draw_mask & (SGCOMPMASK_DATA | SGCOMPMASK_GRID))
	{
	  XCopyArea
	    (sgi->display, sgi->plotpix,
	     sgi->pixmap, sgi->gc,
	     0, 0, sgi->rect[SGCOMP_DATA].width, sgi->rect[SGCOMP_DATA].height,
	     sgi->rect[SGCOMP_DATA].x, sgi->rect[SGCOMP_DATA].y);
	  sgi->draw_mask &= ~(SGCOMPMASK_DATA | SGCOMPMASK_GRID);
	}
      
      /* draw the grid lines, updating if necessary */
      XSetLineAttributes
	(sgi->display, sgi->gc,
	 BORDERLINEWIDTH,
	 LineOnOffDash, CapButt, JoinMiter);
      
      if (sgi->config->Option.grid_xon)
	{
	  if (StripGraph_getstat (sgi, SGSTAT_GRIDX_RECALC))
	    {
	      Axis_getattr
		(sgi->xAxis,
		 AXIS_TICOFFSETS, tic_offset,
		 NULL);
	      
	      for (i = 0; i < sgi->config->Option.axis_xnumtics; i++)
		{
		  sgi->grid.v_seg[i].x1 = sgi->rect[SGCOMP_GRID].x +
		    tic_offset[i];
		  sgi->grid.v_seg[i].x2 = sgi->grid.v_seg[i].x1;
		  sgi->grid.v_seg[i].y1 = sgi->rect[SGCOMP_GRID].y;
		  sgi->grid.v_seg[i].y2 = sgi->rect[SGCOMP_GRID].y +
		    sgi->rect[SGCOMP_GRID].height - 1;
		}
	      
	      StripGraph_clearstat (sgi, SGSTAT_GRIDX_RECALC);
	    }
	  
	  XSetForeground (sgi->display, sgi->gc, sgi->config->Color.grid);
	  XDrawSegments
	    (sgi->display, sgi->pixmap, sgi->gc,
	     sgi->grid.v_seg, sgi->config->Option.axis_xnumtics);
	} 
      
      if (sgi->config->Option.grid_yon)
	{
	  if (StripGraph_getstat (sgi, SGSTAT_GRIDY_RECALC))
	    {
	      Axis_getattr (sgi->yAxis, AXIS_TICOFFSETS, tic_offset, 0);
	      
	      for (i = 0; i < sgi->config->Option.axis_ynumtics; i++)
		{
		  sgi->grid.h_seg[i].x1 = sgi->rect[SGCOMP_GRID].x;
		  sgi->grid.h_seg[i].x2 = sgi->rect[SGCOMP_GRID].x +
		    sgi->rect[SGCOMP_GRID].width - 1;
		  sgi->grid.h_seg[i].y1 = sgi->rect[SGCOMP_GRID].y +
		    sgi->rect[SGCOMP_GRID].height - 1 - tic_offset[i];
		  sgi->grid.h_seg[i].y2 = sgi->grid.h_seg[i].y1;
		}
	      
	      StripGraph_clearstat (sgi, SGSTAT_GRIDY_RECALC);
	    }
	  
	  XSetForeground (sgi->display, sgi->gc, sgi->config->Color.grid);
	  XDrawSegments
	    (sgi->display, sgi->pixmap, sgi->gc,
	     sgi->grid.h_seg, sgi->config->Option.axis_ynumtics);
	}
      
      /* put up the title */
      if (sgi->draw_mask & SGCOMPMASK_TITLE)
	{
	  if (sgi->title != NULL)
	    {
	      XSetForeground
		(sgi->display, sgi->gc, sgi->config->Color.foreground);
	      font = get_font
		(sgi->display, sgi->rect[SGCOMP_DATA].y - 2,
		 NULL, 0, 0, STRIPCHARSET_ALL);
	      XSetFont (sgi->display, sgi->gc, font->fid);
	      w = XTextWidth (font, sgi->title, strlen (sgi->title));
	      XDrawString
		(sgi->display, sgi->pixmap, sgi->gc,
		 sgi->rect[SGCOMP_TITLE].x + sgi->rect[SGCOMP_TITLE].width/2 -
		 w/2,
		 sgi->rect[SGCOMP_TITLE].y +  sgi->rect[SGCOMP_TITLE].height -
		 font->descent - 3,
		 sgi->title, strlen (sgi->title));
	    }
	    sgi->draw_mask &= ~SGCOMPMASK_TITLE;
	}
    }

  else	/* copy the entire pixmap to the window */
    XUnionRectWithRegion (&sgi->window_rect, clip_region, clip_region);
      
  /* copy pixmap to window */
  StripGraph_refresh (sgi, area? *area : clip_region);
  XDestroyRegion (clip_region);
}


/*
 * StripGraph_plotdata
 *
 * This routine updates the graph pixmap, plotting all data on {t0, t1} to
 * the points along the width of the pixmap.
 */
static void StripGraph_plotdata	(StripGraphInfo *sgi)
{
  static struct	_data {
    XPoint	   	*points;
    XPoint		*p;
    size_t		size;
    size_t		s;
  } data = { NULL, NULL, 0, 0 };


  int			which_shape;
  int			need_xcoords = 1;
  int			get_point;
  int 			i, j, k, m, n;
  int			idx;
  struct timeval	t, delta_tv, *times;
  double		delta, s;
  DataPoint		*values;
  StripCurveInfo	*curve;


  
  
  delta = subtract_times (&delta_tv, &sgi->t0, &sgi->t1);
  subtract_times (&tv, &sgi->plotted_t0, &sgi->plotted_t1);

  if ((compare_times (&delta_tv, &tv) != 0) ||
      (compare_times (&sgi->t1, &sgi->plotted_t0) <= 0) ||
      (compare_times (&sgi->t0, &sgi->plotted_t1) >= 0) ||
      (compare_times (&sgi->t0, &sgi->plotted_t0) < 0))
    StripGraph_setstat (sgi, SGSTAT_GRAPH_REFRESH);

  /* if everything needs to be re-plotted, erase the whole pixmap */
  if (StripGraph_getstat (sgi, SGSTAT_GRAPH_REFRESH))
    {
      /* clear the pixmap */
      XSetForeground (sgi->display, sgi->gc, sgi->config->Color.background);
      XFillRectangle
	(sgi->display, sgi->plotpix, sgi->gc,
	 0, 0, sgi->rect[SGCOMP_DATA].width, sgi->rect[SGCOMP_DATA].height);

      sgi->marker_t = t = sgi->t0;
      sgi->marker_x = 0;
    }

  /* if only a portion needs to be plotted, re-arrange the displayed data
   * to reflect the new time range and clear the vacated portion of the
   * pixmap */
  else
    {
      /* the bin width (number of seconds for each vertical pixel line) */
      delta /= (double)sgi->rect[SGCOMP_DATA].width;
      
      /* the new x position of the marker point */
      /* note that |a-b| == |b-a| */
      i = (int) (subtract_times(&tv, &sgi->marker_t, &sgi->t0) / delta);

      /* let n be the number of pixels to shift the plot */
      if ((n = i - sgi->marker_x) != 0)
	{
	  /* copy plot area to pixmap, shifted left i pixels */
	  XCopyArea
	    (sgi->display,
	     sgi->plotpix, sgi->plotpix,
	     sgi->gc,
	     n, 0,
	     sgi->rect[SGCOMP_DATA].width - n, sgi->rect[SGCOMP_DATA].height,
	     0, 0);

	  /* clear the vacated area */
	  XSetForeground
	    (sgi->display, sgi->gc, sgi->config->Color.background);
	  XFillRectangle
	    (sgi->display, sgi->plotpix, sgi->gc,
	     sgi->rect[SGCOMP_DATA].width - n, 0,
	     n, sgi->rect[SGCOMP_DATA].height + 1);

	  sgi->latest_plotted_x -= n;
	  sgi->marker_x = i;
	}

      t = sgi->latest_plotted_t;
    }

  XSetLineAttributes
    (sgi->display, sgi->gc,
     sgi->config->Option.graph_linewidth,
     LineSolid, CapButt, JoinMiter);

  
  /* several notes:
   *
   * (1) data.points[] will store the (x, y) values for all points to
   * be plotted.  Note that data.points[0] will correspond to the most
   * recent sample, and each successive index will correspond to an
   * earlier sample.
   * (2) Since all points are sampled at the same discrete times, then
   * the x values need be calculated only once, while the y values will
   * vary from curve to curve.
   * (3) note that we want all points to the right of time t.  However,
   * if there is a valid point to the left of t, we should include it
   * so that there won't be a gap between the last point and the left
   * extremity --it won't be a problem to plot data off the pixmap
   * since clipping is turned on.
   */
  delta = subtract_times(&tv, &sgi->t0, &sgi->t1);

  /* how many points are there in the buffer for the given range? */
  if ((n = StripDataBuffer_init_range (sgi->data, &t, &sgi->t1)) > 0)
    {
      if (data.size < n)
	{
	  data.s = data.size;
	  data.p = data.points;
	  
	  StripDataBuffer_getattr (sgi->data, SDB_NUMSAMPLES, &data.size, 0);
	  if (data.points == NULL)
	    data.points = (XPoint *)malloc (data.size * sizeof (XPoint));
	  else data.points = (XPoint *)realloc
	    (data.points, data.size * sizeof (XPoint));
	}

      if (data.points != NULL)
	{
	  /* get the x-coordinates */
	  i = 0;
	  while ((j = StripDataBuffer_get_times (sgi->data, &times)) > 0)
	    for (k = 0; k < j; k++)
	      {
		/* if this is the first stamp, compare it to the stamp of
		 * the most recently plotted sample.  If it's the same, and
		 * the plot method is pixmap scrolling, then set its x-coord
		 * from the saved value from last plot */
		if (i == 0 &&
		    !StripGraph_getstat (sgi, SGSTAT_GRAPH_REFRESH) &&
		    (compare_times(&times[k], &sgi->latest_plotted_t) == 0))
		  data.points[i].x = sgi->latest_plotted_x;
		else
		  {
		    s = subtract_times (&tv, &sgi->t0, &times[k]);
		    data.points[i].x = (short)
		      ((s * sgi->rect[SGCOMP_DATA].width - 1) / delta);
		  }
		sgi->latest_plotted_t = times[k];
		sgi->latest_plotted_x = data.points[i].x;
		if (i == 0) StripGraph_clearstat (sgi, SGSTAT_GRAPH_REFRESH);
		i++;
	      }
	  
	  /* for each plotted curve ... */
	  for (m = 0; m < STRIP_MAX_CURVES; m++)
	    {
	      curve = sgi->curves[m];
	      if (curve == NULL)
		continue;
	      if (curve->details->plotstat != STRIPCURVE_PLOTTED)
		continue;

	      delta = curve->details->max - curve->details->min;
	      if (delta == 0) continue;
	      
	      XSetForeground (sgi->display, sgi->gc, curve->details->pixel);
	      
	      /* get the y-coordinates */
	      i = 0;	      
	      n = -1;	/* (n < 0) 	=> looking for first good point
			 * (n >= 0)	=> have good point (n is its index) */

	      while ((j = StripDataBuffer_get_data
		      (sgi->data, (StripCurve)curve, &values))
		     > 0)
		for (k = 0; k < j; k++)
		  {
		    if (values[k].status & DATASTAT_PLOTABLE)
		      {
			if (n < 0) n = i;
			data.points[i].y = sgi->rect[SGCOMP_DATA].height *
			  ((delta - values[k].value + curve->details->min) /
			   delta);
		      }
		    else if (n >= 0)	/* plot the points from n to i */
		      {
			XDrawLines
			  (sgi->display, sgi->plotpix, sgi->gc,
			   &data.points[n], i-n, CoordModeOrigin);
			n = -1;
		      }
		    i++;
		  }

	      /* plot any as yet un-plotted points */
	      if (n >= 0)
		XDrawLines
		  (sgi->display, sgi->plotpix, sgi->gc,
		   &data.points[n], i-n, CoordModeOrigin);
	    }
	}
	else
	  {
	    fprintf
	      (stderr,
	       "StripGraph_plotdata: out of memory --unable to plot\n");
	    data.size = data.s;
	    data.points = data.p;
	  }
    }

  sgi->plotted_t0 = sgi->t0;
  sgi->plotted_t1 = sgi->t1;
}


/*
 * StripGraph_refresh
 */
void StripGraph_refresh (StripGraph the_sgi, Region clip_region)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;

  XSetRegion (sgi->display, sgi->gc, clip_region);
  XCopyArea
    (sgi->display, sgi->pixmap, sgi->window, sgi->gc,
     0, 0,
     sgi->window_rect.width, sgi->window_rect.height,
     sgi->window_rect.x, sgi->window_rect.y);
  XFlush(sgi->display);
}


/*
 * StripGraph_addcurve
 */
int 	StripGraph_addcurve	(StripGraph the_sgi, StripCurve curve)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  int			i;
  int			ret_val;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (sgi->curves[i] == NULL)
      break;

  if (ret_val = (i < STRIP_MAX_CURVES))
    {
      sgi->curves[i] = (StripCurveInfo *)curve;
      Legend_additem (sgi->legend, i, sgi->curves[i]);
      StripGraph_setstat (sgi, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }

  return ret_val;
}


/*
 * StripGraph_removecurve
 */
int 	StripGraph_removecurve	(StripGraph the_sgi, StripCurve curve)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  int			i;
  int			ret_val;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (sgi->curves[i] == (StripCurveInfo *)curve)
      break;

  if (ret_val = (i < STRIP_MAX_CURVES))
    {
      Legend_removeitem (sgi->legend, i);
      if (sgi->selected_curve == (StripCurveInfo *)curve)
	sgi->selected_curve = NULL;
      sgi->curves[i] = NULL;
      StripGraph_setstat (sgi, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }

  return ret_val;
}


/*
 * StripGraph_dumpdata
 */
int	StripGraph_dumpdata	(StripGraph the_sgi, FILE *f)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  int			i, n;
  StripCurve		curves[STRIP_MAX_CURVES+1];

  for (i = 0, n = 0; i < STRIP_MAX_CURVES; i++)
    {
      if (sgi->curves[i] == NULL)
	continue;
      else if (sgi->curves[i]->details->plotstat != STRIPCURVE_PLOTTED)
	continue;
      else curves[n++] = (StripCurve)sgi->curves[i];
    }
  curves[n] = (StripCurve)0;

  return StripDataBuffer_dump (sgi->data, curves, &sgi->t0, &sgi->t1, f);
}


/*
 * StripGraph_print
 */
void StripGraph_print (StripGraph the_sgi)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;

  fprintf (stdout, "StripGraph_print() is a work in progress :)\n");
}


/*
 * StripGraph_inputevent
 */
void	StripGraph_inputevent	(StripGraph the_sgi, XEvent *event)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  LegendItem		*item;

  switch (event->xany.type)
    {
    case ButtonPress:
      if (event->xbutton.button == Button1)
	{
	  item = Legend_getitem_xy
	    (sgi->legend, event->xbutton.x, event->xbutton.y);
	  if (item != NULL)
	    {
	      sgi->selected_curve = item->crv;
	      StripGraph_draw (the_sgi, SGCOMPMASK_YAXIS, (Region *)0);
	    }
	}
      break;
    default:
      fprintf (stdout, "StripGraph_inputevent(): unknown event type\n");
    }
}



/*
 * StripGraph_setstat
 */
StripGraphStatus	StripGraph_setstat	(StripGraph       the_sgi,
						 StripGraphStatus stat)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  
  return (sgi->status |= stat);
}

/*
 * StripGraph_getstat
 */
StripGraphStatus	StripGraph_getstat	(StripGraph       the_sgi,
						 StripGraphStatus stat)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  
  return (sgi->status & stat);
}

/*
 * StripGraph_clearstat
 */
StripGraphStatus	StripGraph_clearstat	(StripGraph       the_sgi,
						 StripGraphStatus stat)
{
  StripGraphInfo	*sgi = (StripGraphInfo *)the_sgi;
  
  return (sgi->status &= ~stat);
}
