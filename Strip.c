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
#include "refresh.bm"

#ifdef USE_XPM
#  include <X11/xpm.h>          
#  include "StripGraphIcon.xpm"         
#  include "StripDialogIcon.xpm"        
#else
#  include "StripGraphIcon.bm"
#  include "StripDialogIcon.bm"
#endif

#ifdef USE_XMU
#  include <X11/Xmu/Editres.h>
#endif

#ifdef USE_CLUES
#include "LiteClue.h"
#endif

#include "ComboBox.h"

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
#include <Xm/TextF.h>
#include <Xm/FileSB.h>
#include <Xm/RowColumn.h>
#include <Xm/DragDrop.h>
#include <Xm/Protocols.h>
#include <X11/Xlib.h>

#if defined(HP_UX) || defined(SOLARIS) || defined(linux)
#  include <unistd.h>
#else
#  include <vfork.h>
#endif

#ifndef MAX_PATH
#  define MAX_PATH      1024
#endif

#include <math.h>
#include <string.h>

#define DEF_WOFFSET             3
#define STRIP_MAX_FDS           64


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
  STRIPEVENTMASK_SAMPLE         = (1 << STRIPEVENT_SAMPLE),
  STRIPEVENTMASK_REFRESH        = (1 << STRIPEVENT_REFRESH),
  STRIPEVENTMASK_CHECK_CONNECT  = (1 << STRIPEVENT_CHECK_CONNECT)
} StripEventMask;

static char     *StripEventTypeStr[LAST_STRIPEVENT] =
{
  "Sample",
  "Refresh",
  "Check Connect"
};

/* ====== PrintInfo stuff ====== */
typedef struct _PrintInfo
{
  char  printer[128];
  char  device[128];
}
PrintInfo;

typedef struct _PrinterDialog
{
  Widget        msgbox;
  Widget        name_textf, device_combo;
}
PrinterDialog;

/* ====== Strip stuff ====== */
typedef enum
{
  STRIPSTAT_OK                  = (1 << 0),
  STRIPSTAT_PROCESSING          = (1 << 1),
  STRIPSTAT_TERMINATED          = (1 << 2),
  STRIPSTAT_BROWSE_MODE         = (1 << 3)
} StripStatus;

enum
{
  STRIPBTN_LEFT = 0,
  STRIPBTN_RIGHT,
  STRIPBTN_ZOOMIN,
  STRIPBTN_ZOOMOUT,
  STRIPBTN_AUTOSCROLL,
  STRIPBTN_REFRESH,
  STRIPBTN_COUNT
};
  
typedef struct _stripFdInfo
{
  XtInputId             id;
  int                   fd;
}
stripFdInfo;
  
typedef struct _StripInfo
{
  /* == X Stuff == */
  XtAppContext          app;
  Display               *display;
  Widget                toplevel, shell, canvas;
  Widget                graph_form, graph_panel;
  Widget                btn[STRIPBTN_COUNT];
  Widget                popup_menu, message_box;
  Widget                fs_dlg;
  char                  app_name[128];

  /* == file descriptor management ==  */
  stripFdInfo           fdinfo[STRIP_MAX_FDS];

  /* == Strip Components == */
  StripConfig           *config;
  StripDialog           dialog;
  StripCurveInfo        curves[STRIP_MAX_CURVES];
  StripDataSource       data;
  StripHistory          history;
  StripGraph            graph;
  unsigned              status;
  PrintInfo             print_info;
  PrinterDialog         *pd;

  /* == Callback stuff == */
  StripCallback         connect_func, disconnect_func, client_io_func;
  void                  *connect_data, *disconnect_data, *client_io_data;
  int                   client_registered;
  int                   grab_count;

  /* timing and event stuff */
  unsigned              event_mask;
  struct timeval        last_event[LAST_STRIPEVENT];
  struct timeval        next_event[LAST_STRIPEVENT];

  XtIntervalId          tid;
}
StripInfo;


/* ====== Miscellaneous stuff ====== */
typedef enum
{
  STRIPWINDOW_GRAPH = 0,
  STRIPWINDOW_COUNT
}
StripWindowType;

static char     *StripWindowTypeStr[STRIPWINDOW_COUNT] =
{
  "Graph",
};

typedef void    (*fsdlg_functype)       (Strip, char *);

/* ====== Static Data ====== */
static struct timeval   tv;


/* ====== Static Function Prototypes ====== */
static void     Strip_watchevent        (StripInfo *, unsigned);
static void     Strip_ignoreevent       (StripInfo *, unsigned);

static void     Strip_dispatch          (StripInfo *);
static void     Strip_eventmgr          (XtPointer, XtIntervalId *);

static void     Strip_child_msg         (void *);
static void     Strip_config_callback   (StripConfigMask, void *);

static void     Strip_forgetcurve       (StripInfo *, StripCurve);

static void     Strip_graphdrop_handle  (Widget, XtPointer, XtPointer);
static void     Strip_graphdrop_xfer    (Widget, XtPointer, Atom *, Atom *,
                                         XtPointer, unsigned long *, int *);

static void     Strip_printer_init      (StripInfo *);

static void     callback                (Widget, XtPointer, XtPointer);
static int      X_error_handler         (Display *, XErrorEvent *);


static void     dlgrequest_connect      (void *, void *);
static void     dlgrequest_show         (void *, void *);
static void     dlgrequest_clear        (void *, void *);
static void     dlgrequest_delete       (void *, void *);
static void     dlgrequest_dismiss      (void *, void *);
static void     dlgrequest_quit         (void *, void *);
static void     dlgrequest_window_popup (void *, void *);

static Widget   PopupMenu_build         (Widget);
static void     PopupMenu_position      (Widget, XButtonPressedEvent *);
static void     PopupMenu_popup         (Widget);
static void     PopupMenu_popdown       (Widget);
static void     PopupMenu_cb            (Widget, XtPointer, XtPointer);

static PrinterDialog    *PrinterDialog_build    (Widget);
static void     PrinterDialog_popup     (PrinterDialog *, StripInfo *);
static void     PrinterDialog_cb        (Widget, XtPointer, XtPointer);

static void     fsdlg_popup             (StripInfo *, fsdlg_functype);
static void     fsdlg_cb                (Widget, XtPointer, XtPointer);

