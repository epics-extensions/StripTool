/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "ColorDialog.h"

#include <X11/Xlib.h>
#include <Xm/Frame.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/Scale.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/Separator.h>

#include <stdio.h>

#define RGB_FILE			"/usr/lib/X11/rgb.txt"
#define DEF_WOFFSET			3
#define MAX_COLOR_NAMES			1024
#define MAX_COLOR_NAME_LEN		32

#define NAMEDLG_APPLY			0
#define NAMEDLG_OK			1
#define NAMEDLG_CANCEL			2

typedef enum
{
  Red, Green, Blue, NUM_COLOR_COMPONENTS
}
ColorComponent;

char	*ColorComponentStr[NUM_COLOR_COMPONENTS] =
{
  "Red",
  "Green",
  "Blue"
};

typedef struct
{
  unsigned short	red, green, blue;
} RGB;

typedef struct
{
  Display		*display;
  Widget		shell;
  Widget		lbl;
  Widget		slider[NUM_COLOR_COMPONENTS];
  Widget		color_area;
  Widget		name_dlg;
  Widget		name_dlg_list;
  Colormap		cmap;
  XColor		color;
  XColor		white_defs, black_defs;
}
ColorDialogInfo;

/* ====== Static Functions ====== */
static void	ColorDialog_showcolor		(ColorDialogInfo *, XColor *);
static int	ColorDialog_build_dialog	(ColorDialogInfo *);

/* ====== Callback Functions ====== */
void 	slider_drag 	(Widget, XtPointer, XtPointer);
void 	dismiss_push 	(Widget, XtPointer, XtPointer);
void 	names_push 	(Widget, XtPointer, XtPointer);
void 	name_dlg_cb 	(Widget, XtPointer, XtPointer);



