/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripMisc
#define _StripMisc

#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

/*
 * this is some serious brain-damage
 * by calling this macro in place of unused arguments in the function
 * headers, then the code will compile without warning or error in
 * either C++ or C.  (C++ complains about unused arguments, C complains
 * about missing arguments)
 */
#if defined(__cplusplus) || defined(C_plusplus)
#  define BOGUS(x)
#else
#  define BOGUS(x)      BOGUS_ARG_ ## x
#endif

#define STRIPCURVE_PENDOWN      1
#define STRIPCURVE_PLOTTED      1

#define ONE_MILLION             1000000.0
#define ONE_THOUSAND            1000.0

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#  define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef DBL_EPSILON
#  define DBL_EPSILON   2.2204460492503131E-16
#endif

#ifndef ABS
#  define ABS(x)        ((x) < 0? (-(x)) : (x))
#endif

/* ====== Various useful constants initialized by StripMisc_init() ====== */
extern float            vertical_pixels_per_mm;
extern float            horizontal_pixels_per_mm;

/* Strip_x_error_code
 *
 *      This value is set by the X error handler (in Strip.c) any time
 *      it is invoked.  In order to catch an error, Synchronize the X
 *      server, set this variable to Success, make an X protocol request,
 *      then check it again.  If changed, then the indicated error has
 *      occurred.  Unsynchronize afterwards!
 */
extern unsigned char    Strip_x_error_code;

/*
 * StripMisc_init
 *
 *      Initializes global constants.
 */
void    StripMisc_init  (Display *, int);       /* display, screen */


/* strip_version
 *
 *      Returns a string describing the current version.
 */
char    *strip_version  (void);


/* ====== Various Time Functions ====== */

/* get_current_time
 *
 *      Stores current time in the supplied buffer.
 */
void            get_current_time        (struct timeval *);

/* time2str
 *
 *      Returns pointer to converted time rep.
 */
char            *time2str       (struct timeval *);


/* dbl2time
 */
#define dbl2time(t,d) \
(void) \
((t)->tv_sec = (unsigned long)(d), \
 (t)->tv_usec = ((d) - (t)->tv_sec) * (long)ONE_MILLION)

/* time2dbl
 */
#define time2dbl(t) \
(double)((double)(t)->tv_sec + ((double)(t)->tv_usec / (double)ONE_MILLION))

/* add_times
 */
#define add_times(s,a,b) \
(void) \
((s)->tv_sec = (a)->tv_sec + (b)->tv_sec + \
 (((a)->tv_usec + (b)->tv_usec) / (unsigned long)ONE_MILLION), \
 (s)->tv_usec = ((a)->tv_usec + (b)->tv_usec) % (long)ONE_MILLION)

/* diff_times
 */
#define diff_times(s,b,a) \
(void) \
( (s)->tv_sec = (a)->tv_sec, (s)->tv_usec = (a)->tv_usec, \
  ( ((b)->tv_sec > (a)->tv_sec) \
    ? ((s)->tv_sec = 0, (s)->tv_usec = 0) \
    : ((((s)->tv_usec < (b)->tv_usec) \
        ? ((s)->tv_usec += ONE_MILLION, (s)->tv_sec--) \
        : 0), \
       ( ((s)->tv_sec < (b)->tv_sec) \
         ? ((s)->tv_sec = 0, (s)->tv_usec = 0) \
         : ((s)->tv_sec -= (b)->tv_sec, (s)->tv_usec -= (b)->tv_usec) ))))

/* compare_times
 */
#define compare_times(a,b) \
(int) \
(((a)->tv_sec > (b)->tv_sec)? 1 : \
 (((a)->tv_sec < (b)->tv_sec)? -1 : ((a)->tv_usec - (b)->tv_usec)))

/* subtract_times
 */
#define subtract_times(s,b,a) \
(double) \
((compare_times((a),(b)) >= 0) \
 ? (diff_times((s),(b),(a)), time2dbl((s))) \
 : (diff_times((s),(a),(b)), -time2dbl((s)))) 


/* ====== Various Window Functions ====== */

int     window_isviewable       (Display *, Window);
int     window_ismapped         (Display *, Window);
void    window_map              (Display *, Window);
void    window_unmap            (Display *, Window);

void    MessageBox_popup        (Widget,        /* parent */
                                 Widget *,      /* MessageBox */
                                 int,           /* type: XmDIALOG_XXX */
                                 char *,        /* title */
                                 char *,        /* button label */
                                 char *,        /* message format */
                                 ...);          /* message args */

/* ====== Miscellaneous ====== */

void    sec2hms (unsigned sec, int *h, int *m, int *s);

char    *dbl2str        (double,        /* value */
                         int,           /* precision: num digits after radix */
                         char[],        /* result buffer */
                         int);          /* max length of converted string */

char    *int2str        (int x, char buf[], int n);


/* basename
 *
 *      Returns the filename portion of a fully qualified path.
 */
char    *basename       (char *);

#endif
