/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripDialog.h"
#include "ColorDialog.h"
#include "StripConfig.h"
#include "StripMisc.h"

#include <stdlib.h>

#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeB.h>
#include <Xm/CascadeBG.h>
#include <Xm/SeparatoG.h>
#include <Xm/PushBG.h>
#include <Xm/FileSB.h>
#include <Xm/Scale.h>

#define DEF_WOFFSET		3
#define COLOR_BTN_STR		"  "
#define MAX_WINDOW_MENU_ITEMS	10
#define MAX_REALNUM_LEN		8

typedef enum
{
  SDCALLBACK_CONNECT = 0,
  SDCALLBACK_SHOW,
  SDCALLBACK_CLEAR,
  SDCALLBACK_DELETE,
  SDCALLBACK_DISMISS,
  SDCALLBACK_QUIT,
  SDCALLBACK_COUNT
}
StripDialogCallback;

typedef enum
{
  SDCURVE_NAME,
#ifdef STRIPDIALOG_SHOW_STATUS
  SDCURVE_STATUS,
#endif
  SDCURVE_COLOR,
  SDCURVE_PENSTAT,
  SDCURVE_PLOTSTAT,
  SDCURVE_PRECISION,
  SDCURVE_MIN,
  SDCURVE_MAX,
  SDCURVE_MODIFY,
  SDCURVE_REMOVE,
  SDCURVE_LAST_ATTRIBUTE
}
SDCurveAttribute;

typedef enum
{
  SDTMOPT_TSHOUR,
  SDTMOPT_TSMINUTE,
  SDTMOPT_TSSECOND,
  SDTMOPT_DS,
  SDTMOPT_GR,
  SDTMOPT_MODIFY,
  SDTMOPT_CANCEL,
  SDTMOPT_LAST_ATTRIBUTE,
}
SDTmoptAttribute;

typedef enum
{
  SDGROPT_FG,
  SDGROPT_BG,
  SDGROPT_GRIDCLR,
  SDGROPT_LGNDCLR,
  SDGROPT_GRIDX,
  SDGROPT_GRIDY,
  SDGROPT_YAXISCLR,
  SDGROPT_LINEWIDTH,
  SDGROPT_YAXISNUM,
  SDGROPT_XAXISNUM,
  SDGROPT_MODIFY,
  SDGROPT_CANCEL,
  SDGROPT_LAST_ATTRIBUTE,
}
SDGroptAttribute;

typedef int	SDCurveMask;
typedef int	SDTimeMask;
typedef int	SDGraphMask;

typedef enum
{
  FSDLG_TGL_TIME = 0,
  FSDLG_TGL_COLORS,
  FSDLG_TGL_OPTIONS,
  FSDLG_TGL_CURVES,
  FSDLG_TGL_COUNT
}
FsDlgTglAttributes;

char 	*FsDlgTglStr[FSDLG_TGL_COUNT] =
{
  "Timing",
  "Colors",
  "Graph Attributes",
  "Curve Attributes"
};

StripConfigMask	FsDlgTglVal[FSDLG_TGL_COUNT] =
{
  STRIPCFGMASK_TIME,
  STRIPCFGMASK_COLOR,
  STRIPCFGMASK_OPTION,
  STRIPCFGMASK_CURVE
};

typedef enum
{
  FSDLG_SAVE,
  FSDLG_LOAD
}
FsDlgState;

#define FSDLG_OK	0
#define FSDLG_CANCEL	1


char 	*SDCurveAttributeStr[SDCURVE_LAST_ATTRIBUTE] =
{
  "Name",
#ifdef STRIPDIALOG_SHOW_STATUS
  "Status",
#endif
  "Color",
  "Pen Status",
  "Plot Status",
  "Precision",
  "Min",
  "Max",
  "Modify",
  "Remove"
};

typedef struct _CBInfo
{
  char	*str;
  void	*p;
}
CBInfo;

typedef struct
{
  StripCurve	curve;
  Widget	widgets[SDCURVE_LAST_ATTRIBUTE];
  Widget	precision_txt, precision_lbl;
  Widget	min_txt, min_lbl;
  Widget	max_txt, max_lbl;
  Widget	top_sep;
  SDCurveMask	modified;
  int		modifying;
}
SDCurveInfo;

typedef struct
{
  Widget	widgets[SDTMOPT_LAST_ATTRIBUTE];
  Widget	ts_hour_txt, ts_hour_lbl;
  Widget	ts_minute_txt, ts_minute_lbl;
  Widget	ts_second_txt, ts_second_lbl;
  Widget	ds_txt, ds_lbl;
  Widget	gr_txt, gr_lbl;
  SDTimeMask	modified;
  int		modifying;
}
SDTimeInfo;

typedef struct
{
  Widget	widgets[SDGROPT_LAST_ATTRIBUTE];
  Widget	ynum_txt, ynum_lbl;
  Widget	xnum_txt, xnum_lbl;
  SDGraphMask	modified;
  int		modifying;
  CBInfo	cb_info[SDGROPT_LAST_ATTRIBUTE];
}
SDGraphInfo;

typedef struct _SDWindowMenuInfo
{
  Widget	menu;
  
  struct	_item
  {
    SDWindowMenuItem	info;
    Widget		entry;
  }
  items[MAX_WINDOW_MENU_ITEMS];

  int		count;
}
SDWindowMenuInfo;

typedef struct
{
  Display		*display;
  StripConfig		*config;
  ColorDialog		clrdlg;
  SDCurveInfo		curve_info[STRIP_MAX_CURVES];
  int			sdcurve_count;
  SDTimeInfo		time_info;
  SDGraphInfo		graph_info;
  Widget		shell, curve_form, data_form, graph_form, connect_txt;
  Widget		message_box;
  SDWindowMenuInfo	window_menu_info;

  struct		_fs	/* file selection box */
  {
    Widget		dlg;
    Widget		tgl[FSDLG_TGL_COUNT];
    StripConfigMask	mask;
    FsDlgState		state;
  }
  fs;

  struct 		_callback
  {
    SDcallback	func;
    void	*data;
  }
  callback[SDCALLBACK_COUNT];
}
StripDialogInfo;


/* ====== static data ====== */
#ifdef STRIPDIALOG_SHOW_STATUS
static XmString		status_lbl_str[2];
#endif
static XmString		modify_btn_str[2];
static XmString		showhide_btn_str[2];
static XmString		penstat_tgl_str[2];
static XmString		plotstat_tgl_str[2];
static XmString		gridstat_tgl_str[2];
static XmString		yaxisclr_tgl_str[2];

static char		char_buf[256];
static XmString		xstr;

/* ====== static function prototypes ====== */
static void	insert_sdcurve	(StripDialogInfo *, int, StripCurve, int);
static void	delete_sdcurve	(StripDialogInfo *, int);
static void	show_sdcurve	(StripDialogInfo *, int);
static void	hide_sdcurve	(StripDialogInfo *, int);

/* these functions operate on curve rows in the curve control area */
static void	setwidgetval_name	(StripDialogInfo *, int, char *);
static void	setwidgetval_status	(StripDialogInfo *, int,
					 StripCurveStatus);
static void	setwidgetval_color	(StripDialogInfo *, int, Pixel);
static void	setwidgetval_penstat	(StripDialogInfo *, int, int);
static void	setwidgetval_plotstat	(StripDialogInfo *, int, int);
static void	setwidgetval_precision	(StripDialogInfo *, int, int);
static void	setwidgetval_min	(StripDialogInfo *, int, double);
static void	setwidgetval_max	(StripDialogInfo *, int, double);
static void	setwidgetval_modify	(StripDialogInfo *, int, int);

static char	*getwidgetval_name	(StripDialogInfo *, int);
static char	*getwidgetval_status	(StripDialogInfo *, int);
static Pixel	getwidgetval_color	(StripDialogInfo *, int);
static int	getwidgetval_penstat	(StripDialogInfo *, int);
static int	getwidgetval_plotstat	(StripDialogInfo *, int);
static char	*getwidgetval_precision	(StripDialogInfo *, int, int *);
static char	*getwidgetval_min	(StripDialogInfo *, int, double *);
static char	*getwidgetval_max	(StripDialogInfo *, int, double *);

/*
 * these functions operate on StripConfig attribute control widgets
 */
/* time controls */
static void	setwidgetval_tm_tshour		(StripDialogInfo *, int);
static void	setwidgetval_tm_tsminute	(StripDialogInfo *, int);
static void	setwidgetval_tm_tssecond	(StripDialogInfo *, int);
static void	setwidgetval_tm_ds		(StripDialogInfo *, double);
static void	setwidgetval_tm_gr		(StripDialogInfo *, double);
static void	setwidgetval_tm_modify		(StripDialogInfo *, int);

static char	*getwidgetval_tm_tshour		(StripDialogInfo *, int *);
static char	*getwidgetval_tm_tsminute	(StripDialogInfo *, int *);
static char	*getwidgetval_tm_tssecond	(StripDialogInfo *, int *);
static char	*getwidgetval_tm_ds		(StripDialogInfo *, double *);
static char	*getwidgetval_tm_gr		(StripDialogInfo *, double *);

/* graph controls */
static void	setwidgetval_gr_fg		(StripDialogInfo *, Pixel);
static void	setwidgetval_gr_bg		(StripDialogInfo *, Pixel);
static void	setwidgetval_gr_gridclr		(StripDialogInfo *, Pixel);
static void	setwidgetval_gr_lgndclr		(StripDialogInfo *, Pixel);
static void	setwidgetval_gr_linewidth	(StripDialogInfo *, int);
static void	setwidgetval_gr_gridx		(StripDialogInfo *, int);
static void	setwidgetval_gr_gridy		(StripDialogInfo *, int);
static void	setwidgetval_gr_yaxisclr	(StripDialogInfo *, int);
static void	setwidgetval_gr_yaxisnum	(StripDialogInfo *, int);
static void	setwidgetval_gr_xaxisnum	(StripDialogInfo *, int);
static void	setwidgetval_gr_modify		(StripDialogInfo *, int);

static Pixel	getwidgetval_gr_fg		(StripDialogInfo *);
static Pixel	getwidgetval_gr_bg		(StripDialogInfo *);
static Pixel	getwidgetval_gr_gridclr		(StripDialogInfo *);
static Pixel	getwidgetval_gr_lgndclr		(StripDialogInfo *);
static int	getwidgetval_gr_linewidth	(StripDialogInfo *);
static int	getwidgetval_gr_gridx		(StripDialogInfo *);
static int	getwidgetval_gr_gridy		(StripDialogInfo *);
static int	getwidgetval_gr_yaxisclr	(StripDialogInfo *);
static char	*getwidgetval_gr_yaxisnum	(StripDialogInfo *, int *);
static char	*getwidgetval_gr_xaxisnum	(StripDialogInfo *, int *);


/* ====== static callback function prototypes ====== */
static void	connect_btn_cb	(Widget, XtPointer, XtPointer);

static void	color_btn_cb	(Widget, XtPointer, XtPointer);
static void	modify_btn_cb	(Widget, XtPointer, XtPointer);
static void	remove_btn_cb	(Widget, XtPointer, XtPointer);
static void	penstat_tgl_cb	(Widget, XtPointer, XtPointer);
static void	plotstat_tgl_cb	(Widget, XtPointer, XtPointer);

static void	text_focus_cb	(Widget, XtPointer, XtPointer);

static void	tmmodify_btn_cb	(Widget, XtPointer, XtPointer);
static void	tmcancel_btn_cb	(Widget, XtPointer, XtPointer);
static void	grmodify_btn_cb	(Widget, XtPointer, XtPointer);
static void	grcancel_btn_cb	(Widget, XtPointer, XtPointer);
static void	grcolor_btn_cb	(Widget, XtPointer, XtPointer);
static void	gropt_slider_cb	(Widget, XtPointer, XtPointer);
static void	gropt_tgl_cb	(Widget, XtPointer, XtPointer);

static void	filemenu_cb	(Widget, XtPointer, XtPointer);
static void	windowmenu_cb	(Widget, XtPointer, XtPointer);

