/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "Legend.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_REALNUM_LEN	12

/*
 * == Legend Status flags ==
 *
 * VISIBLE	: legend visibility (not related to window visibility)
 * REPACK	: legend items need to be re-fitted
 * RESIZE	: legend window must be resized
 * MOVE		: legend window must be re-positioned
 * UPDATE	: legend items must be re-drawn
 */
typedef enum
{
  LEGENDSTAT_VISIBLE 	= 1,	LEGENDSTAT_REPACK 	= 2,
  LEGENDSTAT_RESIZE	= 4,	LEGENDSTAT_MOVE		= 8,
  LEGENDSTAT_UPDATE	= 16,	LEGENDSTAT_DRAWSHAPES	= 32,
  LEGENDSTAT_MONOCHROME	= 64
} LegendStatFlags;

static char	*LegendStatStr[] =
{
  "Visible",
  "Repack",
  "Resize",
  "Move",
  "Update",
  "Drawshapes",
  "Monochrome"
};

typedef struct
{
  StripConfig	*config;
  int		width, height;
  int		xpos, ypos;
  Display	*display;
  int		screen;
  Window	window;
  Pixmap	pixmap;
  GC		gc;
  XFontStruct	*font_info;
  int		max_items;
  int		n_items;
  LegendItem	*items;
  int		item_width, item_height;
  int		status;
} LegendInfo;

/* prototypes */

static void	initialize	(Display *display, int screen);
static void	Legend_draw 	(LegendInfo *legend, Drawable canvas);

static int	initialized = 0;
static int	ItemMaxWidth, ItemMaxHeight;

static void	initialize	(Display *display, int screen)
{
  int pixels_per_mm;

  pixels_per_mm = (double)DisplayWidth(display, screen) /
    (double)DisplayWidthMM(display, screen);
  ItemMaxWidth = pixels_per_mm * LEGEND_ITEM_MAX_WIDTH;

  pixels_per_mm = (double)DisplayHeight(display, screen) /
    (double)DisplayHeightMM(display, screen);
  ItemMaxHeight = pixels_per_mm * LEGEND_ITEM_MAX_HEIGHT;

  initialized = 1;
}

/*
 * Legend_init
 */
Legend	Legend_init (Display 		*display,
		     int 		screen,
		     Window 		window,
		     GC 		gc,
		     StripConfig 	*cfg)
{
  int			mem_error;
  LegendInfo		*legend;

  if (!initialized) initialize (display, screen);

  legend = (LegendInfo *)malloc (sizeof (LegendInfo));
  if (!(mem_error = (legend == NULL)))
    {
      legend->max_items = STRIP_MAX_CURVES;
      legend->items = (LegendItem *)calloc
	(legend->max_items, sizeof (LegendItem));
      if (!(mem_error = (legend->items == NULL)))
	{
	  legend->config	= cfg;
	  legend->screen 	= screen;
	  legend->window 	= window;
	  legend->pixmap 	= 0;
	  legend->display 	= display;
	  legend->xpos 		= legend->ypos 		= 0;
	  legend->width 	= legend->height 	= 0;
	  legend->n_items 	= 0;
	  legend->status 	=
	    LEGENDSTAT_RESIZE | LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE;

	  legend->gc = XCreateGC
	    (display, DefaultRootWindow(display), 0, NULL);
	}
    }
  
  if (mem_error)
    {
      Legend_delete (legend);
      legend = NULL;
    }
  return legend;
}

/*
 * Legend_delete
 */
void	Legend_delete (Legend the_legend)
{
  LegendInfo 	*legend = (LegendInfo *)the_legend;
  
  if (legend->items)
    free (legend->items);
  if (legend->pixmap)
    XFreePixmap (legend->display, legend->pixmap);
  if (legend->gc)
    XFreeGC (legend->display, legend->gc);

  free (legend);
}

/*
 * Legend_setattr
 */
