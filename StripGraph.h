/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripGraph
#define _StripGraph

#include "StripConfig.h"
#include "StripDataBuffer.h"



/* ======= Data Types ======= */
typedef void *	StripGraph;


typedef enum
{
  SGCOMP_XAXIS = 0,
  SGCOMP_YAXIS,
  SGCOMP_LEGEND,
  SGCOMP_DATA,
  SGCOMP_TITLE,
  SGCOMP_GRID,
  SGCOMP_COUNT
}
SGComponent;

typedef enum
{
  SGCOMPMASK_XAXIS		= (1 << SGCOMP_XAXIS),
  SGCOMPMASK_YAXIS		= (1 << SGCOMP_YAXIS),
  SGCOMPMASK_LEGEND		= (1 << SGCOMP_LEGEND),
  SGCOMPMASK_DATA		= (1 << SGCOMP_DATA),
  SGCOMPMASK_TITLE		= (1 << SGCOMP_TITLE),
  SGCOMPMASK_GRID		= (1 << SGCOMP_GRID),
}
SGComponentMask;

#define SGCOMPMASK_ALL \
(SGCOMPMASK_XAXIS 	|\
 SGCOMPMASK_YAXIS 	|\
 SGCOMPMASK_LEGEND 	|\
 SGCOMPMASK_DATA 	|\
 SGCOMPMASK_TITLE 	|\
 SGCOMPMASK_GRID)


/* ======= Status bits ======= */
typedef enum
{
  SGSTAT_GRIDX_RECALC	= (1 << 0),	/* x-grid lines need recalculating */
  SGSTAT_GRIDY_RECALC	= (1 << 1),	/* y-grid lines need recalculating */
  SGSTAT_GRAPH_REFRESH	= (1 << 2),	/* do not scroll graph: repolot */
  SGSTAT_LEGEND_REFRESH	= (1 << 3),	/* recalculate the legend */
  SGSTAT_MANAGE_GEOMETRY= (1 << 4),	/* recalculate component lay-out */
} StripGraphStatus;

/* ======= Attributes ======= */
typedef enum
{
  STRIPGRAPH_WINDOW = 1,	/* (Window)  window in which to draw 	rw */
  STRIPGRAPH_XPOS,		/* (int)     x co-ord inside window 	rw */
  STRIPGRAPH_YPOS,		/* (int)     y co-ord inside window 	rw */
  STRIPGRAPH_WIDTH,		/* (int)     graph width in window	rw */
  STRIPGRAPH_HEIGHT,		/* (int)     graph height in window	rw */
  STRIPGRAPH_TITLE,		/* (char *)  graph title		rw */
  STRIPGRAPH_DATA_BUFFER,	/* (StripDataBuffer)			rw */
  STRIPGRAPH_BEGIN_TIME,	/* (struct timeval *)			rw */
  STRIPGRAPH_END_TIME,		/* (struct timeval *)			rw */
  STRIPGRAPH_USER_DATA,		/* (void *)  miscellaneous client data	rw */
  STRIPGRAPH_LAST_ATTRIBUTE
} StripGraphAttribute;




/* ======= Functions ======= */
/*
 * StripGraph_init
 *
 *	Creates a new StripGraph data structure, setting all
 *	values to defaults.
 */
StripGraph 	StripGraph_init		(Display *, Window, StripConfig *);


/*
 * StripGraph_delete
 *
 *	Destroys the specified StripGraph.
 */
void 	StripGraph_delete	(StripGraph);


/*
 * Strip_set/getattr
 *
 *	Sets or gets the specified attribute, returning true on success.
 */
int 	StripGraph_setattr	(StripGraph, ...);
int	StripGraph_getattr	(StripGraph, ...);


/*
 * StripGraph_addcurve
 *
 *	Tells the Graph to plot the given curve.
 */
int 	StripGraph_addcurve	(StripGraph, StripCurve);


/*
 * StripGraph_removecurve
 *
 *	Removes the given curve from those the Graph plots.
 */
int	StripGraph_removecurve	(StripGraph, StripCurve);


/*
 * StripGraph_draw
 *
 *	Draws the components of the strip chart specified by the mask.  If
 *	the region is non-null, then output is restricted to that region
 *	of the window.
 */
void 	StripGraph_draw		(StripGraph, SGComponentMask, Region *);


/*
 * StripGraph_refresh
 *
 *	Draws the StripGraph by copying the internal pixmap --not putting
 *	down any fresh data-- to the area of the window constrained by the
 *	specified region.  If the region is empty, then the entire pixmap
 *	is redrawn.
 */
void 	StripGraph_refresh	(StripGraph, Region);


/*
 * StripGraph_inputevent
 *
 *	Handles any input events.
 */
void	StripGraph_inputevent	(StripGraph, XEvent *);


/*
 * StripGraph_set/get/clearstat
 *
 *	Set:	sets the specified status bit high.
 *	Get: 	returns true iff the specified status bit is high.
 *	Clear: 	sets the specified status bit low.
 */
StripGraphStatus	StripGraph_setstat	(StripGraph, StripGraphStatus);
StripGraphStatus	StripGraph_getstat	(StripGraph, StripGraphStatus);
StripGraphStatus	StripGraph_clearstat	(StripGraph, StripGraphStatus);
 
#endif
