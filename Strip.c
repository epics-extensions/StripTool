/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "Strip.h"
#include "StripDialog.h"
#include "StripDataSource.h"
#include "StripHistory.h"
#include "StripGraph.h"
#include "StripMisc.h"
#include "StripFallback.h"

#include "pan_left.bm"
#include "pan_right.bm"
#include "zoom_in.bm"
#include "zoom_out.bm"
#include "auto_scroll.bm"

#ifdef USE_XPM
#  include <X11/xpm.h>		
#  include "StripIcon.xpm" 	
#else
#  include "StripIcon.bm"
#endif

#ifdef USE_CLUES
#include "LiteClue.h"
#endif

#if defined(HP_UX) || defined(SOLARIS) || defined(linux)
#  include <unistd.h>
#else
#  include <vfork.h>
#endif

#include <Xm/Xm.h>
#include <Xm/AtomMgr.h>
#include <Xm/DrawingA.h>
#include <Xm/DialogS.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/MessageB.h>
#include <Xm/List.h>
#include <Xm/FileSB.h>
#include <Xm/RowColumn.h>
#include <Xm/DragDrop.h>
#include <X11/Xlib.h>

#include <math.h>
#include <string.h>

#define DEF_WOFFSET	3

#define STRIP_MAX_FDS	64



/* ====== StripEvent stuff ====== */
typedef enum
{
  STRIPEVENT_SAMPLE = 0,
  STRIPEVENT_REFRESH,
  STRIPEVENT_CHECK_CONNECT,
  LAST_STRIPEVENT
} StripEventType;

typedef enum
{
  STRIPEVENTMASK_SAMPLE		= (1 << STRIPEVENT_SAMPLE),
  STRIPEVENTMASK_REFRESH	= (1 << STRIPEVENT_REFRESH),
  STRIPEVENTMASK_CHECK_CONNECT	= (1 << STRIPEVENT_CHECK_CONNECT)
} StripEventMask;

static char	*StripEventTypeStr[LAST_STRIPEVENT] =
{
  "Sample",
  "Refresh",
  "Check Connect"
};

/* ====== ListDialog stuff ====== */
typedef struct _ListDialog
{
  Strip		strip;
  Widget	msg_box, list;
  StripCurve	curves[STRIP_MAX_CURVES];
}
ListDialog;

/* ====== PrintInfo stuff ====== */
typedef struct _PrintInfo
{
  char		dev_name[32];
  char		pr_name[32];
  char		is_color;
}
PrintInfo;

/* ====== Strip stuff ====== */
typedef enum
{
  STRIPSTAT_OK			= (1 << 0),
  STRIPSTAT_PROCESSING		= (1 << 1),
  STRIPSTAT_TERMINATED		= (1 << 2),
  STRIPSTAT_BROWSE_MODE		= (1 << 3)
} StripStatus;

enum
{
  STRIPBTN_LEFT = 0,
  STRIPBTN_RIGHT,
  STRIPBTN_ZOOMIN,
  STRIPBTN_ZOOMOUT,
  STRIPBTN_AUTOSCROLL,
  STRIPBTN_COUNT
};
  
typedef struct _stripFdInfo
{
  XtInputId		id;
  int			fd;
}
stripFdInfo;
  
typedef struct _StripInfo
{
  /* == X Stuff == */
  XtAppContext		app;
  Display		*display;
  Widget		toplevel, shell, canvas;
  Widget		graph_panel;
  Widget		btn[STRIPBTN_COUNT];
  Widget		popup_menu, message_box;
  Widget		fs_dlg;
  char			app_name[128];

  /* == file descriptor management ==  */
  stripFdInfo		fdinfo[STRIP_MAX_FDS];

  /* == Strip Components == */
  StripConfig		*config;
  StripDialog		dialog;
  StripCurveInfo	curves[STRIP_MAX_CURVES];
  StripDataSource	data;
  StripHistory		history;
  StripGraph		graph;
  unsigned		status;
  PrintInfo		print_info;
  ListDialog		*list_dlg;

  /* == Callback stuff == */
  StripCallback		connect_func, disconnect_func, client_io_func;
  void 			*connect_data, *disconnect_data, *client_io_data;
  int			client_registered;
  int			grab_count;

  /* timing and event stuff */
  unsigned		event_mask;
  struct timeval	last_event[LAST_STRIPEVENT];
  struct timeval	next_event[LAST_STRIPEVENT];

  XtIntervalId		tid;
}
StripInfo;


/* ====== Miscellaneous stuff ====== */
typedef enum
{
  STRIPWINDOW_GRAPH = 0,
  STRIPWINDOW_LISTDLG,
  STRIPWINDOW_COUNT
}
StripWindowType;

static char	*StripWindowTypeStr[STRIPWINDOW_COUNT] =
{
  "Graph",
  "Unconnected Signals"
};

typedef void	(*fsdlg_functype)	(Strip, char *);

/* ====== Static Data ====== */
static struct timezone	tz;
static struct timeval	tv;



/* ====== Static Function Prototypes ====== */
static void	Strip_watchevent	(StripInfo *, unsigned);
static void	Strip_ignoreevent	(StripInfo *, unsigned);

static void	Strip_dispatch		(StripInfo *);
static void	Strip_eventmgr		(XtPointer, XtIntervalId *);

static void	Strip_child_msg		(void *);
static void	Strip_config_callback	(StripConfigMask, void *);

static void	Strip_forgetcurve	(StripInfo *, StripCurve);

static void	Strip_graphdrop_handle	(Widget, XtPointer, XtPointer);
static void	Strip_graphdrop_xfer 	(Widget, XtPointer, Atom *, Atom *,
                                         XtPointer, unsigned long *, int *);

static void	callback		(Widget, XtPointer, XtPointer);


static void	dlgrequest_connect	(void *, void *);
static void	dlgrequest_show		(void *, void *);
static void	dlgrequest_clear	(void *, void *);
static void	dlgrequest_delete	(void *, void *);
static void	dlgrequest_dismiss	(void *, void *);
static void	dlgrequest_quit		(void *, void *);
static void	dlgrequest_window_popup	(void *, void *);

static Widget	PopupMenu_build		(Widget);
static void	PopupMenu_position	(Widget, XButtonPressedEvent *);
static void	PopupMenu_popup		(Widget);
static void	PopupMenu_popdown	(Widget);
static void	PopupMenu_cb		(Widget, XtPointer, XtPointer);

static void	fsdlg_popup		(StripInfo *, fsdlg_functype);
static void	fsdlg_cb		(Widget, XtPointer, XtPointer);

static ListDialog	*ListDialog_init	(Strip, Widget);
static void		ListDialog_delete	(ListDialog *);
static void		ListDialog_popup	(ListDialog *);
static void		ListDialog_popdown	(ListDialog *);
static void		ListDialog_addcurves	(ListDialog *, StripCurve[]);
static void		ListDialog_removecurves	(ListDialog *, StripCurve[]);
static int		ListDialog_count	(ListDialog *);
static int		ListDialog_isviewable	(ListDialog *);
static int		ListDialog_ismapped	(ListDialog *);
static void		ListDialog_cb		(Widget, XtPointer, XtPointer);