int	Legend_setattr (Legend the_legend, ...)
{
  va_list	ap;
  int		attrib;
  LegendInfo	*legend = (LegendInfo *)the_legend;
  int 		ret_val = 1;
  int		tmp;

  va_start (ap, the_legend);
  for (attrib = va_arg (ap, LegendAttribute);
       attrib != 0;
       attrib = va_arg (ap, LegendAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < LEGEND_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case LEGEND_WIDTH:
	    tmp =  va_arg (ap, int);
	    if (tmp != legend->width) legend->status |= LEGENDSTAT_RESIZE;
	    legend->width = tmp;
	    break;
	  case LEGEND_HEIGHT:
	    tmp =  va_arg (ap, int);
	    if (tmp != legend->height) legend->status |= LEGENDSTAT_RESIZE;
	    legend->height = tmp;
	    break;
	  case LEGEND_XPOS:
	    tmp =  va_arg (ap, int);
	    if (tmp != legend->xpos) legend->status |= LEGENDSTAT_MOVE;
	    legend->xpos = tmp;
	    break;
	  case LEGEND_YPOS:
	    tmp =  va_arg (ap, int);
	    if (tmp != legend->ypos) legend->status |= LEGENDSTAT_MOVE;
	    legend->ypos = tmp;
	    break;
	  case LEGEND_DRAWSHAPES:
	    if (va_arg (ap, int))
	      legend->status |= LEGENDSTAT_DRAWSHAPES;
	    else legend->status &= ~LEGENDSTAT_DRAWSHAPES;
	    legend->status |= LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE;
	    break;
	  case LEGEND_MONOCHROME:
	    if (va_arg (ap, int))
	      legend->status |= LEGENDSTAT_MONOCHROME;
	    else legend->status &= ~LEGENDSTAT_MONOCHROME;
	    legend->status |= LEGENDSTAT_UPDATE;
	    break;
	  default:
	    fprintf
	      (stdout, "Legend_setattr(): can't set read-only attribute\n");
	    ret_val = 0;
	  }
      else break;
    }
  return ret_val;
}

/*
 * Legend_getattr
 */
int	Legend_getattr (Legend the_legend, ...)
{
  va_list	ap;
  int		attrib;
  LegendInfo	*legend = (LegendInfo *)the_legend;
  int		ret_val = 0;

  va_start (ap, the_legend);
  for (attrib = va_arg (ap, LegendAttribute);
       attrib != 0;
       attrib = va_arg (ap, LegendAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < LEGEND_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case LEGEND_VISIBLE:
	    *(va_arg (ap, int *)) = legend->status & LEGENDSTAT_VISIBLE;
	    break;
	  case LEGEND_WIDTH:
	    *(va_arg (ap, int *)) = legend->width;
	    break;
	  case LEGEND_HEIGHT:
	    *(va_arg (ap, int *)) = legend->height;
	    break;
	  case LEGEND_WINDOW:
	    *(va_arg (ap, int *)) = legend->window;
	    break;
	  case LEGEND_XPOS:
	    *(va_arg (ap, int *)) = legend->xpos;
	    break;
	  case LEGEND_YPOS:
	    *(va_arg (ap, int *)) = legend->ypos;
	    break;
	  case LEGEND_NUM_ITEMS:
	    *(va_arg (ap, int *)) = legend->n_items;
	    break;
	  case LEGEND_ITEMS:
	    *(va_arg (ap, LegendItem **)) = legend->items;
	    break;
	  case LEGEND_DRAWSHAPES:
	    *(va_arg (ap, int *)) = legend->status & LEGENDSTAT_DRAWSHAPES;
	    break;
	  case LEGEND_MONOCHROME:
	    *(va_arg (ap, int *)) = legend->status & LEGENDSTAT_MONOCHROME;
	    break;
	  }
      else break;
    }
  return ret_val;
}

/*
 * Legend_additem
 */
int	Legend_additem (Legend the_legend, int item_id, StripCurveInfo *crv)
{
  int		ret_val = 1;
  LegendInfo	*legend = (LegendInfo *)the_legend;

  if (legend->n_items < legend->max_items)
    {
      legend->items[legend->n_items].id = item_id;
      legend->items[legend->n_items].crv = crv;
      legend->n_items++;
      legend->status |= LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE;
    }
  else ret_val = 0;

  return ret_val;
}

