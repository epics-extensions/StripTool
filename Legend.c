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
#define LEGEND_BUFSIZE	255

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
  LEGENDSTAT_VISIBLE 	= (1 << 0),
  LEGENDSTAT_REPACK 	= (1 << 1),
  LEGENDSTAT_RESIZE	= (1 << 3),
  LEGENDSTAT_MOVE	= (1 << 4),
  LEGENDSTAT_UPDATE	= (1 << 5)
} LegendStatFlags;


typedef enum
{
  LEGENDSHOW_NAME,
  LEGENDSHOW_RANGE,
  LEGENDSHOW_EGU,
  LEGENDSHOW_COMMENT
} LegendShowComponents;

typedef enum
{
  LEGENDSHOWMASK_NAME		= (1 << LEGENDSHOW_NAME),
  LEGENDSHOWMASK_RANGE		= (1 << LEGENDSHOW_RANGE),
  LEGENDSHOWMASK_EGU		= (1 << LEGENDSHOW_EGU),
  LEGENDSHOWMASK_COMMENT	= (1 << LEGENDSHOW_COMMENT)
} LegendShowFlags;

static char	*LegendStatStr[] =
{
  "Visible",
  "Repack",
  "Resize",
  "Move",
  "Update",
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
  XFontStruct	*font;
  int		max_items;
  int		n_items;
  LegendItem	*items;
  int		status;
  int		components;
} LegendInfo;

/* prototypes */
static void		Legend_draw 	(LegendInfo *,
                                         Drawable);

static XFontStruct	*Legend_getfont	(LegendInfo *,
                                         int,		/* width */
                                         int);		/* component mask */

static int		Legend_recalc	(LegendInfo *,
                                         XFontStruct *,	/* font */
                                         int);		/* component mask */

static void		Legend_rangestr	(char *,	/* buffer */
                                         LegendItem *);


/*
 * Legend_init
 */
