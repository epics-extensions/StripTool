/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripMisc.h"
#include "StripDefines.h"

#include <Xm/Xm.h>
#include <Xm/MessageB.h>

#include <stdio.h>
#include <stdlib.h>

#define MAX_PRE		32
#define MAX_LEN		64
#define EXP_DIGITS	3	/* digits in exp numerical portion */
#define EXP_LEN		(2+EXP_DIGITS)	/* length of exponent string */


#if 0
#define STRIP_MAX_FONTS 16
static char		*FontNameTable[STRIP_MAX_FONTS] = {
  "widgetDM_4",
  "widgetDM_6",
  "widgetDM_8",
  "widgetDM_10",
  "widgetDM_12",
  "widgetDM_14",
  "widgetDM_16",
  "widgetDM_18",
  "widgetDM_20",
  "widgetDM_22",
  "widgetDM_24",
  "widgetDM_30",
  "widgetDM_36",
  "widgetDM_40",
  "widgetDM_48",
  "widgetDM_60",
};
#else
#define FONT_FAMILY	"helvetica-bold"
#define STRIP_MAX_FONTS 11
static char		*FontNameTable[STRIP_MAX_FONTS] = {
  "*" FONT_FAMILY "-r-normal--8-*",
  "*" FONT_FAMILY "-r-normal--10-*",
  "*" FONT_FAMILY "-r-normal--11-*",
  "*" FONT_FAMILY "-r-normal--12-*",
  "*" FONT_FAMILY "-r-normal--14-*",
  "*" FONT_FAMILY "-r-normal--17-*",
  "*" FONT_FAMILY "-r-normal--18-*",
  "*" FONT_FAMILY "-r-normal--20-*",
  "*" FONT_FAMILY "-r-normal--24-*",
  "*" FONT_FAMILY "-r-normal--25-*",
  "*" FONT_FAMILY "-r-normal--34-*",
};
#endif

static XFontStruct 	*font_table[STRIP_MAX_FONTS];

static char	*StripCharSetChars[STRIPCHARSET_COUNT] = {
  "blah!!",
  TIME_CHARACTERS,
  REALNUM_CHARACTERS
};
static int	StripCharSetMaxCharlen[STRIPCHARSET_COUNT][STRIP_MAX_FONTS];
static int	fonts_loaded = 0;
static int	num_fonts_loaded = 0;


/*
 * load_fonts
 */
static int	load_fonts	(Display *d)
{
  int		i, j, k;
  char		c[2];
  double	pixels_per_mm;

  pixels_per_mm = (double)DisplayHeight (d, DefaultScreen (d)) /
    (double)DisplayHeightMM (d, DefaultScreen (d));

  for (i = num_fonts_loaded = 0; i < STRIP_MAX_FONTS; i++)
    {
      if ((font_table[i] = XLoadQueryFont (d, FontNameTable[i])) == NULL)
	{
	  fprintf
	    (stderr,
	     "StripMisc:\n"
	     "  load_fonts: unable to load font %s.\n"
	     "  ...using default font, %s, instead.\n",
	     FontNameTable[i],
	     STRIP_FALLBACK_FONT_STR);
	  font_table[i] = XLoadQueryFont (d, STRIP_FALLBACK_FONT_STR);
	  if (font_table[i] == NULL)
	    {
	      fprintf
		(stderr, "Oops, that didn't work!  Can't load any fonts\n");
	      exit (1);
	    }
	}

      j = font_table[i]->descent + font_table[i]->ascent;
      k = (int)(pixels_per_mm * STRIP_FONT_MAXHEIGHT_MM);
      if (j > k)
	{
	  XFreeFont (d, font_table[i]);
	  font_table[i] = NULL;
	  break;
	}

      num_fonts_loaded++;
    }

  
  c[1] = '\0';

  for (i = 0; i < STRIPCHARSET_COUNT; i++)
    for (j = 0; j < num_fonts_loaded; j++)
      {
	StripCharSetMaxCharlen[i][j] = 0;
	if (i == STRIPCHARSET_ALL)
	  StripCharSetMaxCharlen[i][j] = font_table[j]->max_bounds.width;
	else for (k = 0; StripCharSetChars[i][k]; k++)
	  {
	    c[0] = StripCharSetChars[i][k];
	    StripCharSetMaxCharlen[i][j] = max
	      (StripCharSetMaxCharlen[i][j], XTextWidth (font_table[j], c, 1));
	  }
      }
  

  fonts_loaded = 1;
  return 1;
}