/* ====== Functions ====== */
Strip   Strip_init      (int    *argc,
                         char   *argv[],
                         FILE   *logfile)
{
  StripInfo             *si;
  Dimension             width, height;
  XSetWindowAttributes  xswa;
  XVisualInfo           xvi;
  Widget                form, rowcol, hintshell, w;
  Pixmap                pixmap;
  Pixel                 fg, bg;
  SDWindowMenuItem      wm_items[STRIPWINDOW_COUNT];
  cColorManager         scm;
  int                   i, n;
  Arg                   args[10];
  Atom                  import_list[10];
  int                   stat = 1;
  double                tmp_dbl;
  StripConfigMask       scfg_mask;
  Atom                  WM_DELETE_WINDOW;
  XrmDatabase           db, db_fall, db_site, db_user;
  char                  *env;
  char                  path[MAX_PATH];
  char                  **pstr;
  struct passwd         user;
  char                  *a, *b;

  StripConfig_preinit();
  StripConfigMask_clear (&scfg_mask);

  if ((si = (StripInfo *)malloc (sizeof (StripInfo))) != NULL)
  {
    /* initialize the X-toolkit */
    XtSetLanguageProc (0, 0, 0);
    XtToolkitInitialize();

    /* create the app context and open the display */
    si->app = XtCreateApplicationContext();
    si->display = XtOpenDisplay
      (si->app, 0, 0, STRIP_APP_CLASS, 0, 0, argc, argv);
    if (!si->display) {
      free (si);
      return 0;
    }

    /* create a resource database from the fallbacks, then merge in
     * the site defaults, followed by the user defaults
     */

    /* current resource database */
    db = XrmGetDatabase (si->display);

    /* build database from fallback specs */
    db_fall = (XrmDatabase)0;
    pstr = fallback_resources;
    while (*pstr) XrmPutLineResource (&db_fall, *pstr++);

    /* merge into current database without overriding */
    XrmCombineDatabase (db_fall, &db, False);

    /* build site default resource database */
    env = getenv (STRIP_SITE_DEFAULTS_FILE_ENV);
    if (!env) env = STRIP_SITE_DEFAULTS_FILE;
    db_site = XrmGetFileDatabase (env);

    /* merge into current resource database, overrinding */
    if (db_site) XrmCombineDatabase (db_site, &db, True);

    /* build user default resource database
     */

    /* first, get path of user's home directory and append the
     * resource file name to that */
    memcpy (&user, getpwuid (getuid()), sizeof (struct passwd));
    a = user.pw_dir;
    b = path;
    while (*a) *b++ = *a++;
    *b++ = '/';
    a = STRIP_USER_DEFAULTS_FILE;
    while (*a) *b++ = *a++;
    *b = 0;

    /* build the database */
    db_user = XrmGetFileDatabase (path);

    /* merge into current resource database, overrinding */
    if (db_user) XrmCombineDatabase (db_user, &db, True);
      
    
    /* create the top-level shell, but don't realize it until the
     * appropriate visual has been chosen.
     */
    si->toplevel = XtVaAppCreateShell
      (0, STRIP_APP_CLASS, applicationShellWidgetClass, si->display,
       XmNmappedWhenManaged, False,
       0);

#ifdef USE_XMU
    /* editres support */
    XtAddEventHandler
      (si->toplevel, (EventMask)0, True, _XEditResCheckMessages, 0);
#endif

    /* replace default error handler so that we don't exit on
     * non-fatal exceptions */
    XSetErrorHandler (X_error_handler);

    
    /* initialize the color manager */
    scm = cColorManager_init (si->display, /*STRIPCONFIG_NUMCOLORS*/ 0);
    if (scm)
    {
      xvi = *cColorManager_get_visinfo (scm);
      XtVaSetValues
        (si->toplevel,
         XmNvisual,     xvi.visual,
         XmNcolormap,   cColorManager_getcmap (scm),
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
    si->config->user = user;

    /* build the other shells now that the toplevel
     * has been taken care of.
     */
#ifdef USE_CLUES
    hintshell = XtVaCreatePopupShell
      ("hintShell", xcgLiteClueWidgetClass, si->toplevel, 0);
#endif

    si->shell = XtVaCreatePopupShell
      ("StripGraph",
       topLevelShellWidgetClass,        si->toplevel,
       XmNdeleteResponse,               XmDO_NOTHING,
       XmNmappedWhenManaged,            False,
       XmNvisual,                       si->config->xvi.visual,
       XmNcolormap,                     cColorManager_getcmap (scm),
       NULL);

#ifdef USE_XMU
    /* editres support */
    XtAddEventHandler
      (si->shell, (EventMask)0, True, _XEditResCheckMessages, 0);
#endif


    /* hook window manager delete message so we can shut down gracefully */
    WM_DELETE_WINDOW = XmInternAtom (si->display, "WM_DELETE_WINDOW", False);
    XmAddWMProtocolCallback (si->shell, WM_DELETE_WINDOW, callback, si);
    
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
       XmNwidth,        width,
       XmNheight,       height,
       NULL);
    XtRealizeWidget (si->shell);
    
    form = XtVaCreateManagedWidget
      ("graphBaseForm",
       xmFormWidgetClass,       si->shell,
       0);

    /* the the motif foreground and background colors */
    XtVaGetValues
      (form,
       XmNforeground,           &fg,
       XmNbackground,           &bg,
       0);

    /* the graph control panel */
    si->graph_panel = XtVaCreateManagedWidget
      ("graphPanel",
       xmFrameWidgetClass,              form,
       XmNtopAttachment,                XmATTACH_NONE,
       XmNleftAttachment,               XmATTACH_FORM,
       XmNrightAttachment,              XmATTACH_FORM,
       XmNbottomAttachment,             XmATTACH_FORM,
       0);
    rowcol = XtVaCreateManagedWidget
      ("controlsRowColumn",
       xmRowColumnWidgetClass,  si->graph_panel,
       XmNorientation,          XmHORIZONTAL,
       0);
      
    /* pan left button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)pan_left_bits,
       pan_left_width, pan_left_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_LEFT] = XtVaCreateManagedWidget
      ("panLeftButton",
       xmPushButtonWidgetClass, rowcol,
       XmNlabelType,            XmPIXMAP,
       XmNlabelPixmap,          pixmap,
       0);

    /* pan right button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)pan_right_bits, pan_right_width, pan_right_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_RIGHT] = XtVaCreateManagedWidget
      ("panRightButton",
       xmPushButtonWidgetClass, rowcol,
       XmNlabelType,            XmPIXMAP,
       XmNlabelPixmap,          pixmap,
       0);
      
    XtVaCreateManagedWidget
      ("separator",
       xmSeparatorWidgetClass,  rowcol,
       XmNorientation,          XmVERTICAL,
       0);

    /* zoom in button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)zoom_in_bits, zoom_in_width, zoom_in_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_ZOOMIN] = XtVaCreateManagedWidget
      ("zoomInButton",
       xmPushButtonWidgetClass, rowcol,
       XmNlabelType,            XmPIXMAP,
       XmNlabelPixmap,          pixmap,
       0);

    /* zoom out button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)zoom_out_bits, zoom_out_width, zoom_out_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_ZOOMOUT] = XtVaCreateManagedWidget
      ("zoomOutButton",
       xmPushButtonWidgetClass, rowcol,
       XmNlabelType,            XmPIXMAP,
       XmNlabelPixmap,          pixmap,
       0);
      
    XtVaCreateManagedWidget
      ("separator",
       xmSeparatorWidgetClass,  rowcol,
       XmNorientation,          XmVERTICAL,
       0);

    /* auto scroll button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)auto_scroll_bits, auto_scroll_width, auto_scroll_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_AUTOSCROLL] = XtVaCreateManagedWidget
      ("autoScrollButton",
       xmPushButtonWidgetClass, rowcol,
       XmNlabelType,            XmPIXMAP,
       XmNlabelPixmap,          pixmap,
       0);

    /* refresh button */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)refresh_bits, refresh_width, refresh_height, fg, bg,
       xvi.depth);
    si->btn[STRIPBTN_REFRESH] = XtVaCreateManagedWidget
      ("refreshButton",
       xmPushButtonWidgetClass, rowcol,
       XmNlabelType,            XmPIXMAP,
       XmNlabelPixmap,          pixmap,
       0);

    for (i = 0; i < STRIPBTN_COUNT; i++)
      XtAddCallback (si->btn[i], XmNactivateCallback, callback, si);

#ifdef USE_CLUES
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_LEFT], "Pan left", 0, 0);
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_RIGHT], "Pan right", 0, 0);
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_ZOOMIN], "Zoom in", 0, 0);
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_ZOOMOUT], "Zoom out", 0, 0);
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_AUTOSCROLL], "Auto scroll", 0, 0);
    XcgLiteClueAddWidget
      (hintshell, si->btn[STRIPBTN_REFRESH], "Refresh", 0, 0);