Legend	Legend_init (Display 		*display,
		     int 		screen,
		     Window 		window,
		     GC			BOGUS(1),
		     StripConfig 	*cfg)
{
  int			mem_error;
  LegendInfo		*legend;

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
      legend->xpos 	= legend->ypos 		= 0;
      legend->width 	= legend->height 	= 0;
      legend->n_items 	= 0;
      legend->status 	=
        LEGENDSTAT_RESIZE | LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE;
      legend->components 	= 0;

      legend->gc = XCreateGC (display, window, 0, NULL);
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
	    *(va_arg (ap, int *)) = (int)legend->window;
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

  for (idx = 0; idx < legend->n_items; idx++)
    if ((y >= legend->items[idx].rect.y) &&
        (y < legend->items[idx].rect.y + legend->items[idx].rect.height) &&
        (x >= legend->items[idx].rect.x) &&
        (x < legend->items[idx].rect.x + legend->items[idx].rect.width))
    {
      item = &legend->items[idx];
      break;
    }
  
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
  int			xpos, ypos;
  char			buf[LEGEND_BUFSIZE+1];
  char			*str;
  XFontStruct		*font, *font_tmp;
  int			width, height;
  int			total_height;
  int			components;
  XCharStruct		overall;
  int			direction_return, font_ascent_return, font_descent_return;
  Region		clip_region;
  XRectangle		rectangle;

  
  if (legend->status & LEGENDSTAT_RESIZE)
    Legend_resize (legend, legend->width, legend->height);

  if (legend->status & (LEGENDSTAT_REPACK | LEGENDSTAT_UPDATE))
  {
    /* clear the pixmap */
    XSetForeground
      (legend->display, legend->gc,
       legend->config->Color.background.xcolor.pixel);
    XFillRectangle
      (legend->display, legend->pixmap, legend->gc,
       0, 0, legend->width+1, legend->height+1);
  }

  /* This is where the new item dimensions are calculated
   */
  if (legend->n_items > 0)
  {
    components =
      LEGENDSHOWMASK_NAME |
      LEGENDSHOWMASK_RANGE |
      LEGENDSHOWMASK_EGU |
      LEGENDSHOWMASK_COMMENT;

    if (legend->status & LEGENDSTAT_REPACK)
    {
      do
      {
        font = Legend_getfont (legend, legend->width, components);
        
        while ((total_height = Legend_recalc (legend, font, components)) >
               legend->height)
        {
          if (font_tmp = shrink_font (font))
            font = font_tmp;
          else {
            components >>= 1;
            break;
          }
        }
      }
      while ((total_height > legend->height) && (components));

      if (!components) components = LEGENDSHOWMASK_NAME;
      legend->components = components;
      legend->font = font;
    }
      
    /* This is where the legend items actually get drawn to the screen
     */
    if (legend->status & (LEGENDSTAT_UPDATE | LEGENDSTAT_REPACK))
    {
      /* draw the items into the legend */
      XSetFont (legend->display, legend->gc, legend->font->fid);
      for (i = 0; i < legend->n_items; i++)
      {
        xpos = legend->items[i].rect.x;
        ypos = legend->items[i].rect.y;
        width = legend->items[i].rect.width;
        height = legend->items[i].rect.height;

        XSetForeground
          (legend->display, legend->gc,
           legend->config->Color.foreground.xcolor.pixel);
        XDrawRectangle
          (legend->display, legend->pixmap, legend->gc,
           xpos, ypos, width, height);

        XSetForeground
          (legend->display, legend->gc,
           legend->items[i].crv->details->color->xcolor.pixel);
        XFillRectangle
          (legend->display, legend->pixmap, legend->gc,
           xpos+1, ypos+1,
           width-1, height-1);

        XSetForeground
          (legend->display, legend->gc,
           legend->config->Color.legendtext.xcolor.pixel);

        if (legend->components & LEGENDSHOWMASK_NAME)
        {
          ypos += LEGEND_PADDING;
          XTextExtents
            (legend->font, legend->items[i].crv->details->name,
             strlen (legend->items[i].crv->details->name),
             &direction_return, &font_ascent_return, &font_descent_return,
             &overall);
          XDrawString
            (legend->display, legend->pixmap, legend->gc,
             xpos+1, ypos+overall.ascent,
             legend->items[i].crv->details->name,
             strlen (legend->items[i].crv->details->name));
          ypos += overall.ascent + overall.descent;
        }

        str = legend->items[i].crv->details->comment;
        if ((legend->components & LEGENDSHOWMASK_COMMENT) &&
            strcmp (str, STRIPDEF_CURVE_COMMENT))
        {
          ypos += LEGEND_PADDING;
          XTextExtents
            (legend->font, legend->items[i].crv->details->egu,
             strlen (legend->items[i].crv->details->egu),
             &direction_return, &font_ascent_return, &font_descent_return,
             &overall);
          XDrawString
            (legend->display, legend->pixmap, legend->gc,
             xpos+1, ypos+overall.ascent,
             legend->items[i].crv->details->comment,
             strlen (legend->items[i].crv->details->comment));
          ypos += overall.ascent + overall.descent;
        }


        str = legend->items[i].crv->details->egu;
        if ((legend->components & LEGENDSHOWMASK_EGU) &&
            strcmp (str, STRIPDEF_CURVE_EGU))
        {
          ypos += LEGEND_PADDING;
          XTextExtents
            (legend->font, legend->items[i].crv->details->egu,
             strlen (legend->items[i].crv->details->egu),
             &direction_return, &font_ascent_return, &font_descent_return,
             &overall);
          XDrawString
            (legend->display, legend->pixmap, legend->gc,
             xpos+1, ypos+overall.ascent,
             legend->items[i].crv->details->egu,
             strlen (legend->items[i].crv->details->egu));
          ypos += overall.ascent + overall.descent;
        }


        if (legend->components & LEGENDSHOWMASK_RANGE)
        {
          ypos += LEGEND_PADDING;
          Legend_rangestr (buf, &legend->items[i]);
          XTextExtents
            (legend->font, buf,
             strlen (buf),
             &direction_return, &font_ascent_return, &font_descent_return,
             &overall);
          XDrawString
            (legend->display, legend->pixmap, legend->gc,
             xpos+1, ypos+overall.ascent,
             buf,strlen (buf));
          ypos += overall.ascent + overall.descent;
        }
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
     legend->config->xvi.depth);

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


/*
 * Legend_getfont
 *
 *	Returns the most suitable font with which to draw the legend items,
 *	given the width constraint.
 */
static XFontStruct	*Legend_getfont	(LegendInfo 	*legend,
                                         int 		width,
                                         int		components)
{
  XFontStruct	*font_tmp = 0, *font;
  int		i;
  int		bound_h = legend->height;	/* just some "large" number */
  int		bound_w = legend->width - 2*(LEGEND_PADDING+1);
  char		buf[LEGEND_BUFSIZE+1];
  char		*str;

  font = get_font (legend->display, bound_h, 0, 0, 0, STRIPCHARSET_ALL);
  for (i = 0; i < legend->n_items; i++)
  {
    if (components & LEGENDSHOWMASK_NAME)
    {
      font_tmp = get_font
        (legend->display, bound_h,
         legend->items[i].crv->details->name, bound_w,
         0, STRIPCHARSET_ALL);
      if (font_tmp->max_bounds.width < font->max_bounds.width)
        font = font_tmp;
    }
    
    if (components & LEGENDSHOWMASK_RANGE)
    {
      Legend_rangestr	(buf, &legend->items[i]);
      font_tmp = get_font
        (legend->display, bound_h, buf, bound_w,0, STRIPCHARSET_ALL);
      if (font_tmp->max_bounds.width < font->max_bounds.width)
        font = font_tmp;
    }
    
    str = legend->items[i].crv->details->egu;
    if ((components & LEGENDSHOWMASK_EGU) &&
        strcmp (str, STRIPDEF_CURVE_EGU))
    {
      font_tmp = get_font
        (legend->display, bound_h,
         legend->items[i].crv->details->egu, bound_w,
         0, STRIPCHARSET_ALL);
      if (font_tmp->max_bounds.width < font->max_bounds.width)
        font = font_tmp;
    }
    
    str = legend->items[i].crv->details->comment;
    if ((components & LEGENDSHOWMASK_COMMENT) &&
        strcmp (str, STRIPDEF_CURVE_COMMENT))
    {
      font_tmp = get_font
        (legend->display, bound_h,
         legend->items[i].crv->details->comment, bound_w,
         0, STRIPCHARSET_ALL);
      if (font_tmp->max_bounds.width < font->max_bounds.width)
        font = font_tmp;
    }
  }

  return font;
}


/*
 * Legend_recalc
 *
 *	Calculates the item heights and positions for the legend
 *	items, given the font with which to render them and the
 *	mask specifying which components should be visible.
 *	Returns the height of the resulting legend.
 */
static int	Legend_recalc	(LegendInfo 	*legend,
                                 XFontStruct	*font,
                                 int		components)
{
  int		i;
  int		xpos, ypos;
  XCharStruct	overall;
  int		direction_return, font_ascent_return, font_descent_return;
  char		buf[LEGEND_BUFSIZE+1];

  for (i = 0, xpos = ypos = LEGEND_PADDING; i < legend->n_items; i++)
  {
    legend->items[i].rect.x = xpos;
    legend->items[i].rect.y = ypos;
    legend->items[i].rect.width = legend->width;

    /* determine the height */
    if (components & LEGENDSHOWMASK_NAME)
    {
      ypos += LEGEND_PADDING;
      XTextExtents
        (font, legend->items[i].crv->details->name,
         strlen (legend->items[i].crv->details->name),
         &direction_return, &font_ascent_return, &font_descent_return,
         &overall);
      ypos += overall.ascent + overall.descent;
    }

    if (components & LEGENDSHOWMASK_RANGE)
    {
      ypos += LEGEND_PADDING;
      Legend_rangestr (buf, &legend->items[i]);
      XTextExtents
        (font, buf,
         strlen (buf),
         &direction_return, &font_ascent_return, &font_descent_return,
         &overall);
      ypos += overall.ascent + overall.descent;
    }

    if ((components & LEGENDSHOWMASK_EGU) &&
        (legend->items[i].crv->details->egu[0]))
    {
      ypos += LEGEND_PADDING;
      XTextExtents
        (font, legend->items[i].crv->details->egu,
         strlen (legend->items[i].crv->details->egu),
         &direction_return, &font_ascent_return, &font_descent_return,
         &overall);
      ypos += overall.ascent + overall.descent;
    }

    if ((components & LEGENDSHOWMASK_COMMENT) &&
        (legend->items[i].crv->details->comment[0]))
    {
      ypos += LEGEND_PADDING;
      XTextExtents
        (font, legend->items[i].crv->details->comment,
         strlen (legend->items[i].crv->details->comment),
         &direction_return, &font_ascent_return, &font_descent_return,
         &overall);
      ypos += overall.ascent + overall.descent;
    }

    ypos += LEGEND_PADDING;
    legend->items[i].rect.height = ypos - legend->items[i].rect.y;
  }

  return ypos;
}


/*
 * Legend_rangestr
 *
 *	Creates a string representation for the (min, max) of the
 *	given item, placing the result in the supplied buffer.
 */
static void		Legend_rangestr	(char *buf, LegendItem *item)
{
  static char	minstr[MAX_REALNUM_LEN+1];
  static char	maxstr[MAX_REALNUM_LEN+1];

  dbl2str
    (item->crv->details->min, item->crv->details->precision,
     minstr, MAX_REALNUM_LEN);
  dbl2str
    (item->crv->details->max, item->crv->details->precision,
     maxstr, MAX_REALNUM_LEN);
  sprintf (buf, "(%s, %s)", minstr, maxstr);
}