/* ====== Function Definitions ====== */
ColorDialog	ColorDialog_init	(Display *d, Colormap c, char *title)
{
  ColorDialogInfo	*cd;
  Widget		frame1, frame2, base_form, form;
  Widget		act_area, ctrl_area;
  Widget		btn, w;
  XmString		str;
  int			clr;

  if ((cd = (ColorDialogInfo *)malloc (sizeof (ColorDialogInfo))) != NULL)
    {
      cd->display = d;
      cd->cmap = c;

      /* determine the min and max RGB values for this display */
      XLookupColor
	(cd->display, cd->cmap, "Black", &cd->color, &cd->black_defs);
      XLookupColor
	(cd->display, cd->cmap, "White", &cd->color, &cd->white_defs);

      cd->shell = XtVaAppCreateShell
	(NULL, "ColorDialog",
	 topLevelShellWidgetClass, 	cd->display,
	 XmNdeleteResponse,		XmUNMAP,
	 XmNmappedWhenManaged,		False,
	 XmNtitle,			title == NULL? "Modify Color" : title,
	 XmNcolormap,			cd->cmap,
	 NULL);
      base_form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		cd->shell,
	 XmNfractionBase,		9,
	 XmNnoResize,			True,
	 XmNresizable,			False,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 XmNverticalSpacing,		DEF_WOFFSET,
	 XmNhorizontalSpacing,		DEF_WOFFSET,
	 NULL);
      
      frame1 = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		base_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XtVaCreateManagedWidget
	("Color Control",
	 xmLabelWidgetClass,		frame1,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 NULL);
      form = XtVaCreateManagedWidget
	("form",
	 xmFormWidgetClass,		frame1,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 NULL);

      str = XmStringCreateLocalized (ColorComponentStr[Red]);
      cd->slider[Red] = XtVaCreateManagedWidget
	("slider",
	 xmScaleWidgetClass,		form,
	 XmNorientation,		XmHORIZONTAL,
	 XmNtitleString,		str,
	 XmNminimum,			cd->black_defs.red,
	 XmNmaximum,			cd->white_defs.red,
	 XmNuserData,			cd,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XmStringFree (str);
      XtAddCallback
	(cd->slider[Red], XmNdragCallback, slider_drag, (XtPointer)Red);
      XtAddCallback
	(cd->slider[Red], XmNvalueChangedCallback, slider_drag,
	 (XtPointer)Red);
      str = XmStringCreateLocalized (ColorComponentStr[Green]);
      cd->slider[Green] = XtVaCreateManagedWidget
	("slider",
	 xmScaleWidgetClass,		form,
	 XmNorientation,		XmHORIZONTAL,
	 XmNtitleString,		str,
	 XmNminimum,			cd->black_defs.green,
	 XmNmaximum,			cd->white_defs.green,
	 XmNuserData,			cd,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			cd->slider[Red],
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XmStringFree (str);
      XtAddCallback
	(cd->slider[Green], XmNdragCallback, slider_drag, (XtPointer)Green);
      XtAddCallback
	(cd->slider[Green], XmNvalueChangedCallback, slider_drag,
	 (XtPointer)Green);
      str = XmStringCreateLocalized (ColorComponentStr[Blue]);
      cd->slider[Blue] = XtVaCreateManagedWidget
	("slider",
	 xmScaleWidgetClass,		form,
	 XmNorientation,		XmHORIZONTAL,
	 XmNtitleString,		str,
	 XmNminimum,			cd->black_defs.blue,
	 XmNmaximum,			cd->white_defs.blue,
	 XmNuserData,			cd,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			cd->slider[Green],
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_FORM,
	 NULL);
      XmStringFree (str);
      XtAddCallback
	(cd->slider[Blue], XmNdragCallback, slider_drag, (XtPointer)Blue);
      XtAddCallback
	(cd->slider[Blue], XmNvalueChangedCallback, slider_drag,
	 (XtPointer)Blue);
      
      frame2 = XtVaCreateManagedWidget
	("frame",
	 xmFrameWidgetClass,		base_form,
	 XmNshadowType,			XmSHADOW_ETCHED_IN,
	 XmNresizePolicy,		XmRESIZE_NONE,
	 XmNtopAttachment,		XmATTACH_FORM,
	 XmNleftAttachment,		XmATTACH_FORM,
	 XmNrightAttachment,		XmATTACH_WIDGET,
	 XmNrightWidget,		frame1,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 NULL);
      cd->lbl = XtVaCreateManagedWidget
	("A Really Long Label",
	 xmLabelWidgetClass,		frame2,
	 XmNchildType,			XmFRAME_TITLE_CHILD,
	 NULL);
      cd->color_area = XtVaCreateManagedWidget
	("drawing area",
	 xmDrawingAreaWidgetClass,	frame2,
	 XmNchildType,			XmFRAME_WORKAREA_CHILD,
	 NULL);
      
      btn = XtVaCreateManagedWidget
	("Dismiss",
	 xmPushButtonWidgetClass,	base_form,
	 XmNtopAttachment,		XmATTACH_WIDGET,
	 XmNtopWidget,			frame1,
	 XmNrightAttachment,		XmATTACH_FORM,
	 XmNbottomAttachment,		XmATTACH_FORM,
	 XmNbottomOffset,		DEF_WOFFSET,
	 NULL);
      XtAddCallback (btn, XmNactivateCallback, dismiss_push, cd);
      w = btn;


      /* now read the RGB file and create the color name dialog */
      if (ColorDialog_build_dialog (cd))
	{
	  btn = XtVaCreateManagedWidget
	    ("Names",
	     xmPushButtonWidgetClass,	base_form,
	     XmNtopAttachment,		XmATTACH_WIDGET,
	     XmNtopWidget,		frame1,
	     XmNrightAttachment,	XmATTACH_WIDGET,
	     XmNrightWidget,		w,
	     XmNbottomAttachment,	XmATTACH_FORM,
	     XmNbottomOffset,		DEF_WOFFSET,
	     NULL);
	  XtAddCallback (btn, XmNactivateCallback, names_push, cd);
	}
      else cd->name_dlg = 0;
      
      XtRealizeWidget (cd->shell);
    }
  return (ColorDialog)cd;
}


void		ColorDialog_delete	(ColorDialog the_cd)
{
  ColorDialogInfo	*cd = (ColorDialogInfo *)the_cd;
  XtDestroyWidget (cd->shell);
}


void		ColorDialog_popup	(ColorDialog	the_cd,
					 char 		*title,
					 Pixel		pixel)
{
  ColorDialogInfo	*cd = (ColorDialogInfo *)the_cd;
  XmString		str;

  cd->color.pixel = pixel;
  XQueryColor (cd->display, cd->cmap, &cd->color);
  
  ColorDialog_showcolor (cd, &cd->color);

  /* set the frame title */
  str = XmStringCreateLocalized (title);
  XtVaSetValues (cd->lbl, XmNlabelString, str, NULL);

  /* set the color indicator */
  XtVaSetValues (cd->color_area, XmNbackground, pixel, NULL);

  XMapRaised (XtDisplay (cd->shell), XtWindow (cd->shell));
}


/* ====== Static Functions ====== */
static void	ColorDialog_showcolor (ColorDialogInfo *cd, XColor *color)
{
  /* set the sliders */
  XmScaleSetValue (cd->slider[Red], color->red);
  XmScaleSetValue (cd->slider[Green], color->green);
  XmScaleSetValue (cd->slider[Blue], color->blue);
}

static int	ColorDialog_build_dialog	(ColorDialogInfo *cd)
{
  FILE		*f;
  char		buf[MAX_COLOR_NAME_LEN * MAX_COLOR_NAMES];
  char		*names[MAX_COLOR_NAMES];
  char		fbuf[128];
  char		*p, *c;
  int		i, n;
  int		buf_idx;
  XmString	xstr;
  XmStringTable	name_list;
  int		ret_val;
  Widget	form, lbl, sep, btn, w;

  if (ret_val = ((f = fopen (RGB_FILE, "r")) != NULL))
    {
      i = 0;
      buf_idx = 0;
      while ((fgets (fbuf, 128, f) != NULL) && (i < MAX_COLOR_NAMES))
	{
	  /* skip the red field */
	  if ((p = strtok (fbuf, " \t")) == NULL)
	    continue;
	  /* skip the green field */
	  if ((p = strtok (NULL, " \t")) == NULL)
	    continue;
	  /* skip the blue field */
	  if ((p = strtok (NULL, " \t")) == NULL)
	    continue;
	  /* read first token of name field */
	  if ((p = strtok (NULL, " \t")) == NULL)
	    continue;
	  /* strtok places a NULL character after the returned token.  If
	   * the name consists of several tokens, then this NULL character
	   * will have to be removed.  So, run through the returned string
	   * replacing any NULL charaters with a space character until a
	   * newline character is found.  Replace this character with a
	   * NULL */
	  for (c = p; *c != '\n'; c++)
	    if (*c == '\0') *c = ' ';
	  *c = '\0';
	  /* store color name, minus any trailing whitespace */
	  strcpy (&buf[buf_idx], p);
	  names[i] = &buf[buf_idx];
	  buf_idx += strlen (p) + 1;
	  
	  i++;
	}
      fclose (f);

      if (ret_val = (i > 0))
	{
	  n = i;
	  name_list = (XmStringTable)XtMalloc (n*sizeof(XmString));
	  for (i = 0; i < n; i++)
	    name_list[i] = XmStringCreateLocalized (names[i]);
	  
	  cd->name_dlg = XtVaCreateManagedWidget
	    ("Color Selection",
	     transientShellWidgetClass,	cd->shell,
	     XmNmappedWhenManaged,	False,
	     XmNtransientFor,		cd->shell,
	     XmNdeleteResponse,		XmUNMAP,
	     NULL);
	  form = XtVaCreateManagedWidget
	    ("form",
	     xmFormWidgetClass,		cd->name_dlg,
	     XmNuserData,		cd,
	     XmNnoResize,		True,
	     NULL);
	  lbl = XtVaCreateManagedWidget
	    ("Predefined Colors",
	     xmLabelWidgetClass,	form,
	     XmNtopAttachment,		XmATTACH_FORM,
	     XmNleftAttachment,		XmATTACH_FORM,
	     NULL);
	  btn = XtVaCreateManagedWidget
	    ("Cancel",
	     xmPushButtonWidgetClass,	form,
	     XmNrightAttachment,	XmATTACH_FORM,
	     XmNrightOffset,		DEF_WOFFSET,
	     XmNbottomAttachment,	XmATTACH_FORM,
	     XmNbottomOffset,		DEF_WOFFSET,
	     NULL);
	  XtAddCallback
	    (btn, XmNactivateCallback, name_dlg_cb, (XtPointer)NAMEDLG_CANCEL);
	  w = btn;
	  btn = XtVaCreateManagedWidget
	    ("Ok",
	     xmPushButtonWidgetClass,	form,
	     XmNrightAttachment,	XmATTACH_WIDGET,
	     XmNrightWidget,		w,
	     XmNbottomAttachment,	XmATTACH_FORM,
	     XmNbottomOffset,		DEF_WOFFSET,
	     NULL);
	  XtAddCallback
	    (btn, XmNactivateCallback, name_dlg_cb, (XtPointer)NAMEDLG_OK);
	  w = btn;
	  btn = XtVaCreateManagedWidget
	    ("Apply",
	     xmPushButtonWidgetClass,	form,
	     XmNrightAttachment,	XmATTACH_WIDGET,
	     XmNrightWidget,		w,
	     XmNbottomAttachment,	XmATTACH_FORM,
	     XmNbottomOffset,		DEF_WOFFSET,
	     NULL);
	  XtAddCallback
	    (btn, XmNactivateCallback, name_dlg_cb, (XtPointer)NAMEDLG_APPLY);
	  
	  sep = XtVaCreateManagedWidget
	    ("separator",
	     xmSeparatorWidgetClass,	form,
	     XmNshadowType,		XmSHADOW_IN,
	     XmNleftAttachment,		XmATTACH_FORM,
	     XmNrightAttachment,	XmATTACH_FORM,
	     XmNbottomAttachment,	XmATTACH_WIDGET,
	     XmNbottomWidget,		w,
	     NULL);
	  
	  cd->name_dlg_list = XmCreateScrolledList
	    (form, "ScrolledList", NULL, 0);
	  XtVaSetValues
	    (cd->name_dlg_list,
	     XmNitems,		name_list,
	     XmNitemCount,		n,
	     XmNselectionPolicy,	XmSINGLE_SELECT,
	     XmNvisibleItemCount,	10,
	     NULL);
	  XtVaSetValues
	    (XtParent (cd->name_dlg_list),
	     XmNtopAttachment,	XmATTACH_WIDGET,
	     XmNtopWidget,		lbl,
	     XmNleftAttachment,	XmATTACH_FORM,
	     XmNrightAttachment,	XmATTACH_FORM,
	     XmNbottomAttachment,	XmATTACH_WIDGET,
	     XmNbottomWidget,	sep,
	     NULL);
	  
	  XtManageChild (cd->name_dlg_list);
	  
	  for (i = 0; i < n; i++)
	    XmStringFree (name_list[i]);
	  
	  XtManageChild (cd->name_dlg);
	}
    }

  return ret_val;
}


void		ColorDialog_popdown	(ColorDialog the_cd)
{
  ColorDialogInfo	*cd = (ColorDialogInfo *)the_cd;
  XUnmapWindow (XtDisplay (cd->shell), XtWindow (cd->shell));
}


/* ====== Callback Functions ====== */
void 	slider_drag	(Widget 	w,
			 XtPointer	client_data,
			 XtPointer 	call_data)
{
  XmScaleCallbackStruct	*cbs = (XmScaleCallbackStruct *)call_data;
  ColorComponent	which = (ColorComponent)client_data;
  ColorDialogInfo	*cd;

  XtVaGetValues (w, XmNuserData, &cd, NULL);

  switch (which)
    {
    case Red:
      cd->color.red = cbs->value;
      cd->color.flags = DoRed;
      break;
    case Green:
      cd->color.green = cbs->value;
      cd->color.flags = DoGreen;
      break;
    case Blue:
      cd->color.blue = cbs->value;
      cd->color.flags = DoBlue;
      break;
    }

  XStoreColor (cd->display, cd->cmap, &cd->color);
}

void 	dismiss_push	(Widget 	w,
			 XtPointer	client_data,
			 XtPointer 	call_data)
{
  ColorDialogInfo	*cd = (ColorDialogInfo *)client_data;
  ColorDialog_popdown (cd);
}

void 	names_push	(Widget 	w,
			 XtPointer	client_data,
			 XtPointer 	call_data)
{
  ColorDialogInfo	*cd = (ColorDialogInfo *)client_data;
  Dimension		width;
  Position		x, y;
  
  if (cd->name_dlg != 0)
    {
      XtVaGetValues
	(cd->shell,
	 XmNwidth,	&width,
	 XmNx,		&x,
	 XmNy,		&y,
	 NULL);
      x += width + 10;
      XtVaSetValues
	(cd->name_dlg,
	 XmNx,		x,
	 XmNy,		y,
	 NULL);
      XMapRaised (XtDisplay(cd->name_dlg), XtWindow (cd->name_dlg));
    }
}

void	name_dlg_cb	(Widget 	w,
			 XtPointer	client_data,
			 XtPointer 	call_data)
{
  ColorDialogInfo	*cd;
  int 			button = (int)client_data;
  XColor		tmp_color, dummy;
  XmString		*items;
  int			count;
  char			*name;

  XtVaGetValues (XtParent (w), XmNuserData, &cd, NULL);

  XtVaGetValues
    (cd->name_dlg_list,
     XmNselectedItemCount,	&count,
     XmNselectedItems,		&items,
     NULL);
  if (count > 0)
    {
      XmStringGetLtoR (items[0], XmFONTLIST_DEFAULT_TAG, &name);
      XLookupColor (cd->display, cd->cmap, name, &dummy, &tmp_color);
      fprintf (stdout, "selected color: %hd %hd %hd\n",
	       tmp_color.red, tmp_color.blue, tmp_color.green);
      tmp_color.flags = DoRed | DoGreen | DoBlue;
      tmp_color.pixel = cd->color.pixel;
    }
      
  switch (button)
    {
    case NAMEDLG_APPLY:
      if (count > 0)
	{
	  XStoreColor (cd->display, cd->cmap, &tmp_color);
	  ColorDialog_showcolor (cd, &tmp_color);
	}
      break;
    case NAMEDLG_OK:
      if (count > 0)
	{
	  XStoreColor (cd->display, cd->cmap, &tmp_color);
	  ColorDialog_showcolor (cd, &tmp_color);
	}
      XUnmapWindow (XtDisplay (cd->name_dlg), XtWindow (cd->name_dlg));
      break;
    case NAMEDLG_CANCEL:
      cd->color.flags = DoRed | DoGreen | DoBlue;
      XStoreColor (cd->display, cd->cmap, &cd->color);
      ColorDialog_showcolor (cd, &cd->color);
      XUnmapWindow (XtDisplay (cd->name_dlg), XtWindow (cd->name_dlg));
      break;
    }
}


#if defined (COLORDIALOG_MAIN)

#include <Xm/RowColumn.h>

#define MAX_COLORS	16
typedef struct
{
  XColor	colors[MAX_COLORS];
  ColorDialog	dlg;
} InfoStruct;

InfoStruct	info;

char	*color_names[MAX_COLORS] =
{
};

void 	do_popup	(Widget, XtPointer, XtPointer);

int	main	(int argc, char *argv[])
{
  Widget	toplevel, w, rowcol;
  XtAppContext	app;
  Colormap	cmap;
  Pixel		pixels[MAX_COLORS];
  int		i;

  XtSetLanguageProc (NULL, NULL, NULL);

  toplevel = XtVaAppInitialize
    (&app, "ColorDialog Test", NULL, 0, &argc, argv, NULL, NULL);
  if (DefaultDepthOfScreen (XtScreen (toplevel)) < 2)
    {
      puts ("This program requires a color screen.");
      exit (1);
    }
  cmap = DefaultColormapOfScreen (XtScreen (toplevel));

  if (!XAllocColorCells
      (XtDisplay (toplevel), cmap, 0, 0, 0, pixels, MAX_COLORS))
    {
      fprintf (stderr, "can't allocate %d private colorcells\n", MAX_COLORS);
      exit (1);
    }
  for (i = 0; i < MAX_COLORS; i++)
    {
      if (!XParseColor
	  (XtDisplay (toplevel), cmap, color_names[i], &info.colors[i]))
	{
	  fprintf
	    (stderr, "Color %s doesn't exist in database\n", color_names[i]);
	  exit (1);
	}
      info.colors[i].pixel = pixels[i];
      info.colors[i].flags = DoRed | DoGreen | DoBlue;
    }
  XStoreColors (XtDisplay (toplevel), cmap, info.colors, MAX_COLORS);

  rowcol = XtVaCreateManagedWidget
    ("row column",
     xmRowColumnWidgetClass,		toplevel,
     XmNpacking,			XmPACK_COLUMN,
     XmNnumColumns,			4,
     XmNorientation,			XmVERTICAL,
     NULL);
  for (i = 0; i < MAX_COLORS; i++)
    {
      w = XtVaCreateManagedWidget
	("Push Me",
	 xmPushButtonWidgetClass,	rowcol,
	 XmNbackground,			info.colors[i].pixel,
	 NULL);
      XtAddCallback (w, XmNactivateCallback, do_popup, (XtPointer)i);
    }
     
  info.dlg = ColorDialog_init (toplevel, cmap);
  XtRealizeWidget (toplevel);
  XtAppMainLoop (app);
  XFreeColors (XtDisplay (toplevel), cmap, pixels, MAX_COLORS, 0);
}

void 	do_popup	(Widget w, XtPointer client_data, XtPointer blah)
{
  char		buf[16];
  static int	num = 0;
  int		which = (int)client_data;

  sprintf (buf, "Call %d", ++num);
  ColorDialog_popup (info.dlg, buf, &info.colors[which].pixel);
}
#endif