static void	ctrl_btn_cb	(Widget, XtPointer, XtPointer);

static void	fsdlg_cb	(Widget, XtPointer, XtPointer);

static void	StripDialog_cfgupdate	(StripConfigMask, void *);



StripDialog	StripDialog_init	(Display *d, StripConfig *cfg)
{
  StripDialogInfo	*sd;
  Widget		frame;
  Widget		base_form, form;
  Widget		w, lbl, txt, btn, tmp;
  Widget		curve_column_lbl[SDCURVE_LAST_ATTRIBUTE];
  Widget		leftmost_col, rightmost_col;
  Widget		sep;
  Widget		menu;
  int			i, j;
  Dimension		dim1, dim2;
  Dimension		widths[SDCURVE_LAST_ATTRIBUTE];
  Dimension		row_height;
  Colormap		cmap;

  if ((sd = (StripDialogInfo *)malloc (sizeof (StripDialogInfo))) != NULL)
    {
#ifdef STRIPDIALOG_SHOW_STATUS
      status_lbl_str[0]		= XmStringCreateLocalized ("Unconn");
      status_lbl_str[1]		= XmStringCreateLocalized ("Conn");
#endif
      modify_btn_str[0] 	= XmStringCreateLocalized ("Modify");
      modify_btn_str[1] 	= XmStringCreateLocalized ("Update");
      showhide_btn_str[0] 	= XmStringCreateLocalized ("Show StripChart");
      showhide_btn_str[1] 	= XmStringCreateLocalized ("Hide StripChart");
      penstat_tgl_str[0] 	= XmStringCreateLocalized ("down");
      penstat_tgl_str[1] 	= XmStringCreateLocalized ("up");
      plotstat_tgl_str[0] 	= XmStringCreateLocalized ("visible");
      plotstat_tgl_str[1] 	= XmStringCreateLocalized ("hidden");
      gridstat_tgl_str[0]	= XmStringCreateLocalized ("on");
      gridstat_tgl_str[1]	= XmStringCreateLocalized ("off");
      yaxisclr_tgl_str[0]	= XmStringCreateLocalized ("selected curve");
      yaxisclr_tgl_str[1]	= XmStringCreateLocalized ("foreground");

      for (i = 0; i < STRIP_MAX_CURVES; i++)
	sd->curve_info[i].curve = 0;
      sd->sdcurve_count = 0;
      sd->time_info.modifying = 0;
      sd->graph_info.modifying = 0;
      sd->message_box = (Widget)0;

      sd->display = d;

      for (i = 0; i < SDCALLBACK_COUNT; i++)
	{
	  sd->callback[i].func = NULL;
	  sd->callback[i].data = NULL;
	}
      
      /* store the configuration pointer, and add all update callbacks */
      sd->config = cfg;
      StripConfig_addcallback
	(sd->config,
	 STRIPCFGMASK_TIME | STRIPCFGMASK_OPTION | STRIPCFGMASK_CURVE,
	 StripDialog_cfgupdate,
	 sd);

      /* initialize the color editor */
      cmap = StripConfig_getcmap (sd->config);
      sd->clrdlg = ColorDialog_init (sd->display, cmap, "Strip ColorEditor");

      
      sd->shell = XtVaAppCreateShell
	(NULL, 				"StripDialog",
	 topLevelShellWidgetClass,	sd->display,
	 XmNdeleteResponse,		XmDO_NOTHING,
	 XmNmappedWhenManaged,		False,
	 XmNcolormap,			cmap,
	 NULL);
      base_form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		sd->shell,
	 NULL);

      /* build the file selection box */
      sd->fs.dlg = XmCreateFileSelectionDialog
	(sd->shell, "Configuration File", NULL, 0);
      XtVaSetValues
	(sd->fs.dlg,
	 XmNuserData, 			sd,
	 XmNfileTypeMask,		XmFILE_REGULAR,
	 NULL);
      xstr = XmStringCreateLocalized (STRIP_CONFIGFILE_DIR);
      XtVaSetValues (sd->fs.dlg, XmNdirectory, xstr, NULL);
      XmStringFree (xstr);
      xstr = XmStringCreateLocalized (STRIP_CONFIGFILE_PATTERN);
      XtVaSetValues (sd->fs.dlg, XmNpattern, xstr, NULL);
      XmStringFree (xstr);
      
      XtAddCallback
	(sd->fs.dlg, XmNcancelCallback, fsdlg_cb, (XtPointer)FSDLG_CANCEL);
      XtAddCallback (sd->fs.dlg, XmNokCallback, fsdlg_cb, (XtPointer)FSDLG_OK);
      frame = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		sd->fs.dlg,
	 NULL);
      XtVaCreateManagedWidget
	("Attribute Mask",
	 xmLabelWidgetClass,		frame,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 NULL);
      w = XtVaCreateManagedWidget
	("rowcol",
	 xmRowColumnWidgetClass,	frame,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 NULL);
      sd->fs.mask = 0;
      sd->fs.state = FSDLG_SAVE;
      for (i = 0; i < FSDLG_TGL_COUNT; i++)
	{
	  xstr = XmStringCreateLocalized (FsDlgTglStr[i]);
	  sd->fs.tgl[i] = XtVaCreateManagedWidget
	    ("togglebutton",
	     xmToggleButtonWidgetClass,	w,
	     XmNlabelString,		xstr,
	     XmNset,			True,
	     NULL);
	  XmStringFree (xstr);
	  sd->fs.mask |= FsDlgTglVal[i];
	}
	     

      /* build the menu bar and related components */
      menu = XmCreateMenuBar (base_form, "MenuBar", NULL, 0);
      XtVaSetValues
	(menu,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      
      w = XmCreatePulldownMenu (menu, "FilePulldown", NULL, 0);
      XtVaCreateManagedWidget
	("File",
	 xmCascadeButtonGadgetClass,	menu,
	 XmNmnemonic,			'F',
	 XmNsubMenuId,			w,
	 NULL);
      tmp = XtVaCreateManagedWidget
	("Load Configuration...",
	 xmPushButtonGadgetClass,	w,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(tmp, XmNactivateCallback, filemenu_cb, (XtPointer)FSDLG_LOAD);
      tmp = XtVaCreateManagedWidget
	("Save Configuration...",
	 xmPushButtonGadgetClass,	w,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(tmp, XmNactivateCallback, filemenu_cb, (XtPointer)FSDLG_SAVE);
      XtVaCreateManagedWidget
	("separator",
	 xmSeparatorGadgetClass,	w,
	 NULL);
      tmp = XtVaCreateManagedWidget
	("Exit Program",
	 xmPushButtonGadgetClass,	w,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(tmp, XmNactivateCallback, ctrl_btn_cb, (XtPointer)SDCALLBACK_QUIT);

      sd->window_menu_info.menu = XmCreatePulldownMenu
	(menu, "WindowPulldown", NULL, 0);
      XtVaCreateManagedWidget
	("Window",
	 xmCascadeButtonGadgetClass,	menu,
	 XmNmnemonic,			'W',
	 XmNsubMenuId,			sd->window_menu_info.menu,
	 NULL);
      for (i = 0; i < MAX_WINDOW_MENU_ITEMS; i++)
	{
	  sd->window_menu_info.items[i].entry = w = XtVaCreateManagedWidget
	    ("blah",
	     xmPushButtonGadgetClass,	sd->window_menu_info.menu,
	     XmNuserData,		sd,
	     NULL);
	  XtAddCallback (w, XmNactivateCallback, windowmenu_cb, (XtPointer)i);
	  XtUnmanageChild (sd->window_menu_info.items[i].entry);
	}
      sd->window_menu_info.count = 0;

      w = XmCreatePulldownMenu (menu, "HelpPulldown", NULL, 0);
      tmp = XtVaCreateManagedWidget
	("Help",
	 xmCascadeButtonGadgetClass,	menu,
	 XmNmnemonic,			'H',
	 XmNsubMenuId,			w,
	 NULL);
      XtVaSetValues
	(menu,
	 XmNmenuHelpWidget,		tmp,
	 NULL);
      
      XtManageChild (menu);
	 

      /* create the curve area form and controls */
      frame = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		base_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			menu,
	 XmNtopOffset,			DEF_WOFFSET,
	 XmNrightAttachment,		XmATTACH_FORM,
	 XmNrightOffset,		DEF_WOFFSET,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);
      XtVaCreateManagedWidget
	("Curves",
	 xmLabelWidgetClass,		frame,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);

      sd->curve_form = form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		frame,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 XmNfractionBase,		(STRIP_MAX_CURVES + 1) * 10,
	 NULL);
      curve_column_lbl[i = SDCURVE_NAME] = XtVaCreateManagedWidget
	("A Long Curve Name",
	 xmLabelWidgetClass,		form,
	 XmNrecomputeSize,		False,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNtopOffset,			DEF_WOFFSET,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNleftOffset,			DEF_WOFFSET,
	 NULL);
#ifdef STRIPDIALOG_SHOW_STATUS
      curve_column_lbl[++i] = XtVaCreateManagedWidget
	("Unconn",
	 xmLabelWidgetClass,		form,
	 XmNalignment,			XmALIGNMENT_CENTER,
	 XmNtopAttachment,		XmATTACH_POSITION,
	 XmNtopPosition,		1,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			curve_column_lbl[SDCURVE_NAME],
	 XmNleftOffset,			DEF_WOFFSET,
	 NULL);
#endif
      for (i = i+1, row_height = 0; 
	   i < SDCURVE_LAST_ATTRIBUTE; 
	   i++)
	{
	  curve_column_lbl[i] = XtVaCreateManagedWidget
	    (SDCurveAttributeStr[i],
	     xmLabelWidgetClass,	form,
	     XmNtopAttachment,		XmATTACH_POSITION,
	     XmNtopPosition,		1,
	     XmNleftAttachment,		XmATTACH_WIDGET,
	     XmNleftWidget,		curve_column_lbl[i-1],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
	  XtVaGetValues (curve_column_lbl[i], XmNheight, &dim1, NULL);
	  row_height = max (row_height, dim1);
	}
#ifdef STRIPDIALOG_SHOW_STATUS
	XtVaSetValues
	  (curve_column_lbl[SDCURVE_STATUS],
	   XmNalignment,		XmALIGNMENT_END,
	   XmNrecomputeSize,		False,
	   NULL);
#endif
      for (i = SDCURVE_PRECISION; i <= SDCURVE_MAX; i++)
	XtVaSetValues
	  (curve_column_lbl[i],
	   XmNalignment,		XmALIGNMENT_END,
	   XmNrecomputeSize,		False,
	   NULL);

      leftmost_col = curve_column_lbl[0];
      rightmost_col = curve_column_lbl[SDCURVE_LAST_ATTRIBUTE-1];

      for (j = 0; j < STRIP_MAX_CURVES; j++)
	{
	  sep = sd->curve_info[j].top_sep = XtVaCreateManagedWidget
	    ("separator",
	     xmSeparatorWidgetClass,	form,
	     XmNshadowType,		XmSHADOW_ETCHED_IN,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_POSITION,
	     XmNtopPosition,		((j+1)*10) + 1,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		leftmost_col,
	     XmNleftOffset,		DEF_WOFFSET,
	     XmNrightAttachment,	XmATTACH_OPPOSITE_WIDGET,
	     XmNrightWidget,		rightmost_col,
	     NULL);
	     
	  sd->curve_info[j].widgets[SDCURVE_NAME] = XtVaCreateManagedWidget
	    ("label",
	     xmLabelWidgetClass,	form,
	     XmNalignment,		XmALIGNMENT_BEGINNING,
	     XmNmappedWhenManaged,	False,
	     XmNrecomputeSize,		False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_NAME],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
#ifdef STRIPDIALOG_SHOW_STATUS
	  sd->curve_info[j].widgets[SDCURVE_STATUS] = XtVaCreateManagedWidget
	    ("label",
	     xmLabelWidgetClass,	form,
	     XmNalignment,		XmALIGNMENT_CENTER,
	     XmNmappedWhenManaged,	False,
	     XmNrecomputeSize,		False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_STATUS],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
#endif
	  sd->curve_info[j].widgets[SDCURVE_COLOR] = XtVaCreateManagedWidget
	    (COLOR_BTN_STR,
	     xmPushButtonWidgetClass,	form,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_COLOR],
	     XmNleftOffset,		DEF_WOFFSET,
	     XmNuserData,		sd,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].widgets[SDCURVE_COLOR],
	     XmNactivateCallback, color_btn_cb, (XtPointer)j);
	  sd->curve_info[j].widgets[SDCURVE_PENSTAT] = XtVaCreateManagedWidget
	    ("togglebutton",
	     xmToggleButtonWidgetClass,	form,
	     XmNlabelString,		penstat_tgl_str[0],
	     XmNset,			True,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_PENSTAT],
	     XmNleftOffset,		DEF_WOFFSET,
	     XmNuserData,		sd,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].widgets[SDCURVE_PENSTAT],
	     XmNvalueChangedCallback, penstat_tgl_cb, (XtPointer)j);
	  sd->curve_info[j].widgets[SDCURVE_PLOTSTAT] = XtVaCreateManagedWidget
	    ("togglebutton",
	     xmToggleButtonWidgetClass,	form,
	     XmNlabelString,		plotstat_tgl_str[0],
	     XmNset,			True,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_PLOTSTAT],
	     XmNleftOffset,		DEF_WOFFSET,
	     XmNuserData,		sd,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].widgets[SDCURVE_PLOTSTAT],
	     XmNvalueChangedCallback, plotstat_tgl_cb, (XtPointer)j);
	  sd->curve_info[j].precision_lbl = XtVaCreateManagedWidget
	    ("label",
	     xmLabelWidgetClass,	form,
	     XmNalignment,		XmALIGNMENT_END,
	     XmNrecomputeSize,		False,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_PRECISION],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
	  sd->curve_info[j].widgets[SDCURVE_PRECISION] =
	    sd->curve_info[j].precision_txt = XtVaCreateManagedWidget
	    ("text",
	     xmTextWidgetClass,		form,
	     XmNcolumns,		2,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_PRECISION],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].precision_txt, XmNfocusCallback,
	     text_focus_cb, (XtPointer)0);
	  sd->curve_info[j].min_lbl = XtVaCreateManagedWidget
	    ("label",
	     xmLabelWidgetClass,	form,
	     XmNalignment,		XmALIGNMENT_END,
	     XmNrecomputeSize,		False,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_MIN],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
	  sd->curve_info[j].widgets[SDCURVE_MIN] =
	    sd->curve_info[j].min_txt = XtVaCreateManagedWidget
	    ("text",
	     xmTextWidgetClass,	form,
	     XmNcolumns,		MAX_REALNUM_LEN,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_MIN],
	     XmNleftOffset,			DEF_WOFFSET,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].min_txt, XmNfocusCallback,
	     text_focus_cb, (XtPointer)0);
	  sd->curve_info[j].max_lbl = XtVaCreateManagedWidget
	    ("label",
	     xmLabelWidgetClass,	form,
	     XmNalignment,		XmALIGNMENT_END,
	     XmNrecomputeSize,		False,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_MAX],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
	  sd->curve_info[j].widgets[SDCURVE_MAX] = 
	    sd->curve_info[j].max_txt = XtVaCreateManagedWidget
	    ("text",
	     xmTextWidgetClass,		form,
	     XmNcolumns,		MAX_REALNUM_LEN,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_MAX],
	     XmNleftOffset,		DEF_WOFFSET,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].max_txt, XmNfocusCallback,
	     text_focus_cb, (XtPointer)0);
	  sd->curve_info[j].widgets[SDCURVE_MODIFY] = XtVaCreateManagedWidget
	    ("pushbutton",
	     xmPushButtonWidgetClass,	form,
	     XmNlabelString,		modify_btn_str[0],
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_MODIFY],
	     XmNleftOffset,		DEF_WOFFSET,
	     XmNuserData,		sd,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].widgets[SDCURVE_MODIFY],
	     XmNactivateCallback, modify_btn_cb, (XtPointer)j);
	  sd->curve_info[j].modifying = 0;
	  
	  sd->curve_info[j].widgets[SDCURVE_REMOVE] = XtVaCreateManagedWidget
	    ("Remove",
	     xmPushButtonWidgetClass,	form,
	     XmNmappedWhenManaged,	False,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		sep,
	     XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	     XmNleftWidget,		curve_column_lbl[SDCURVE_REMOVE],
	     XmNleftOffset,		DEF_WOFFSET,
	     XmNuserData,		sd,
	     NULL);
	  XtAddCallback
	    (sd->curve_info[j].widgets[SDCURVE_REMOVE],
	     XmNactivateCallback, remove_btn_cb, (XtPointer)j);
	}
      
      /* set the column widths approriately */
      for (i = 0; i < SDCURVE_LAST_ATTRIBUTE; i++)
	{
	  XtVaGetValues (sd->curve_info[0].widgets[i], XmNwidth, &dim1, NULL);
	  XtVaGetValues (curve_column_lbl[i], XmNwidth, &dim2, NULL);
	  widths[i] = max (dim1, dim2);
	  XtVaSetValues (curve_column_lbl[i], XmNwidth, widths[i], NULL);
	}
      xstr = XmStringCreateLocalized (SDCurveAttributeStr[SDCURVE_NAME]);
      XtVaSetValues
	(curve_column_lbl[SDCURVE_NAME],
	 XmNlabelString,		xstr,
	 NULL);
      XmStringFree (xstr);