/* ====== Functions ====== */
Strip	Strip_init	(int 	*argc,
			 char 	*argv[],
			 FILE 	*logfile)
{
  StripInfo		*si;
  Dimension		width, height;
  XSetWindowAttributes	xswa;
  XVisualInfo		xvi;
  Widget		form, rowcol, hintshell;
  Pixmap		pixmap;
  Pixel			fg, bg;
  SDWindowMenuItem	wm_items[STRIPWINDOW_COUNT];
  cColorManager		scm;
  int			i, n;
  Arg			args[10];
  Atom			import_list[10];
  int			stat = 1;
  double		tmp_dbl;
  StripConfigMask	scfg_mask;

  StripConfig_preinit();
  StripConfigMask_clear (&scfg_mask);

  if ((si = (StripInfo *)malloc (sizeof (StripInfo))) != NULL)
  {
    /* create the application context and top-level shell, but don't
     * realize the shell until the appropriate visual has been
     * chosen.
     */
    XtSetLanguageProc (NULL, NULL, NULL);
    si->toplevel = XtVaAppInitialize
      (&si->app, STRIP_APP_CLASS, (XrmOptionDescList)0,
       (Cardinal)0, argc, (String *)argv, fallback_resources,
       XmNmappedWhenManaged, False,
       NULL);
    si->display = XtDisplay (si->toplevel);

    /* initialize the color manager */
    scm = cColorManager_init (si->display, /*STRIPCONFIG_NUMCOLORS*/ 0);
    if (scm)
    {
      xvi = *cColorManager_get_visinfo (scm);
      XtVaSetValues
        (si->toplevel,
         XmNvisual, 	xvi.visual,
         XmNcolormap, 	cColorManager_getcmap (scm),
         0);
      XtRealizeWidget (si->toplevel);
    }
    else
    {
      fprintf (stdout, "Strip_init: cannot initialize color manager.\n");
      XtDestroyWidget (si->toplevel);
      free (si);
      return NULL;
    }

    /* initialize any miscellaeous routines */
    StripMisc_init (si->display, xvi.screen);

    /* initialize the Config module */
    if (!(si->config = StripConfig_init (scm, &xvi, NULL, scfg_mask)))
    {
      fprintf
        (stdout,
         "Strip_init: cannot initialize configuration manager.\n");
      cColorManager_delete (scm);
      XtDestroyWidget (si->toplevel);
      free (si);
      return NULL;
    }
    si->config->logfile = logfile;

    /* build the other shells now that the toplevel
     * has been taken care of.
     */
#ifdef USE_CLUES
    hintshell = XtVaCreatePopupShell
      ("hintShell", xcgLiteClueWidgetClass, si->toplevel, 0);
#endif

    si->shell = XtVaCreatePopupShell
      ("StripGraph",
       topLevelShellWidgetClass,	si->toplevel,
       XmNdeleteResponse,		XmDO_NOTHING,
       XmNmappedWhenManaged,		False,
       XmNvisual,			si->config->xvi.visual,
       XmNcolormap,			cColorManager_getcmap (scm),
       NULL);

    width = (Dimension)
      (((double)DisplayWidth (si->display, DefaultScreen (si->display)) /
        (double)DisplayWidthMM (si->display, DefaultScreen (si->display)))
       * STRIP_GRAPH_WIDTH_MM);
    height = (Dimension)
      (((double)DisplayHeight (si->display, DefaultScreen (si->display)) /
        (double)DisplayHeightMM (si->display, DefaultScreen (si->display)))
       * STRIP_GRAPH_HEIGHT_MM);
    XtVaSetValues
      (si->shell,
       XmNwidth,	width,
       XmNheight,	height,
       NULL);
    form = XtVaCreateManagedWidget
      ("graphBaseForm",
       xmFormWidgetClass,		si->shell,
       0);

    /* the the motif foreground and background colors */
    XtVaGetValues
      (form,
       XmNforeground,		&fg,
       XmNbackground,		&bg,
       0);

    /* the graph control panel */
    si->graph_panel = XtVaCreateManagedWidget
      ("graphPanel",
       xmFrameWidgetClass,		form,
       XmNtopAttachment,		XmATTACH_NONE,
       XmNleftAttachment,		XmATTACH_FORM,
       XmNrightAttachment,		XmATTACH_FORM,
       XmNbottomAttachment,		XmATTACH_FORM,
       0);
    rowcol = XtVaCreateManagedWidget
      ("controlsRowColumn",
       xmRowColumnWidgetClass,	si->graph_panel,
       XmNorientation,		XmHORIZONTAL,
       0);
      
    /* pan left button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen), pan_left_bits,
       pan_left_width, pan_left_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_LEFT] = XtVaCreateManagedWidget
      ("panLeftButton",
       xmPushButtonWidgetClass,	rowcol,
       XmNlabelType,			XmPIXMAP,
       XmNlabelPixmap,		pixmap,
       0);

    /* pan right button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen), pan_right_bits,
       pan_right_width, pan_right_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_RIGHT] = XtVaCreateManagedWidget
      ("panRightButton",
       xmPushButtonWidgetClass,	rowcol,
       XmNlabelType,			XmPIXMAP,
       XmNlabelPixmap,		pixmap,
       0);
      
    XtVaCreateManagedWidget
      ("separator",
       xmSeparatorWidgetClass, 	rowcol,
       XmNorientation,		XmVERTICAL,
       0);

    /* zoom in button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen), zoom_in_bits,
       zoom_in_width, zoom_in_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_ZOOMIN] = XtVaCreateManagedWidget
      ("zoomInButton",
       xmPushButtonWidgetClass, 	rowcol,
       XmNlabelType,			XmPIXMAP,
       XmNlabelPixmap,		pixmap,
       0);

    /* zoom out button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen), zoom_out_bits,
       zoom_out_width, zoom_out_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_ZOOMOUT] = XtVaCreateManagedWidget
      ("zoomOutButton",
       xmPushButtonWidgetClass, 	rowcol,
       XmNlabelType,			XmPIXMAP,
       XmNlabelPixmap,		pixmap,
       0);
      
    XtVaCreateManagedWidget
      ("separator",
       xmSeparatorWidgetClass, 	rowcol,
       XmNorientation,		XmVERTICAL,
       0);

    /* auto scroll button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen), auto_scroll_bits,
       auto_scroll_width, auto_scroll_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_AUTOSCROLL] = XtVaCreateManagedWidget
      ("autoScrollButton",
       xmPushButtonWidgetClass, 	rowcol,
       XmNlabelType,			XmPIXMAP,
       XmNlabelPixmap,		pixmap,
       0);

    for (i = 0; i < STRIPBTN_COUNT; i++)
      XtAddCallback (si->btn[i], XmNactivateCallback, callback, si);

#ifdef USE_CLUES
    XcgLiteClueAddWidget (hintshell, si->btn[STRIPBTN_LEFT], "Pan left", 0, 0);
    XcgLiteClueAddWidget (hintshell, si->btn[STRIPBTN_RIGHT], "Pan right", 0, 0);
    XcgLiteClueAddWidget (hintshell, si->btn[STRIPBTN_ZOOMIN], "Zoom in", 0, 0);
    XcgLiteClueAddWidget (hintshell, si->btn[STRIPBTN_ZOOMOUT], "Zoom out", 0, 0);
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_AUTOSCROLL], "Auto scroll", 0, 0);
#endif

    /* the graph drawing area */
    si->canvas = XtVaCreateManagedWidget
      ("drawing area",
       xmDrawingAreaWidgetClass,	form,
       XmNmappedWhenManaged,		True,
       XmNuserData,			si,
       XmNtopAttachment,		XmATTACH_FORM,
       XmNleftAttachment,		XmATTACH_FORM,
       XmNrightAttachment,		XmATTACH_FORM,
       XmNbottomAttachment,		XmATTACH_WIDGET,
       XmNbottomWidget,			si->graph_panel,
       NULL);

    
    /* register the drawing area as a drop site accepting compound text
     * and string type data.  Note that, since there is no way to
     * register client data fo rthe callback, we need to store the
     * StripInfo pointer as the widget's user data */
    i = 0; n = 0;
    import_list[i++] = XmInternAtom (si->display, "COMPOUND_TEXT", False);
    XtSetArg (args[n], XmNimportTargets, import_list); n++;
    XtSetArg (args[n], XmNnumImportTargets, i); n++;
    XtSetArg (args[n], XmNdropSiteOperations, XmDROP_COPY); n++;
    XtSetArg (args[n], XmNdropProc, Strip_graphdrop_handle); n++;
    XmDropSiteRegister (si->canvas, args, n);
      

    /* popup menu, etc. */
    si->popup_menu = PopupMenu_build (si->canvas);
    si->message_box = (Widget)0;
    si->list_dlg = ListDialog_init ((Strip)si, si->toplevel);
    XtVaSetValues (si->popup_menu, XmNuserData, si, NULL);
    XtRealizeWidget (si->shell);

    si->fs_dlg = 0;

    for (i = 0; i < STRIPWINDOW_COUNT; i++)
    {
      strcpy (wm_items[i].name, StripWindowTypeStr[i]);
      wm_items[i].window_id = (void *)i;
      wm_items[i].cb_func = dlgrequest_window_popup;
      wm_items[i].cb_data = (void *)si;
    }
	  
    si->dialog	= StripDialog_init (si->toplevel, si->config);
    StripDialog_setattr
      (si->dialog,
       STRIPDIALOG_CONNECT_FUNC,	dlgrequest_connect,
       STRIPDIALOG_CONNECT_DATA,	si,
       STRIPDIALOG_SHOW_FUNC,		dlgrequest_show,
       STRIPDIALOG_SHOW_DATA,		si,
       STRIPDIALOG_CLEAR_FUNC,		dlgrequest_clear,
       STRIPDIALOG_CLEAR_DATA,		si,
       STRIPDIALOG_DELETE_FUNC,		dlgrequest_delete,
       STRIPDIALOG_DELETE_DATA,		si,
       STRIPDIALOG_DISMISS_FUNC,	dlgrequest_dismiss,
       STRIPDIALOG_DISMISS_DATA,	si,
       STRIPDIALOG_QUIT_FUNC,		dlgrequest_quit,
       STRIPDIALOG_QUIT_DATA,		si,
       STRIPDIALOG_WINDOW_MENU,	wm_items, STRIPWINDOW_COUNT,
       0);

    si->history = StripHistory_init ((Strip)si);
    si->data = StripDataSource_init (si->history);
    tmp_dbl = ceil
      ((double)si->config->Time.timespan /
       si->config->Time.sample_interval);
    StripDataSource_setattr (si->data, SDS_NUMSAMPLES, (size_t)tmp_dbl, 0);

    si->graph = StripGraph_init
      (si->display, XtWindow (si->canvas), si->config, si->shell);

    StripGraph_setattr
      (si->graph,
       STRIPGRAPH_DATA_SOURCE,	si->data,
       0);

    si->connect_func = si->disconnect_func = si->client_io_func = NULL;
    si->connect_data = si->disconnect_data = si->client_io_data = NULL;

    si->tid = (XtIntervalId)0;

    si->event_mask = 0;
    for (i = 0; i < LAST_STRIPEVENT; i++)
    {
      si->last_event[i].tv_sec	= 0;
      si->last_event[i].tv_usec	= 0;
      si->next_event[i].tv_sec 	= 0;
      si->next_event[i].tv_usec = 0;
    }

    for (i = 0; i < STRIP_MAX_CURVES; i++)
    {
      si->curves[i].scfg 			= si->config;
      si->curves[i].details 			= NULL;
      si->curves[i].func_data 			= NULL;
      si->curves[i].get_value			= NULL;
      si->curves[i].connect_request.tv_sec 	= 0;
      si->curves[i].id 				= NULL;
      si->curves[i].status			= 0;
    }

      
#ifdef USE_PSPRINTER
    if (getenv("PSPRINTER"))
    {
      strcpy (si->print_info.pr_name, getenv("PSPRINTER"));
      strcpy (si->print_info.dev_name, "ps");
      si->print_info.is_color = 0;
    }
    else
    {
      strcpy (si->print_info.dev_name, STRIP_PRINTER_DEVNAME);
      strcpy (si->print_info.pr_name, STRIP_PRINTER_NAME); 
      si->print_info.is_color = STRIP_PRINTER_ISCOLOR;
    }
#else
    strcpy (si->print_info.dev_name, STRIP_PRINTER_DEVNAME);
    strcpy (si->print_info.pr_name, STRIP_PRINTER_NAME); 
    si->print_info.is_color = STRIP_PRINTER_ISCOLOR;
#endif

    /* load the icon pixmap */
#ifdef USE_XPM
    if (XpmSuccess == XpmCreatePixmapFromData 
        (si->display, RootWindow (si->display, xvi.screen), stripTool_icon_xpm, 
         &pixmap, NULL, NULL))
      XtVaSetValues(si->shell, XtNiconPixmap, pixmap, NULL);
#else
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen), StripIcon_bits,
       StripIcon_width, StripIcon_height, fg, bg,
       xvi.depth);
    if (pixmap)
      XtVaSetValues(si->shell, XtNiconPixmap, pixmap, NULL);