#endif

    /* the graph base widget */
    si->graph_form = XtVaCreateManagedWidget
      ("graphForm",
       xmFormWidgetClass,       form,
       XmNtopAttachment,        XmATTACH_FORM,
       XmNleftAttachment,       XmATTACH_FORM,
       XmNrightAttachment,      XmATTACH_FORM,
       XmNbottomAttachment,     XmATTACH_WIDGET,
       XmNbottomWidget,         si->graph_panel,
       XmNbackground,           si->config->Color.background.xcolor.pixel,
       NULL);

    si->graph = StripGraph_init (si->graph_form, si->config);
    StripGraph_getattr (si->graph, STRIPGRAPH_WIDGET, &si->canvas, 0);
       
    /* register the drawing area as a drop site accepting compound text
     * and string type data.  Note that, since there is no way to
     * register client data fo the callback, we need to store the
     * StripInfo pointer as the widget's user data */
    i = 0; n = 0;
    import_list[i++] = XmInternAtom (si->display, "COMPOUND_TEXT", False);
    XtSetArg (args[n], XmNimportTargets, import_list); n++;
    XtSetArg (args[n], XmNnumImportTargets, i); n++;
    XtSetArg (args[n], XmNdropSiteOperations, XmDROP_COPY); n++;
    XtSetArg (args[n], XmNdropProc, Strip_graphdrop_handle); n++;
    XmDropSiteRegister (si->canvas, args, n);
    XtVaSetValues (si->canvas, XmNuserData, (XtPointer)si, 0);
      

    /* popup menu, etc. */
    si->popup_menu = PopupMenu_build (si->canvas);
    si->message_box = (Widget)0;
    XtVaSetValues (si->popup_menu, XmNuserData, si, NULL);

    si->fs_dlg = 0;

    for (i = 0; i < STRIPWINDOW_COUNT; i++)
    {
      strcpy (wm_items[i].name, StripWindowTypeStr[i]);
      wm_items[i].window_id = (void *)i;
      wm_items[i].cb_func = dlgrequest_window_popup;
      wm_items[i].cb_data = (void *)si;
    }
          
    si->dialog  = StripDialog_init (si->toplevel, si->config);
    StripDialog_setattr
      (si->dialog,
       STRIPDIALOG_CONNECT_FUNC,        dlgrequest_connect,
       STRIPDIALOG_CONNECT_DATA,        si,
       STRIPDIALOG_SHOW_FUNC,           dlgrequest_show,
       STRIPDIALOG_SHOW_DATA,           si,
       STRIPDIALOG_CLEAR_FUNC,          dlgrequest_clear,
       STRIPDIALOG_CLEAR_DATA,          si,
       STRIPDIALOG_DELETE_FUNC,         dlgrequest_delete,
       STRIPDIALOG_DELETE_DATA,         si,
       STRIPDIALOG_DISMISS_FUNC,        dlgrequest_dismiss,
       STRIPDIALOG_DISMISS_DATA,        si,
       STRIPDIALOG_QUIT_FUNC,           dlgrequest_quit,
       STRIPDIALOG_QUIT_DATA,           si,
       STRIPDIALOG_WINDOW_MENU, wm_items, STRIPWINDOW_COUNT,
       0);

    si->history = StripHistory_init ((Strip)si);
    si->data = StripDataSource_init (si->history);
    StripDataSource_setattr
      (si->data, SDS_NUMSAMPLES, (size_t)si->config->Time.num_samples, 0);

    StripGraph_setattr (si->graph, STRIPGRAPH_DATA_SOURCE, si->data, 0);

    si->connect_func = si->disconnect_func = si->client_io_func = NULL;
    si->connect_data = si->disconnect_data = si->client_io_data = NULL;

    si->tid = (XtIntervalId)0;

    si->event_mask = 0;
    for (i = 0; i < LAST_STRIPEVENT; i++)
    {
      si->last_event[i].tv_sec  = 0;
      si->last_event[i].tv_usec = 0;
      si->next_event[i].tv_sec  = 0;
      si->next_event[i].tv_usec = 0;
    }

    for (i = 0; i < STRIP_MAX_CURVES; i++)
    {
      si->curves[i].scfg                        = si->config;
      si->curves[i].details                     = NULL;
      si->curves[i].func_data                   = NULL;
      si->curves[i].get_value                   = NULL;
      si->curves[i].connect_request.tv_sec      = 0;
      si->curves[i].id                          = NULL;
      si->curves[i].status                      = 0;
    }

    /* load the icon pixmap */
    StripDialog_getattr (si->dialog, STRIPDIALOG_SHELL_WIDGET, &w, 0);
    
#ifdef USE_XPM
    /* graph icon */
    if (XpmSuccess == XpmCreatePixmapFromData 
        (si->display, RootWindow (si->display, xvi.screen),
         StripGraphIcon_xpm, &pixmap, 0, 0))
      XtVaSetValues (si->shell, XtNiconPixmap, pixmap, NULL);

    /* dialog icon */
    if (XpmSuccess == XpmCreatePixmapFromData 
        (si->display, RootWindow (si->display, xvi.screen),
         StripDialogIcon_xpm, &pixmap, 0, 0))
      XtVaSetValues (w, XtNiconPixmap, pixmap, NULL);
#else
    /* graph icon */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)StripGraphIcon_bm_bits,
       StripGraphIcon_bm_width, StripGraphIcon_bm_height,
       fg, bg, xvi.depth);
    if (pixmap)
      XtVaSetValues(si->shell, XtNiconPixmap, pixmap, NULL);

    /* dialog icon */
    pixmap = XCreatePixmapFromBitmapData
      (si->display, RootWindow (si->display, xvi.screen),
       (char *)StripDialogIcon_bm_bits,
       StripDialogIcon_bm_width, StripDialogIcon_bm_height,
       fg, bg, xvi.depth);
    if (pixmap)
      XtVaSetValues(w, XtNiconPixmap, pixmap, NULL);
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
    XtAddCallback (si->canvas, XmNinputCallback, callback, si);
    XUnmapWindow (si->display, XtWindow (si->shell));

    si->status = STRIPSTAT_OK;
    memset (si->fdinfo, 0, STRIP_MAX_FDS * sizeof (stripFdInfo));

    Strip_printer_init (si);
    si->pd = PrinterDialog_build (si->shell);
  }

  return (Strip)si;
}