/*
 * get_font
 */
XFontStruct	*get_font	(Display 	*display,
				 int 		h,
				 char 		*txt,
				 int 		w,
				 int		n,
				 StripCharSet	s)
{
  int a, b, i, d;

  if (!fonts_loaded) load_fonts (display);

  a = 0;
  b = num_fonts_loaded - 1;
  
  while (a < b)
    {
      i = (b-a)/2 + a;

      d = font_table[i]->ascent + font_table[i]->descent - h;

      if (d > 0)	/* font height is too big */
	b = i-1;
      else if (d < 0)	/* font height is too small */
	a = i+1;
      else break;
    }

  if (i <= 0)
    i = 0;
  else if (i >= num_fonts_loaded)
    i = num_fonts_loaded - 1;
  else if (d > 0)
    i--;
  else if (d < 0)
    i++;

  if (txt != NULL)
    while ((i > 0) && (XTextWidth (font_table[i], txt, strlen (txt)) > w))
      i--;
  else while ((i > 0) && ((StripCharSetMaxCharlen[s][i] * n) > w))
    i--;

  if ((i < 0) || (i >= num_fonts_loaded))
    {
      fprintf (stdout, "StripMisc:get_font(): Arggh! bad font index\n");
      fflush (stdout);
      exit (1);
    }
  return font_table[i];
}



struct timeval 	*add_times
(struct timeval *result, struct timeval *t1, struct timeval *t2)
{
  result->tv_sec = t1->tv_sec + t2->tv_sec +
    ((t1->tv_usec + t2->tv_usec) / (unsigned long)ONE_MILLION);
  result->tv_usec = (t1->tv_usec + t2->tv_usec) % (long)ONE_MILLION;
  
  return result;
}

struct timeval	*dbl2time	(struct timeval *result, double d)
{
  result->tv_sec = (unsigned long)d;
  result->tv_usec = (d - result->tv_sec) * (long)ONE_MILLION;
  return result;
}

/*
 * subtract_times:
 *
 * This function fills the *result timeval structure with the value (t1 - t2),
 * and returns the corresponding difference in seconds as a double.  If
 * t2 > t1, then a negative number is returned and the contents of result
 * are undefined.
 */
double	subtract_times
(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
  double	retval;

  result->tv_sec = t1->tv_sec;
  result->tv_usec = t1->tv_usec;

  if (t2->tv_sec > t1->tv_sec)
    retval = -1;
  else
    {
      if (result->tv_usec < t2->tv_usec)
	{
	  result->tv_usec += ONE_MILLION;
	  result->tv_sec--;
	}
      
      if (result->tv_sec < t2->tv_sec)
	retval = -1;
      else
	{
	  result->tv_sec -= t2->tv_sec;
	  result->tv_usec -= t2->tv_usec;
	  retval = (double)result->tv_sec +
	    (double)result->tv_usec / (double)ONE_MILLION;
	}
    }
  return retval;
}

int 	compare_times (struct timeval *t1, struct timeval *t2)
{
  int 	retval;

  retval = (int)((double)t1->tv_sec - (double)t2->tv_sec);
  return retval;
}

char	*time_str (struct timeval *t)
{
  static char buf[512];

  sprintf (buf, "sec:\t %lu\nusec:\t %ld", t->tv_sec, t->tv_usec);
  return buf;
}


/*
 * window_isviewable
 */
int	window_isviewable	(Display *d, Window w)
{
  XWindowAttributes	xwa;

  XGetWindowAttributes (d, w, &xwa);
  return (xwa.map_state == IsViewable);
}


/*
 * window_ismapped
 */
int	window_ismapped		(Display *d, Window w)
{
  XWindowAttributes	xwa;

  XGetWindowAttributes (d, w, &xwa);
  return (xwa.map_state != IsUnmapped);
}