#ifdef STRIPDIALOG_SHOW_STATUS
      xstr = XmStringCreateLocalized (SDCurveAttributeStr[SDCURVE_STATUS]);
      XtVaSetValues
	(curve_column_lbl[SDCURVE_STATUS],
	 XmNlabelString,		xstr,
	 NULL);
      XmStringFree (xstr);
#endif
      for (i = 0; i < STRIP_MAX_CURVES; i++)
	for (j = 0; j < SDCURVE_LAST_ATTRIBUTE; j++)
	  XtVaSetValues
	    (sd->curve_info[i].widgets[j], XmNwidth, widths[j], NULL);
      
      /* Point the Precision, Min and Max widgets to the label widgets, not
       * the text widgets.  Also set the width. */
      for (i = 0; i < STRIP_MAX_CURVES; i++)
	{
	  sd->curve_info[i].widgets[SDCURVE_PRECISION] =
	    sd->curve_info[i].precision_lbl;
	  sd->curve_info[i].widgets[SDCURVE_MIN] =
	    sd->curve_info[i].min_lbl;
	  sd->curve_info[i].widgets[SDCURVE_MAX] =
	    sd->curve_info[i].max_lbl;
	  XtVaSetValues
	    (sd->curve_info[i].widgets[SDCURVE_PRECISION],
	     XmNwidth, widths[SDCURVE_PRECISION],
	     NULL);
	  XtVaSetValues
	    (sd->curve_info[i].widgets[SDCURVE_MIN],
	     XmNwidth, widths[SDCURVE_MIN],
	     NULL);
	  XtVaSetValues
	    (sd->curve_info[i].widgets[SDCURVE_MAX],
	     XmNwidth, widths[SDCURVE_MAX],
	     NULL);
	}

      
	 
      /* the data control form */
      sd->data_form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		base_form,
	 XmNnoResize,			True,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			menu,
	 XmNtopOffset,			DEF_WOFFSET,
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		sd->curve_form,
	 XmNrightOffset,		DEF_WOFFSET,
	 NULL);
      /* create the signal connect area and controls */
      frame = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		sd->data_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XtVaCreateManagedWidget
	("Signal Connect",
	 xmLabelWidgetClass,		frame,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);
      form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		frame,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 XmNfractionBase,		2,
	 NULL);
      sd->connect_txt = w = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			16,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 XmNrightAttachment,		XmATTACH_POSITION,
	 XmNrightPosition,		2,
	 NULL);
      XtAddCallback (sd->connect_txt, XmNactivateCallback, connect_btn_cb, sd);
      XtAddCallback
	(sd->connect_txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      XtVaCreateManagedWidget
	("Plot New Signal: ",
	 xmLabelWidgetClass,		form,
	 XmNalignment,			XmALIGNMENT_BEGINNING,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		0,
	 XmNrightAttachment,		XmATTACH_POSITION,
	 XmNrightPosition,		1,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 XmNleftAttachment,		XmATTACH_FORM,
	 NULL);
      btn = XtVaCreateManagedWidget
	("Connect",
	 xmPushButtonWidgetClass,	form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
	 XmNrightAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 NULL);
      XtAddCallback (btn, XmNactivateCallback, connect_btn_cb, sd);
      w = frame;

      /* create the time control area */
      frame = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		sd->data_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XtVaCreateManagedWidget
	("Time Controls",
	 xmLabelWidgetClass,		frame,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);
      form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		frame,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 XmNfractionBase,		2,
	 NULL);
      
      sd->time_info.ts_hour_txt = w = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			2,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->time_info.ts_hour_lbl = XtVaCreateManagedWidget
	("HH",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->time_info.ts_hour_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->time_info.ts_hour_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->time_info.ts_hour_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->time_info.ts_hour_txt,
	 NULL);
      sd->time_info.widgets[SDTMOPT_TSHOUR] = sd->time_info.ts_hour_lbl;
      lbl = XtVaCreateManagedWidget
	(":",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      sd->time_info.ts_minute_txt = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			2,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			lbl,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->time_info.ts_minute_lbl = XtVaCreateManagedWidget
	("MM",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->time_info.ts_minute_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->time_info.ts_minute_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->time_info.ts_minute_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->time_info.ts_minute_txt,
	 NULL);
      sd->time_info.widgets[SDTMOPT_TSMINUTE] = sd->time_info.ts_minute_lbl;
      lbl = XtVaCreateManagedWidget
	(":",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      sd->time_info.ts_second_txt = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			2,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			lbl,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->time_info.ts_second_lbl = XtVaCreateManagedWidget
	("SS",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->time_info.ts_second_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->time_info.ts_second_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->time_info.ts_second_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->time_info.ts_second_txt,
	 NULL);
      sd->time_info.widgets[SDTMOPT_TSSECOND] = sd->time_info.ts_second_lbl;
      lbl = XtVaCreateManagedWidget
	("Time Span: ",
	 xmLabelWidgetClass,		form,
	 xmLabelWidgetClass,		form,
	 XmNalignment,			XmALIGNMENT_BEGINNING,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		0,
	 XmNrightAttachment,		XmATTACH_POSITION,
	 XmNrightPosition,		1,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      
      sd->time_info.ds_txt = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			4,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->time_info.ds_lbl = XtVaCreateManagedWidget
	("????",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->time_info.ds_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->time_info.ds_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->time_info.ds_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->time_info.ds_txt,
	 NULL);
      sd->time_info.widgets[SDTMOPT_DS] = sd->time_info.ds_lbl;
      w = txt;
      XtVaCreateManagedWidget
	("seconds",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      lbl = XtVaCreateManagedWidget
	("Data Sample Interval: ",
	 xmLabelWidgetClass,		form,
	 xmLabelWidgetClass,		form,
	 XmNalignment,			XmALIGNMENT_BEGINNING,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		0,
	 XmNrightAttachment,		XmATTACH_POSITION,
	 XmNrightPosition,		1,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      
      sd->time_info.gr_txt = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			4,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->time_info.gr_lbl = XtVaCreateManagedWidget
	("????",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->time_info.gr_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->time_info.gr_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->time_info.gr_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->time_info.gr_txt,
	 NULL);
      sd->time_info.widgets[SDTMOPT_GR] = sd->time_info.gr_lbl;
      w = txt;
      XtVaCreateManagedWidget
	("seconds",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_WIDGET,
	 XmNleftWidget,			txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      lbl = XtVaCreateManagedWidget
	("Graph Redraw Interval: ",
	 xmLabelWidgetClass,		form,
	 XmNalignment,			XmALIGNMENT_BEGINNING,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		0,
	 XmNrightAttachment,		XmATTACH_POSITION,
	 XmNrightPosition,		1,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		w,
	 NULL);
      
      sep = XtVaCreateManagedWidget
	("separator",
	 xmSeparatorWidgetClass,	form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);

      xstr = modify_btn_str[sd->time_info.modifying? 1 : 0];
      sd->time_info.widgets[SDTMOPT_MODIFY] = w = XtVaCreateManagedWidget
	("push button",
	 xmPushButtonWidgetClass,	form,
	 XmNlabelString,		xstr,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			sep,
	 XmNrightAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 NULL);
      XtAddCallback (w, XmNactivateCallback, tmmodify_btn_cb, sd);
      sd->time_info.widgets[SDTMOPT_CANCEL] = XtVaCreateManagedWidget
	("Cancel",
	 xmPushButtonWidgetClass,	form,
	 XmNmappedWhenManaged,		sd->time_info.modifying,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			sep,
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		w,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 NULL);
      XtAddCallback
	(sd->time_info.widgets[SDTMOPT_CANCEL],
	 XmNactivateCallback, tmcancel_btn_cb, sd);
      w = frame;
	 
      /* create the graph form and control widgets */
      frame = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		sd->data_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
	 XmNtopAttachment,		DEF_WOFFSET,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XtVaCreateManagedWidget
	("Graph Options",
	 xmLabelWidgetClass,		frame,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);
      form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		frame,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 XmNfractionBase,		2,
	 NULL);

      sd->graph_info.widgets[SDGROPT_FG] = btn = XtVaCreateManagedWidget
	(COLOR_BTN_STR,
	 xmPushButtonWidgetClass,	form,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 XmNbackground,			sd->config->Color.foreground,
	 NULL);
      sd->graph_info.cb_info[SDGROPT_FG].str = "Graph Foreground";
      sd->graph_info.cb_info[SDGROPT_FG].p = sd;
      XtAddCallback
	(btn, XmNactivateCallback, grcolor_btn_cb,
	 &sd->graph_info.cb_info[SDGROPT_FG]);
      lbl = XtVaCreateManagedWidget
	(sd->graph_info.cb_info[SDGROPT_FG].str,
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = btn;
      
      sd->graph_info.widgets[SDGROPT_BG] = btn = XtVaCreateManagedWidget
	(COLOR_BTN_STR,
	 xmPushButtonWidgetClass,	form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 XmNbackground,			sd->config->Color.background,
	 XmNuserData,			sd,
	 NULL);
      sd->graph_info.cb_info[SDGROPT_BG].str = "Graph Background";
      sd->graph_info.cb_info[SDGROPT_BG].p = sd;
      XtAddCallback
	(btn, XmNactivateCallback, grcolor_btn_cb,
	 &sd->graph_info.cb_info[SDGROPT_BG]);
      lbl = XtVaCreateManagedWidget
	(sd->graph_info.cb_info[SDGROPT_BG].str,
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = btn;
      
      sd->graph_info.widgets[SDGROPT_GRIDCLR] = btn = XtVaCreateManagedWidget
	(COLOR_BTN_STR,
	 xmPushButtonWidgetClass,	form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 XmNbackground,			sd->config->Color.grid,
	 XmNuserData,			sd,
	 NULL);
      sd->graph_info.cb_info[SDGROPT_GRIDCLR].str = "Grid Color";
      sd->graph_info.cb_info[SDGROPT_GRIDCLR].p = sd;
      XtAddCallback
	(btn, XmNactivateCallback, grcolor_btn_cb,
	 &sd->graph_info.cb_info[SDGROPT_GRIDCLR]);
      lbl = XtVaCreateManagedWidget
	(sd->graph_info.cb_info[SDGROPT_GRIDCLR].str,
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = btn;
      
      sd->graph_info.widgets[SDGROPT_LGNDCLR] = btn = XtVaCreateManagedWidget
	(COLOR_BTN_STR,
	 xmPushButtonWidgetClass,	form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 XmNbackground,			sd->config->Color.legendtext,
	 XmNuserData,			sd,
	 NULL);
      sd->graph_info.cb_info[SDGROPT_LGNDCLR].str = "Legend Text Color";
      sd->graph_info.cb_info[SDGROPT_LGNDCLR].p = sd;
      XtAddCallback
	(btn, XmNactivateCallback, grcolor_btn_cb,
	 &sd->graph_info.cb_info[SDGROPT_LGNDCLR]);
      lbl = XtVaCreateManagedWidget
	(sd->graph_info.cb_info[SDGROPT_LGNDCLR].str,
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = btn;
      
      sd->graph_info.widgets[SDGROPT_GRIDX] = btn = XtVaCreateManagedWidget
	("on",
	 xmToggleButtonWidgetClass,	form,
	 XmNlabelString,		gridstat_tgl_str[0],
	 XmNset,			True,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (btn, XmNvalueChangedCallback, gropt_tgl_cb, sd);
      lbl = XtVaCreateManagedWidget
	("x-grid lines: ",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = btn;
      
      sd->graph_info.widgets[SDGROPT_GRIDY] = btn = XtVaCreateManagedWidget
	("on",
	 xmToggleButtonWidgetClass,	form,
	 XmNlabelString,		gridstat_tgl_str[0],
	 XmNset,			True,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (btn, XmNvalueChangedCallback, gropt_tgl_cb, sd);
      lbl = XtVaCreateManagedWidget
	("y-grid lines: ",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = lbl;
      
      sd->graph_info.widgets[SDGROPT_YAXISCLR] = btn = XtVaCreateManagedWidget
	("selected curve",
	 xmToggleButtonWidgetClass,	form,
	 XmNlabelString,		yaxisclr_tgl_str[0],
	 XmNset,			True,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (btn, XmNvalueChangedCallback, gropt_tgl_cb, sd);
      lbl = XtVaCreateManagedWidget
	("y-axis label color: ",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			btn,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		btn,
	 NULL);
      w = btn;
      
/* ====== New  ====== */
      sd->graph_info.widgets[SDGROPT_LINEWIDTH] = tmp = XtVaCreateManagedWidget
	("slider",
	 xmScaleWidgetClass,		form,
	 XmNorientation,		XmHORIZONTAL,
	 XmNminimum,			STRIPMIN_OPTION_GRAPH_LINEWIDTH,
	 XmNmaximum,			STRIPMAX_OPTION_GRAPH_LINEWIDTH,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (tmp, XmNvalueChangedCallback, gropt_slider_cb, sd);
      lbl = XtVaCreateManagedWidget
	("data line-width: ",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			tmp,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		tmp,
	 NULL);
      w = tmp;
/* ====== New  ====== */
	
      sd->graph_info.ynum_txt = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			2,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->graph_info.ynum_lbl = XtVaCreateManagedWidget
	("??",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->graph_info.ynum_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->graph_info.ynum_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->graph_info.ynum_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->graph_info.ynum_txt,
	 NULL);
      sd->graph_info.widgets[SDGROPT_YAXISNUM] = sd->graph_info.ynum_lbl;
      lbl = XtVaCreateManagedWidget
	("y-axis hash marks: ",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			txt,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		txt,
	 NULL);
      w = txt;
      
      sd->graph_info.xnum_txt = txt = XtVaCreateManagedWidget
	("text",
	 xmTextWidgetClass,		form,
	 XmNcolumns,			2,
	 XmNmappedWhenManaged,		False,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			w,
         /*
	 XmNrightAttachment,		XmATTACH_FORM,
	 */
	 XmNleftAttachment,		XmATTACH_POSITION,
	 XmNleftPosition,		1,
	 NULL);
      XtAddCallback (txt, XmNfocusCallback, text_focus_cb, (XtPointer)0);
      sd->graph_info.xnum_lbl = XtVaCreateManagedWidget
	("??",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			sd->graph_info.xnum_txt,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->graph_info.xnum_txt,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->graph_info.xnum_txt,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->graph_info.xnum_txt,
	 NULL);
      sd->graph_info.widgets[SDGROPT_XAXISNUM] = sd->graph_info.xnum_lbl;
      lbl = XtVaCreateManagedWidget
	("x-axis hash marks: ",
	 xmLabelWidgetClass,		form,
	 XmNtopAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNtopWidget,			txt,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		txt,
	 NULL);

      sep = XtVaCreateManagedWidget
	("separator",
	 xmSeparatorWidgetClass,	form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			lbl,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      xstr = modify_btn_str[sd->graph_info.modifying? 1 : 0];
      sd->graph_info.widgets[SDGROPT_MODIFY] = w = XtVaCreateManagedWidget
	("push button",
	 xmPushButtonWidgetClass,	form,
	 XmNlabelString,		xstr,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			sep,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XtAddCallback
	(sd->graph_info.widgets[SDGROPT_MODIFY],
	 XmNactivateCallback, grmodify_btn_cb, sd);
      sd->graph_info.widgets[SDGROPT_CANCEL] = XtVaCreateManagedWidget
	("Cancel",
	 xmPushButtonWidgetClass,	form,
	 XmNmappedWhenManaged,		sd->graph_info.modifying,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			sep,
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		w,
	 NULL);
      XtAddCallback
	(sd->graph_info.widgets[SDGROPT_CANCEL], XmNactivateCallback,
	 grcancel_btn_cb, sd);
		     

      
      /* create the miscellaneous area form and controls */
      frame = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		base_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			sd->curve_form,
	 XmNtopOffset,			DEF_WOFFSET,
	 XmNleftAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNleftWidget,			sd->curve_form,
	 XmNrightAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNrightWidget,		sd->curve_form,
	 XmNbottomAttachment,		XmATTACH_OPPOSITE_WIDGET,
	 XmNbottomWidget,		sd->data_form,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);
      XtVaCreateManagedWidget
	("Application Controls",
	 xmLabelWidgetClass,		frame,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 NULL);
      form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		frame,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 NULL);
      btn = XtVaCreateManagedWidget
	("End Program",
	 xmPushButtonWidgetClass,	form,
	 XmNrightAttachment,		XmATTACH_FORM,
	 XmNrightOffset,		DEF_WOFFSET,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 XmNbottomOffset,		DEF_WOFFSET,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(btn, XmNactivateCallback, ctrl_btn_cb, (XtPointer)SDCALLBACK_QUIT);
      btn = XtVaCreateManagedWidget
	("Dismiss",
	 xmPushButtonWidgetClass,	form,
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		btn,
	 XmNrightOffset,		DEF_WOFFSET,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 XmNbottomOffset,		DEF_WOFFSET,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(btn, XmNactivateCallback, ctrl_btn_cb, (XtPointer)SDCALLBACK_DISMISS);
      btn = XtVaCreateManagedWidget
	("Clear StripChart",
	 xmPushButtonWidgetClass,	form,
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		btn,
	 XmNrightOffset,		DEF_WOFFSET,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 XmNbottomOffset,		DEF_WOFFSET,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(btn, XmNactivateCallback, ctrl_btn_cb, (XtPointer)SDCALLBACK_CLEAR);
      btn = XtVaCreateManagedWidget
	("push_button",
	 xmPushButtonWidgetClass,	form,
	 XmNlabelString,		showhide_btn_str[0],
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		btn,
	 XmNrightOffset,		DEF_WOFFSET,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 XmNbottomOffset,		DEF_WOFFSET,
	 XmNuserData,			sd,
	 NULL);
      XtAddCallback
	(btn, XmNactivateCallback, ctrl_btn_cb, (XtPointer)SDCALLBACK_SHOW);

      XtRealizeWidget (sd->shell);
      XUnmapWindow (sd->display, XtWindow (sd->shell));

      for (i = 0; i < STRIP_MAX_CURVES; i++)
	sd->curve_info[i].modified = 0;
      sd->time_info.modified = 0;
      sd->graph_info.modified = 0;

      StripDialog_cfgupdate (STRIPCFGMASK_ALL & ~STRIPCFGMASK_CURVE, sd);
    }
  return (StripDialog)sd;
}


/*
 * StripDialog_delete
 */
void	StripDialog_delete	(StripDialog the_sd)
{
  free ((StripDialogInfo *)the_sd);
}


/*
 * StripDialog_setattr
 */
int	StripDialog_setattr	(StripDialog the_sd, ...)
{
  va_list		ap;
  int			attrib;
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int			ret_val = 1;
  int			count;
  int			i;
  SDWindowMenuItem	*items;

  va_start (ap, the_sd);
  for (attrib = va_arg (ap, StripDialogAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripDialogAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPDIALOG_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPDIALOG_CONNECT_FUNC:
	    sd->callback[SDCALLBACK_CONNECT].func = va_arg (ap, SDcallback);
	    break;
	  case STRIPDIALOG_CONNECT_DATA:
	    sd->callback[SDCALLBACK_CONNECT].data = va_arg (ap, void *);
	    break;
	  case STRIPDIALOG_SHOW_FUNC:
	    sd->callback[SDCALLBACK_SHOW].func = va_arg (ap, SDcallback);
	    break;
	  case STRIPDIALOG_SHOW_DATA:
	    sd->callback[SDCALLBACK_SHOW].data = va_arg (ap, void *);
	    break;
	  case STRIPDIALOG_CLEAR_FUNC:
	    sd->callback[SDCALLBACK_CLEAR].func = va_arg (ap, SDcallback);
	    break;
	  case STRIPDIALOG_CLEAR_DATA:
	    sd->callback[SDCALLBACK_CLEAR].data = va_arg (ap, void *);
	    break;
	  case STRIPDIALOG_DELETE_FUNC:
	    sd->callback[SDCALLBACK_DELETE].func = va_arg (ap, SDcallback);
	    break;
	  case STRIPDIALOG_DELETE_DATA:
	    sd->callback[SDCALLBACK_DELETE].data = va_arg (ap, void *);
	    break;
	  case STRIPDIALOG_DISMISS_FUNC:
	    sd->callback[SDCALLBACK_DISMISS].func = va_arg (ap, SDcallback);
	    break;
	  case STRIPDIALOG_DISMISS_DATA:
	    sd->callback[SDCALLBACK_DISMISS].data = va_arg (ap, void *);
	    break;
	  case STRIPDIALOG_QUIT_FUNC:
	    sd->callback[SDCALLBACK_QUIT].func = va_arg (ap, SDcallback);
	    break;
	  case STRIPDIALOG_QUIT_DATA:
	    sd->callback[SDCALLBACK_QUIT].data = va_arg (ap, void *);
	    break;
	  case STRIPDIALOG_WINDOW_MENU:
	    items = va_arg (ap, SDWindowMenuItem *);
	    count = va_arg (ap, int);
	    for (i = 0; (i < count) && (i < MAX_WINDOW_MENU_ITEMS); i++)
	      {
		sd->window_menu_info.items[i].info = items[i];
		xstr = XmStringCreateLocalized (items[i].name);
		XtVaSetValues
		  (sd->window_menu_info.items[i].entry,
		   XmNlabelString,		xstr,
		   NULL);
		XmStringFree (xstr);
		XtManageChild (sd->window_menu_info.items[i].entry);
	      }
	    sd->window_menu_info.count = i;
	    
	    for (; i < MAX_WINDOW_MENU_ITEMS; i++)
	      XtUnmanageChild (sd->window_menu_info.items[i].entry);
	  }
      else break;
    }

  va_end (ap);
  return ret_val;
}


/*
 * StripDialog_getattr
 */
int	StripDialog_getattr	(StripDialog the_sd, ...)
{
  va_list		ap;
  int			attrib;
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int			ret_val = 1;

  va_start (ap, the_sd);
  for (attrib = va_arg (ap, StripDialogAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripDialogAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPDIALOG_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPDIALOG_SHELL_WIDGET:
	    *(va_arg (ap, Widget *)) = sd->shell;
	    break;
	  case STRIPDIALOG_CONNECT_FUNC:
	    *(va_arg (ap, SDcallback *)) =
	      sd->callback[SDCALLBACK_CONNECT].func;
	    break;
	  case STRIPDIALOG_CONNECT_DATA:
	    *(va_arg (ap, void **)) =
	      sd->callback[SDCALLBACK_CONNECT].data;
	    break;
	  case STRIPDIALOG_SHOW_FUNC:
	    *(va_arg (ap, SDcallback *)) =
	      sd->callback[SDCALLBACK_SHOW].func;
	    break;
	  case STRIPDIALOG_SHOW_DATA:
	    *(va_arg (ap, void **)) =
	      sd->callback[SDCALLBACK_SHOW].data;
	    break;
	  case STRIPDIALOG_CLEAR_FUNC:
	    *(va_arg (ap, SDcallback *)) =
	      sd->callback[SDCALLBACK_CLEAR].func;
	    break;
	  case STRIPDIALOG_CLEAR_DATA:
	    *(va_arg (ap, void **)) =
	      sd->callback[SDCALLBACK_CLEAR].data;
	    break;
	  case STRIPDIALOG_DELETE_FUNC:
	    *(va_arg (ap, SDcallback *)) =
	      sd->callback[SDCALLBACK_DELETE].func;
	    break;
	  case STRIPDIALOG_DELETE_DATA:
	    *(va_arg (ap, void **)) =
	      sd->callback[SDCALLBACK_DELETE].data;
	    break;
	  case STRIPDIALOG_DISMISS_FUNC:
	    *(va_arg (ap, SDcallback *)) =
	      sd->callback[SDCALLBACK_DISMISS].func;
	    break;
	  case STRIPDIALOG_DISMISS_DATA:
	    *(va_arg (ap, void **)) =
	      sd->callback[SDCALLBACK_DISMISS].data;
	    break;
	  case STRIPDIALOG_QUIT_FUNC:
	    *(va_arg (ap, SDcallback *)) =
	      sd->callback[SDCALLBACK_QUIT].func;
	    break;
	  case STRIPDIALOG_QUIT_DATA:
	    *(va_arg (ap, void **)) =
	      sd->callback[SDCALLBACK_QUIT].data;
	    break;
	  }
      else break;
    }

  va_end (ap);
  return ret_val;
}


/*
 * StripDialog_popup
 */
void	StripDialog_popup	(StripDialog the_sd)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;

  XMapRaised (XtDisplay (sd->shell), XtWindow(sd->shell));
}


/*
 * StripDialog_popdown
 */
void	StripDialog_popdown	(StripDialog the_sd)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;

  XUnmapWindow (XtDisplay (sd->shell), XtWindow(sd->shell));
}


/*
 * StripDialog_addcurve
 */
int	StripDialog_addcurve		(StripDialog the_sd, StripCurve curve)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int 			ret_val;

  if ((ret_val = (sd->sdcurve_count < STRIP_MAX_CURVES)))
    {
      insert_sdcurve (sd, sd->sdcurve_count, curve, False);
      show_sdcurve (sd, sd->sdcurve_count);
      sd->sdcurve_count++;

      /* if the curve's name matches the string in the connect text box,
       * then clear the the text box
       */
      if (strcmp
	  (XmTextGetString (sd->connect_txt),
	   StripCurve_getattr_val (curve, STRIPCURVE_NAME))
	  == 0)
	XmTextSetString (sd->connect_txt, "");
    }
  return ret_val;
}


/*
 * StripDialog_removecurve
 */
int	StripDialog_removecurve	(StripDialog the_sd, StripCurve curve)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int 			ret_val;
  int			i;

  for (i = 0; i < sd->sdcurve_count; i++)
    if (sd->curve_info[i].curve == curve)
      break;

  if ((ret_val = (i < sd->sdcurve_count)))
    delete_sdcurve (sd, i);
  return ret_val;
}


/*
 * StripDialog_addsomecurves
 */
int	StripDialog_addsomecurves	(StripDialog	the_sd,
					 StripCurve 	curves[])
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int			i, n;
  int 			ret_val;

  for (n = 0; curves[n]; n++);	/* how many curves? */
  
  if ((ret_val = (n < (STRIP_MAX_CURVES - sd->sdcurve_count))))
    {
      XtUnmapWidget (sd->curve_form);
      for (i = 0; i < n; i++)
	{
	  insert_sdcurve (sd, sd->sdcurve_count+i, curves[i], False);
	  show_sdcurve (sd, sd->sdcurve_count+i);
	}
      sd->sdcurve_count += n;
      XtMapWidget (sd->curve_form);
    }
  return ret_val;
}


/*
 * StripDialog_removesomecurves
 */
int	StripDialog_removesomecurves	(StripDialog	the_sd,
					 StripCurve	curves[])
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int 			ret_val;
  int			i, j, n;

  XtUnmapWidget (sd->curve_form);
  
  for (i = 0,  n = 0; i < sd->sdcurve_count; i++)
    for (j = 0; curves[j]; j++)
      if (sd->curve_info[i].curve == curves[j])
	{
	  /* must remove this curve */
	  sd->curve_info[i].curve = 0;
	  n++;
	}
  
  for (i = 0, j = 1; (i < sd->sdcurve_count) && (j < sd->sdcurve_count); i++)
    if (sd->curve_info[i].curve == 0)
      {
	for (j = i+1; j < sd->sdcurve_count; j++)
	  if (sd->curve_info[j].curve != 0)
	    break;
	if (j < sd->sdcurve_count)
	  {
	    insert_sdcurve
	      (sd, i, sd->curve_info[j].curve,
	       sd->curve_info[j].modifying);
	    sd->curve_info[j].curve = 0;
	  }
      }
  
  sd->sdcurve_count -= n;
  for (i = 0; i < n; i++)
    hide_sdcurve (sd, sd->sdcurve_count+i);
  
  XtMapWidget (sd->curve_form);

  return 1;
}


/*
 * StripDialog_update_curvestat
 */
int	StripDialog_update_curvestat	(StripDialog the_sd, StripCurve curve)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;
  int 			ret_val;
  int			i;

  for (i = 0; i < sd->sdcurve_count; i++)
    if (sd->curve_info[i].curve == curve)
      break;

  if ((ret_val = (i < sd->sdcurve_count)))
    setwidgetval_status	(sd, i, ((StripCurveInfo *)curve)->status);
  return ret_val;
}


/*
 * StripDialog_isviewable
 */
int	StripDialog_isviewable	(StripDialog the_sd)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;

  return (window_isviewable (sd->display, XtWindow (sd->shell)));
}


/*
 * StripDialog_ismapped
 */
int	StripDialog_ismapped	(StripDialog the_sd)
{
  StripDialogInfo	*sd = (StripDialogInfo *)the_sd;

  return (window_ismapped (sd->display, XtWindow (sd->shell)));
}


/* ====== Static Function Definitions ====== */
/*
 * insert_sdcurve
 */
static void	insert_sdcurve	(StripDialogInfo	*sd,
				 int 			idx,
				 StripCurve		curve,
				 int			modifying)
{
  StripCurveInfo	*sc;
  
  sc = (StripCurveInfo *)(sd->curve_info[idx].curve = curve);

  setwidgetval_name 		(sd, idx, sc->details->name);
  setwidgetval_status 		(sd, idx, sc->status);
  setwidgetval_color 		(sd, idx, sc->details->pixel);
  setwidgetval_penstat		(sd, idx, sc->details->penstat);
  setwidgetval_plotstat		(sd, idx, sc->details->plotstat);
  setwidgetval_precision	(sd, idx, sc->details->precision);
  setwidgetval_min		(sd, idx, sc->details->min);
  setwidgetval_max		(sd, idx, sc->details->max);
  
  XtUnmapWidget (sd->curve_info[idx].widgets[SDCURVE_PRECISION]);
  XtUnmapWidget (sd->curve_info[idx].widgets[SDCURVE_MIN]);
  XtUnmapWidget (sd->curve_info[idx].widgets[SDCURVE_MAX]);
  setwidgetval_modify		(sd, idx, modifying);
  XtMapWidget (sd->curve_info[idx].widgets[SDCURVE_PRECISION]);
  XtMapWidget (sd->curve_info[idx].widgets[SDCURVE_MIN]);
  XtMapWidget (sd->curve_info[idx].widgets[SDCURVE_MAX]);
}


/*
 * delete_sdcurve
 */
static void	delete_sdcurve	(StripDialogInfo *sd, int idx)
{
  int	i;

  XtUnmapWidget (sd->curve_form);
  /* shift all the remaining curves up one */
  for (i = idx+1; i < sd->sdcurve_count; i++)
    {
      insert_sdcurve
	(sd, i-1, sd->curve_info[i].curve, sd->curve_info[i].modifying);
    }
  sd->sdcurve_count--;
  hide_sdcurve (sd, sd->sdcurve_count);
  XtMapWidget (sd->curve_form);
}


/*
 * show_sdcurve
 */
static void	show_sdcurve	(StripDialogInfo *sd, int idx)
{
  int 	i;
  for (i = 0; i < SDCURVE_LAST_ATTRIBUTE; i++)
    XtMapWidget (sd->curve_info[idx].widgets[i]);
  XtMapWidget (sd->curve_info[idx].top_sep);
}
     

/*
 * hide_sdcurve
 */
static void	hide_sdcurve	(StripDialogInfo *sd, int idx)
{
  int 	i;
  for (i = 0; i < SDCURVE_LAST_ATTRIBUTE; i++)
    XtUnmapWidget (sd->curve_info[idx].widgets[i]);
  XtUnmapWidget (sd->curve_info[idx].top_sep);
}


/*
 * setwidgetval_name
 */
static void	setwidgetval_name	(StripDialogInfo 	*sd,
					 int 			which,
					 char 			*name)
{
  xstr = XmStringCreateLocalized (name);
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_NAME],
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


/*
 * setwidgetval_status
 */
static void	setwidgetval_status	(StripDialogInfo 	*sd,
					 int 			which,
					 StripCurveStatus	stat)
{
  int	s = (!(stat & STRIPCURVE_CONNECTED)? 0 : 1);
  
#ifdef STRIPDIALOG_SHOW_STATUS
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_STATUS],
     XmNlabelString,			status_lbl_str[s],
     NULL);
#endif

  /* if the curve is not connected, grey out its name */
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_NAME],
     XmNsensitive,			s,
     NULL);
}


/*
 * setwidgetval_color
 */
static void	setwidgetval_color	(StripDialogInfo 	*sd,
					 int 			which,
					 Pixel 			color)
{
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_COLOR],
     XmNbackground,			color,
     NULL);
}


/*
 * setwidgetval_penstat
 */
static void	setwidgetval_penstat	(StripDialogInfo 	*sd,
					 int 			which,
					 int 			stat)
{
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_PENSTAT],
     XmNset,				stat,
     XmNlabelString,			penstat_tgl_str[stat? 0 : 1],
     NULL);
}