/*
 * Legend_removeitem
 */
int	Legend_removeitem (Legend the_legend, int item_id)
{
  int		i;
  size_t	n;
  int		ret_val = 0;
  LegendInfo	*legend = (LegendInfo *)the_legend;

  for (i = 0; i < legend->n_items; i++)
    if (legend->items[i].id == item_id)
      {
	/* move trailing elements of array to the left one slot */
	n = legend->n_items - i - 1;
	memmove (&legend->items[i], &legend->items[i+1],
		 n * sizeof (LegendItem));
	legend->n_items--;

	legend->status |= LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE;
	ret_val = 1;
	break;
      }
  
  return ret_val;
}

/*
 * Legend_getitem_xy: returns the item covering the specifed (x, y) coordinates
 */
LegendItem	*Legend_getitem_xy	(Legend the_legend, int x, int y)
{
  int		idx;
  LegendInfo	*legend = (LegendInfo *)the_legend;
  LegendItem	*item = NULL;

  /* translate to legend coordinates */
  y -= legend->ypos;
  x -= legend->xpos;

  idx = y / (legend->height / legend->max_items);
  if (idx < legend->n_items &&
      (y >= legend->items[idx].pos.y) &&
      (y < legend->items[idx].pos.y + legend->item_height) &&
      (x >= legend->items[idx].pos.x) &&
      (x < legend->items[idx].pos.x + legend->item_width))
    item = &legend->items[idx];
  return item;
}

/*
 * Legend_getitem_id: returns the item with the specifed id
 */
LegendItem	*Legend_getitem_id	(Legend the_legend, int id)
{
  int		idx;
  LegendInfo	*legend = (LegendInfo *)the_legend;
  LegendItem	*item = NULL;

  for (idx = 0; idx < legend->n_items; idx++)
    if (legend->items[idx].id == id)
      {
	item = &legend->items[idx];
	break;
      }
  return item;
}

/*
 * Legend_show
 */
void	Legend_show	(Legend the_legend, Drawable canvas)
{
  LegendInfo	*legend = (LegendInfo *)the_legend;

  Legend_draw (legend, canvas);
  legend->status |= LEGENDSTAT_VISIBLE;
}

/*
 * Legend_hide
 */
void	Legend_hide	(Legend the_legend)
{
  LegendInfo	*legend = (LegendInfo *)the_legend;

  legend->status &= ~LEGENDSTAT_VISIBLE;
}

/*
 * Legend_draw
 */