#endif
      

    si->grab_count = 0;

    StripConfig_addcallback
      (si->config, SCFGMASK_ALL, Strip_config_callback, si);

    /* this little snippet insures that an expose event will be generated
     * every time the window is resized --not just when its size increases
     */
    xswa.bit_gravity = ForgetGravity;
    xswa.background_pixel = si->config->Color.background.xcolor.pixel;
    XChangeWindowAttributes
      (si->display, XtWindow (si->canvas),
       CWBitGravity | CWBackPixel, &xswa);
    XChangeWindowAttributes
      (si->display, XtWindow (si->shell),
       CWBitGravity | CWBackPixel, &xswa);
    XtAddCallback (si->canvas, XmNresizeCallback, callback, si);
    XtAddCallback (si->canvas, XmNexposeCallback, callback, si);
    XtAddCallback (si->canvas, XmNinputCallback, callback, si);
    XUnmapWindow (si->display, XtWindow (si->shell));

    si->status = STRIPSTAT_OK;
    memset (si->fdinfo, 0, STRIP_MAX_FDS * sizeof (stripFdInfo));
  }

  return (Strip)si;
}


/*
 * Strip_delete
 */
void	Strip_delete	(Strip the_strip)
{
  StripInfo	*si = (StripInfo *)the_strip;
  Display	*disp = si->display;

  StripGraph_delete 		(si->graph);
  StripDataSource_delete 	(si->data);
  StripHistory_delete 		(si->history);
  StripDialog_delete		(si->dialog);
  StripConfig_delete		(si->config);
  ListDialog_delete		(si->list_dlg);

  XtDestroyWidget (si->toplevel);
  free (si);
}


/*
 * Strip_setattr
 */
int	Strip_setattr	(Strip the_strip, ...)
{
  va_list	ap;
  StripInfo	*si = (StripInfo *)the_strip;
  int		attrib;
  int		ret_val = 1;


  va_start (ap, the_strip);
  for (attrib = va_arg (ap, StripAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripAttribute))
  {
    if ((ret_val = ((attrib > 0) && (attrib < STRIP_LAST_ATTRIBUTE))))
      switch (attrib)
      {
	  case STRIP_CONNECT_FUNC:
	    si->connect_func = va_arg (ap, StripCallback);
	    break;
	  case STRIP_CONNECT_DATA:
	    si->connect_data = va_arg (ap, void *);
	    break;
	  case STRIP_DISCONNECT_FUNC:
	    si->disconnect_func = va_arg (ap, StripCallback);
	    break;
	  case STRIP_DISCONNECT_DATA:
	    si->disconnect_data = va_arg (ap, void *);
	    break;
      }
  }

  va_end (ap);
  return ret_val;
}


/*
 * Strip_getattr
 */
int	Strip_getattr	(Strip the_strip, ...)
{
  va_list	ap;
  StripInfo	*si = (StripInfo *)the_strip;
  int		attrib;
  int		ret_val = 1;


  va_start (ap, the_strip);
  for (attrib = va_arg (ap, StripAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripAttribute))
  {
    if ((ret_val = ((attrib > 0) && (attrib < STRIP_LAST_ATTRIBUTE))))
      switch (attrib)
      {
	  case STRIP_CONNECT_FUNC:
	    *(va_arg (ap, StripCallback *)) = si->connect_func;
	    break;
	  case STRIP_CONNECT_DATA:
	    *(va_arg (ap, void **)) = si->connect_data;
	    break;
	  case STRIP_DISCONNECT_FUNC:
	    *(va_arg (ap, StripCallback *)) = si->disconnect_func;
	    break;
	  case STRIP_DISCONNECT_DATA:
	    *(va_arg (ap, void **)) = si->disconnect_data;
	    break;
      }
  }

  va_end (ap);
  return ret_val;
}


/*
 * Strip_addfd
 */
int	Strip_addfd	(Strip 			the_strip,
			 int 			fd,
			 XtInputCallbackProc 	func,
			 XtPointer		data)
{
  StripInfo	*si = (StripInfo *)the_strip;
  int		i;

  for (i = 0; i < STRIP_MAX_FDS; i++)
    if (!si->fdinfo[i].id) break;

  if (i < STRIP_MAX_FDS)
  {
    si->fdinfo[i].id = XtAppAddInput
      (si->app, fd, (XtPointer)XtInputReadMask, func, data);
    si->fdinfo[i].fd = fd;
  }

  return (i < STRIP_MAX_FDS);
}


/*
 * Strip_addtimeout
 */
XtIntervalId	Strip_addtimeout	(Strip			the_strip,
                                         double 		sec,
                                         XtTimerCallbackProc 	cb_func,
                                         XtPointer		cb_data)
{
  StripInfo		*si = (StripInfo *)the_strip;

  return XtAppAddTimeOut
    (si->app, (unsigned long)(sec * 1000), cb_func, cb_data);
}


/*
 * Strip_clearfd
 */
int	Strip_clearfd	(Strip the_strip, int fd)
{
  StripInfo	*si = (StripInfo *)the_strip;
  int		i;

  for (i = 0; i < STRIP_MAX_FDS; i++)
    if (si->fdinfo[i].fd == fd)
      break;

  if (i < STRIP_MAX_FDS)
  {
    XtRemoveInput (si->fdinfo[i].id);
    si->fdinfo[i].id = 0;
  }
  
  return (i < STRIP_MAX_FDS);
}



/*
 * Strip_go
 */
void	Strip_go	(Strip the_strip)
{
  StripInfo	*si = (StripInfo *)the_strip;
  int		some_curves_connected = 0;
  int		some_curves_waiting = 0;
  int		i;
  XEvent	xevent;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
  {
    if (si->curves[i].status & STRIPCURVE_CONNECTED)
      some_curves_connected = 1;
    if (si->curves[i].connect_request.tv_sec != 0)
      some_curves_waiting = 1;
  }

  if (some_curves_connected)
    window_map (si->display, XtWindow(si->shell));
  else if (some_curves_waiting)
    fprintf (stdout, "Strip_go: waiting for connection...\n");
  else StripDialog_popup (si->dialog);

  /* register the events */
  Strip_dispatch (si);

  XFlush (si->display);

  /* wait for file IO activities on all registered file descriptors */
  while (!(si->status & STRIPSTAT_TERMINATED))
  {
    XtAppNextEvent (si->app, &xevent);
    XtDispatchEvent (&xevent);
  }
}



/*
 * Strip_getcurve
 */
StripCurve	Strip_getcurve	(Strip the_strip)
{
  StripInfo	*si = (StripInfo *)the_strip;
  StripCurve	ret_val = NULL;
  int		i, j;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (si->curves[i].details == NULL)
      break;

  if (i < STRIP_MAX_CURVES)
  {
    /* now find an available StripCurveDetail in the StripConfig */
    for (j = 0; j < STRIP_MAX_CURVES; j++)
      if (si->config->Curves.Detail[j]->id == NULL)
        break;

    if (j < STRIP_MAX_CURVES)
    {
      si->curves[i].details = si->config->Curves.Detail[j];
      si->curves[i].details->id = &si->curves[i];
      ret_val = (StripCurve)&si->curves[i];
    }
  }

  return ret_val;
}


/*
 * Strip_freecurve
 */
void	Strip_freecurve	(Strip the_strip, StripCurve the_curve)
{
  StripInfo		*si = (StripInfo *)the_strip;
  StripCurveInfo	*sci = (StripCurveInfo *)the_curve;
  StripCurve		curve[2];
  int			i;

  for (i = 0; (i < STRIP_MAX_CURVES) && (sci != &si->curves[i]); i++);

  if (i < STRIP_MAX_CURVES)
  {
    curve[0] = the_curve;
    curve[1] = (StripCurve)0;
    Strip_freesomecurves (the_strip, curve);
  }
}


/*
 * Strip_freesomecurves
 */
void	Strip_freesomecurves	(Strip the_strip, StripCurve curves[])
{
  StripInfo		*si = (StripInfo *)the_strip;
  int			i;

  for (i = 0; curves[i]; i++)
  {
    StripGraph_removecurve (si->graph, curves[i]);
    StripDataSource_removecurve (si->data, curves[i]);
    if (si->disconnect_func != NULL)
      si->disconnect_func (curves[i], si->disconnect_data);
  }

  if (i > 0)
  {
    ListDialog_removecurves (si->list_dlg, curves);
    if (ListDialog_count (si->list_dlg) == 0)
      ListDialog_popdown (si->list_dlg);
    StripDialog_removesomecurves (si->dialog, curves);
    StripGraph_draw
      (si->graph,
       SGCOMPMASK_LEGEND | SGCOMPMASK_DATA | SGCOMPMASK_YAXIS,
       (Region *)0);

    for (i = 0; curves[i]; i++)
    {
      StripCurve_clearstat
        ((StripCurve)curves[i], STRIPCURVE_CONNECTED | STRIPCURVE_WAITING);
      Strip_forgetcurve (si, curves[i]);
    }
  }
}