/*
 * setwidgetval_plotstat
 */
static void	setwidgetval_plotstat	(StripDialogInfo 	*sd,
					 int 			which,
					 int 			stat)
{
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_PLOTSTAT],
     XmNset,				stat,
     XmNlabelString,			plotstat_tgl_str[stat? 0 : 1],
     NULL);
}


/*
 * setwidgetval_precision
 */
static void	setwidgetval_precision	(StripDialogInfo 	*sd,
					 int 			which,
					 int 			p)
{
  sprintf (char_buf, "%d", p);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->curve_info[which].precision_txt, char_buf);
  XtVaSetValues
    (sd->curve_info[which].precision_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


/*
 * setwidgetval_min
 */
static void	setwidgetval_min	(StripDialogInfo 	*sd,
					 int 			which,
					 double 		val)
{
  StripCurveInfo 	*sc = (StripCurveInfo *)sd->curve_info[which].curve;

  dbl2str (val, sc->details->precision, char_buf, MAX_REALNUM_LEN);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->curve_info[which].min_txt, char_buf);
  XtVaSetValues
    (sd->curve_info[which].min_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


/*
 * setwidgetval_max
 */
static void	setwidgetval_max	(StripDialogInfo 	*sd,
					 int 			which,
					 double 		val)
{
  StripCurveInfo 	*sc = (StripCurveInfo *)sd->curve_info[which].curve;

  dbl2str (val, sc->details->precision, char_buf, MAX_REALNUM_LEN);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->curve_info[which].max_txt, char_buf);
  XtVaSetValues
    (sd->curve_info[which].max_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


/*
 * setwidgetval_modify
 */
static void	setwidgetval_modify	(StripDialogInfo 	*sd,
					 int 			which,
					 int 			modify)
{
  if ((sd->curve_info[which].modifying = modify))
    {
      sd->curve_info[which].widgets[SDCURVE_PRECISION] =
	sd->curve_info[which].precision_txt;
      sd->curve_info[which].widgets[SDCURVE_MIN] =
	sd->curve_info[which].min_txt;
      sd->curve_info[which].widgets[SDCURVE_MAX] =
	sd->curve_info[which].max_txt;
      xstr =  modify_btn_str[1];
    }
  else
    {
      sd->curve_info[which].widgets[SDCURVE_PRECISION] =
	sd->curve_info[which].precision_lbl;
      sd->curve_info[which].widgets[SDCURVE_MIN] =
	sd->curve_info[which].min_lbl;
      sd->curve_info[which].widgets[SDCURVE_MAX] =
	sd->curve_info[which].max_lbl;
      xstr =  modify_btn_str[0];
    }
  XtVaSetValues
    (sd->curve_info[which].widgets[SDCURVE_MODIFY],
     XmNlabelString, 			xstr,
     NULL);
}


/*
 * getwidgetval_name
 */
static char	*getwidgetval_name	(StripDialogInfo *sd, int which)
{
  return XmTextGetString (sd->curve_info[which].widgets[SDCURVE_NAME]);
}


/*
 * getwidgetval_status
 */
static char	*getwidgetval_status	(StripDialogInfo *sd, int which)
{
#ifdef STRIPDIALOG_SHOW_STATUS
  return XmTextGetString (sd->curve_info[which].widgets[SDCURVE_STATUS]);
#else
  return "unknown";
#endif
}


/*
 * getwidgetval_color
 */
static Pixel	getwidgetval_color	(StripDialogInfo *sd, int which)
{
  Pixel	color;
  XtVaGetValues
    (sd->curve_info[which].widgets[SDCURVE_COLOR],
     XmNforeground,			&color,
     NULL);
  return color;
}


/*
 * getwidgetval_penstat
 */
static int	getwidgetval_penstat	(StripDialogInfo *sd, int which)
{
  return XmToggleButtonGetState
    (sd->curve_info[which].widgets[SDCURVE_PENSTAT]);
}


/*
 * getwidgetval_plotstat
 */
static int	getwidgetval_plotstat	(StripDialogInfo *sd, int which)
{
  return XmToggleButtonGetState
    (sd->curve_info[which].widgets[SDCURVE_PLOTSTAT]);
}


/*
 * getwidgetval_precision
 */
static char	*getwidgetval_precision	(StripDialogInfo 	*sd,
					 int 			which,
					 int			*val)
{
  char	*str;

  str = XmTextGetString (sd->curve_info[which].precision_txt);
  *val = atoi (str);
  return str;
}


/*
 * getwidgetval_min
 */
static char	*getwidgetval_min	(StripDialogInfo 	*sd,
					 int 			which,
					 double			*val)
{
  char	*str;

  str = XmTextGetString (sd->curve_info[which].min_txt);
  *val = atof (str);
  return str;
}


/*
 * getwidgetval_max
 */
static char	*getwidgetval_max	(StripDialogInfo 	*sd,
					 int 			which,
					 double			*val)
{
  char	*str;

  str = XmTextGetString (sd->curve_info[which].max_txt);
  *val = atof (str);
  return str;
}


static void	setwidgetval_tm_tshour		(StripDialogInfo *sd, int val)
{
  sprintf (char_buf, "%d", val);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->time_info.ts_hour_txt, char_buf);
  XtVaSetValues
    (sd->time_info.ts_hour_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_tm_tsminute	(StripDialogInfo *sd, int val)
{
  sprintf (char_buf, "%d", val);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->time_info.ts_minute_txt, char_buf);
  XtVaSetValues
    (sd->time_info.ts_minute_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_tm_tssecond	(StripDialogInfo *sd, int val)
{
  sprintf (char_buf, "%d", val);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->time_info.ts_second_txt, char_buf);
  XtVaSetValues
    (sd->time_info.ts_second_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_tm_ds	(StripDialogInfo 	*sd,
					 double			val)
{
  gcvt (val, 3, char_buf);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->time_info.ds_txt, char_buf);
  XtVaSetValues
    (sd->time_info.ds_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_tm_gr	(StripDialogInfo 	*sd,
					 double 		val)
{
  gcvt (val, 3, char_buf);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->time_info.gr_txt, char_buf);
  XtVaSetValues
    (sd->time_info.gr_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_tm_modify	(StripDialogInfo 	*sd,
					 int 			modify)
{
  if ((sd->time_info.modifying = modify))
    {
      sd->time_info.widgets[SDTMOPT_TSHOUR] = sd->time_info.ts_hour_txt;
      sd->time_info.widgets[SDTMOPT_TSMINUTE] = sd->time_info.ts_minute_txt;
      sd->time_info.widgets[SDTMOPT_TSSECOND] = sd->time_info.ts_second_txt;
      sd->time_info.widgets[SDTMOPT_DS] = sd->time_info.ds_txt;
      sd->time_info.widgets[SDTMOPT_GR] = sd->time_info.gr_txt;
      xstr = modify_btn_str[1];
    }
  else
    {
      sd->time_info.widgets[SDTMOPT_TSHOUR] = sd->time_info.ts_hour_lbl;
      sd->time_info.widgets[SDTMOPT_TSMINUTE] = sd->time_info.ts_minute_lbl;
      sd->time_info.widgets[SDTMOPT_TSSECOND] = sd->time_info.ts_second_lbl;
      sd->time_info.widgets[SDTMOPT_DS] = sd->time_info.ds_lbl;
      sd->time_info.widgets[SDTMOPT_GR] = sd->time_info.gr_lbl;
      xstr = modify_btn_str[0];
    }
  XtVaSetValues
    (sd->time_info.widgets[SDTMOPT_MODIFY],
     XmNlabelString, 			xstr,
     NULL);
}


static char	*getwidgetval_tm_tshour		(StripDialogInfo 	*sd,
						 int 			*val)
{
  char	*str;

  str = XmTextGetString (sd->time_info.ts_hour_txt);
  *val = atoi (str);
  return str;
}


static char	*getwidgetval_tm_tsminute	(StripDialogInfo 	*sd,
						 int 			*val)
{
  char	*str;

  str = XmTextGetString (sd->time_info.ts_minute_txt);
  *val = atoi (str);
  return str;
}


static char	*getwidgetval_tm_tssecond	(StripDialogInfo 	*sd,
						 int 			*val)
{
  char	*str;

  str = XmTextGetString (sd->time_info.ts_second_txt);
  *val = atoi (str);
  return str;
}


static char	*getwidgetval_tm_ds		(StripDialogInfo	*sd,
						 double			*val)
{
  char	*str;

  char *p;
  str = XmTextGetString (sd->time_info.ds_txt);
  *val = atof (str);
  return str;
}


static char	*getwidgetval_tm_gr		(StripDialogInfo 	*sd,
						 double			*val)
{
  char	*str;

  str = XmTextGetString (sd->time_info.gr_txt);
  *val = atof (str);
  return str;
}


/* graph controls */
static void	setwidgetval_gr_fg		(StripDialogInfo 	*sd,
						 Pixel 			val)
{
}


static void	setwidgetval_gr_bg		(StripDialogInfo 	*sd,
						 Pixel 			val)
{
}


static void	setwidgetval_gr_gridclr		(StripDialogInfo 	*sd,
						 Pixel 			val)
{
}


static void	setwidgetval_gr_lgndclr		(StripDialogInfo 	*sd,
						 Pixel 			val)
{
}


static void	setwidgetval_gr_linewidth	(StripDialogInfo 	*sd,
						 int 			val)
{
  XtVaSetValues
    (sd->graph_info.widgets[SDGROPT_LINEWIDTH],
     XmNvalue,				val,
     NULL);
}


static void	setwidgetval_gr_gridx		(StripDialogInfo 	*sd,
						 int 			stat)
{
  XtVaSetValues
    (sd->graph_info.widgets[SDGROPT_GRIDX],
     XmNset,				stat,
     XmNlabelString,			gridstat_tgl_str[stat? 0 : 1],
     NULL);
}


static void	setwidgetval_gr_gridy		(StripDialogInfo 	*sd,
						 int 			stat)
{
  XtVaSetValues
    (sd->graph_info.widgets[SDGROPT_GRIDY],
     XmNset,				stat,
     XmNlabelString,			gridstat_tgl_str[stat? 0 : 1],
     NULL);
}


static void	setwidgetval_gr_yaxisclr	(StripDialogInfo 	*sd,
						 int 			stat)
{
  XtVaSetValues
    (sd->graph_info.widgets[SDGROPT_YAXISCLR],
     XmNset,				stat,
     XmNlabelString,			yaxisclr_tgl_str[stat? 0 : 1],
     NULL);
}


static void	setwidgetval_gr_yaxisnum	(StripDialogInfo 	*sd,
						 int 			val)
{
  sprintf (char_buf, "%d", val);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->graph_info.ynum_txt, char_buf);
  XtVaSetValues
    (sd->graph_info.ynum_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_gr_xaxisnum	(StripDialogInfo 	*sd,
						 int 			val)
{
  sprintf (char_buf, "%d", val);
  xstr = XmStringCreateLocalized (char_buf);
  XmTextSetString (sd->graph_info.xnum_txt, char_buf);
  XtVaSetValues
    (sd->graph_info.xnum_lbl,
     XmNlabelString,			xstr,
     NULL);
  XmStringFree (xstr);
}


static void	setwidgetval_gr_modify		(StripDialogInfo 	*sd,
						 int 			modify)
{
  if ((sd->graph_info.modifying = modify))
    {
      sd->graph_info.widgets[SDGROPT_YAXISNUM] = sd->graph_info.ynum_txt;
      sd->graph_info.widgets[SDGROPT_XAXISNUM] = sd->graph_info.xnum_txt;
      xstr = modify_btn_str[1];
    }
  else
    {
      sd->graph_info.widgets[SDGROPT_YAXISNUM] = sd->graph_info.ynum_lbl;
      sd->graph_info.widgets[SDGROPT_XAXISNUM] = sd->graph_info.xnum_lbl;
      xstr = modify_btn_str[0];
    }
  XtVaSetValues
    (sd->graph_info.widgets[SDGROPT_MODIFY],
     XmNlabelString, 			xstr,
     NULL);
}


static Pixel	getwidgetval_gr_fg		(StripDialogInfo *sd)
{
}


static Pixel	getwidgetval_gr_bg		(StripDialogInfo *sd)
{
}


static Pixel	getwidgetval_gr_gridclr		(StripDialogInfo *sd)
{
}


static Pixel	getwidgetval_gr_lgndclr		(StripDialogInfo *sd)
{
}


static int	getwidgetval_gr_linewidth	(StripDialogInfo *sd)
{
}


static int	getwidgetval_gr_gridx		(StripDialogInfo *sd)
{
}


static int	getwidgetval_gr_gridy		(StripDialogInfo *sd)
{
}


static int	getwidgetval_gr_yaxisclr	(StripDialogInfo *sd)
{
}


static char	*getwidgetval_gr_yaxisnum	(StripDialogInfo 	*sd,
						 int 			*val)
{
  char	*str;

  str = XmTextGetString (sd->graph_info.ynum_txt);
  *val = atoi (str);
  return str;
}


static char	*getwidgetval_gr_xaxisnum	(StripDialogInfo 	*sd,
						 int 			*val)
{
  char	*str;

  str = XmTextGetString (sd->graph_info.xnum_txt);
  *val = atoi (str);
  return str;
}




/* ====== Static Callback Function Definitions ====== */
static void	connect_btn_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmAnyCallbackStruct	*cbs = (XmAnyCallbackStruct *)call;
  XEvent		*event = cbs->event;
  StripDialogInfo	*sd = (StripDialogInfo *)data;
  char			*str;
  Time			when;

  when = (event->type == ButtonPress? event->xbutton.time : event->xkey.time);

  if (strlen (str = XmTextGetString (sd->connect_txt)) > 0)
    if (sd->callback[SDCALLBACK_CONNECT].func != NULL)
      sd->callback[SDCALLBACK_CONNECT].func
	(sd->callback[SDCALLBACK_CONNECT].data, str);

  XmTextSetSelection
    (sd->connect_txt, (XmTextPosition)0, (XmTextPosition)strlen(str), when);
}

static void	color_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd;
  StripCurveInfo	*sc;
  int			which = (int)data;

  XtVaGetValues (w, XmNuserData, &sd, NULL);
  sc = (StripCurveInfo *)sd->curve_info[which].curve;
  ColorDialog_popup (sd->clrdlg, sc->details->name, sc->details->pixel);
}

static void	modify_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd;
  StripCurveInfo	*sc;
  StripConfigMask	mask = 0;
  int			which = (int)data;
  int			ival;
  double		dval;


  XtVaGetValues (w, XmNuserData, &sd, NULL);
  XtUnmapWidget (sd->curve_info[which].widgets[SDCURVE_PRECISION]);
  XtUnmapWidget (sd->curve_info[which].widgets[SDCURVE_MIN]);
  XtUnmapWidget (sd->curve_info[which].widgets[SDCURVE_MAX]);
  
  if (sd->curve_info[which].modifying)
    {
      sc = (StripCurveInfo *)sd->curve_info[which].curve;
      
      getwidgetval_precision (sd, which, &ival);
      if (StripCurve_setattr (sc, STRIPCURVE_PRECISION, ival, 0))
	{
	  setwidgetval_precision 	(sd, which, ival);
	  mask |= STRIPCFGMASK_CURVE_PRECISION;
	}
      else setwidgetval_precision (sd, which, sc->details->precision);
      
      getwidgetval_min (sd, which, &dval);
      if (StripCurve_setattr (sc, STRIPCURVE_MIN, dval, 0))
	{
	  setwidgetval_min (sd, which, dval);
	  mask |= STRIPCFGMASK_CURVE_MIN;
	}
      else setwidgetval_min (sd, which, sc->details->min);
      
      getwidgetval_max (sd, which, &dval);
      if (StripCurve_setattr (sc, STRIPCURVE_MAX, dval, 0))
	{
	  setwidgetval_max (sd, which, dval);
	  mask |= STRIPCFGMASK_CURVE_MAX;
	}
      else setwidgetval_max (sd, which, sc->details->min);

      if (mask) StripConfig_update (sd->config, mask);
    }

  setwidgetval_modify (sd, which, !sd->curve_info[which].modifying);
  
  XtMapWidget (sd->curve_info[which].widgets[SDCURVE_PRECISION]);
  XtMapWidget (sd->curve_info[which].widgets[SDCURVE_MIN]);
  XtMapWidget (sd->curve_info[which].widgets[SDCURVE_MAX]);
}

static void	penstat_tgl_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmToggleButtonCallbackStruct	*cbs = (XmToggleButtonCallbackStruct *)call;
  StripDialogInfo	*sd;
  StripCurve		*sc;
  int			idx = (int)data;

  XtVaGetValues (w, XmNuserData, &sd, NULL);
  sc = (StripCurve)sd->curve_info[idx].curve;
  setwidgetval_penstat (sd, idx, cbs->set);
  StripCurve_setattr (sc, STRIPCURVE_PENSTAT, cbs->set, 0);
  StripConfig_update (sd->config, STRIPCFGMASK_CURVE_PENSTAT);
}

static void	plotstat_tgl_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmToggleButtonCallbackStruct	*cbs = (XmToggleButtonCallbackStruct *)call;
  StripDialogInfo	*sd;
  StripCurve		*sc;
  int			idx = (int)data;

  XtVaGetValues (w, XmNuserData, &sd, NULL);
  sc = (StripCurve)sd->curve_info[idx].curve;
  setwidgetval_plotstat (sd, idx, cbs->set);
  StripCurve_setattr (sc, STRIPCURVE_PLOTSTAT, cbs->set, 0);
  StripConfig_update (sd->config, STRIPCFGMASK_CURVE_PLOTSTAT);
}


static void	text_focus_cb	(Widget w, XtPointer data, XtPointer call)
{
  /*
  int	s = strlen (XmTextGetString (w));
  XmTextSetSelection (w, 0, s, CurrentTime);
  */
}

static void	remove_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd;
  StripCurve		sc;
  int			idx = (int)data;
  
  XtVaGetValues (w, XmNuserData, &sd, NULL);
  sc = sd->curve_info[idx].curve;
  delete_sdcurve (sd, idx);
  if (sd->callback[SDCALLBACK_DELETE].func != NULL)
    sd->callback[SDCALLBACK_DELETE].func
      (sd->callback[SDCALLBACK_DELETE].data, sc);
}

static void	tmmodify_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd = (StripDialogInfo *)data;
  StripConfigMask	mask = 0;
  int			i;
  int			h, m, s;
  double		dval;

  for (i = SDTMOPT_TSHOUR; i <= SDTMOPT_GR; i++)
    XtUnmapWidget (sd->time_info.widgets[i]);
  
  if (sd->time_info.modifying)
    {
      XtUnmapWidget (sd->time_info.widgets[SDTMOPT_CANCEL]);
      
      getwidgetval_tm_tshour (sd, &h);
      getwidgetval_tm_tsminute (sd, &m);
      getwidgetval_tm_tssecond (sd, &s);

      h = max (0, h);
      m = max (0, m);
      s = max (0, s);
      
      if (StripConfig_setattr
	  (sd->config,
	   STRIPCONFIG_TIME_TIMESPAN, 	(unsigned)(h*3600 + m*60 + s),
	   0))
	mask |= STRIPCFGMASK_TIME_TIMESPAN;
      else
	{
	  sec2hms (sd->config->Time.timespan, &h, &m, &s);
	  setwidgetval_tm_tshour (sd, h);
	  setwidgetval_tm_tsminute (sd, m);
	  setwidgetval_tm_tssecond (sd, s);
	}
      
      getwidgetval_tm_ds (sd, &dval);
      if (StripConfig_setattr
	  (sd->config,
	   STRIPCONFIG_TIME_SAMPLE_INTERVAL, dval,
	   0))
	mask |= STRIPCFGMASK_TIME_SAMPLE_INTERVAL;
      else setwidgetval_tm_ds (sd, sd->config->Time.sample_interval);
      
      getwidgetval_tm_gr (sd, &dval);
      if (StripConfig_setattr
	  (sd->config,
	   STRIPCONFIG_TIME_REFRESH_INTERVAL, dval,
	   0))
	mask |= STRIPCFGMASK_TIME_REFRESH_INTERVAL;
      else setwidgetval_tm_gr (sd, sd->config->Time.refresh_interval);

      if (mask) StripConfig_update (sd->config, mask);
    }
  else XtMapWidget (sd->time_info.widgets[SDTMOPT_CANCEL]);
  
  setwidgetval_tm_modify (sd, !sd->time_info.modifying);

  for (i = SDTMOPT_TSHOUR; i <= SDTMOPT_GR; i++)
    XtMapWidget (sd->time_info.widgets[i]);
}


static void	tmcancel_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd = (StripDialogInfo *)data;
  int			i;
  int			h, m, s;

  for (i = SDTMOPT_TSHOUR; i <= SDTMOPT_GR; i++)
    XtUnmapWidget (sd->time_info.widgets[i]);
  
  XtUnmapWidget (sd->time_info.widgets[SDTMOPT_CANCEL]);
      
  sec2hms (sd->config->Time.timespan, &h, &m, &s);
  setwidgetval_tm_tshour (sd, h);
  setwidgetval_tm_tsminute (sd, m);
  setwidgetval_tm_tssecond (sd, s);
      
  setwidgetval_tm_ds (sd, sd->config->Time.sample_interval);
  setwidgetval_tm_gr (sd, sd->config->Time.refresh_interval);
  setwidgetval_tm_modify (sd, !sd->time_info.modifying);

  for (i = SDTMOPT_TSHOUR; i <= SDTMOPT_GR; i++)
    XtMapWidget (sd->time_info.widgets[i]);
}


static void	grmodify_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd = (StripDialogInfo *)data;
  StripConfigMask	mask = 0;
  int			i;
  int			int_val;

  XtUnmapWidget (sd->graph_info.widgets[SDGROPT_YAXISNUM]);
  XtUnmapWidget (sd->graph_info.widgets[SDGROPT_XAXISNUM]);

  if (sd->graph_info.modifying)
    {
      XtUnmapWidget (sd->graph_info.widgets[SDGROPT_CANCEL]);
      
      getwidgetval_gr_yaxisnum (sd, &int_val);
      if (!StripConfig_setattr
	  (sd->config, STRIPCONFIG_OPTION_AXIS_YNUMTICS, int_val, 0))
	setwidgetval_gr_yaxisnum (sd, sd->config->Option.axis_ynumtics);
      else mask |= STRIPCFGMASK_OPTION_AXIS_YNUMTICS;
      
      getwidgetval_gr_xaxisnum (sd, &int_val);
      if (!StripConfig_setattr
	  (sd->config, STRIPCONFIG_OPTION_AXIS_XNUMTICS, int_val, 0))
	setwidgetval_gr_xaxisnum (sd, sd->config->Option.axis_xnumtics);
      else mask |= STRIPCFGMASK_OPTION_AXIS_XNUMTICS;

      if (mask) StripConfig_update (sd->config, mask);
    }
  else XtMapWidget (sd->graph_info.widgets[SDGROPT_CANCEL]);
  
  setwidgetval_gr_modify (sd, !sd->graph_info.modifying);

  XtMapWidget (sd->graph_info.widgets[SDGROPT_YAXISNUM]);
  XtMapWidget (sd->graph_info.widgets[SDGROPT_XAXISNUM]);
}


