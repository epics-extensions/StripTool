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
#include <string.h>

#define MAX_PRE         32
#define MAX_LEN         64
#define EXP_DIGITS      3       /* digits in exp numerical portion */
#define EXP_LEN         (2+EXP_DIGITS)  /* length of exponent string */

static char             *StripVersion   = "2.4b8";
unsigned char   Strip_x_error_code = Success;

float   vertical_pixels_per_mm;
float   horizontal_pixels_per_mm;

/*
 * StripMisc_init
 *
 *      Initializes global constants.
 */
void    StripMisc_init  (Display *display, int screen)
{
  horizontal_pixels_per_mm = 
    ((float)DisplayWidth(display, screen) /
     (float)DisplayWidthMM(display, screen));

  vertical_pixels_per_mm = 
    ((float)DisplayHeight(display, screen) /
     (float)DisplayHeightMM(display, screen));
}


/* strip_version
 */
char    *strip_version  (void)
{
  return StripVersion;
}


/*
 * load_fonts
 */
void    get_current_time        (struct timeval *tv)
{
  gettimeofday (tv, 0);
}


char    *time2str (struct timeval *t)
{
  static char buf[512];

  sprintf (buf, "sec:\t %lu\nusec:\t %ld", t->tv_sec, t->tv_usec);
  return buf;
}


/*
 * window_isviewable
 */
int     window_isviewable       (Display *d, Window w)
{
  XWindowAttributes     xwa;

  XGetWindowAttributes (d, w, &xwa);
  return (xwa.map_state == IsViewable);
}


/*
 * window_ismapped
 */
int     window_ismapped         (Display *d, Window w)
{
  XWindowAttributes     xwa;

  XGetWindowAttributes (d, w, &xwa);
  return (xwa.map_state != IsUnmapped);
}


/*
 * window_map
 */
void    window_map              (Display *d, Window w)
{
  XMapRaised (d, w);
}


/*
 * window_unmap
 */
void    window_unmap    (Display *d, Window w)
{
  XUnmapWindow (d, w);
}


void    sec2hms (unsigned sec, int *h, int *m, int *s)
{
  *s = sec;
  *h = *s / 3600;
  *s %= 3600;
  *m = *s / 60;
  *s %= 60;
}