/*
 * Strip_connectcurve
 */
int	Strip_connectcurve	(Strip the_strip, StripCurve the_curve)
{
  StripInfo		*si = (StripInfo *)the_strip;
  StripCurveInfo	*sci = (StripCurveInfo *)the_curve;
  StripCurve		curve[2];
  int			ret_val;

  if (si->connect_func != NULL)
  {
#if 0
    StripCurve_setstat
      (the_curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
#else
    StripCurve_setstat (the_curve, STRIPCURVE_WAITING);
#endif
    gettimeofday (&sci->connect_request, &tz);
      
    if (ret_val = si->connect_func (the_curve, si->connect_data))
      if (!StripCurve_getstat (the_curve,  STRIPCURVE_CONNECTED))
        Strip_setwaiting (the_strip, the_curve);
    StripDialog_addcurve (si->dialog, the_curve);
  }
  else
  {
    fprintf (stderr,
             "Strip_connectcurve:\n"
             "  Warning! no connection routine\n");
    ret_val = 0;
  }

  return ret_val;
}


/*
 * Strip_setconnected
 */
void	Strip_setconnected	(Strip the_strip, StripCurve the_curve)
{
  StripInfo		*si = (StripInfo *)the_strip;
  StripCurveInfo	*sci = (StripCurveInfo *)the_curve;
  StripCurve		curve[2];

  /* add the curve to the various modules only once --the first time it
   * is connected
   */
  if (!StripCurve_getstat (the_curve, STRIPCURVE_CONNECTED))
  {
    StripDataSource_addcurve (si->data, the_curve);
    StripGraph_addcurve (si->graph, the_curve);
  }
  StripCurve_clearstat
    (the_curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
  StripCurve_setstat (the_curve, STRIPCURVE_CONNECTED);
  
  Strip_watchevent (si, STRIPEVENTMASK_SAMPLE | STRIPEVENTMASK_REFRESH);
  if (!window_ismapped (si->display, XtWindow (si->shell)))
    window_map (si->display, XtWindow (si->shell));
  StripGraph_draw
    (si->graph,
     SGCOMPMASK_DATA | SGCOMPMASK_LEGEND | SGCOMPMASK_YAXIS,
     (Region *)0);

  curve[0] = the_curve;
  curve[1] = (StripCurve)0;
  ListDialog_removecurves (si->list_dlg, curve);
  if (ListDialog_count (si->list_dlg) == 0)
    ListDialog_popdown (si->list_dlg);
  
  StripDialog_update_curvestat (si->dialog, the_curve);
}


/*
 * Strip_setwaiting
 */
void	Strip_setwaiting	(Strip the_strip, StripCurve the_curve)
{
  StripInfo		*si = (StripInfo *)the_strip;
  StripCurveInfo	*sci = (StripCurveInfo *)the_curve;
  StripCurve		curve[2];

#if 0
  StripCurve_setstat
    (the_curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
#else
  StripCurve_setstat (the_curve, STRIPCURVE_WAITING);
  Strip_watchevent (si, STRIPEVENTMASK_CHECK_CONNECT);
#endif
  
  curve[0] = the_curve;
  curve[1] = (StripCurve)0;
  ListDialog_addcurves (si->list_dlg, curve);

  StripDialog_update_curvestat (si->dialog, the_curve);
}


/*
 * Strip_clear
 */
void	Strip_clear	(Strip the_strip)
{
  StripInfo		*si = (StripInfo *)the_strip;
  StripCurve		curves[STRIP_MAX_CURVES+1];
  int			i, j;
  StripConfigMask	mask;

  Strip_ignoreevent
    (si,
     STRIPEVENTMASK_SAMPLE |
     STRIPEVENTMASK_REFRESH |
     STRIPEVENTMASK_CHECK_CONNECT);
  
  for (i = 0, j = 0; i < STRIP_MAX_CURVES; i++)
    if (si->curves[i].details != NULL)
    {
      curves[j] = (StripCurve)(&si->curves[i]);
      j++;
    }

  curves[j] = (StripCurve)0;
  Strip_freesomecurves ((Strip)si, curves);

  StripConfig_setattr (si->config, STRIPCONFIG_TITLE, NULL, 0);
  StripConfigMask_clear (&mask);
  StripConfigMask_set (&mask, SCFGMASK_TITLE);
  StripConfig_update (si->config, mask);
}


/*
 * Strip_dump
 */
int	Strip_dumpdata	(Strip the_strip, char *fname)
{
  StripInfo		*si = (StripInfo *)the_strip;
  FILE			*f;
  int			ret_val = 0;

  if (ret_val = ((f = fopen (fname, "w")) != NULL))
    ret_val = StripGraph_dumpdata (si->graph, f);
  return ret_val;
}


/*
 * Strip_writeconfig
 */
int	Strip_writeconfig	(Strip the_strip, FILE *f, StripConfigMask m)
{
  StripInfo	*si = (StripInfo *)the_strip;

  return StripConfig_write (si->config, f, m);
}

/*
 * Strip_readconfig
 */
int	Strip_readconfig	(Strip            the_strip,
				 FILE            *f, 
				 StripConfigMask  m,
				 char            *fileName)
{
  StripInfo	*si = (StripInfo *)the_strip;
  int		ret_val;
  
  ret_val = StripConfig_load (si->config, f, m);
  if (ret_val) { /* VTR */
    si->config->title = strdup(fileName);
    StripConfig_update (si->config, m);
    XtVaSetValues(si->shell, 
		  XtNiconName, GetFileName (si->config->title), 
		  NULL);
  }
  return ret_val;
}



/* ====== Static Functions ====== */

/*
 * Strip_forgetcurve
 */
static void	Strip_forgetcurve	(StripInfo 	*si,
                                         StripCurve 	the_curve)
{
  StripCurveInfo	*sci = (StripCurveInfo *)the_curve;

  StripConfig_reset_details (si->config, sci->details);
  sci->details = NULL;
  sci->get_value = NULL;
  sci->func_data = NULL;
}


/*
 * Strip_graphdrop_handle
 *
 *	Handles drop events on the graph drawing area widget.
 */
static void	Strip_graphdrop_handle	(Widget 	w,
                                         XtPointer 	BOGUS(1),
                                         XtPointer 	call)
{
  StripInfo			*si;
  Display			*dpy;
  Widget			dc;
  XmDropProcCallback		drop_data;
  XmDropTransferEntryRec	xfer_entries[10];
  XmDropTransferEntry		xfer_list;
  Cardinal			n_export_targets;
  Atom				*export_targets;
  Atom				COMPOUND_TEXT;
  Arg				args[10];
  int				n, i;
  Boolean			ok;

  dpy = XtDisplay (w);

  XtVaGetValues (w, XmNuserData, &si, 0);
  drop_data = (XmDropProcCallback) call;
  dc = drop_data->dragContext;

  /* retireve the data targets, and search for COMPOUND_TEXT */
  COMPOUND_TEXT = XmInternAtom (dpy, "COMPOUND_TEXT", False);
  
  n = 0;
  XtSetArg (args[n], XmNexportTargets, &export_targets); n++;
  XtSetArg (args[n], XmNnumExportTargets, &n_export_targets); n++;
  XtGetValues (dc, args, n);

  for (i = 0, ok = False; (i < n_export_targets) && !ok; i++)
    if (ok = (export_targets[i] == COMPOUND_TEXT))
      break;

  n = 0;
  if (!ok || (drop_data->dropAction != XmDROP))
  {
    XtSetArg (args[n], XmNtransferStatus, XmTRANSFER_FAILURE); n++;
    XtSetArg (args[n], XmNnumDropTransfers, 0); n++;
  }
  else
  {
    xfer_entries[0].target = COMPOUND_TEXT;
    xfer_entries[0].client_data = (char *)si;
    xfer_list = xfer_entries;
    XtSetArg (args[n], XmNdropTransfers, xfer_entries); n++;
    XtSetArg (args[n], XmNnumDropTransfers, 1); n++;
    XtSetArg (args[n], XmNtransferProc, Strip_graphdrop_xfer); n++;
  }

  XmDropTransferStart (dc, args, n);
}



/*
 * Strip_graphdrop_xfer
 *
 *	Handles the transfer of data initiated by the drop handler
 *	routine.
 */
static void	Strip_graphdrop_xfer	(Widget		BOGUS(w),
                                         XtPointer	client,
                                         Atom		*BOGUS(sel_type),
                                         Atom		*type,
                                         XtPointer	value,
                                         unsigned long	*BOGUS(length),
                                         int		*BOGUS(format))
{
  StripInfo	*si = (StripInfo *)client;
  StripCurve	curve;
  Atom		COMPOUND_TEXT;
  XmString	xstr;
  char		*name;

  COMPOUND_TEXT = XmInternAtom (si->display, "COMPOUND_TEXT", False);
  if (*type == COMPOUND_TEXT)
  {
    xstr = XmCvtCTToXmString ((char *)value);
    XmStringGetLtoR (xstr, XmFONTLIST_DEFAULT_TAG, &name);
    fprintf (stdout, "dropped string is: %s\n", name);

    if (curve = Strip_getcurve ((Strip)si))
    {
      StripCurve_setattr (curve, STRIPCURVE_NAME, name, 0);
      Strip_connectcurve ((Strip)si, curve);
    }
  }
}


/*
 * Strip_config_callback
 */
static void	Strip_config_callback	(StripConfigMask mask, void *data)
{
  StripInfo		*si = (StripInfo *)data;
  StripCurveInfo	*sci;

  struct		_dcon
  {
    StripCurve		curves[STRIP_MAX_CURVES+1];
    int			n;
  } dcon;
  
  struct		_conn
  {
    StripCurve		curves[STRIP_MAX_CURVES+1];
    int			n;
  } conn;
  
  unsigned	comp_mask = 0;
  int		i, j;
  double	tmp_dbl;


  if (StripConfigMask_stat (&mask, SCFGMASK_TITLE))
  {
    StripGraph_setattr
      (si->graph, STRIPGRAPH_TITLE, si->config->title, 0);
    comp_mask |= SGCOMPMASK_TITLE;
  }

  if (StripConfigMask_intersect (&mask, &SCFGMASK_TIME))
  {
    /* If the plotted timespan has changed, then the graph needs to be
     * redrawn and the data buffer resized.  Force a refresh event by
     * setting the last refresh time to 0 and calling dispatch().  If
     * the sample interval has changed then the data buffer must be
     * resized.  In any time-related event, the dispatch function needs
     * to be called.
     *
     * 8/18/97: if in browse mode, do not force refresh, but set the
     *          graph's time range appropriately, and then force it
     *	  to redraw by setting the component mask.
     */
    if (StripConfigMask_stat (&mask, SCFGMASK_TIME_TIMESPAN))
      if (si->status & STRIPSTAT_BROWSE_MODE)
      {
        struct timeval	t0, t1;
          
        StripGraph_getattr (si->graph, STRIPGRAPH_END_TIME, &t1, 0);
        dbl2time (&tv, si->config->Time.timespan);
        subtract_times (&t0, &tv, &t1);
          
        StripGraph_setattr
          (si->graph,
           STRIPGRAPH_BEGIN_TIME,	&t0,
           STRIPGRAPH_END_TIME,	&t1,
           0);
        comp_mask |=  SGCOMPMASK_DATA | SGCOMPMASK_XAXIS;
      }
      else si->last_event[STRIPEVENT_REFRESH].tv_sec = 0;

    if (StripConfigMask_stat (&mask, SCFGMASK_TIME_TIMESPAN) ||
        StripConfigMask_stat (&mask, SCFGMASK_TIME_SAMPLE_INTERVAL))
    {
      tmp_dbl = ceil
        ((double)si->config->Time.timespan /
         si->config->Time.sample_interval);
      StripDataSource_setattr
        (si->data, SDS_NUMSAMPLES, (size_t)tmp_dbl, 0);
    }
      
    Strip_dispatch (si);
  }


  /* colors */
  if (StripConfigMask_intersect (&mask, &SCFGMASK_COLOR))
  {
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_BACKGROUND))
    {
      /* set the cavas' background color so that when an expose
       * occurs we won't see ugly colors */
      XtVaSetValues
        (si->canvas,
         XmNbackground,	si->config->Color.background.xcolor.pixel,
         0);
      
      /* have to redraw everything when the background color changes */
      comp_mask |= SGCOMPMASK_ALL;
      StripGraph_setstat
        (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_FOREGROUND))
    {
      comp_mask |= SGCOMPMASK_YAXIS | SGCOMPMASK_XAXIS;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_GRID))
    {
      comp_mask |= SGCOMPMASK_GRID;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_LEGENDTEXT))
    {
      comp_mask |= SGCOMPMASK_LEGEND;
      StripGraph_setstat (si->graph, SGSTAT_LEGEND_REFRESH);
    }

    /* if any of the curves have changed color, must redraw */
    for (i = 0; i < STRIP_MAX_CURVES; i++)
      if (StripConfigMask_stat
          (&mask, (StripConfigMaskElement)SCFGMASK_COLOR_COLOR1+i))
      {
        /* no way of knowing if the curve whose color changed was the
         * one defining the yaxis color */
        comp_mask |= SGCOMPMASK_DATA | SGCOMPMASK_LEGEND | SGCOMPMASK_YAXIS;
        StripGraph_setstat
          (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
      }
  }

  
  /* graph options */
  if (StripConfigMask_intersect (&mask, &SCFGMASK_OPTION))
  {
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_GRID_XON))
    {
      comp_mask |= SGCOMPMASK_GRID;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_GRID_YON))
    {
      comp_mask |= SGCOMPMASK_GRID;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_AXIS_YCOLORSTAT))
    {
      comp_mask |= SGCOMPMASK_YAXIS;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_AXIS_XNUMTICS))
    {
      comp_mask |= SGCOMPMASK_XAXIS | SGCOMPMASK_GRID;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_AXIS_YNUMTICS))
    {
      comp_mask |= SGCOMPMASK_YAXIS | SGCOMPMASK_GRID;
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_GRAPH_LINEWIDTH))
    {
      comp_mask |= SGCOMPMASK_DATA;
      StripGraph_setstat (si->graph, SGSTAT_GRAPH_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_LEGEND_VISIBLE))
    {
      comp_mask |= SGCOMPMASK_LEGEND;
      StripGraph_setstat (si->graph, SGSTAT_MANAGE_GEOMETRY);
    }
  }

  if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_NAME))
  {
    dcon.n = conn.n = 0;
    for (i = 0; i < STRIP_MAX_CURVES; i++)
    {
      if (StripConfigMask_stat
          (&si->config->Curves.Detail[i]->update_mask,
           SCFGMASK_CURVE_NAME))
      {
        if ((sci = (StripCurveInfo *)si->config->Curves.Detail[i]->id)
            != NULL)
        {
          /* the detail structure is used by some StripCurve */
          if (StripCurve_getstat
              (sci, STRIPCURVE_CONNECTED | STRIPCURVE_WAITING))
            dcon.curves[dcon.n++] = (StripCurve)sci;
        }
        else
        {
          /* find an available StripCurve to contain the details */
          for (j = 0; j < STRIP_MAX_CURVES; j++)
            if (si->curves[j].details == NULL)
              break;
          if (j < STRIP_MAX_CURVES)
          {
            sci = &si->curves[j];
            sci->details = si->config->Curves.Detail[i];
            sci->details->id = sci;
          }
        }
	      
        if (sci != NULL)
          conn.curves[conn.n++] = (StripCurve)sci;
      }
    }

    /* disconnect any currently connected curves whose names have changed */
    for (i = 0; i < dcon.n; i++)
    {
      StripGraph_removecurve (si->graph, dcon.curves[i]);
      StripDataSource_removecurve (si->data, dcon.curves[i]);
      if (si->disconnect_func != NULL)
        si->disconnect_func (dcon.curves[i], si->disconnect_data);
      StripCurve_clearstat
        (dcon.curves[i], STRIPCURVE_CONNECTED | STRIPCURVE_WAITING);
      sci = (StripCurveInfo *)dcon.curves[i];
    }
    /* remove the disconnected curves from the dialog */
    if (dcon.n > 0)
    {
      dcon.curves[dcon.n] = (StripCurve)0;
      StripDialog_removesomecurves (si->dialog, dcon.curves);
      ListDialog_removecurves (si->list_dlg, dcon.curves);
      if (ListDialog_count (si->list_dlg) == 0)
        ListDialog_popdown (si->list_dlg);
    }
    /* finally attempt to connect all new curves */
    for (i = 0; i < conn.n; i++)
    {
      if (!Strip_connectcurve ((Strip)si, conn.curves[i]))
      {
        fprintf
          (stderr,
           "Strip_config_callback:\n"
           "  Warning! connect failed for curve, %s\n",
           (char *) StripCurve_getattr_val
           (conn.curves[i], STRIPCURVE_NAME));
        Strip_forgetcurve (si, conn.curves[i]);
      }
    }
  }

  if (StripConfigMask_intersect (&mask, &SCFGMASK_CURVE))
  {
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_NAME))
    {
      comp_mask |= SGCOMPMASK_LEGEND | SGCOMPMASK_DATA | SGCOMPMASK_YAXIS;
      StripGraph_setstat
        (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_EGU))
    {
      comp_mask |= SGCOMPMASK_LEGEND;
      StripGraph_setstat (si->graph, SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_PRECISION))
    {
      comp_mask |= SGCOMPMASK_LEGEND | SGCOMPMASK_YAXIS;
      StripGraph_setstat (si->graph, SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_MIN))
    {
      comp_mask |= SGCOMPMASK_LEGEND | SGCOMPMASK_DATA | SGCOMPMASK_YAXIS;
      StripGraph_setstat
        (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_MAX))
    {
      comp_mask |= SGCOMPMASK_LEGEND | SGCOMPMASK_DATA | SGCOMPMASK_YAXIS;
      StripGraph_setstat
        (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_PENSTAT))
    {
      comp_mask |= SGCOMPMASK_DATA;
      StripGraph_setstat (si->graph, SGSTAT_GRAPH_REFRESH);
    }
      
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_PLOTSTAT))
    {
      /* re-arrange the plot order */
      int shift;
          
      for (i = 0; i < STRIP_MAX_CURVES; i++)
        if (si->curves[i].details)
          if (StripConfigMask_stat
              (&si->curves[i].details->update_mask,
               SCFGMASK_CURVE_PLOTSTAT))
          {
            for (j = 0, shift = 0; j < STRIP_MAX_CURVES; j++)
              if (si->config->Curves.plot_order[j] == i)
                shift = 1;
              else if (shift)
                si->config->Curves.plot_order[j-1] =
                  si->config->Curves.plot_order[j];
            si->config->Curves.plot_order[STRIP_MAX_CURVES-1] = i;
            break;
          }
      
      comp_mask |= SGCOMPMASK_DATA;
      StripGraph_setstat (si->graph, SGSTAT_GRAPH_REFRESH);
    }
  }

  if (comp_mask) StripGraph_draw (si->graph, comp_mask, (Region *)0);
}


