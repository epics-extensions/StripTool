/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripConfig
#define _StripConfig

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <stdio.h>
#include <stdarg.h>
#include <float.h>
#include <limits.h>

#include "StripDefines.h"
#include "StripMisc.h"


/* ======= Default Values ======= */
#define STRIPDEF_TITLE			NULL
#define STRIPDEF_TIME_TIMESPAN		STRIP_DEFAULT_TIMESPAN
#define STRIPDEF_TIME_SAMPLE_INTERVAL	1
#define STRIPDEF_TIME_REFRESH_INTERVAL	1
#define STRIPDEF_COLOR_BACKGROUND_STR	"White"
#define STRIPDEF_COLOR_FOREGROUND_STR	"Black"
#define STRIPDEF_COLOR_GRID_STR		"Grey75"
#define STRIPDEF_COLOR_LEGENDTEXT_STR	"White"
#define STRIPDEF_COLOR_COLOR1_STR	"Blue"
#define STRIPDEF_COLOR_COLOR2_STR	"OliveDrab"
#define STRIPDEF_COLOR_COLOR3_STR	"Brown"
#define STRIPDEF_COLOR_COLOR4_STR	"CadetBlue"
#define STRIPDEF_COLOR_COLOR5_STR	"Orange"
#define STRIPDEF_COLOR_COLOR6_STR	"Purple"
#define STRIPDEF_COLOR_COLOR7_STR	"Red"
#define	STRIPDEF_COLOR_COLOR8_STR	"Gold"
#define STRIPDEF_COLOR_COLOR9_STR	"RosyBrown"
#define STRIPDEF_COLOR_COLOR10_STR	"YellowGreen"
#define STRIPDEF_OPTION_GRID_XON	1
#define STRIPDEF_OPTION_GRID_YON	1
#define STRIPDEF_OPTION_AXIS_XNUMTICS	5
#define STRIPDEF_OPTION_AXIS_YNUMTICS	10
#define STRIPDEF_OPTION_AXIS_YCOLORSTAT	1
#define STRIPDEF_OPTION_GRAPH_LINEWIDTH	2
#define STRIPDEF_OPTION_LEGEND_VISIBLE	1
#define STRIPDEF_CURVE_NAME		"Curve"
#define STRIPDEF_CURVE_EGU		"Undefined"
#define STRIPDEF_CURVE_PRECISION	4
#define STRIPDEF_CURVE_MIN		1e-7
#define STRIPDEF_CURVE_MAX		1e+7
#define STRIPDEF_CURVE_PENSTAT		STRIPCURVE_PENDOWN
#define STRIPDEF_CURVE_PLOTSTAT		STRIPCURVE_PLOTTED
#define STRIPDEF_CURVE_ID		NULL

/* ====== Min/Max values for all attributes requiring range checking ====== */
#define	STRIPMIN_TIME_TIMESPAN		1
#define STRIPMAX_TIME_TIMESPAN		UINT_MAX
#define STRIPMIN_TIME_SAMPLE_INTERVAL	0.01
#define STRIPMAX_TIME_SAMPLE_INTERVAL	DBL_MAX
#define STRIPMIN_TIME_REFRESH_INTERVAL	0.1
#define STRIPMAX_TIME_REFRESH_INTERVAL	DBL_MAX
#define STRIPMIN_OPTION_AXIS_XNUMTICS	1
#define STRIPMAX_OPTION_AXIS_XNUMTICS	32
#define STRIPMIN_OPTION_AXIS_YNUMTICS	1
#define STRIPMAX_OPTION_AXIS_YNUMTICS	32
#define STRIPMIN_OPTION_GRAPH_LINEWIDTH	0
#define STRIPMAX_OPTION_GRAPH_LINEWIDTH	10
#define STRIPMIN_CURVE_PRECISION	0
#define STRIPMAX_CURVE_PRECISION	8

#define STRIPCONFIG_NUMCOLORS		(STRIP_MAX_CURVES + 4)
#define STRIPCONFIG_MAX_CALLBACKS	10

typedef enum
{
  STRIPCONFIG_TITLE = 1,		/* (char *)			rw */
  STRIPCONFIG_TIME_TIMESPAN,		/* (unsigned)			rw */
  STRIPCONFIG_TIME_SAMPLE_INTERVAL,	/* (double)			rw */
  STRIPCONFIG_TIME_REFRESH_INTERVAL,	/* (double)			rw */
  STRIPCONFIG_COLOR_BACKGROUND,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_FOREGROUND,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_GRID,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_LEGENDTEXT,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR1,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR2,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR3,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR4,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR5,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR6,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR7,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR8,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR9,		/* (Pixel)			r  */
  STRIPCONFIG_COLOR_COLOR10,		/* (Pixel)			r  */
  STRIPCONFIG_OPTION_GRID_XON,		/* (int)			rw */
  STRIPCONFIG_OPTION_GRID_YON,		/* (int)			rw */
  STRIPCONFIG_OPTION_AXIS_XNUMTICS,	/* (int)			rw */
  STRIPCONFIG_OPTION_AXIS_YNUMTICS,	/* (int)			rw */
  STRIPCONFIG_OPTION_AXIS_YCOLORSTAT,	/* (int)			rw */
  STRIPCONFIG_OPTION_GRAPH_LINEWIDTH,	/* (int)			rw */
  STRIPCONFIG_OPTION_LEGEND_VISIBLE,	/* (int)			rw */
  STRIPCONFIG_LAST_ATTRIBUTE,
}
StripConfigAttribute;