char    *dbl2str        (double d, int p, char buf[], int n)
{
#if 0

  /* 
   * <sign>     1 char
   * <e>        1 char
   * <sign>     1 char
   * <exponent> 1-3 chars
   *
   * Also, if the field width is less than the precision
   * (significant digits), then the latter will have to
   * be decreased in order to avoid over-running the buffer.
   *
   * In order to determine the appropriate precision, if it must
   * be decreased, we need to know whether or not the converted
   * number will include an exponent, and if so, how many digits
   * the exponent will include.
   */
#define STR_SIZE        1023
  static char           str[STR_SIZE+1];
  
  sprintf (str, "% #*.*g", n, p);
  strncpy (buf, str, n);
  buf[n] = 0;
  
#else
  int   decpt, sign;
  char  tmp[MAX_LEN+1];
  char  e_str[EXP_LEN+1];
  char  e_cnt;
  int   i = 0, j = 0;

  memset (e_str, EXP_LEN+1, 1);
  e_str[0] = 'e';

  strcpy (tmp, ecvt (d, n, &decpt, &sign));
  buf[i++] = sign? '-' : ' ';
                   
  e_str[1] = (decpt > 0? '+' : '-');
  
  if (decpt > 0)                /* magnitude >= 1? */
  {
    if (p > 0)          /* print some digits after decimal point */
    {
      if (decpt+p > n-2)        /* need scientific notation */
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
      else                      /* print out d+p digits */
      {
        for (; decpt > 0; decpt--) buf[i++] = tmp[j++];
        buf[i++] = '.';
        for (; p > 0; p--) buf[i++] = tmp[j++];
        buf[i++] = '\0';
      }
    }
    else                        /* not interested in digits after decimal */
    {
      if (decpt > n-1)          /* need scientific notation */
      {
        
      }
      else while (i < decpt+1) buf[i++] = tmp[j++];
      buf[i++] = 0;
    }
  }
  else                          /* magnitude < 1*/
  {
    if (p > 0)          /* */
    {
      if (p+decpt > 0)          /* print some digits out */
      {
        if (p-decpt > n-3)      /* need scientific notation */
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
        else            /* print 0.(-decpt zeroes)(p+decpt digits) */
        {
          buf[i++] = '0';
          buf[i++] = '.';
          p += decpt;
          for (; decpt < 0; decpt++) buf[i++] = '0';
          for (; p > 0; p--) buf[i++] = tmp[j++];
          buf[i++] = '\0';
        }
      }
      else                      /* number too small --effectively zero */
      {
        buf[i++] = '0';
        buf[i++] = '.';
        for (; (i < n) && (p > 0); p--) buf[i++] = '0';
        buf[i++] = '\0';
      }
    }
    else                        /* effectively zero */
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
#endif
}


char    *int2str        (int x, char buf[], int n)
{
  static char   digits[10] = 
  {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

  char  tmp[MAX_LEN];
  int   i, j;

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
static void     MessageBox_cb           (Widget, XtPointer, XtPointer);

typedef enum
{
  MSGBOX_OK = 0,
  MSGBOX_MAP
}
MsgBoxEvent;

/*
 * MessageBox_popup
 */
void    MessageBox_popup        (Widget         parent,
                                 Widget         *message_box,
                                 int            type,
                                 char           *title,
                                 char           *btn_txt,
                                 char           *format,
                                 ...)
{
  va_list       args;
  XmString      xstr_title;
  XmString      xstr_msg;
  XmString      xstr_btn;
  char          buf[2048];

  /* make string */
  va_start (args, format);
  vsprintf (buf, format, args);
  va_end (args);

  /* initialize widget if necessary */
  if (*message_box != (Widget)0)
    if (parent != XtParent (*message_box))
    {
      XtDestroyWidget (*message_box);
      *message_box = (Widget)0;
    }
  if (*message_box == (Widget)0)
    *message_box = XmCreateMessageDialog (parent, "Oops", NULL, 0);
  
  xstr_msg = XmStringCreateLocalized (buf);
  xstr_btn = XmStringCreateLocalized (btn_txt);
  xstr_title = XmStringCreateLocalized (title);

  XtVaSetValues
    (*message_box,
     XmNdialogType,             type,
     XmNdefaultButtonType,      XmDIALOG_OK_BUTTON,
     XmNnoResize,               True,
     XmNdefaultPosition,        False,
     XmNdialogTitle,            xstr_title,
     XmNmessageString,          xstr_msg,
     XmNokLabelString,          xstr_btn,
     NULL);

  XtUnmanageChild
    (XmMessageBoxGetChild (*message_box, XmDIALOG_CANCEL_BUTTON));
  XtUnmanageChild
    (XmMessageBoxGetChild (*message_box, XmDIALOG_HELP_BUTTON));

  XtAddCallback
    (*message_box, XmNokCallback, MessageBox_cb, (XtPointer)MSGBOX_OK);
  XtAddCallback
    (*message_box, XmNmapCallback, MessageBox_cb, (XtPointer)MSGBOX_MAP);

  XmStringFree (xstr_msg);
  XmStringFree (xstr_btn);
  XmStringFree (xstr_title);

  XtManageChild (*message_box);
  XtPopup (XtParent (*message_box), XtGrabNone);
}

/*
 * MessageBox_cb
 */
static void     MessageBox_cb   (Widget w, XtPointer client, XtPointer BOGUS(1))
{
  MsgBoxEvent           event = (MsgBoxEvent)client;
  Window                root, child;
  int                   root_x, root_y;
  int                   win_x, win_y;
  unsigned              mask;
  int                   screen;
  Display               *display;
  XWindowAttributes     xwa;
  Dimension             width, height;

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
           XmNx,                        xwa.x + ((xwa.width - (int)width)/2),
           XmNy,                        xwa.y + ((xwa.height - (int)height)/2),
           NULL);
      
        break;


      case MSGBOX_OK:

        break;
  }
}


char    *basename       (char *path)
{
  char  *p = 0;

  if (path)
  {
    /* find end of string */
    for (p = path; *p; p++);

    /* move backwards, looking for '/' character, or
     * beginning of string --whichever comes first. */
    while ((p > path) && (*p != '/')) p--;
    if (*p == '/') p++;
  }

  return p;
}