static void	Legend_draw (LegendInfo *legend, Drawable canvas)
{
  int			i;
  int			good_fit;
  int			bound_h, bound_w;
  int			dummy1, dummy2;
  int			xpos, ypos;
  char			minstr[MAX_REALNUM_LEN+1], maxstr[MAX_REALNUM_LEN+1];
  char			minmax[(STRIPMAX_CURVE_PRECISION*2)+5];
  XPoint		shape_pos;
  XFontStruct		*font;

/*
  fprintf (stdout, "\n++ Legend_draw(): status ++\n");
  for (i = 0; i < 7; i++)
    if (legend->status & (1 << i))
      fprintf (stdout, "  %s\n", LegendStatStr[i]);
  fprintf (stdout, "\n");
  fflush (stdout);
*/
  
  if (legend->status & LEGENDSTAT_RESIZE)
    Legend_resize (legend, legend->width, legend->height);

  if (legend->status & (LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE))
    {
      /* clear the pixmap */
      XSetForeground
	(legend->display, legend->gc, legend->config->Color.background);
      XFillRectangle
	(legend->display, legend->pixmap, legend->gc,
	 0, 0, legend->width, legend->height);
    }
	 
  if (legend->n_items > 0)
    {
      if (legend->status & LEGENDSTAT_REPACK)
	{
	  legend->item_height = 
	    (legend->height - (legend->max_items+1)*LEGEND_PADDING) / 
	      legend->max_items;
	  legend->item_width = legend->width - 2*LEGEND_PADDING;
	  
	  /* now need to get the best font for the legend items */
	  bound_h = legend->item_height / 3;
	  bound_w = legend->item_width - 2;

	  if (legend->status & LEGENDSTAT_DRAWSHAPES)
	    /* need room in which to draw the shape --decrease the font width
	     */
	    bound_w -= MAX_SHAPE_WIDTH + LEGEND_PADDING;
	  
	  legend->font_info = NULL;

	  /* now select on width */
	  for (i = 0; i < legend->n_items; i++)
	    {
	      font = get_font
		(legend->display, bound_h,
		 legend->items[i].crv->details->name, bound_w,
		 0, STRIPCHARSET_ALL);
	      if (legend->font_info == NULL)
		legend->font_info = font;
	      else if (font->max_bounds.width >
		       legend->font_info->max_bounds.width)
		legend->font_info = font;

	      font = get_font
		(legend->display, bound_h,
		 legend->items[i].crv->details->egu, bound_w,
		 0, STRIPCHARSET_ALL);
	      if (font->max_bounds.width > legend->font_info->max_bounds.width)
		legend->font_info = font;
	    }
	  
	  /* finally, recalculate the item positions */
	  for (i = 0, xpos = ypos = LEGEND_PADDING; i < legend->n_items; i++)
	    {
	      if (ypos + legend->item_height > legend->height)
		{
		  ypos = LEGEND_PADDING;
		  xpos += legend->item_width + LEGEND_PADDING;
		}
	      legend->items[i].pos.x = xpos;
	      legend->items[i].pos.y = ypos;
	      
	      ypos += legend->item_height + LEGEND_PADDING;
	    }
	}
      
      if (legend->status & (LEGENDSTAT_UPDATE | LEGENDSTAT_REPACK))
	{
	  /* draw the items into the legend */
	  XSetFont (legend->display, legend->gc, legend->font_info->fid);
	  for (i = 0; i < legend->n_items; i++)
	    {
	      xpos = legend->items[i].pos.x;
	      ypos = legend->items[i].pos.y;

	      XSetForeground
		(legend->display, legend->gc,
		 legend->config->Color.foreground);
	      XDrawRectangle
		(legend->display, legend->pixmap, legend->gc,
		 xpos, ypos, legend->item_width, legend->item_height);

	      if (!(legend->status & LEGENDSTAT_MONOCHROME))
		{
		  XSetForeground
		    (legend->display, legend->gc,
		     legend->items[i].crv->details->pixel);
		  XFillRectangle
		    (legend->display, legend->pixmap, legend->gc,
		     xpos+1, ypos+1,
		     legend->item_width-1, legend->item_height-1);
		}

	      if (legend->status & LEGENDSTAT_MONOCHROME)
		XSetForeground
		  (legend->display, legend->gc,
		   legend->config->Color.foreground);
	      else
		XSetForeground
		  (legend->display, legend->gc,
		   legend->config->Color.legendtext);
	      
	      if (legend->status & LEGENDSTAT_DRAWSHAPES)
		{
		  shape_pos.x = xpos + MAX_SHAPE_WIDTH / 2 + LEGEND_PADDING;
		  shape_pos.y = ypos + MAX_SHAPE_HEIGHT / 2 + LEGEND_PADDING;
		  drawShape
		    (legend->display, legend->pixmap, legend->gc,
		     legend->items[i].id, &shape_pos);
		  xpos += MAX_SHAPE_WIDTH + LEGEND_PADDING;
		}

	      XDrawString
		(legend->display, legend->pixmap, legend->gc,
		 xpos+1, ypos+legend->font_info->ascent,
		 legend->items[i].crv->details->name,
		 strlen (legend->items[i].crv->details->name));
	      
	      ypos += legend->font_info->ascent + legend->font_info->descent;
	      XDrawString
		(legend->display, legend->pixmap, legend->gc,
		 xpos+1, ypos+legend->font_info->ascent,
		 legend->items[i].crv->details->egu,
		 strlen (legend->items[i].crv->details->egu));

	      dbl2str
		(legend->items[i].crv->details->min,
		 legend->items[i].crv->details->precision,
		 minstr, MAX_REALNUM_LEN);
	      dbl2str
		(legend->items[i].crv->details->max,
		 legend->items[i].crv->details->precision,
		 maxstr, MAX_REALNUM_LEN);
	      bound_w = max (strlen (minstr), strlen (maxstr));
	      sprintf
		(minmax, "(%-*s, %*s)", bound_w, minstr, bound_w, maxstr);
	      
	      ypos += legend->font_info->ascent + legend->font_info->descent;
	      XDrawString
		(legend->display, legend->pixmap, legend->gc,
		 xpos+1, ypos+legend->font_info->ascent,
		 minmax,
		 strlen (minmax));
	    }
	}
    }

  /* copy the legend pixmap into the window */
  XCopyArea
    (legend->display, legend->pixmap, canvas, legend->gc,
     0, 0, legend->width, legend->height, legend->xpos, legend->ypos);

  legend->status &= ~(LEGENDSTAT_UPDATE | LEGENDSTAT_REPACK);
}