/*
 * window_map
 */
void	window_map		(Display *d, Window w)
{
  XMapRaised (d, w);
}


/*
 * window_unmap
 */
void	window_unmap	(Display *d, Window w)
{
  XUnmapWindow (d, w);
}



void	sec2hms	(unsigned sec, int *h, int *m, int *s)
{
  *s = sec;
  *h = *s / 3600;
  *s %= 3600;
  *m = *s / 60;
  *s %= 60;
}


char	*dbl2str	(double d, int p, char buf[], int n)
{
  int	decpt, sign;
  char	tmp[MAX_LEN+1];
  char	e_str[EXP_LEN+1];
  char	e_cnt;
  int	i = 0, j = 0;

  memset (e_str, EXP_LEN+1, 1);
  e_str[0] = 'e';

  strcpy (tmp, ecvt (d, n, &decpt, &sign));
  buf[i++] = sign? '-' : ' ';
		   
  e_str[1] = (decpt > 0? '+' : '-');
  
  if (decpt > 0)		/* magnitude >= 1? */
    {
      if (p > 0)		/* print some digits after decimal point */
	{
	  if (decpt+p > n-2)	/* need scientific notation */
	    {
	      int2str (decpt-1, &e_str[2], 2);
	      for (e_cnt = 0; e_str[e_cnt]; e_cnt++);
	      if (e_cnt+2 > n) goto no_room;
	      buf[i++] = tmp[j++];
	      if (i < n-e_cnt-1)
		{
		  buf[i++] = '.';
		  while (i < n-EXP_LEN) buf[i++] = tmp[j++];
		}
	      strcpy (&buf[i], e_str);
	    }
	  else			/* print out d+p digits */
	    {
	      for (; decpt > 0; decpt--) buf[i++] = tmp[j++];
	      buf[i++] = '.';
	      for (; p > 0; p--) buf[i++] = tmp[j++];
	      buf[i++] = '\0';
	    }
	}
      else			/* not interested in digits after decimal */
	{
	  while (i < decpt+1) buf[i++] = tmp[j++];
	  buf[i++] = 0;
	}
    }
  else 				/* magnitude < 1*/
    {
      if (p > 0)		/* */
	{
	  if (p+decpt > 0)		/* print some digits out */
	    {
	      if (p-decpt > n-3)	/* need scientific notation */
		{
		  int2str (-(decpt-1), &e_str[2], 2);
		  for (e_cnt = 0; e_str[e_cnt]; e_cnt++);
		  if (e_cnt+2 > n) goto no_room;
		  buf[i++] = tmp[j++];
		  if (i < n-e_cnt-1)
		    {
		      buf[i++] = '.';
		      while (i < n-e_cnt) buf[i++] = tmp[j++];
		    }
		  strcpy (&buf[i], e_str);
		}
	      else		/* print 0.(-decpt zeroes)(p+decpt digits) */
		{
		  buf[i++] = '0';
		  buf[i++] = '.';
		  p += decpt;
		  for (; decpt < 0; decpt++) buf[i++] = '0';
		  for (; p > 0; p--) buf[i++] = tmp[j++];
		  buf[i++] = '\0';
		}
	    }
	  else			/* number too small --effectively zero */
	    {
	      buf[i++] = '0';
	      buf[i++] = '.';
	      for (; (i < n) && (p > 0); p--) buf[i++] = '0';
	      buf[i++] = '\0';
	    }
	}
      else			/* effectively zero */
	{
	  buf[i++] = '0';
	  buf[i++] = '\0';
	}
    }
  
  return buf;

 no_room:
  for (i = 0; i < n; i++) buf[i] = '#';
  buf[i++] = '\0';
  return buf;
}


char	*int2str	(int x, char buf[], int n)
{
  static char	digits[10] = 
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

  char	tmp[MAX_LEN];
  int	i, j;

  /* convert digits to characters (reverse order) */
  for (i = 0; x != 0; i++)
    {
      tmp[i] = digits[x%10];
      x /= 10;
    }

  /* pad front of buffer with zeros */
  for (j = 0; j < n-i; j++) buf[j] = digits[0];

  /* fill in rest of buffer with number string */
  for (i -= 1; i >= 0; i--) buf[j++] = tmp[i];
  buf[j++] = '\0';
  return buf;
}