static void	grcancel_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd = (StripDialogInfo *)data;

  XtUnmapWidget (sd->graph_info.widgets[SDGROPT_YAXISNUM]);
  XtUnmapWidget (sd->graph_info.widgets[SDGROPT_XAXISNUM]);

  XtUnmapWidget (sd->graph_info.widgets[SDGROPT_CANCEL]);
      
  setwidgetval_gr_yaxisnum (sd, sd->config->Option.axis_ynumtics);
  setwidgetval_gr_xaxisnum (sd, sd->config->Option.axis_xnumtics);
  setwidgetval_gr_modify (sd, !sd->graph_info.modifying);

  XtMapWidget (sd->graph_info.widgets[SDGROPT_YAXISNUM]);
  XtMapWidget (sd->graph_info.widgets[SDGROPT_XAXISNUM]);
}


static void	grcolor_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  CBInfo		*cbi = (CBInfo *)data;
  StripDialogInfo	*sd = (StripDialogInfo *)cbi->p;
  Pixel			pixel;

  XtVaGetValues (w, XmNbackground, &pixel, 0);
  ColorDialog_popup (sd->clrdlg, cbi->str, pixel);
}


static void	gropt_slider_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmScaleCallbackStruct	*cbs = (XmScaleCallbackStruct *)call;
  StripDialogInfo	*sd = (StripDialogInfo *)data;

  if (StripConfig_setattr
      (sd->config, STRIPCONFIG_OPTION_GRAPH_LINEWIDTH, cbs->value, 0))
    StripConfig_update (sd->config, STRIPCFGMASK_OPTION_GRAPH_LINEWIDTH);
  else setwidgetval_gr_linewidth (sd, sd->config->Option.graph_linewidth);
}