/*
 * Strip_eventmgr
 */
static void	Strip_eventmgr		(XtPointer arg, XtIntervalId *BOGUS(id))
{
  StripInfo		*si = (StripInfo *)arg;
  struct timeval	event_time;
  struct timeval	t;
  double		diff;
  unsigned		event;
  int			i, n;

  for (event = 0; event < LAST_STRIPEVENT; event++)
  {
    /* only process desired events */
    if (!(si->event_mask & (1 << event))) continue;
    
    gettimeofday (&event_time, &tz);
    diff = subtract_times (&tv, &event_time, &si->next_event[event]);
    if (diff <= STRIP_TIMER_ACCURACY)
    {
      si->last_event[event] = event_time;
      
      switch (event)
      {
          case STRIPEVENT_SAMPLE:
            StripDataSource_sample (si->data);
            break;
            
          case STRIPEVENT_REFRESH:
            /* if we are in browse mode, then only refresh the graph if the
             * current time falls within the displayed time range */
            if (si->status & STRIPSTAT_BROWSE_MODE)
            {
              struct timeval	t0, t1;

              StripGraph_getattr
                (si->graph,
                 STRIPGRAPH_BEGIN_TIME,	&t0,
                 STRIPGRAPH_END_TIME,	&t1,
                 0);
              if ((compare_times (&event_time, &t0) >= 0) &&
                  (compare_times (&event_time, &t1) <= 0))
                StripGraph_draw (si->graph, SGCOMPMASK_DATA, (Region *)0);
            }
            /* if we are not in browse mode then refresh the graph with
             * the current time as the rightmost value */
            else
            {
              dbl2time (&tv, si->config->Time.timespan);
              subtract_times (&t, &tv, &event_time);
              StripGraph_setattr
                (si->graph,
                 STRIPGRAPH_BEGIN_TIME,	&t,
                 STRIPGRAPH_END_TIME,	&event_time,
                 0);
              StripGraph_draw
                (si->graph, SGCOMPMASK_XAXIS | SGCOMPMASK_DATA, (Region *)0);
            }
            break;
            
          case STRIPEVENT_CHECK_CONNECT:
            for (i = 0, n = 0; i < STRIP_MAX_CURVES; i++)
              if (si->curves[i].details != 0)
                if (StripCurve_getstat
                    ((StripCurve)&si->curves[i], STRIPCURVE_WAITING) &&
                    StripCurve_getstat
                    ((StripCurve)&si->curves[i], STRIPCURVE_CHECK_CONNECT))
                {
                  diff = subtract_times
                    (&tv, &si->curves[i].connect_request, &event_time);
                  if (diff >= STRIP_CONNECTION_TIMEOUT)
                  {
                    StripCurve_clearstat
                      ((StripCurve)&si->curves[i],
                       STRIPCURVE_CHECK_CONNECT);
                    n++;
                  }
                  if (n > 0) ListDialog_popup (si->list_dlg);
                }
            break;
      }
    }
  }
  
  si->tid = (XtInputId)0;
  Strip_dispatch (si);
}