/*
 * Legend_resize: resizes the legend
 */
void	Legend_resize (Legend the_legend, int width, int height)
{
  LegendInfo		*legend = (LegendInfo *)the_legend;

  legend->width = width;
  legend->height = height;
      
  if (legend->pixmap != 0)
    XFreePixmap (legend->display, legend->pixmap);

  legend->pixmap = XCreatePixmap
    (legend->display, legend->window,
     width, height,
     DefaultDepth (legend->display, legend->screen));

  legend->status |= LEGENDSTAT_REPACK;

  legend->status &= ~LEGENDSTAT_RESIZE;
  legend->status |= LEGENDSTAT_UPDATE | LEGENDSTAT_REPACK;
}

/*
 * Legend_update: sets the update flag so that legend items will be
 *                re-drawn
 */
void	Legend_update	(Legend the_legend)
{
  LegendInfo	*legend = (LegendInfo *)the_legend;
  legend->status |= LEGENDSTAT_UPDATE;
}


/*********************/

/*
 * Shape stuff
 */
int     shapes[NUM_SHAPES][MAX_COORDS] =
{
  { 0,  0,  0, 10, 10, 10, 10,  0,  0,  0, -1, -1, -1, -1 },
  { 0,  0,  5, 10, 10,  0,  0,  0, -1, -1, -1, -1, -1, -1 },
  { 0,  0, 10, 10,  0, 10, 10,  0,  0,  0, -1, -1, -1, -1 },
  { 0,  0, 10, 10, 10,  0,  0, 10,  0,  0, -1, -1, -1, -1 },
  { 0,  5,  5, 10, 10,  5,  5,  0,  0,  5, -1, -1, -1, -1 },
  { 0,  0, 10, 10,  5,  5,  0, 10, 10,  0, -1, -1, -1, -1 },
  { 5, 10,  5,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
  { 0,  0, 10,  0,  5,  0,  5, 10,  0, 10, 10, 10, -1, -1 },
  { 0,  0,  5, 10, 10,  0,  5,  5,  0,  0, -1, -1, -1, -1 },
  { 0,  3,  0,  8,  5, 10, 10,  8, 10,  3,  5,  0,  0,  3 }
};

void    drawShape       (Display *display, Drawable canvas, GC gc,
                         int which_shape, XPoint *center)
{
  static XPoint points[MAX_COORDS];
  int i = 0;

  for (i = 0; i < MAX_COORDS; i += 2)
    {
      if (shapes[which_shape][i] < 0)
	break;
      points[i/2].x = center->x - MAX_SHAPE_WIDTH/2 +
        shapes[which_shape][i];
      points[i/2].y = center->y - MAX_SHAPE_HEIGHT/2 +
        shapes[which_shape][i+1];
    }

  XDrawLines
    (display, canvas, gc, points, i/2, CoordModeOrigin);
}