static void	gropt_tgl_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmToggleButtonCallbackStruct	*cbs = (XmToggleButtonCallbackStruct *)call;
  StripDialogInfo	*sd = (StripDialogInfo *)data;
  StripConfigMask	mask = 0;

  if (w == sd->graph_info.widgets[SDGROPT_GRIDX])
    {
      if (StripConfig_setattr
	  (sd->config, STRIPCONFIG_OPTION_GRID_XON, cbs->set, 0))
	StripConfig_update (sd->config, STRIPCFGMASK_OPTION_GRID_XON);
      else setwidgetval_gr_linewidth (sd, sd->config->Option.grid_xon);
    }
  else if (w == sd->graph_info.widgets[SDGROPT_GRIDY])
    {
      if (StripConfig_setattr
	  (sd->config, STRIPCONFIG_OPTION_GRID_YON, cbs->set, 0))
	StripConfig_update (sd->config, STRIPCFGMASK_OPTION_GRID_YON);
      else setwidgetval_gr_gridy (sd, sd->config->Option.grid_yon);
    }
  else if (w == sd->graph_info.widgets[SDGROPT_YAXISCLR])
    {
      if (StripConfig_setattr
	  (sd->config, STRIPCONFIG_OPTION_AXIS_YCOLORSTAT, cbs->set, 0))
	StripConfig_update (sd->config, STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT);
      else setwidgetval_gr_yaxisclr (sd, sd->config->Option.axis_ycolorstat);
    }
}