/*
 * Strip_watchevent
 */
static void	Strip_watchevent	(StripInfo *si, unsigned mask)
{
  si->event_mask |= mask;
  Strip_dispatch (si);
}

/*
 * Strip_ignoreevent
 */
static void	Strip_ignoreevent	(StripInfo *si, unsigned mask)
{
  si->event_mask &= ~mask;
  Strip_dispatch (si);
}


/*
 * Strip_dispatch
 */
static void	Strip_dispatch		(StripInfo *si)
{
  struct timeval	t;
  struct timeval	now;
  struct timeval	*next;
  double		interval[LAST_STRIPEVENT];
  int			i;
  double		sec;

  /* first, if an alarm has already been registered, disable it */
  if (si->tid != (XtIntervalId)0) XtRemoveTimeOut (si->tid);
  
  interval[STRIPEVENT_SAMPLE] 		= si->config->Time.sample_interval;
  interval[STRIPEVENT_REFRESH] 		= si->config->Time.refresh_interval;
  interval[STRIPEVENT_CHECK_CONNECT] 	= (double)STRIP_CONNECTION_TIMEOUT;

  next = NULL;
  
  /* first determine the event time for each possible next event */
  for (i = 0; i < LAST_STRIPEVENT; i++)
  {
    if (!(si->event_mask & (1 << i))) continue;
      
#ifdef DEBUG_EVENT
    fprintf
      (stdout, "=> last %-15s:\n%s\n",
       StripEventTypeStr[i],
       time_str (&si->last_event[i]));
#endif
    dbl2time (&t, interval[i]);
    add_times (&si->next_event[i], &si->last_event[i], &t);

    if (next == NULL)
      next = &si->next_event[i];
    else if (compare_times (&si->next_event[i], next) <= 0)
      next = &si->next_event[i];
  }
  
  /* event time */
  /* now calculate the number of milliseconds until the next event */
  gettimeofday (&now, &tz);

  if (next != NULL)
  {
#ifdef DEBUG_EVENT
    fprintf (stdout, "============\n");
    for (i = 0; i < LAST_STRIPEVENT; i++)
    {
      if (si->event_mask & (1 << i))
        fprintf
          (stdout, "=> Next %s\n%s\n",
           StripEventTypeStr[i],
           time_str (&si->next_event[i]));
    }
    fprintf (stdout, "=> Next Event\n%s\n", time_str (next));
    fprintf (stdout, "=> Current Time\n%s\n", time_str (&now));
    fprintf (stdout, "=> si->next_event - current_time = %lf (seconds)\n",
             subtract_times (&t, &now, next));
    fprintf (stdout, "============\n");
#endif

    /* finally, register the timer callback */
    if ((sec = subtract_times (&t, &now, next)) < 0)
      sec = 0;
    si->tid = Strip_addtimeout ((Strip)si, sec, Strip_eventmgr, (XtPointer)si);
  }

#ifdef DEBUG_EVENT
  fflush (stdout);
#endif
}


/*
 * callback
 */
static void	callback	(Widget w, XtPointer client, XtPointer call)
{
  XmDrawingAreaCallbackStruct	*cbs;
  XEvent			*event;
  StripInfo			*si;
  XWindowAttributes		xwa;
  XRectangle			r;
  StripConfigMask		scfg_mask;

  static int 			x, y;
  static Region			expose_region = (Region)0;

  cbs = (XmDrawingAreaCallbackStruct *)call;
  event = cbs->event;
  si = (StripInfo *)client;

  switch (cbs->reason)
  {
      case XmCR_RESIZE:
        
        XGetWindowAttributes (XtDisplay (w), XtWindow (w), &xwa);
        StripGraph_setattr
          (si->graph,
           STRIPGRAPH_WIDTH,	xwa.width,
           STRIPGRAPH_HEIGHT,	xwa.height,
           0);
        StripGraph_draw (si->graph, SGCOMPMASK_ALL, (Region *)0);
        break;
        
        
      case XmCR_EXPOSE:
        
        if (expose_region == (Region)0)
          expose_region = XCreateRegion();
        
        r.x = event->xexpose.x;
        r.y = event->xexpose.y;
        r.width = event->xexpose.width;
        r.height = event->xexpose.height;
        
        XUnionRectWithRegion (&r, expose_region, expose_region);
        
        if (event->xexpose.count == 0)
	{
	  StripGraph_draw (si->graph, 0, &expose_region);
	  XDestroyRegion (expose_region);
	  expose_region = (Region)0;
	}
        break;

        
      case XmCR_ACTIVATE:

        /*
         * Pan left or right
         */
        if (w == si->btn[STRIPBTN_LEFT] || w == si->btn[STRIPBTN_RIGHT])
        {
          struct timeval	t, tb, t0, t1;

          /* pan by half a screen */
          dbl2time (&t, si->config->Time.timespan / 2.0);
          StripGraph_getattr (si->graph, STRIPGRAPH_END_TIME, &tb, 0);

          if (w == si->btn[STRIPBTN_LEFT])
            /* t1 = tb - t */
            subtract_times (&t1, &t, &tb);
          else
            /* t1 = tb + t */
            add_times (&t1, &t, &tb);

          /* go into browse mode, and redraw the graph with the new range */
          si->status |= STRIPSTAT_BROWSE_MODE;
          
          dbl2time (&t, si->config->Time.timespan);
          subtract_times (&t0, &t, &t1);
          
          StripGraph_setattr
            (si->graph,
             STRIPGRAPH_BEGIN_TIME,	&t0,
             STRIPGRAPH_END_TIME,	&t1,
             0);
          StripGraph_draw
            (si->graph, SGCOMPMASK_DATA | SGCOMPMASK_XAXIS, (Region *)0);
        }

        /*
         * Zoom in or out
         *
         *	(1) set new end time of graph object (begin time is calculated
         *          as offset from end time)
         *	(2) turn on browse mode
         *	(3) set new timespan via StripConfig_setattr()
         *	(4) generate configuration update callbacks, causing
         *          the graph to be updated.
         */
        else if (w == si->btn[STRIPBTN_ZOOMIN] || w == si->btn[STRIPBTN_ZOOMOUT])
        {
          struct timeval	t, tb, t1;
          unsigned		t_new;
          
          StripGraph_getattr (si->graph, STRIPGRAPH_END_TIME, &tb, 0);
          
          if (w == si->btn[STRIPBTN_ZOOMIN])
          {
            /* halve viewable area by subtracting 1/4 current range from the
             * end-point */
            dbl2time (&t, si->config->Time.timespan * 0.25);
            subtract_times (&t1, &t, &tb);
            t_new = si->config->Time.timespan / 2;
          }
          else
          {
            /* double viewable area by adding 1/2 current range to the end-point */
            dbl2time (&t, si->config->Time.timespan * 0.5);
            add_times (&t1, &t, &tb);
            t_new = si->config->Time.timespan * 2;
          }

          StripGraph_setattr (si->graph, STRIPGRAPH_END_TIME, &t1, 0);
          si->status |= STRIPSTAT_BROWSE_MODE;
          StripConfig_setattr
            (si->config, STRIPCONFIG_TIME_TIMESPAN, t_new, 0);

          StripConfigMask_clear (&scfg_mask);
          StripConfigMask_set (&scfg_mask, SCFGMASK_TIME_TIMESPAN);
          StripConfig_update (si->config, scfg_mask);
        }

        /*
         * Reset auto-scroll
         */
        else if (w == si->btn[STRIPBTN_AUTOSCROLL])
        {
          si->status &= ~STRIPSTAT_BROWSE_MODE;
        }
        
        
      case XmCR_INPUT:
        
        if (event->xany.type == ButtonPress)
	{
	  x = event->xbutton.x;
	  y = event->xbutton.y;
          
	  if (event->xbutton.button == Button3)
          {
            PopupMenu_position
              (si->popup_menu, (XButtonPressedEvent *)cbs->event);
            PopupMenu_popup (si->popup_menu);
          }
	  StripGraph_inputevent (si->graph, event);
	}
        else if (event->xany.type == ButtonRelease)
	{
	}
        break;
  }
}

