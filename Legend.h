/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _Legend
#define _Legend

#include "StripConfig.h"
#include "StripCurve.h"

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <stdarg.h>

#define LEGEND_PADDING		2	/* num pixels for Legend offsets */
#define LEGEND_ITEM_MAX_WIDTH	25
#define LEGEND_ITEM_MAX_HEIGHT	20
#define LEGEND_ITEM_WH_RATIO	0.67	/* desired width to height ratio */

/*
 * ========== LegendItem ==========
 */

typedef struct
{
  int			id;
  XRectangle		rect;
  StripCurveInfo 	*crv;
} LegendItem;

/*
 * ========== Legend ==========
 */

typedef enum
{
  LEGEND_VISIBLE = 1,	/* (int)					r  */
  LEGEND_WIDTH,		/* (int)					rw */
  LEGEND_HEIGHT,	/* (int)					rw */
  LEGEND_WINDOW,	/* (Window)					r  */
  LEGEND_XPOS,		/* (int)					rw */
  LEGEND_YPOS,		/* (int)					rw */
  LEGEND_NUM_ITEMS,	/* (int)					r  */
  LEGEND_ITEMS,		/* (LegendItem **)				r  */
  LEGEND_LAST_ATTRIBUTE
}
LegendAttribute;

typedef void *	Legend;

/*
 * Legend_init: creates a new Legend object
 */
Legend	Legend_init	(Display	*display,
			 int 		screen,
			 Window 	window,
			 GC 		gc,
			 StripConfig	*cfg);

/*
 * Legend_delete: destroys a Legend object
 */
void	Legend_delete	(Legend the_legend);

/*
 * Legend_set/getattr:	set or get the specified attribute.
 *                      Return true on success.
 */
int	Legend_setattr	(Legend the_legend, ...);
int	Legend_getattr	(Legend the_legend, ...);

/*
 * Legend_additem: adds an item to the legend
 */
int	Legend_additem	(Legend the_legend, int item_id, StripCurveInfo *crv);

/*
 * Legend_removeitem: deletes an item from the legend
 */
int	Legend_removeitem	(Legend the_legend, int item_id);

/*
 * Legend_getitem_xy: returns the item covering the specifed (x, y) coordinates
 */
LegendItem	*Legend_getitem_xy	(Legend the_legend, int x, int y);

/*
 * Legend_getitem_lbl: returns the item with the specifed id
 */
LegendItem	*Legend_getitem_id	(Legend the_legend, int id);

/*
 * Legend_show: shows the legend
 */
void	Legend_show	(Legend the_legend, Drawable canvas);

/*
 * Legend_resize: resizes the legend
 */
void	Legend_resize	(Legend the_legend, int width, int height);

/*
 * Legend_update: sets the update flag so that next show will cause the
 *                legend item fields to be re-drawn
 */
void	Legend_update	(Legend the_legend);
#endif