static void	filemenu_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd;
  int			i;

  XtVaGetValues (w, XmNuserData, &sd, NULL);
  sd->fs.state = (FsDlgState)data;

  for (i = 0; i < FSDLG_TGL_COUNT; i++)
    XmToggleButtonSetState (sd->fs.tgl[i], True, False);
  
  XtManageChild (sd->fs.dlg);
}



static void	windowmenu_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd;
  int			i = (int)data;

  XtVaGetValues (w, XmNuserData, &sd, NULL);
  sd->window_menu_info.items[i].info.cb_func
    (sd->window_menu_info.items[i].info.cb_data,
     sd->window_menu_info.items[i].info.window_id);
}



static void	ctrl_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd;
  StripDialogCallback	which = (StripDialogCallback)data;

  XtVaGetValues (w, XmNuserData, &sd, 0);
  if (sd->callback[which].func != NULL)
    sd->callback[which].func (sd->callback[which].data, NULL);
}


static void	dismiss_btn_cb	(Widget w, XtPointer data, XtPointer blah)
{
  StripDialogInfo	*sd = (StripDialogInfo *)data;

  StripDialog_popdown (sd);
}

static void	fsdlg_cb	(Widget w, XtPointer data, XtPointer call)
{
  XmFileSelectionBoxCallbackStruct *cbs =
    (XmFileSelectionBoxCallbackStruct *) call;
  StripDialogInfo	*sd;
  int	mode = (int)data;
  char	*fname;
  FILE	*f;
  int	i;

  XtUnmanageChild (w);

  if (mode == FSDLG_OK)
    if (XmStringGetLtoR (cbs->value, XmFONTLIST_DEFAULT_TAG, &fname))
      if (fname != NULL)
	{
	  XtVaGetValues (w, XmNuserData, &sd, NULL);
	  if (f = fopen (fname, sd->fs.state == FSDLG_SAVE? "w" : "r"))
	    {
	      sd->fs.mask = 0;
	      for (i = 0; i < FSDLG_TGL_COUNT; i++)
		if (XmToggleButtonGetState (sd->fs.tgl[i]))
		  sd->fs.mask |= FsDlgTglVal[i];
	      if (sd->fs.state == FSDLG_SAVE)
		{
		  if (!StripConfig_write (sd->config, f, sd->fs.mask))
		    MessageBox_popup
		      (sd->shell, &sd->message_box,
		       "Unable to write file", "Ok");
		}
	      else
		{
		  if (StripConfig_load (sd->config, f, sd->fs.mask))
		    {
		      StripConfig_setattr
			(sd->config, STRIPCONFIG_TITLE, fname, 0);
		      StripConfig_update
			(sd->config, sd->fs.mask | STRIPCFGMASK_TITLE);
		    }
		  else
		    {
		      MessageBox_popup
			(sd->shell, &sd->message_box,
			 "Unable to load file", "Ok");
		    }
		}
	      fclose (f);
	    }
	  else
	    {
	      MessageBox_popup
		(sd->shell, &sd->message_box, "Unable to open file", "Ok");
	    }
	}
}