typedef enum
{
  STRIPCFGMASK_TITLE			= (1 << 0),
  STRIPCFGMASK_TIME_TIMESPAN 		= (1 << 1),
  STRIPCFGMASK_TIME_SAMPLE_INTERVAL	= (1 << 2),
  STRIPCFGMASK_TIME_REFRESH_INTERVAL	= (1 << 3),
  STRIPCFGMASK_COLOR_BACKGROUND		= (1 << 4),
  STRIPCFGMASK_COLOR_FOREGROUND		= (1 << 5),
  STRIPCFGMASK_COLOR_GRID		= (1 << 6),
  STRIPCFGMASK_COLOR_LEGENDTEXT		= (1 << 7),
  STRIPCFGMASK_COLOR_COLOR1		= (1 << 8),
  STRIPCFGMASK_COLOR_COLOR2		= (1 << 9),
  STRIPCFGMASK_COLOR_COLOR3		= (1 << 10),
  STRIPCFGMASK_COLOR_COLOR4		= (1 << 11),
  STRIPCFGMASK_COLOR_COLOR5		= (1 << 12),
  STRIPCFGMASK_COLOR_COLOR6		= (1 << 13),
  STRIPCFGMASK_COLOR_COLOR7		= (1 << 14),
  STRIPCFGMASK_COLOR_COLOR8		= (1 << 15),
  STRIPCFGMASK_COLOR_COLOR9		= (1 << 16),
  STRIPCFGMASK_COLOR_COLOR10		= (1 << 17),
  STRIPCFGMASK_OPTION_GRID_XON		= (1 << 18),
  STRIPCFGMASK_OPTION_GRID_YON		= (1 << 19),
  STRIPCFGMASK_OPTION_AXIS_XNUMTICS	= (1 << 20),
  STRIPCFGMASK_OPTION_AXIS_YNUMTICS	= (1 << 21),
  STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT	= (1 << 22),
  STRIPCFGMASK_OPTION_GRAPH_LINEWIDTH	= (1 << 23),
  STRIPCFGMASK_OPTION_LEGEND_VISIBLE	= (1 << 24),
  STRIPCFGMASK_CURVE_NAME		= (1 << 25),
  STRIPCFGMASK_CURVE_EGU		= (1 << 26),
  STRIPCFGMASK_CURVE_PRECISION		= (1 << 27),
  STRIPCFGMASK_CURVE_MIN		= (1 << 28),
  STRIPCFGMASK_CURVE_MAX		= (1 << 29),
  STRIPCFGMASK_CURVE_PENSTAT		= (1 << 30),
  STRIPCFGMASK_CURVE_PLOTSTAT		= ((unsigned)1 << 31),
}
StripConfigMask;

#define	STRIPCFGMASK_TIME \
(STRIPCFGMASK_TIME_TIMESPAN 		| \
 STRIPCFGMASK_TIME_SAMPLE_INTERVAL 	| \
 STRIPCFGMASK_TIME_REFRESH_INTERVAL)

#define STRIPCFGMASK_COLOR \
(STRIPCFGMASK_COLOR_BACKGROUND 	| \
 STRIPCFGMASK_COLOR_FOREGROUND 	| \
 STRIPCFGMASK_COLOR_GRID 	| \
 STRIPCFGMASK_COLOR_LEGENDTEXT 	| \
 STRIPCFGMASK_COLOR_COLOR1 	| \
 STRIPCFGMASK_COLOR_COLOR2 	| \
 STRIPCFGMASK_COLOR_COLOR3 	| \
 STRIPCFGMASK_COLOR_COLOR4 	| \
 STRIPCFGMASK_COLOR_COLOR5 	| \
 STRIPCFGMASK_COLOR_COLOR6 	| \
 STRIPCFGMASK_COLOR_COLOR7 	| \
 STRIPCFGMASK_COLOR_COLOR8 	| \
 STRIPCFGMASK_COLOR_COLOR9 	| \
 STRIPCFGMASK_COLOR_COLOR10)
 
#define STRIPCFGMASK_OPTION \
(STRIPCFGMASK_OPTION_GRID_XON 		| \
 STRIPCFGMASK_OPTION_GRID_YON 		| \
 STRIPCFGMASK_OPTION_AXIS_XNUMTICS 	| \
 STRIPCFGMASK_OPTION_AXIS_YNUMTICS 	| \
 STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT 	| \
 STRIPCFGMASK_OPTION_GRAPH_LINEWIDTH	| \
 STRIPCFGMASK_OPTION_LEGEND_VISIBLE)