/* ====== MessageBox stuff ====== */
static void	MessageBox_cb		(Widget, XtPointer, XtPointer);

typedef enum
{
  MSGBOX_OK = 0,
  MSGBOX_MAP
}
MsgBoxEvent;

/*
 * MessageBox_popup
 */
void	MessageBox_popup 	(Widget 	parent,
				 Widget		*message_box,
				 char 		*msg_txt,
				 char 		*ok_txt)
{
  XmString	msg;
  XmString	ok;

  if (*message_box != (Widget)0)
    if (parent != XtParent (*message_box))
      {
	XtDestroyWidget (*message_box);
	*message_box = (Widget)0;
      }
  if (*message_box == (Widget)0)
    *message_box = XmCreateMessageDialog (parent, "Oops", NULL, 0);
  
  msg = XmStringCreateLocalized (msg_txt);
  if (ok_txt == NULL) ok_txt = "Ok";
  ok = XmStringCreateLocalized (ok_txt);

  XtVaSetValues
    (*message_box,
     XmNdialogType,		XmDIALOG_MESSAGE,
     XmNdefaultButtonType,	XmDIALOG_OK_BUTTON,
     XmNnoResize,		True,
     XmNdefaultPosition,	False,
     XmNmessageString,		msg,
     XmNokLabelString,		ok,
     NULL);

  XtUnmanageChild
    (XmMessageBoxGetChild (*message_box, XmDIALOG_CANCEL_BUTTON));
  XtUnmanageChild
    (XmMessageBoxGetChild (*message_box, XmDIALOG_HELP_BUTTON));

  XtAddCallback
    (*message_box, XmNokCallback, MessageBox_cb, (XtPointer)MSGBOX_OK);
  XtAddCallback
    (*message_box, XmNmapCallback, MessageBox_cb, (XtPointer)MSGBOX_MAP);

  XmStringFree (msg);
  XmStringFree (ok);

  XtManageChild (*message_box);
  XtPopup (XtParent (*message_box), XtGrabNone);
}

/*
 * MessageBox_cb
 */
static void	MessageBox_cb	(Widget w, XtPointer client, XtPointer BOGUS(1))
{
  MsgBoxEvent		event = (MsgBoxEvent)client;
  Window		root, child;
  int			root_x, root_y;
  int			win_x, win_y;
  unsigned		mask;
  int			screen;
  Display		*display;
  XWindowAttributes	xwa;
  Dimension		width, height;

  switch (event)
    {
    case MSGBOX_MAP:

      /* find out where the pointer is */
      display = XtDisplay (w);
      screen = DefaultScreen (display);
      XQueryPointer
	(XtDisplay (w), RootWindow (display, screen), 
	 &root, &child,
	 &root_x, &root_y,
	 &win_x, &win_y,
	 &mask);

      if (child != (Window)0)
	XGetWindowAttributes (display, child, &xwa);
      else XGetWindowAttributes (display, root, &xwa);

      XtVaGetValues (w, XmNwidth, &width, XmNheight, &height, NULL);

      /* place the dialog box in the center of the window in which the
       * pointer currently resides */
      XtVaSetValues
	(w,
	 XmNx,			xwa.x + ((xwa.width - (int)width)/2),
	 XmNy,			xwa.y + ((xwa.height - (int)height)/2),
	 NULL);
      
      break;


    case MSGBOX_OK:

      break;
    }
}


char	*GetFileName 	(char *fullName)
{
  char *copyFullName;
  char *fileName = 0;
  
  if (fullName)
  {
    if (copyFullName = strdup(fullName)) 
    {
      if (strtok(copyFullName, "/"))
      {
        char * tmp;
        
        while (tmp = strtok(0, "/"))
          fileName = tmp;
        fileName = strdup(fileName?fileName:fullName);
      }
      free(copyFullName);
    }
  }

  return fileName;
}