/*
 * Strip_delete
 */
void    Strip_delete    (Strip the_strip)
{
  StripInfo     *si = (StripInfo *)the_strip;
  Display       *disp = si->display;

  StripGraph_delete             (si->graph);
  StripDataSource_delete        (si->data);
  StripHistory_delete           (si->history);
  StripDialog_delete            (si->dialog);
  StripConfig_delete            (si->config);

  XtDestroyWidget (si->toplevel);
  free (si);
}


/*
 * Strip_setattr
 */
int     Strip_setattr   (Strip the_strip, ...)
{
  va_list       ap;
  StripInfo     *si = (StripInfo *)the_strip;
  int           attrib;
  int           ret_val = 1;


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
int     Strip_getattr   (Strip the_strip, ...)
{
  va_list       ap;
  StripInfo     *si = (StripInfo *)the_strip;
  int           attrib;
  int           ret_val = 1;


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
int     Strip_addfd     (Strip                  the_strip,
                         int                    fd,
                         XtInputCallbackProc    func,
                         XtPointer              data)
{
  StripInfo     *si = (StripInfo *)the_strip;
  int           i;

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
XtIntervalId    Strip_addtimeout        (Strip                  the_strip,
                                         double                 sec,
                                         XtTimerCallbackProc    cb_func,
                                         XtPointer              cb_data)
{
  StripInfo             *si = (StripInfo *)the_strip;

  return XtAppAddTimeOut
    (si->app, (unsigned long)(sec * 1000), cb_func, cb_data);
}


/*
 * Strip_clearfd
 */
int     Strip_clearfd   (Strip the_strip, int fd)
{
  StripInfo     *si = (StripInfo *)the_strip;
  int           i;

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
void    Strip_go        (Strip the_strip)
{
  StripInfo     *si = (StripInfo *)the_strip;
  int           some_curves_connected = 0;
  int           some_curves_waiting = 0;
  int           i;
  XEvent        xevent;

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
StripCurve      Strip_getcurve  (Strip the_strip)
{
  StripInfo     *si = (StripInfo *)the_strip;
  StripCurve    ret_val = NULL;
  int           i, j;

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    if (!si->curves[i].details)
      break;

  if (i < STRIP_MAX_CURVES)
  {
    /* now find an available StripCurveDetail in the StripConfig */
    for (j = 0; j < STRIP_MAX_CURVES; j++)
      if (si->config->Curves.Detail[j].id == NULL)
        break;

    if (j < STRIP_MAX_CURVES)
    {
      si->curves[i].details = &si->config->Curves.Detail[j];
      si->curves[i].details->id = &si->curves[i];
      ret_val = (StripCurve)&si->curves[i];
    }
  }

  return ret_val;
}


/*
 * Strip_freecurve
 */
void    Strip_freecurve (Strip the_strip, StripCurve the_curve)
{
  StripInfo             *si = (StripInfo *)the_strip;
  StripCurveInfo        *sci = (StripCurveInfo *)the_curve;
  StripCurve            curve[2];
  int                   i;

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
void    Strip_freesomecurves    (Strip the_strip, StripCurve curves[])
{
  StripInfo             *si = (StripInfo *)the_strip;
  int                   i;

  for (i = 0; curves[i]; i++)
  {
    StripGraph_removecurve (si->graph, curves[i]);
    StripDataSource_removecurve (si->data, curves[i]);
    if (si->disconnect_func != NULL)
      si->disconnect_func (curves[i], si->disconnect_data);
  }

  if (i > 0)
  {
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
int     Strip_connectcurve      (Strip the_strip, StripCurve the_curve)
{
  StripInfo             *si = (StripInfo *)the_strip;
  StripCurveInfo        *sci = (StripCurveInfo *)the_curve;
  StripCurve            curve[2];
  int                   ret_val;

  if (si->connect_func != NULL)
  {
#if 0
    StripCurve_setstat
      (the_curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
#else
    StripCurve_setstat (the_curve, STRIPCURVE_WAITING);
#endif
    get_current_time (&sci->connect_request);
      
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
void    Strip_setconnected      (Strip the_strip, StripCurve the_curve)
{
  StripInfo             *si = (StripInfo *)the_strip;
  StripCurveInfo        *sci = (StripCurveInfo *)the_curve;
  StripCurve            curve[2];

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

  StripDialog_update_curvestat (si->dialog, the_curve);
}


/*
 * Strip_setwaiting
 */
void    Strip_setwaiting        (Strip the_strip, StripCurve the_curve)
{
  StripInfo             *si = (StripInfo *)the_strip;
  StripCurveInfo        *sci = (StripCurveInfo *)the_curve;
  StripCurve            curve[2];

#if 0
  StripCurve_setstat
    (the_curve, STRIPCURVE_WAITING | STRIPCURVE_CHECK_CONNECT);
#else
  StripCurve_setstat (the_curve, STRIPCURVE_WAITING);
  Strip_watchevent (si, STRIPEVENTMASK_CHECK_CONNECT);
#endif
  
  StripDialog_update_curvestat (si->dialog, the_curve);
}


/*
 * Strip_clear
 */
void    Strip_clear     (Strip the_strip)
{
  StripInfo             *si = (StripInfo *)the_strip;
  StripCurve            curves[STRIP_MAX_CURVES+1];
  int                   i, j;
  StripConfigMask       mask;

  Strip_ignoreevent
    (si,
     STRIPEVENTMASK_SAMPLE |
     STRIPEVENTMASK_REFRESH |
     STRIPEVENTMASK_CHECK_CONNECT);
  
  for (i = 0, j = 0; i < STRIP_MAX_CURVES; i++)
    if (si->curves[i].details)
    {
      curves[j] = (StripCurve)(&si->curves[i]);
      j++;
    }

  curves[j] = (StripCurve)0;
  Strip_freesomecurves ((Strip)si, curves);

  StripConfig_setattr (si->config, STRIPCONFIG_TITLE, 0, 0);
  StripConfig_setattr (si->config, STRIPCONFIG_FILENAME, 0, 0);
  StripConfigMask_clear (&mask);
  StripConfigMask_set (&mask, SCFGMASK_TITLE);
  StripConfigMask_set (&mask, SCFGMASK_FILENAME);
  StripConfig_update (si->config, mask);
}


/*
 * Strip_dump
 */
int     Strip_dumpdata  (Strip the_strip, char *fname)
{
  StripInfo             *si = (StripInfo *)the_strip;
  FILE                  *f;
  int                   ret_val = 0;

  if (ret_val = ((f = fopen (fname, "w")) != NULL))
    ret_val = StripGraph_dumpdata (si->graph, f);
  return ret_val;
}


/*
 * Strip_writeconfig
 */
int     Strip_writeconfig       (Strip                  the_strip,
                                 FILE                   *f,
                                 StripConfigMask        m,
                                 char                   *fname)
{
  StripInfo     *si = (StripInfo *)the_strip;
  int           ret_val;

  if (ret_val = StripConfig_write (si->config, f, m))
  {
    StripConfig_setattr (si->config, STRIPCONFIG_FILENAME, fname, 0);
    StripConfig_setattr (si->config, STRIPCONFIG_TITLE, basename (fname), 0);
    StripConfigMask_set (&m, SCFGMASK_FILENAME);
    StripConfigMask_set (&m, SCFGMASK_TITLE);
    StripConfig_update (si->config, m);
  }
  return ret_val;
}

/*
 * Strip_readconfig
 */
int     Strip_readconfig        (Strip                  the_strip,
                                 FILE                   *f, 
                                 StripConfigMask        m,
                                 char                   *fname)
{
  StripInfo     *si = (StripInfo *)the_strip;
  int           ret_val;
  
  if (ret_val = StripConfig_load (si->config, f, m))
  {
    StripConfig_setattr (si->config, STRIPCONFIG_FILENAME, fname, 0);
    StripConfig_setattr (si->config, STRIPCONFIG_TITLE, basename (fname), 0);
    StripConfigMask_set (&m, SCFGMASK_FILENAME);
    StripConfigMask_set (&m, SCFGMASK_TITLE);
    StripConfig_update (si->config, m);
  }
  return ret_val;
}



/* ====== Static Functions ====== */

/*
 * Strip_forgetcurve
 */
static void     Strip_forgetcurve       (StripInfo      *si,
                                         StripCurve     the_curve)
{
  StripCurveInfo        *sci = (StripCurveInfo *)the_curve;

  StripCurve_clearstat
    (the_curve,
     STRIPCURVE_EGU_SET | STRIPCURVE_COMMENT_SET | STRIPCURVE_PRECISION_SET |
     STRIPCURVE_MIN_SET | STRIPCURVE_MAX_SET | STRIPCURVE_SCALE_SET);
  StripConfig_reset_details (si->config, sci->details);
  sci->details = 0;
  sci->get_value = 0;
  sci->func_data = 0;
}


/*
 * Strip_graphdrop_handle
 *
 *      Handles drop events on the graph drawing area widget.
 */
static void     Strip_graphdrop_handle  (Widget         w,
                                         XtPointer      BOGUS(1),
                                         XtPointer      call)
{
  StripInfo                     *si;
  Display                       *dpy;
  Widget                        dc;
  XmDropProcCallback            drop_data;
  XmDropTransferEntryRec        xfer_entries[10];
  XmDropTransferEntry           xfer_list;
  Cardinal                      n_export_targets;
  Atom                          *export_targets;
  Atom                          COMPOUND_TEXT;
  Arg                           args[10];
  int                           n, i;
  Boolean                       ok;

  dpy = XtDisplay (w);

  XtVaGetValues (w, XmNuserData, &si, 0);
  drop_data = (XmDropProcCallback) call;
  dc = drop_data->dragContext;

  /* retrieve the data targets, and search for COMPOUND_TEXT */
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
 *      Handles the transfer of data initiated by the drop handler
 *      routine.
 */
static void     Strip_graphdrop_xfer    (Widget         BOGUS(w),
                                         XtPointer      client,
                                         Atom           *BOGUS(sel_type),
                                         Atom           *type,
                                         XtPointer      value,
                                         unsigned long  *BOGUS(length),
                                         int            *BOGUS(format))
{
  StripInfo     *si = (StripInfo *)client;
  StripCurve    curve;
  Atom          COMPOUND_TEXT;
  XmString      xstr;
  char          *name;

  COMPOUND_TEXT = XmInternAtom (si->display, "COMPOUND_TEXT", False);
  if (*type == COMPOUND_TEXT)
  {
    xstr = XmCvtCTToXmString ((char *)value);
    XmStringGetLtoR (xstr, XmFONTLIST_DEFAULT_TAG, &name);

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
static void     Strip_config_callback   (StripConfigMask mask, void *data)
{
  StripInfo             *si = (StripInfo *)data;
  StripCurveInfo        *sci;
  char                  str_buf[512];
  Widget                w_dlg;

  struct                _dcon
  {
    StripCurve          curves[STRIP_MAX_CURVES+1];
    int                 n;
  } dcon;
  
  struct                _conn
  {
    StripCurve          curves[STRIP_MAX_CURVES+1];
    int                 n;
  } conn;
  
  unsigned      comp_mask = 0;
  int           i, j;
  double        tmp_dbl;


  if (StripConfigMask_stat (&mask, SCFGMASK_TITLE))
  {
    StripGraph_setattr
      (si->graph, STRIPGRAPH_HEADING, si->config->title, 0);
    comp_mask |= SGCOMPMASK_TITLE;

    StripDialog_getattr (si->dialog, STRIPDIALOG_SHELL_WIDGET, &w_dlg, 0);

    /* update the window and icon titles */
    if (si->config->title)
    {
      sprintf (str_buf, "%s Graph", si->config->title);
      XtVaSetValues
        (si->shell,
         XmNtitle,      str_buf,
         XmNiconName,   si->config->title,
         0);
      sprintf (str_buf, "%s Controls", si->config->title);
      XtVaSetValues
        (w_dlg,
         XmNtitle,      str_buf,
         XmNiconName,   si->config->title,
         0);
    }
    else
    {
      XtVaSetValues
        (si->shell,
         XmNtitle,      STRIPGRAPH_TITLE,
         XmNiconName,   STRIPGRAPH_ICON_NAME,
         0);
      XtVaSetValues
        (w_dlg,
         XmNtitle,      STRIPDIALOG_TITLE,
         XmNiconName,   STRIPDIALOG_ICON_NAME,
         0);
    }
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
     *          to redraw by setting the component mask.
     */
    if (StripConfigMask_stat (&mask, SCFGMASK_TIME_TIMESPAN))
      if (si->status & STRIPSTAT_BROWSE_MODE)
      {
        struct timeval  t0, t1;
          
        StripGraph_getattr (si->graph, STRIPGRAPH_END_TIME, &t1, 0);
        dbl2time (&tv, si->config->Time.timespan);
        subtract_times (&t0, &tv, &t1);
          
        StripGraph_setattr
          (si->graph,
           STRIPGRAPH_BEGIN_TIME,       &t0,
           STRIPGRAPH_END_TIME,         &t1,
           0);
        comp_mask |=  SGCOMPMASK_DATA | SGCOMPMASK_XAXIS;
      }
      else si->last_event[STRIPEVENT_REFRESH].tv_sec = 0;

    if (StripConfigMask_stat (&mask, SCFGMASK_TIME_NUM_SAMPLES))
    {
      StripDataSource_setattr
        (si->data, SDS_NUMSAMPLES, (size_t)si->config->Time.num_samples, 0);
    }
      
    Strip_dispatch (si);
  }


  /* colors */
  if (StripConfigMask_intersect (&mask, &SCFGMASK_COLOR))
  {
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_BACKGROUND))
    {
      /* set the canvas' background color so that when an expose
       * occurs we won't see ugly colors */
      XtVaSetValues
        (si->graph_form,
         XmNbackground, si->config->Color.background.xcolor.pixel,
         0);
      XtVaSetValues
        (si->canvas,
         XmNbackground, si->config->Color.background.xcolor.pixel,
         0);
      
      /* have to redraw everything when the background color changes */
      comp_mask |= SGCOMPMASK_ALL;
      StripGraph_setstat
        (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_FOREGROUND))
    {
      comp_mask |= SGCOMPMASK_YAXIS | SGCOMPMASK_XAXIS | SGCOMPMASK_LEGEND;
      StripGraph_setstat (si->graph, SGSTAT_LEGEND_REFRESH);
    }
    if (StripConfigMask_stat (&mask, SCFGMASK_COLOR_GRID))
    {
      comp_mask |= SGCOMPMASK_GRID;
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
    if (StripConfigMask_stat (&mask, SCFGMASK_OPTION_GRAPH_LINEWIDTH))
    {
      comp_mask |= SGCOMPMASK_DATA;
      StripGraph_setstat (si->graph, SGSTAT_GRAPH_REFRESH);
    }
  }

  if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_NAME))
  {
    dcon.n = conn.n = 0;
    for (i = 0; i < STRIP_MAX_CURVES; i++)
    {
      if (StripConfigMask_stat
          (&si->config->Curves.Detail[i].update_mask,
           SCFGMASK_CURVE_NAME))
      {
        if ((sci = (StripCurveInfo *)si->config->Curves.Detail[i].id)
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
            if (!si->curves[j].details)
            {
              sci = &si->curves[j];
              sci->details = &si->config->Curves.Detail[i];
              sci->details->id = sci;
              break;
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
      
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_SCALE))
    {
      comp_mask |= SGCOMPMASK_LEGEND | SGCOMPMASK_DATA | SGCOMPMASK_YAXIS;
      StripGraph_setstat
        (si->graph, SGSTAT_GRAPH_REFRESH | SGSTAT_LEGEND_REFRESH);
    }
      
    if (StripConfigMask_stat (&mask, SCFGMASK_CURVE_PLOTSTAT))
    {
      /* re-arrange the plot order */
      int shift;
          
      for (i = 0; i < STRIP_MAX_CURVES; i++)
        if (si->curves[i].details)
          if (StripConfigMask_stat
              (&si->curves[i].details->update_mask, SCFGMASK_CURVE_PLOTSTAT))
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
static void     Strip_eventmgr          (XtPointer arg, XtIntervalId *BOGUS(id))
{
  StripInfo             *si = (StripInfo *)arg;
  struct timeval        event_time;
  struct timeval        t;
  double                diff;
  unsigned              event;
  int                   i, n;

  for (event = 0; event < LAST_STRIPEVENT; event++)
  {
    /* only process desired events */
    if (!(si->event_mask & (1 << event))) continue;

    get_current_time (&event_time);
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
              struct timeval    t0, t1;

              StripGraph_getattr
                (si->graph,
                 STRIPGRAPH_BEGIN_TIME, &t0,
                 STRIPGRAPH_END_TIME,   &t1,
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
                 STRIPGRAPH_BEGIN_TIME, &t,
                 STRIPGRAPH_END_TIME,   &event_time,
                 0);
              StripGraph_draw
                (si->graph, SGCOMPMASK_XAXIS | SGCOMPMASK_DATA, (Region *)0);
            }
            break;
            
          case STRIPEVENT_CHECK_CONNECT:
            for (i = 0, n = 0; i < STRIP_MAX_CURVES; i++)
              if (si->curves[i].details)
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
                  if (n > 0) ; /* do nothing */
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
static void     Strip_watchevent        (StripInfo *si, unsigned mask)
{
  si->event_mask |= mask;
  Strip_dispatch (si);
}

/*
 * Strip_ignoreevent
 */
static void     Strip_ignoreevent       (StripInfo *si, unsigned mask)
{
  si->event_mask &= ~mask;
  Strip_dispatch (si);
}


/*
 * Strip_dispatch
 */
static void     Strip_dispatch          (StripInfo *si)
{
  struct timeval        t;
  struct timeval        now;
  struct timeval        *next;
  double                interval[LAST_STRIPEVENT];
  int                   i;
  double                sec;

  /* first, if a timer has already been registered, disable it */
  if (si->tid != (XtIntervalId)0) XtRemoveTimeOut (si->tid);
  
  interval[STRIPEVENT_SAMPLE]           = si->config->Time.sample_interval;
  interval[STRIPEVENT_REFRESH]          = si->config->Time.refresh_interval;
  interval[STRIPEVENT_CHECK_CONNECT]    = (double)STRIP_CONNECTION_TIMEOUT;

  next = NULL;
  
  /* first determine the event time for each possible next event */
  for (i = 0; i < LAST_STRIPEVENT; i++)
  {
    if (!(si->event_mask & (1 << i))) continue;
      
#ifdef DEBUG_EVENT
    fprintf
      (stdout, "=> last %-15s:\n%s\n",
       StripEventTypeStr[i],
       time2str (&si->last_event[i]));
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
  get_current_time (&now);

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
           time2str (&si->next_event[i]));
    }
    fprintf (stdout, "=> Next Event\n%s\n", time2str (next));
    fprintf (stdout, "=> Current Time\n%s\n", time2str (&now));
    fprintf (stdout, "=> si->next_event - current_time = %lf (seconds)\n",
             sec = subtract_times (&t, &now, next));
    fprintf (stdout, "============\n");
#endif

    /* finally, register the timer callback */
    sec = subtract_times (&t, &now, next);
    if (sec < 0) sec = 0;
    si->tid = Strip_addtimeout ((Strip)si, sec, Strip_eventmgr, (XtPointer)si);
  }

#ifdef DEBUG_EVENT
  fflush (stdout);
#endif
}


/*
 * Strip_setup_printer
 */
static void     Strip_printer_init      (StripInfo *si)
{
  char  *printer, *device;
  char  *s;
  
  /* printer name */
  if (printer = getenv (STRIP_PRINTER_NAME_ENV))
  {
    device = getenv (STRIP_PRINTER_DEVICE_ENV);
    if (!device) device = STRIP_PRINTER_DEVICE_FALLBACK;
  }

  /* fallback printer info */
  else if (printer = getenv (STRIP_PRINTER_NAME_FALLBACK_ENV))
    device = STRIP_PRINTER_DEVICE_FALLBACK;

  else
  {
    printer = STRIP_PRINTER_NAME;
    device = STRIP_PRINTER_DEVICE;
  }

  s = si->print_info.printer;
  while (*printer) *s++ = *printer++;
  *s = 0;

  s = si->print_info.device;
  while (*device) *s++ = *device++;
  *s = 0;
}


/*
 * callback
 */
static void     callback        (Widget w, XtPointer client, XtPointer call)
{
  static int                    x, y;

  XmDrawingAreaCallbackStruct   *cbs;
  XEvent                        *event;
  StripInfo                     *si;
  StripConfigMask               scfg_mask;
  struct timeval                t, tb, t0, t1;


  cbs = (XmDrawingAreaCallbackStruct *)call;
  event = cbs->event;
  si = (StripInfo *)client;

  switch (cbs->reason)
  {
      case XmCR_PROTOCOLS:

        dlgrequest_quit (si, 0);
        break;
        
      case XmCR_ACTIVATE:

        /*
         * Pan left or right
         */
        if (w == si->btn[STRIPBTN_LEFT] || w == si->btn[STRIPBTN_RIGHT])
        {
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
             STRIPGRAPH_BEGIN_TIME,     &t0,
             STRIPGRAPH_END_TIME,       &t1,
             0);
          StripGraph_draw
            (si->graph, SGCOMPMASK_DATA | SGCOMPMASK_XAXIS, (Region *)0);
        }

        /*
         * Zoom in or out
         *
         *      (1) set new end time of graph object (begin time is calculated
         *          as offset from end time)
         *      (2) turn on browse mode
         *      (3) set new timespan via StripConfig_setattr()
         *      (4) generate configuration update callbacks, causing
         *          the graph to be updated.
         */
        else if (w == si->btn[STRIPBTN_ZOOMIN] ||
                 w == si->btn[STRIPBTN_ZOOMOUT])
        {
          struct timeval        t, tb, t1;
          unsigned              t_new;
          
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
            /* double viewable area by adding 1/2 current range to the
             * end-point */
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

        /*
         * Refresh the screen
         */
        else if (w == si->btn[STRIPBTN_REFRESH])
        {
          StripGraph_setstat (si->graph, SGSTAT_GRAPH_REFRESH);
          StripGraph_draw (si->graph, SGCOMPMASK_DATA, (Region *)0);
        }

        break;
        
        
      case XmCR_INPUT:
        
        if (event->xany.type == ButtonPress)
        {
          x = event->xbutton.x;
          y = event->xbutton.y;

          /* if this is the third button, then popup the menu */
          if (event->xbutton.button == Button3)
          {
            PopupMenu_position
              (si->popup_menu, (XButtonPressedEvent *)cbs->event);
            PopupMenu_popup (si->popup_menu);
          }

          /* if this is the first button, then initiate a comment entry */
          else if (event->xbutton.button == Button1)
          {
#if 0
            Widget txt;
            
            /* pause the graph by putting it in browse mode */
            si->status |= STRIPSTAT_BROWSE_MODE;

            /* create the text entry widget */
            txt = XtVaCreateManagedWidget
              ("commentTextF",
               xmTextFieldWidgetClass, w,
               XmNx, x, XmNy, y, 0);
            XmProcessTraversal (txt, XmTRAVERSE_CURRENT);
#endif
          }
        }
        
        else if (event->xany.type == ButtonRelease)
        {
          /* nothing here at the moment! */
        }
        break;
  }
}

/* X_error_handler
 */
static int      X_error_handler         (Display *display, XErrorEvent *error)
{
  char  msg[128];

  XGetErrorText (display, error->error_code, msg, 128);
  fprintf
    (stderr,
     "==== StripTool X Error Handler ====\n"
     "error:         %s\n"
     "major opcode:  %d\n"
     "serial:        %d\n"
     "process ID:    %d\n",
     msg, error->request_code, error->serial, getpid());

  /* remember error */
  Strip_x_error_code = error->error_code;
  
  return 0;     /* not used? */
}



/*
 * dlgrequest_connect
 */
static void     dlgrequest_connect      (void *client, void *call)
{
  StripInfo     *si = (StripInfo *)client;
  char          *name = (char *)call;
  StripCurve    curve;
  
  if ((curve = Strip_getcurve ((Strip)si)) != 0)
  {
    StripCurve_setattr (curve, STRIPCURVE_NAME, name, 0);
    Strip_connectcurve ((Strip)si, curve);
  }
}


/*
 * dlgrequest_show
 */
static void     dlgrequest_show         (void *client, void *BOGUS(1))
{
  StripInfo     *si = (StripInfo *)client;
  window_map (si->display, XtWindow (si->shell));
}


/*
 * dlgrequest_clear
 */
static void     dlgrequest_clear        (void *client, void *BOGUS(1))
{
  StripInfo     *si = (StripInfo *)client;
  Strip_clear ((Strip)si);
}


/*
 * dlgrequest_delete
 */
static void     dlgrequest_delete       (void *client, void *call)
{
  StripInfo     *si = (StripInfo *)client;
  StripCurve    curve  = (StripCurve)call;

  Strip_freecurve ((Strip)si, curve);
}


/*
 * dlgrequest_dismiss
 */
static void     dlgrequest_dismiss      (void *client, void *BOGUS(1))
{
  StripInfo     *si = (StripInfo *)client;
  Widget        tmp_w;

  if (window_ismapped (si->display, XtWindow (si->shell)))
    StripDialog_popdown (si->dialog);
  else
  {
    StripDialog_getattr (si->dialog, STRIPDIALOG_SHELL_WIDGET, &tmp_w, 0);
    MessageBox_popup
      (tmp_w, &si->message_box, XmDIALOG_ERROR, "Oops", "Ok",
       "You can't dismiss all your windows");
  }
}


/*
 * dlgrequest_quit
 */
static void     dlgrequest_quit (void *client, void *BOGUS(1))
{
  StripInfo     *si = (StripInfo *)client;
  
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
static void     dlgrequest_window_popup (void *client, void *call)
{
  
  StripInfo             *si = (StripInfo *)client;
  StripWindowType       which = (StripWindowType)call;

  switch (which)
  {
      case STRIPWINDOW_GRAPH:
        window_map (si->display, XtWindow (si->shell));
        break;
  }
}



/* ====== Popup Menu stuff ====== */
typedef enum
{
  POPUPMENU_CONTROLS = 0,
  POPUPMENU_TOGGLE_SCROLL,
  POPUPMENU_PRINTER_SETUP,
  POPUPMENU_PRINT,
  POPUPMENU_SNAPSHOT,
  POPUPMENU_DISMISS,
  POPUPMENU_QUIT,
  POPUPMENU_ITEMCOUNT
}
PopupMenuItem;

char    *PopupMenuItemStr[POPUPMENU_ITEMCOUNT] =
{
  "Controls Dialog...",
  "Toggle buttons",
  "Printer Setup...",
  "Print...",
  "Snapshot...",
  "Dismiss",
  "Quit"
};

char    PopupMenuItemMnemonic[POPUPMENU_ITEMCOUNT] =
{
  'C',
  'T',
  'r',
  'P',
  'S',
  'm',
  'Q'
};

char    *PopupMenuItemAccelerator[POPUPMENU_ITEMCOUNT] =
{
  " ",
  " ",
  " ",
  " ",
  " ",
  " ",
  "Ctrl<Key>c"
};

char    *PopupMenuItemAccelStr[POPUPMENU_ITEMCOUNT] =
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
static Widget   PopupMenu_build (Widget parent)
{
  Widget        menu;
  XmString      label[POPUPMENU_ITEMCOUNT+2];
  XmString      acc_lbl[POPUPMENU_ITEMCOUNT+2];
  KeySym        keySyms[POPUPMENU_ITEMCOUNT+2];
  XmButtonType  buttonType[POPUPMENU_ITEMCOUNT+2];
  int           i;
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
static void     PopupMenu_position      (Widget                 menu,
                                         XButtonPressedEvent    *event)
{
  XmMenuPosition (menu, event);
}


/*
 * PopupMenu_popup
 */
static void     PopupMenu_popup (Widget menu)
{
  XtManageChild (menu);
}


/*
 * PopupMenu_popdown
 */
static void     PopupMenu_popdown (Widget menu)
{
  XtUnmanageChild (menu);
}


/*
 * PopupMenu_cb
 */
static void     PopupMenu_cb    (Widget w, XtPointer client, XtPointer BOGUS(1))
{
  PopupMenuItem item = (PopupMenuItem)client;
  StripInfo     *si;
  char          cmd_buf[256];
  pid_t         pid;

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
          XtVaSetValues
            (si->graph_form, XmNbottomAttachment, XmATTACH_FORM, 0);
        }
        else
        {
          XtManageChild (si->graph_panel);
          XtVaSetValues
            (si->graph_form,
             XmNbottomAttachment,       XmATTACH_WIDGET,
             XmNbottomWidget,           si->graph_panel,
             0);
        }
        break;

        
      case POPUPMENU_PRINTER_SETUP:
        PrinterDialog_popup (si->pd, si);
        break;
        
        
      case POPUPMENU_PRINT:
        window_map (si->display, XtWindow(si->shell));
        sprintf
          (cmd_buf,
           "xwd -id %d | xpr -device %s | lp -d%s -onb",
           XtWindow (si->graph_form),
           si->print_info.device,
           si->print_info.printer);
        
        
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
           XtWindow (si->graph_form));
        if (pid = fork ())
        {
          execl ("/bin/sh", "sh", "-c", cmd_buf, 0);
          exit (0);
        }
        break;
        
      case POPUPMENU_DISMISS:
        if (StripDialog_ismapped (si->dialog))
          window_unmap (si->display, XtWindow (si->shell));
        else MessageBox_popup
               (si->shell, &si->message_box, XmDIALOG_ERROR, "Oops", "Ok",
                "You can't dismiss all your windows");
        break;
        
      case POPUPMENU_QUIT:
        dlgrequest_quit ((void *)si, NULL);
        break;
        
      default:
        fprintf (stderr, "Argh!\n");
        exit (1);
  }
}


static PrinterDialog    *PrinterDialog_build    (Widget parent)
{
  PrinterDialog *pd;
  Widget        base;
  Pixel         bg, fg;

  pd = (PrinterDialog *)malloc (sizeof (PrinterDialog));
  if (!pd) return 0;
  
  pd->msgbox = XmCreateTemplateDialog (parent, "PrinterDialog", 0, 0);
  XtAddCallback (pd->msgbox, XmNokCallback, PrinterDialog_cb, 0);

  base = XtVaCreateManagedWidget
    ("baseForm", xmFormWidgetClass, pd->msgbox, 0);

  pd->name_textf = XtVaCreateManagedWidget
    ("nameTextF", xmTextFieldWidgetClass, base, 0);
  pd->device_combo = XtVaCreateManagedWidget
    ("deviceCombo", xgComboBoxWidgetClass, base, 0);
  XtVaCreateManagedWidget ("nameLabel", xmLabelWidgetClass, base, 0);
  XtVaCreateManagedWidget ("deviceLabel", xmLabelWidgetClass, base, 0);

  /* combo box won't use default CDE colors for text & list */
  XtVaGetValues (pd->name_textf, XmNbackground, &bg, XmNforeground, &fg, 0);
  XtVaSetValues
    (pd->device_combo,
     XgNlistForeground,         fg,
     XgNtextForeground,         fg,
     XgNlistBackground,         bg,
     XgNtextBackground,         bg,
     0);

  return pd;
}


static void     PrinterDialog_popup     (PrinterDialog *pd, StripInfo *si)
{
  Window                root, child;
  Dimension             width, height;
  int                   root_x, root_y;
  int                   win_x, win_y;
  unsigned              mask;
  
  /* set the data fields appropriately */
  XmTextFieldSetString (pd->name_textf, si->print_info.printer);
  XgComboBoxSetString (pd->device_combo, si->print_info.device, False);

  /* find out where the pointer is */
  XQueryPointer
    (XtDisplay (pd->msgbox), DefaultRootWindow (XtDisplay (pd->msgbox)),
     &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

  /* place dialog box so it surrounds the pointer horizontally */
  XtVaGetValues (pd->msgbox, XmNwidth, &width, XmNheight, &height, 0);

  win_x = root_x - (width / 2);         if (win_x < 0) win_x = 0;
  win_y = root_y - (height / 2);        if (win_y < 0) win_y = 0;

  XtVaSetValues
    (pd->msgbox,
     XmNx,              (Dimension)win_x,
     XmNy,              (Dimension)win_y,
     XmNuserData,       si,                     /* need for callback */
     0);

  /* pop it up! */
  XtManageChild (pd->msgbox);
}


static void     PrinterDialog_cb        (Widget         w,
                                         XtPointer      BOGUS(client),
                                         XtPointer      call)
{
  XmAnyCallbackStruct   *cbs = (XmAnyCallbackStruct *)call;
  StripInfo             *si;
  char                  *a, *b;

  if (cbs->reason == XmCR_OK)
  {
    XtVaGetValues (w, XmNuserData, &si, 0);

    /* get printer name, if not empty string */
    a = XmTextFieldGetString (si->pd->name_textf);
    b = si->print_info.printer;
    if (a) if (*a) { while (*a) *b++ = *a++; *b = 0; }
    
    /* get device name, if not empty string */
    a = XgComboBoxGetString (si->pd->device_combo);
    b = si->print_info.device;
    if (a) if (*a) { while (*a) *b++ = *a++; *b = 0; }
  }
}



static void     fsdlg_popup     (StripInfo *si, fsdlg_functype func)
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


static void     fsdlg_cb        (Widget w, XtPointer data, XtPointer call)
{
  XmFileSelectionBoxCallbackStruct *cbs =
    (XmFileSelectionBoxCallbackStruct *) call;
  
  StripInfo             *si = (StripInfo *)data;
  fsdlg_functype        func;
  char                  *fname;

  XtUnmanageChild (w);

  if (si)
    if (XmStringGetLtoR (cbs->value, XmFONTLIST_DEFAULT_TAG, &fname))
      if (fname != NULL)
      {
        XtVaGetValues (w, XmNuserData, &func, NULL);
        func ((Strip)si, fname);
      }
}
