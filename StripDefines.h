/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripDefines
#define _StripDefines

/* the maximum number of curves
 *
 * Note: increasing this value will require that StripConfig be modified
 */
#define STRIP_MAX_CURVES	10


/* maximum number of bytes to use for caching sampled data */
#define STRIP_MAX_CACHE_BYTES		(8L*1024L*1024L)	/* 8 megs */

/* the maximum number of characters in a curve's name string */
#define STRIP_MAX_NAME_CHAR		64

/* the maximum number of characters in a curve's engineering units string */
#define STRIP_MAX_EGU_CHAR		32

/* the smallest fraction of a second which is still accurate */
#define STRIP_TIMER_ACCURACY		0.001

/* timeout period for handling Channel Access events */
#define STRIP_CA_PEND_TIMEOUT		0.001

/* timeout period for handling cdev events */
#define STRIP_CDEV_PEND_TIMEOUT		0.005

/* number of seconds to wait for a curve to connect to its data source
 * before taking some action */
#define STRIP_CONNECTION_TIMEOUT	5.0

/* the default fallback font name */
#define STRIP_FALLBACK_FONT_STR		"fixed"

/* the default dimensions (in millimeters) for the graph window */
#define STRIP_GRAPH_WIDTH_MM		200.0
#define STRIP_GRAPH_HEIGHT_MM		100.0

/* the default number of seconds of data displayed on plot */
#define STRIP_DEFAULT_TIMESPAN		300

/* the maximum font height (in millimeters) */
#define STRIP_FONT_MAXHEIGHT_MM		4.0

/* the default directory in which to find configuration files */
#ifndef STRIP_CONFIGFILE_DIR
#  define STRIP_CONFIGFILE_DIR		"."
#endif

/* the default configuration filename */
#define STRIP_DEFAULT_FILENAME		"StripTool.config"

/* the default wildcard for finding configuration files */
#ifndef STRIP_CONFIGFILE_PATTERN
#define STRIP_CONFIGFILE_PATTERN	"*"
#endif

/* the default printer stuff */
#ifndef STRIP_PRINTER_DEVNAME
#define STRIP_PRINTER_DEVNAME		"pjetxl"
#endif
#ifndef STRIP_PRINTER_NAME
#define STRIP_PRINTER_NAME		"mcc104c"
#endif
#ifndef STRIP_PRINTER_ISCOLOR
#define STRIP_PRINTER_ISCOLOR		1
#endif

#endif