#define STRIPCFGMASK_CURVE \
(STRIPCFGMASK_CURVE_NAME 	|\
 STRIPCFGMASK_CURVE_EGU 	|\
 STRIPCFGMASK_CURVE_PRECISION 	|\
 STRIPCFGMASK_CURVE_MIN 	|\
 STRIPCFGMASK_CURVE_MAX	 	|\
 STRIPCFGMASK_CURVE_PENSTAT 	|\
 STRIPCFGMASK_CURVE_PLOTSTAT)

#define STRIPCFGMASK_ALL \
(STRIPCFGMASK_TITLE	| \
 STRIPCFGMASK_TIME 	| \
 STRIPCFGMASK_COLOR	| \
 STRIPCFGMASK_OPTION	| \
 STRIPCFGMASK_CURVE)

/* ======= Data Types ======= */
typedef void	(*StripConfigUpdateFunc) (StripConfigMask, void *);
  
typedef struct _StripCurveDetail
{
  char			name[STRIP_MAX_NAME_CHAR];
  char			egu[STRIP_MAX_EGU_CHAR];
  int			precision;
  double		min, max;
  int			penstat, plotstat;
  short			valid;
  Pixel			pixel;
  void			*id;
  StripConfigMask	update_mask;	/* fields updated since last update */
  StripConfigMask	set_mask;	/* fields without default values */
}
StripCurveDetail;

typedef struct _StripConfig
{
  char				*title;
  
  struct _Time {
    unsigned			timespan;
    double			sample_interval;
    double			refresh_interval;
  } Time;

  struct _Color {
    Pixel			background;
    Pixel			foreground;
    Pixel			grid;
    Pixel			legendtext;
    Pixel			color[STRIP_MAX_CURVES];
  } Color;

  struct _Option {
    int				grid_xon;
    int				grid_yon;
    int				axis_xnumtics;
    int				axis_ynumtics;
    int				axis_ycolorstat;
    int				graph_linewidth;
    int				legend_visible;
  } Option;

  struct _Curves {
    StripCurveDetail		*Detail[STRIP_MAX_CURVES];
  } Curves;

  struct _UpdateInfo {
    StripConfigMask		update_mask;
    struct _Callbacks {
      StripConfigMask		call_event;
      StripConfigUpdateFunc	call_func;
      void			*call_data;
    } Callbacks[STRIPCONFIG_MAX_CALLBACKS];
    int	callback_count;
  } UpdateInfo;
    
  void				*private_data;
}
StripConfig;



/*
 * StripConfig_init
 *
 *	Allocates storage for a StripConfig structure and initializes it
 *	with default values.  Then reads config info from the specified
 *	stdio stream, if it's not null.  See StripConfig_load() below for
 * 	specifics on how the file is read.
 */
StripConfig	*StripConfig_init	(Display *,
					 Colormap,
					 FILE *,
					 StripConfigMask);


/*
 * StripConfig_delete
 *
 *	Deallocates storage for the StripConfig structure.
 */
void	StripConfig_delete	(StripConfig *);


/*
 * StripConfig_set/getattr
 *
 *	Sets/gets the specified attributes, returning true on success.
 */
int	StripConfig_setattr	(StripConfig *, ...);
int	StripConfig_getattr	(StripConfig *, ...);


/*
 * StripConfig_write
 *
 *	Attempts to write configuration info to the specified stdio stream.
 *	Writing is begun at the current file pointer.  Output format is:
 *	<ascii-attribute-name><whitespace><ascii-attribute-value><end-of-line>
 *	for each selected attribute.  The mask specifies which attributes to
 *	write to the file.
 */
int	StripConfig_write	(StripConfig *, FILE *, StripConfigMask);


/*
 * StripConfig_load
 *
 *	Attempts to read configuration info from the specified stdio stream.
 *	Reading is begun at the current file pointer and continues until an
 *	unrecognized character is encoutered.  When this happens, the ending
 *	character is pushed back into the stream and the function terminates.
 *	The mask specifies which attributes to load from the file.
 */
int	StripConfig_load	(StripConfig *, FILE *, StripConfigMask);


/*
 * StripConfig_addcallback
 *
 *	Causes all update event callbacks which match the mask to be called.
 */
int	StripConfig_addcallback	(StripConfig *,
				 StripConfigMask,
				 StripConfigUpdateFunc,	/* callback function */
				 void *);		/* client data */


/*
 * StripConfig_update
 *
 *	Causes all update event callbacks which match the mask to be called.
 */
void	StripConfig_update	(StripConfig *, StripConfigMask);

/*
 * StripConfig_getcmap
 *
 *	Returns the colormap associated with the StripConfig.
 */
Colormap	StripConfig_getcmap	(StripConfig *);

#endif