/*
 * dlgrequest_connect
 */
static void	dlgrequest_connect	(void *client, void *call)
{
  StripInfo	*si = (StripInfo *)client;
  char		*name = (char *)call;
  StripCurve	curve;
  
  if ((curve = Strip_getcurve ((Strip)si)) != 0)
  {
    StripCurve_setattr (curve, STRIPCURVE_NAME, name, 0);
    Strip_connectcurve ((Strip)si, curve);
  }
}


/*
 * dlgrequest_show
 */
static void	dlgrequest_show		(void *client, void *BOGUS(1))
{
  StripInfo	*si = (StripInfo *)client;
  window_map (si->display, XtWindow (si->shell));
}


/*
 * dlgrequest_clear
 */
static void	dlgrequest_clear	(void *client, void *BOGUS(1))
{
  StripInfo	*si = (StripInfo *)client;
  Strip_clear ((Strip)si);
}


/*
 * dlgrequest_delete
 */
static void	dlgrequest_delete	(void *client, void *call)
{
  StripInfo	*si = (StripInfo *)client;
  StripCurve	curve  = (StripCurve)call;

  Strip_freecurve ((Strip)si, curve);
}


/*
 * dlgrequest_dismiss
 */
static void	dlgrequest_dismiss	(void *client, void *BOGUS(1))
{
  StripInfo	*si = (StripInfo *)client;
  Widget	tmp_w;

  if (window_ismapped (si->display, XtWindow (si->shell)))
    StripDialog_popdown (si->dialog);
  else
  {
    StripDialog_getattr (si->dialog, STRIPDIALOG_SHELL_WIDGET, &tmp_w, 0);
    MessageBox_popup
      (tmp_w, &si->message_box,
       "You can't dismiss all your windows", "Ok");
  }
}


/*
 * dlgrequest_quit
 */
static void	dlgrequest_quit	(void *client, void *BOGUS(1))
{
  StripInfo	*si = (StripInfo *)client;
  
  window_unmap (si->display, XtWindow(si->shell));
  StripDialog_popdown (si->dialog);
  si->status |= STRIPSTAT_TERMINATED;
  Strip_ignoreevent
    (si,
     STRIPEVENTMASK_SAMPLE |
     STRIPEVENTMASK_REFRESH |
     STRIPEVENTMASK_CHECK_CONNECT);
}


/*
 * dlgrequest_window_popup
 */
static void	dlgrequest_window_popup	(void *client, void *call)
{
  
  StripInfo		*si = (StripInfo *)client;
  StripWindowType	which = (StripWindowType)call;

  switch (which)
  {
      case STRIPWINDOW_GRAPH:
        window_map (si->display, XtWindow (si->shell));
        break;
      case STRIPWINDOW_LISTDLG:
        ListDialog_popup (si->list_dlg);
        break;
  }
}



/* ====== Popup Menu stuff ====== */
typedef enum
{
  POPUPMENU_CONTROLS = 0,
  POPUPMENU_TOGGLE_SCROLL,
  POPUPMENU_PRINT,
  POPUPMENU_SNAPSHOT,
  POPUPMENU_DUMP,
  POPUPMENU_DISMISS,
  POPUPMENU_QUIT,
  POPUPMENU_ITEMCOUNT
}
PopupMenuItem;

char	*PopupMenuItemStr[POPUPMENU_ITEMCOUNT] =
{
  "Controls Dialog...",
  "Toggle scroll buttons",
  "Print...",
  "Snapshot...",
  "Dump...",
  "Dismiss",
  "Quit"
};

char	PopupMenuItemMnemonic[POPUPMENU_ITEMCOUNT] =
{
  'C',
  'T',
  'P',
  'S',
  'm', /*VTR*/
  'D',
  'Q'
};

char	*PopupMenuItemAccelerator[POPUPMENU_ITEMCOUNT] =
{
  " ",
  " ",
  " ",
  " ",
  " ",
  " ",
  "Ctrl<Key>c"
};

char	*PopupMenuItemAccelStr[POPUPMENU_ITEMCOUNT] =
{
  " ",
  " ",
  " ",
  " ",
  " ",
  " ",
  "Ctrl+C"
};

/*
 * PopupMenu_build
 */
static Widget	PopupMenu_build	(Widget parent)
{
  Widget	menu;
  XmString	label[POPUPMENU_ITEMCOUNT+2];
  XmString	acc_lbl[POPUPMENU_ITEMCOUNT+2];
  KeySym        keySyms[POPUPMENU_ITEMCOUNT+2];
  XmButtonType  buttonType[POPUPMENU_ITEMCOUNT+2];
  int		i;
  Arg           args[16];
  int           n;


  for (i = 0; i < POPUPMENU_ITEMCOUNT+2; i++)
    buttonType[i] = XmPUSHBUTTON;

  buttonType[2] = buttonType[6] = XmSEPARATOR;


  for (i = 0, n = 0; i < POPUPMENU_ITEMCOUNT+2; i++)
  {
    if (buttonType[i] == XmPUSHBUTTON) {
      label[i]   = XmStringCreateLocalized (PopupMenuItemStr[n]);
      acc_lbl[i] = XmStringCreateLocalized (PopupMenuItemAccelStr[n]);
      keySyms[i] = PopupMenuItemMnemonic[n];
      n++;
    }
    else {
      label[i]   = XmStringCreateLocalized ("Separator");
      acc_lbl[i] = XmStringCreateLocalized (" ");
      keySyms[i] = ' ';
    }
  }

  n = 0;
  XtSetArg(args[n], XmNbuttonCount,           POPUPMENU_ITEMCOUNT+2); n++;
  XtSetArg(args[n], XmNbuttonType,            buttonType); n++;
  XtSetArg(args[n], XmNbuttons,               label); n++;
  XtSetArg(args[n], XmNbuttonMnemonics,       keySyms); n++;
  XtSetArg(args[n], XmNbuttonAcceleratorText, acc_lbl); n++;
  XtSetArg(args[n], XmNsimpleCallback,        PopupMenu_cb); n++;

  menu = XmCreateSimplePopupMenu(parent, "popup", args, n);
 
  for (i = 0; i < POPUPMENU_ITEMCOUNT+2; i++)
  {
    if (label[i])   XmStringFree (label[i]);
    if (acc_lbl[i]) XmStringFree (acc_lbl[i]);
  }

  return menu;
}

/*
 * PopupMenu_position
 */
static void	PopupMenu_position	(Widget 		menu,
					 XButtonPressedEvent 	*event)
{
  XmMenuPosition (menu, event);
}


/*
 * PopupMenu_popup
 */
static void	PopupMenu_popup	(Widget menu)
{
  XtManageChild (menu);
}


/*
 * PopupMenu_popdown
 */
static void	PopupMenu_popdown (Widget menu)
{
  XtUnmanageChild (menu);
}


/*
 * PopupMenu_cb
 */
static void	PopupMenu_cb	(Widget w, XtPointer client, XtPointer BOGUS(1))
{
  PopupMenuItem	item = (PopupMenuItem)client;
  StripInfo	*si;
  char		cmd_buf[256];
  pid_t		pid;

  XtVaGetValues (XtParent(w), XmNuserData, &si, NULL);

  switch (item)
  {
      case POPUPMENU_CONTROLS:
        StripDialog_popup (si->dialog);
        break;
        
      case POPUPMENU_TOGGLE_SCROLL:
        if (XtIsManaged (si->graph_panel))
        {
          XtUnmanageChild (si->graph_panel);
          XtVaSetValues (si->canvas, XmNbottomAttachment, XmATTACH_FORM, 0);
        }
        else
        {
          XtManageChild (si->graph_panel);
          XtVaSetValues
            (si->canvas,
             XmNbottomAttachment,	XmATTACH_WIDGET,
             XmNbottomWidget,		si->graph_panel,
             0);
        }
        break;
        
      case POPUPMENU_PRINT:
        /* fprintf (stdout, "Print\n");
         */
        window_map (si->display, XtWindow(si->shell));
        sprintf
          (cmd_buf,
           "xwd -id %d"
           " | xpr -device %s %s"
           " | lp -d%s -onb",
           XtWindow (si->canvas),
           si->print_info.dev_name,
           si->print_info.is_color? " " : "-gray 4",
           si->print_info.pr_name);
        
        
        if (pid = fork ())
	{
	  execl ("/bin/sh", "sh", "-c", cmd_buf, 0);
	  exit (0);
	}
        break;
        
      case POPUPMENU_SNAPSHOT:
        window_map (si->display, XtWindow(si->shell));
        sprintf
          (cmd_buf,
           "xwd -id %d | xwud -raw",
           XtWindow (si->canvas));
        if (pid = fork ())
	{
	  execl ("/bin/sh", "sh", "-c", cmd_buf, 0);
	  exit (0);
	}
        break;
        
      case POPUPMENU_DUMP:
        fsdlg_popup (si, (fsdlg_functype)Strip_dumpdata);
        break;
        
      case POPUPMENU_DISMISS:
        if (StripDialog_ismapped (si->dialog))
          window_unmap (si->display, XtWindow (si->shell));
        else MessageBox_popup
               (si->shell, &si->message_box,
                "You can't dismiss all your windows",
                "Ok");
        break;
        
      case POPUPMENU_QUIT:
        dlgrequest_quit ((void *)si, NULL);
        break;
        
      default:
        fprintf (stderr, "Argh!\n");
        exit (1);
  }
}