static void	StripDialog_cfgupdate	(StripConfigMask mask, void *data)
{
  StripDialogInfo	*sd = (StripDialogInfo *)data;
  StripCurveInfo	*sc;
  int			i;
  int			h, m, s;

  if (mask & STRIPCFGMASK_TIME_TIMESPAN)
    {
      sec2hms (sd->config->Time.timespan, &h, &m, &s);
      setwidgetval_tm_tshour (sd, h);
      setwidgetval_tm_tsminute (sd, m);
      setwidgetval_tm_tssecond (sd, s);
    }

  if (mask & STRIPCFGMASK_TIME_SAMPLE_INTERVAL)
    {
      setwidgetval_tm_ds (sd, sd->config->Time.sample_interval);
    }
  
  if (mask & STRIPCFGMASK_TIME_REFRESH_INTERVAL)
    {
      setwidgetval_tm_gr (sd, sd->config->Time.refresh_interval);
    }
  
  if (mask & STRIPCFGMASK_OPTION_GRID_XON)
    {
      setwidgetval_gr_gridx (sd, sd->config->Option.grid_xon);
    }
  
  if (mask & STRIPCFGMASK_OPTION_GRID_YON)
    {
      setwidgetval_gr_gridy (sd, sd->config->Option.grid_yon);
    }
  
  if (mask & STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT)
    {
      setwidgetval_gr_gridy (sd, sd->config->Option.grid_yon);
    }
  
  if (mask & STRIPCFGMASK_OPTION_AXIS_XNUMTICS)
    {
      setwidgetval_gr_xaxisnum (sd, sd->config->Option.axis_xnumtics);
    }
  
  if (mask & STRIPCFGMASK_OPTION_AXIS_YNUMTICS)
    {
      setwidgetval_gr_yaxisnum (sd, sd->config->Option.axis_ynumtics);
    }
  
  if (mask & STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT)
    {
      setwidgetval_gr_yaxisclr (sd, sd->config->Option.axis_ycolorstat);
    }

  if (mask & STRIPCFGMASK_OPTION_GRAPH_LINEWIDTH)
    {
      setwidgetval_gr_linewidth (sd, sd->config->Option.graph_linewidth);
    }

  if (mask & STRIPCFGMASK_CURVE)
    {
      for (i = 0; i < sd->sdcurve_count; i++)
	{
	  sc = (StripCurveInfo *)sd->curve_info[i].curve;
	  if (sc->details->update_mask & mask)
	    {
	      if (sc->details->update_mask & STRIPCFGMASK_CURVE_EGU)
		{
		}
  
	      if (sc->details->update_mask & STRIPCFGMASK_CURVE_PRECISION)
		{
		  setwidgetval_precision (sd, i, sc->details->precision);
		  if (!(sc->details->update_mask & STRIPCFGMASK_CURVE_MIN))
		    setwidgetval_min (sd, i,  sc->details->min);
		  if (!(sc->details->update_mask & STRIPCFGMASK_CURVE_MAX))
		    setwidgetval_max (sd, i,  sc->details->max);
		}
  
	      if (sc->details->update_mask & STRIPCFGMASK_CURVE_MIN)
		{
		  setwidgetval_min (sd, i,  sc->details->min);
		}
  
	      if (sc->details->update_mask & STRIPCFGMASK_CURVE_MAX)
		{
		  setwidgetval_max (sd, i,  sc->details->max);
		}
  
	      if (sc->details->update_mask & STRIPCFGMASK_CURVE_PENSTAT)
		{
		  setwidgetval_penstat (sd, i, sc->details->penstat);
		}
  
	      if (sc->details->update_mask & STRIPCFGMASK_CURVE_PLOTSTAT)
		{
		  setwidgetval_plotstat (sd, i, sc->details->plotstat);
		}
	    }
	}
    }
}