/* ====== ListDialog stuff ====== */
typedef enum
{
  LISTDLG_FREE = 0,
  LISTDLG_DISMISS,
  LISTDLG_MAP
}
LDevent;


/*
 * ListDialog_init
 */
static ListDialog	*ListDialog_init (Strip strip, Widget parent)
{
  ListDialog	*ld;
  XmString	msg_lbl;
  XmString	ok_lbl;
  XmString	cancel_lbl;
  int		i;

  if ((ld = (ListDialog *)malloc (sizeof (ListDialog))) != NULL)
  {
    ld->strip = strip;
    for (i = 0; i < STRIP_MAX_CURVES; i++)
      ld->curves[i] = (StripCurve)0;
      
    ld->msg_box = XmCreateMessageBox
      (XmCreateDialogShell (parent, "StripUnconnectedList", NULL, 0),
       "Unconnected Signals",
       NULL,
       0);
	 
    msg_lbl = XmStringCreateLocalized
      ("The following signals are not connected.");
    ok_lbl = XmStringCreateLocalized ("Free Selected Signals");
    cancel_lbl = XmStringCreateLocalized ("Keep Waiting");

    XtVaSetValues
      (ld->msg_box,
       XmNdialogType,			XmDIALOG_MESSAGE,
       XmNdefaultButtonType,		XmDIALOG_CANCEL_BUTTON,
       XmNnoResize,			True,
       XmNdefaultPosition,		False,
       XmNautoUnmanage,		False,
       XmNmessageString,		msg_lbl,
       XmNokLabelString,		ok_lbl,
       XmNcancelLabelString,		cancel_lbl,
       XmNuserData,			ld,
       NULL);

    XtUnmanageChild
      (XmMessageBoxGetChild (ld->msg_box, XmDIALOG_HELP_BUTTON));

    XtAddCallback
      (ld->msg_box, XmNokCallback, ListDialog_cb, (XtPointer)LISTDLG_FREE);
    XtAddCallback
      (ld->msg_box, XmNcancelCallback, ListDialog_cb,
       (XtPointer)LISTDLG_DISMISS);
    XtAddCallback
      (ld->msg_box, XmNmapCallback, ListDialog_cb, (XtPointer)LISTDLG_MAP);

    XmStringFree (msg_lbl);
    XmStringFree (ok_lbl);
    XmStringFree (cancel_lbl);

    ld->list = XmCreateScrolledList
      (ld->msg_box, "ScrolledList", NULL, 0);
    XtVaSetValues
      (ld->list,
       XmNselectionPolicy,	XmEXTENDED_SELECT,
       XmNvisibleItemCount,	STRIP_MAX_CURVES,
       NULL);
    XtManageChild (ld->list);
  }

  return ld;
}

/*
 * ListDialog_popup
 */
static void	ListDialog_popup	(ListDialog *ld)
{
  int		i, j;
  XmString	items[STRIP_MAX_CURVES];

  /* first, pack the front of the curves array so that position in the
   * Motif list corresponds to position in the array.
   */
  i = 0;
  j = 0;
  do 
  {
    /* find the first empty slot */
    for (; j < STRIP_MAX_CURVES; j++)
      if (!ld->curves[j])
      {
        for (i = j+1; i < STRIP_MAX_CURVES; i++)
          if (ld->curves[i])
          {
            ld->curves[j] = ld->curves[i];
            ld->curves[i] = (StripCurve)0;
            break;
          }
        break;
      }
  }
  while ((i < STRIP_MAX_CURVES) && (j < STRIP_MAX_CURVES));

  for (i = 0; (i < STRIP_MAX_CURVES) && ld->curves[i]; i++)
    items[i] = XmStringCreateLocalized
      ((char *)StripCurve_getattr_val (ld->curves[i], STRIPCURVE_NAME));

  XtVaSetValues
    (ld->list,
     XmNitems,		items,
     XmNitemCount,	i,
     NULL);
  
  for (i -=1; i >= 0; i--)
    XmStringFree (items[i]);

  XmListDeselectAllItems (ld->list);
  XtManageChild (ld->msg_box);
  XtPopup (XtParent (ld->msg_box), XtGrabNone);
}


/*
 * ListDialog_delete
 */
static void	ListDialog_delete	(ListDialog *ld)
{
  /*  XtDestroyWidget (ld->msg_box); */
  free (ld);
}


/*
 * ListDialog_popdown
 */
static void	ListDialog_popdown	(ListDialog *ld)
{
  XtPopdown (XtParent (ld->msg_box));
  XtUnmanageChild (ld->msg_box);
}


/*
 * ListDialog_addcurves
 */
static void	ListDialog_addcurves	(ListDialog *ld, StripCurve curves[])
{
  int		i, j, k;

  /* union {ld->curves} with {curves}; no duplicates */
  for (i = 0; curves[i]; i++)
  {
    for (j = 0, k = -1; j < STRIP_MAX_CURVES; j++)
      if (ld->curves[j] == curves[i])		/* duplicate */
      {
        k = -1;
        break;
      }
      else if (ld->curves[j] == (StripCurve)0)
        k = j;

    if (k >= 0) ld->curves[k] = curves[i];
  }

  /* if the list is mapped, then update the list contents */
  if (ListDialog_isviewable (ld))
    ListDialog_popup (ld);
}


/*
 * ListDialog_removecurves
 */
static void	ListDialog_removecurves	(ListDialog *ld, StripCurve curves[])
{
  int		i, j;

  /* subtract {curves} from {ld->curves} */
  for (i = 0; curves[i]; i++)
  {
    for (j = 0; j < STRIP_MAX_CURVES; j++)
      if (ld->curves[j] == curves[i])
        ld->curves[j] = (StripCurve)0;
  }

  /* if the list is mapped, then update the list contents */
  if (ListDialog_isviewable (ld))
    ListDialog_popup (ld);
}


/*
 * ListDialog_isviewable
 */
static int	ListDialog_isviewable	(ListDialog *ld)
{
  return
    (window_isviewable
     (XtDisplay (XtParent (ld->msg_box)), XtWindow (XtParent (ld->msg_box))));
}


/*
 * ListDialog_ismapped
 */
static int	ListDialog_ismapped	(ListDialog *ld)
{
  return
    (window_ismapped
     (XtDisplay (XtParent (ld->msg_box)), XtWindow (XtParent (ld->msg_box))));
}


/*
 * ListDialog_count
 */
static int	ListDialog_count	(ListDialog *ld)
{
  int	i, n;

  for (i = 0, n = 0; i < STRIP_MAX_CURVES; i++)
    if (ld->curves[i] != (StripCurve)0) n++;
  return n;
}


/*
 * ListDialog_cb
 */
static void	ListDialog_cb	(Widget w, XtPointer client, XtPointer BOGUS(1))
{
  ListDialog		*ld;
  StripCurve		curves[STRIP_MAX_CURVES+1];
  LDevent		event = (LDevent)client;
  int			*selected;
  int			count;
  int			i;
  Window		root, child;
  int			root_x, root_y;
  int			win_x, win_y;
  unsigned		mask;
  int			screen;
  Display		*display;
  XWindowAttributes	xwa;
  Dimension		width, height;

  XtVaGetValues (w, XmNuserData, &ld, NULL);
  
  switch (event)
  {
      case LISTDLG_FREE:
      
        if (XmListGetSelectedPos (ld->list, &selected, &count) != False)
	{
	  XtVaGetValues (w, XmNuserData, &ld, NULL);
	  for (i = 0; i < count; i++)
	    curves[i] = ld->curves[selected[i]-1];
	  curves[i] = (StripCurve)0;
	  Strip_freesomecurves (ld->strip, curves);
	}
        break;

      
      case LISTDLG_DISMISS:
      
        break;

      
      case LISTDLG_MAP:
      
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
  }
}


static void	fsdlg_popup	(StripInfo *si, fsdlg_functype func)
{
  if (!si->fs_dlg)
  {
    si->fs_dlg = XmCreateFileSelectionDialog
      (si->shell, "Data dump file", NULL, 0);
    XtVaSetValues (si->fs_dlg, XmNfileTypeMask, XmFILE_REGULAR, NULL);
    XtAddCallback (si->fs_dlg, XmNokCallback, fsdlg_cb, si);
    XtAddCallback (si->fs_dlg, XmNcancelCallback, fsdlg_cb, NULL);
  }
  XtVaSetValues (si->fs_dlg, XmNuserData, func, NULL);
  XtManageChild (si->fs_dlg);
}


static void	fsdlg_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmFileSelectionBoxCallbackStruct *cbs =
    (XmFileSelectionBoxCallbackStruct *) call;
  
  StripInfo		*si = (StripInfo *)data;
  fsdlg_functype	func;
  char			*fname;

  XtUnmanageChild (w);

  if (si)
    if (XmStringGetLtoR (cbs->value, XmFONTLIST_DEFAULT_TAG, &fname))
      if (fname != NULL)
      {
        XtVaGetValues (w, XmNuserData, &func, NULL);
        func ((Strip)si, fname);
      }
}
