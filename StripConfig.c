/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */

#include "StripConfig.h"

#include <stdlib.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>

#define STRIPCFGMASK_FIRST_BIT	0
#define STRIPCFGMASK_LAST_BIT	31

/* ====== Configuration File Stuff ====== */
#define STRIPCONFIG_MAJOR_VERSION	1
#define STRIPCONFIG_MINOR_VERSION	0
#define STRIPCONFIG_HEADER_STR		"StripConfig"
#define LEFT_COLUMNWIDTH		30
#define NUMERIC_COLUMNWIDTH		10

typedef enum
{
  TITLE = 0,
  TIMESPAN,
  SAMPLE_INTERVAL,
  REFRESH_INTERVAL,
  BACKGROUND,
  FOREGROUND,
  GRID,
  LEGENDTEXT,
  COLOR1,
  COLOR2,
  COLOR3,
  COLOR4,
  COLOR5,
  COLOR6,
  COLOR7,
  COLOR8,
  COLOR9,
  COLOR10,
  GRID_XON,
  GRID_YON,
  AXIS_XNUMTICS,
  AXIS_YNUMTICS,
  AXIS_YCOLORSTAT,
  GRAPH_LINEWIDTH,
  LEGEND_VISIBLE,
  NAME,
  EGU,
  PRECISION,
  MIN,
  MAX,
  PENSTAT,
  PLOTSTAT,
  STRIP,
  TIME,
  COLOR,
  OPTION,
  CURVE,
  SEPARATOR,
  LAST_TOKEN
}
SCFToken;

char	*SCFTokenStr[LAST_TOKEN] =
{
  "Title",
  "Timespan",
  "SampleInterval",
  "RefreshInterval",
  "Background",
  "Foreground",
  "Grid",
  "LegenedText",
  "Color1",
  "Color2",
  "Color3",
  "Color4",
  "Color5",
  "Color6",
  "Color7",
  "Color8",
  "Color9",
  "Color10",
  "GridXon",
  "GridYon",
  "AxisXnumTics",
  "AxisYnumTics",
  "AxisYcolorStat",
  "GraphLineWidth",
  "LegendVisible",
  "Name",
  "Units",
  "Precision",
  "Min",
  "Max",
  "PenStatus",
  "PlotStatus",
  "Strip",
  "Time",
  "Color",
  "Option",
  "Curve",
  "."
};

typedef enum
{
  STRIPCFG_BACKGROUND,
  STRIPCFG_FOREGROUND,
  STRIPCFG_GRID,
  STRIPCFG_LEGENDTEXT,
  STRIPCFG_COLOR1,
  STRIPCFG_COLOR2,
  STRIPCFG_COLOR3,
  STRIPCFG_COLOR4,
  STRIPCFG_COLOR5,
  STRIPCFG_COLOR6,
  STRIPCFG_COLOR7,
  STRIPCFG_COLOR8,
  STRIPCFG_COLOR9,
  STRIPCFG_COLOR10,
  STRIPCFG_LASTIDX
}
StripCfgColorIdx;

static char	*StripCfgColorStr[STRIPCFG_LASTIDX] =
{
  STRIPDEF_COLOR_BACKGROUND_STR,
  STRIPDEF_COLOR_FOREGROUND_STR,
  STRIPDEF_COLOR_GRID_STR,
  STRIPDEF_COLOR_LEGENDTEXT_STR,
  STRIPDEF_COLOR_COLOR1_STR,
  STRIPDEF_COLOR_COLOR2_STR,
  STRIPDEF_COLOR_COLOR3_STR,
  STRIPDEF_COLOR_COLOR4_STR,
  STRIPDEF_COLOR_COLOR5_STR,
  STRIPDEF_COLOR_COLOR6_STR,
  STRIPDEF_COLOR_COLOR7_STR,
  STRIPDEF_COLOR_COLOR8_STR,
  STRIPDEF_COLOR_COLOR9_STR,
  STRIPDEF_COLOR_COLOR10_STR
};

typedef struct
{
  Display	*display;
  Colormap	cmap;
  int		private_cmap;
  Pixel		pixels[STRIPCFG_LASTIDX];
}
StripCfgColorInfo;


/* ====== Static Function Declarations ====== */
static int	read_oldformat	(StripConfig *, FILE *, StripConfigMask);

static Pixel	get_pixel	(StripConfig *,
				 StripCfgColorIdx,
				 char *,
				 unsigned short,
				 unsigned short, 
				 unsigned short);

static void	get_xcolor	(StripConfig *,
				 Pixel,
				 XColor *);

/*
 * StripConfig_init
 *
 */
StripConfig	*StripConfig_init	(Display 		*dpy,
					 Screen			*screen,
					 Window			w,
					 FILE 			*f,
					 StripConfigMask	mask)
{
  XVisualInfo		xv_tmpl, *pxv;
  XColor		*color_defs;
  StripConfig		*scfg;
  StripCfgColorInfo	*clr;
  int			i;
  int			stat;

  if ((scfg = (StripConfig *)malloc (sizeof (StripConfig))) == NULL)
    return NULL;
  if ((scfg->private_data = (void *)malloc (sizeof (StripCfgColorInfo)))
      != NULL)
    {
      clr = (StripCfgColorInfo *)scfg->private_data;
      clr->display = dpy;
      clr->cmap = DefaultColormapOfScreen (screen);
      
      /* Colormap stuff.  Try to use the default colormap.  If there
       * aren't enough writable cells, create a private colormap. */
      stat = XAllocColorCells
	(clr->display, clr->cmap, 0, 0, 0, clr->pixels,
	 STRIPCONFIG_NUMCOLORS);
      clr->private_cmap = !stat;
      
      if (clr->private_cmap)
	{
	  fprintf
	    (stderr,
	     "StripConfig_init():"
	     "  can't allocate %d colorcells from default colormap\n"
	     "  ...using virtual colormap\n",
	     STRIPCONFIG_NUMCOLORS);
	  
	  xv_tmpl.visual = DefaultVisualOfScreen (screen);
	  pxv = XGetVisualInfo (dpy, VisualNoMask, &xv_tmpl, &i);
	  color_defs = (XColor *)malloc (pxv->colormap_size * sizeof (XColor));
	  for (i = 0; i < pxv->colormap_size; i++)
	    color_defs[i].pixel = (Pixel)i;
	  XQueryColors (dpy, clr->cmap, color_defs, pxv->colormap_size);

          /* VTR */
#if defined(__cplusplus) || defined(C_plusplus)
          if ((pxv->c_class == StaticColor) ||
              (pxv->c_class == StaticGray) ||
              (pxv->c_class == TrueColor))
#else
          if ((pxv->class == StaticColor) ||
              (pxv->class == StaticGray) ||
              (pxv->class == TrueColor))
#endif
          {
            fprintf
              (stderr,
               "\n\n\tWarning!!\n"
               "\tThis version does not work with\n"
               "\tStaticColor, StaticGray, or TrueColor visuals.\n"
               "\tFor instance;PC-Xware X server has only TrueColor visual.\n\n");
	      return 0;
          }
          
	  clr->cmap = XCreateColormap (dpy, w, pxv->visual, AllocAll);
	  XStoreColors (dpy, clr->cmap, color_defs, pxv->colormap_size);
	  
	  /* set aside the topmost color cells for application colors */
	  for (i = 0; i < STRIPCONFIG_NUMCOLORS; i++)
	    clr->pixels[i] = pxv->colormap_size - (i+1);
	  XFree (pxv);
	}

	scfg->title			= STRIPDEF_TITLE;
	
	scfg->Time.timespan		= STRIPDEF_TIME_TIMESPAN;
	scfg->Time.sample_interval	= STRIPDEF_TIME_SAMPLE_INTERVAL;
	scfg->Time.refresh_interval	= STRIPDEF_TIME_REFRESH_INTERVAL;

	scfg->Color.background		= get_pixel
	  (scfg, STRIPCFG_BACKGROUND, StripCfgColorStr[STRIPCFG_BACKGROUND],
	   0, 0, 0);
	scfg->Color.foreground		= get_pixel
	  (scfg, STRIPCFG_FOREGROUND, StripCfgColorStr[STRIPCFG_FOREGROUND],
	   0, 0, 0);
	scfg->Color.grid		= get_pixel
	  (scfg, STRIPCFG_GRID, StripCfgColorStr[STRIPCFG_GRID],
	   0, 0, 0);
	scfg->Color.legendtext		= get_pixel
	  (scfg, STRIPCFG_LEGENDTEXT, StripCfgColorStr[STRIPCFG_LEGENDTEXT],
	  0, 0, 0);
	scfg->Color.color[0]		= get_pixel
	  (scfg, STRIPCFG_COLOR1, StripCfgColorStr[STRIPCFG_COLOR1],
	   0, 0, 0);
	scfg->Color.color[1]		= get_pixel
	  (scfg, STRIPCFG_COLOR2, StripCfgColorStr[STRIPCFG_COLOR2],
	   0, 0, 0);
	scfg->Color.color[2]		= get_pixel
	  (scfg, STRIPCFG_COLOR3, StripCfgColorStr[STRIPCFG_COLOR3],
	   0, 0, 0);
	scfg->Color.color[3]		= get_pixel
	  (scfg, STRIPCFG_COLOR4, StripCfgColorStr[STRIPCFG_COLOR4],
	   0, 0, 0);
	scfg->Color.color[4]		= get_pixel
	  (scfg, STRIPCFG_COLOR5, StripCfgColorStr[STRIPCFG_COLOR5],
	   0, 0, 0);
	scfg->Color.color[5]		= get_pixel
	  (scfg, STRIPCFG_COLOR6, StripCfgColorStr[STRIPCFG_COLOR6],
	   0, 0, 0);
	scfg->Color.color[6]		= get_pixel
	  (scfg, STRIPCFG_COLOR7, StripCfgColorStr[STRIPCFG_COLOR7],
	   0, 0, 0);
	scfg->Color.color[7]		= get_pixel
	  (scfg, STRIPCFG_COLOR8, StripCfgColorStr[STRIPCFG_COLOR8],
	   0, 0, 0);
	scfg->Color.color[8]		= get_pixel
	  (scfg, STRIPCFG_COLOR9, StripCfgColorStr[STRIPCFG_COLOR9],
	   0, 0, 0);
	scfg->Color.color[9]		= get_pixel
	  (scfg, STRIPCFG_COLOR10, StripCfgColorStr[STRIPCFG_COLOR10],
	   0, 0, 0);

	scfg->Option.grid_xon 		= STRIPDEF_OPTION_GRID_XON;
	scfg->Option.grid_yon		= STRIPDEF_OPTION_GRID_YON;
	scfg->Option.axis_xnumtics	= STRIPDEF_OPTION_AXIS_XNUMTICS;
	scfg->Option.axis_ynumtics	= STRIPDEF_OPTION_AXIS_YNUMTICS;
	scfg->Option.axis_ycolorstat	= STRIPDEF_OPTION_AXIS_YCOLORSTAT;
	scfg->Option.graph_linewidth	= STRIPDEF_OPTION_GRAPH_LINEWIDTH;
	scfg->Option.legend_visible	= STRIPDEF_OPTION_LEGEND_VISIBLE;

	scfg->UpdateInfo.update_mask 	= 0;

	for (i = 0; i < STRIP_MAX_CURVES; i++)
	  {
	    if ((scfg->Curves.Detail[i] = (StripCurveDetail *)malloc
		(sizeof (StripCurveDetail))) == NULL)
	      {
		fprintf (stderr, "StripConfig_init: out of memory\n");
		exit (1);
	      }
	    sprintf
	      (scfg->Curves.Detail[i]->name, "%s%d", STRIPDEF_CURVE_NAME, i);
	    sprintf (scfg->Curves.Detail[i]->egu, STRIPDEF_CURVE_EGU);
	    scfg->Curves.Detail[i]->precision 	= STRIPDEF_CURVE_PRECISION;
	    scfg->Curves.Detail[i]->min		= STRIPDEF_CURVE_MIN;
	    scfg->Curves.Detail[i]->max		= STRIPDEF_CURVE_MAX;
	    scfg->Curves.Detail[i]->penstat	= STRIPDEF_CURVE_PENSTAT;
	    scfg->Curves.Detail[i]->plotstat	= STRIPDEF_CURVE_PLOTSTAT;
	    scfg->Curves.Detail[i]->pixel	= scfg->Color.color[i];
	    scfg->Curves.Detail[i]->id		= STRIPDEF_CURVE_ID;
	    scfg->Curves.Detail[i]->update_mask = 0;
	    scfg->Curves.Detail[i]->set_mask = 0;

            scfg->Curves.plot_order[i] = i;
	  }

	scfg->UpdateInfo.callback_count = 0;
	for (i = 0; i < STRIPCONFIG_MAX_CALLBACKS; i++)
	  scfg->UpdateInfo.Callbacks[i].call_func = NULL;

	if (f != NULL) StripConfig_load (scfg, f, mask);
      }
  return scfg;
}


/*
 * StripConfig_delete
 *
 */
void	StripConfig_delete	(StripConfig *scfg)
{
  StripCfgColorInfo	*clr = (StripCfgColorInfo *)scfg->private_data;
  int			i;

  if (clr->private_cmap)
    XFreeColormap (clr->display, clr->cmap);
  else XFreeColors
	 (clr->display, clr->cmap, clr->pixels, STRIPCONFIG_NUMCOLORS, 0);
  free (clr);

  for (i = 0; i < STRIP_MAX_CURVES; i++)
    free (scfg->Curves.Detail[i]);
  free (scfg);
}


/*
 * StripConfig_setattr
 */
int	StripConfig_setattr (StripConfig *scfg, ...)
{
  va_list	ap;
  int		attrib;
  int		ret_val = 1;
  union _tmp
  {
    int		i;
    unsigned	u;
    double	d;
    char	*str;
  } tmp;


  va_start (ap, scfg);
  for (attrib = va_arg (ap, StripConfigAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripConfigAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPCONFIG_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPCONFIG_TITLE:
	    tmp.str = va_arg (ap, char *);
	    if (scfg->title) free (scfg->title);
	    if (tmp.str) scfg->title = strdup (tmp.str);
	    else scfg->title = STRIPDEF_TITLE;
	    scfg->UpdateInfo.update_mask |= STRIPCFGMASK_TITLE;
	    break;
	  case STRIPCONFIG_TIME_TIMESPAN:
	    tmp.u = va_arg (ap, unsigned);
	    if (tmp.u != scfg->Time.timespan)
	      {
		tmp.u = max (tmp.u, STRIPMIN_TIME_TIMESPAN);
		tmp.u = min (tmp.u, STRIPMAX_TIME_TIMESPAN);
		scfg->Time.timespan = tmp.u;
		scfg->UpdateInfo.update_mask |= STRIPCFGMASK_TIME_TIMESPAN;
	      }
	    break;
	  case STRIPCONFIG_TIME_SAMPLE_INTERVAL:
	    tmp.d = va_arg (ap, double);
	    if (tmp.d != scfg->Time.sample_interval)
	      {
		tmp.d = max (tmp.d, STRIPMIN_TIME_SAMPLE_INTERVAL);
		tmp.d = min (tmp.d, STRIPMAX_TIME_SAMPLE_INTERVAL);
		scfg->Time.sample_interval = tmp.d;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_TIME_SAMPLE_INTERVAL;
	      }
	    break;
	  case STRIPCONFIG_TIME_REFRESH_INTERVAL:
	    tmp.d = va_arg (ap, double);
	    if (tmp.d != scfg->Time.refresh_interval)
	      {
		tmp.d = max (tmp.d, STRIPMIN_TIME_REFRESH_INTERVAL);
		tmp.d = min (tmp.d, STRIPMAX_TIME_REFRESH_INTERVAL);
		scfg->Time.refresh_interval = tmp.d;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_TIME_REFRESH_INTERVAL;
	      }
	    break;
	  case STRIPCONFIG_OPTION_GRID_XON:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.grid_xon)
	      {
		scfg->Option.grid_xon = tmp.i;
		scfg->UpdateInfo.update_mask |= STRIPCFGMASK_OPTION_GRID_XON;
	      }
	    break;
	  case STRIPCONFIG_OPTION_GRID_YON:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.grid_yon)
	      {
		scfg->Option.grid_yon = tmp.i;
		scfg->UpdateInfo.update_mask |= STRIPCFGMASK_OPTION_GRID_YON;
	      }
	    break;
	  case STRIPCONFIG_OPTION_AXIS_XNUMTICS:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.axis_xnumtics)
	      {
		tmp.i = max (tmp.i, STRIPMIN_OPTION_AXIS_XNUMTICS);
		tmp.i = min (tmp.i, STRIPMAX_OPTION_AXIS_XNUMTICS);
		scfg->Option.axis_xnumtics = tmp.i;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_OPTION_AXIS_XNUMTICS;
	      }
	    break;
	  case STRIPCONFIG_OPTION_AXIS_YNUMTICS:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.axis_ynumtics)
	      {
		tmp.i = max (tmp.i, STRIPMIN_OPTION_AXIS_YNUMTICS);
		tmp.i = min (tmp.i, STRIPMAX_OPTION_AXIS_YNUMTICS);
		scfg->Option.axis_ynumtics = tmp.i;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_OPTION_AXIS_YNUMTICS;
	      }
	    break;
	  case STRIPCONFIG_OPTION_AXIS_YCOLORSTAT:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.axis_ycolorstat)
	      {
		scfg->Option.axis_ycolorstat = tmp.i;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT;
	      }
	    break;
	  case STRIPCONFIG_OPTION_GRAPH_LINEWIDTH:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.graph_linewidth)
	      {
		tmp.i = max (tmp.i, STRIPMIN_OPTION_GRAPH_LINEWIDTH);
		tmp.i = min (tmp.i, STRIPMAX_OPTION_GRAPH_LINEWIDTH);
		scfg->Option.graph_linewidth = tmp.i;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_OPTION_GRAPH_LINEWIDTH;
	      }
	    break;
	  case STRIPCONFIG_OPTION_LEGEND_VISIBLE:
	    tmp.i = va_arg (ap, int);
	    if (tmp.i != scfg->Option.legend_visible)
	      {
		scfg->Option.legend_visible = tmp.i;
		scfg->UpdateInfo.update_mask |=
		  STRIPCFGMASK_OPTION_LEGEND_VISIBLE;
	      }
	    break;
	  default:
	    fprintf
	      (stderr,
	       "StripConfig_setattr: cannot set read-only value, %d\n",
	       attrib);
	    ret_val = 0;
	    goto done;
	  }
      else break;
    }

  done:
  va_end (ap);
  return ret_val;
}


/*
 * StripConfig_getattr
 */
int	StripConfig_getattr (StripConfig *scfg, ...)
{
  va_list	ap;
  int		attrib;
  int		ret_val = 1;

  va_start (ap, scfg);
  for (attrib = va_arg (ap, StripConfigAttribute);
       (attrib != 0);
       attrib = va_arg (ap, StripConfigAttribute))
    {
      if ((ret_val = ((attrib > 0) && (attrib < STRIPCONFIG_LAST_ATTRIBUTE))))
	switch (attrib)
	  {
	  case STRIPCONFIG_TITLE:
	    *(va_arg (ap, char **)) = scfg->title;
	    break;
	  case STRIPCONFIG_TIME_TIMESPAN:
	    *(va_arg (ap, unsigned *)) = scfg->Time.timespan;
	    break;
	  case STRIPCONFIG_TIME_SAMPLE_INTERVAL:
	    *(va_arg (ap, double *)) = scfg->Time.sample_interval;
	    break;
	  case STRIPCONFIG_TIME_REFRESH_INTERVAL:
	    *(va_arg (ap, double *)) = scfg->Time.refresh_interval;
	    break;
	  case STRIPCONFIG_COLOR_BACKGROUND:
	    *(va_arg (ap, Pixel *)) = scfg->Color.foreground;
	    break;
	  case STRIPCONFIG_COLOR_FOREGROUND:
	    *(va_arg (ap, Pixel *)) = scfg->Color.foreground;
	    break;
	  case STRIPCONFIG_COLOR_GRID:
	    *(va_arg (ap, Pixel *)) = scfg->Color.grid;
	    break;
	  case STRIPCONFIG_COLOR_LEGENDTEXT:
	    *(va_arg (ap, Pixel *)) = scfg->Color.legendtext;
	    break;
	  case STRIPCONFIG_COLOR_COLOR1:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[0];
	    break;
	  case STRIPCONFIG_COLOR_COLOR2:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[1];
	    break;
	  case STRIPCONFIG_COLOR_COLOR3:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[2];
	    break;
	  case STRIPCONFIG_COLOR_COLOR4:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[3];
	    break;
	  case STRIPCONFIG_COLOR_COLOR5:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[4];
	    break;
	  case STRIPCONFIG_COLOR_COLOR6:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[5];
	    break;
	  case STRIPCONFIG_COLOR_COLOR7:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[6];
	    break;
	  case STRIPCONFIG_COLOR_COLOR8:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[7];
	    break;
	  case STRIPCONFIG_COLOR_COLOR9:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[8];
	    break;
	  case STRIPCONFIG_COLOR_COLOR10:
	    *(va_arg (ap, Pixel *)) = scfg->Color.color[9];
	    break;
	  case STRIPCONFIG_OPTION_GRID_XON:
	    *(va_arg (ap, int *)) = scfg->Option.grid_xon;
	    break;
	  case STRIPCONFIG_OPTION_GRID_YON:
	    *(va_arg (ap, int *)) = scfg->Option.grid_yon;
	    break;
	  case STRIPCONFIG_OPTION_AXIS_XNUMTICS:
	    *(va_arg (ap, int *)) = scfg->Option.axis_xnumtics;
	    break;
	  case STRIPCONFIG_OPTION_AXIS_YNUMTICS:
	    *(va_arg (ap, int *)) = scfg->Option.axis_ynumtics;
	    break;
	  case STRIPCONFIG_OPTION_AXIS_YCOLORSTAT:
	    *(va_arg (ap, int *)) = scfg->Option.axis_ycolorstat;
	    break;
	  case STRIPCONFIG_OPTION_GRAPH_LINEWIDTH:
	    *(va_arg (ap, int *)) = scfg->Option.graph_linewidth;
	    break;
	  case STRIPCONFIG_OPTION_LEGEND_VISIBLE:
	    *(va_arg (ap, int *)) = scfg->Option.legend_visible;
	    break;
	  }
      else break;
    }

  va_end (ap);
  return ret_val;
}


/*
 * StripConfig_write
 *
 */
int	StripConfig_write	(StripConfig 		*scfg,
				 FILE 			*f,
				 StripConfigMask 	mask)
{
  StripConfigMask	n;
  int			i, j;
  char			cbuf[256], fbuf[256], num_buf[32];
  char			*p;
  XColor		color_def;

  fprintf
    (f,
     "%-*s%d.%d\n",
     LEFT_COLUMNWIDTH,
     STRIPCONFIG_HEADER_STR,
     STRIPCONFIG_MAJOR_VERSION,
     STRIPCONFIG_MINOR_VERSION);

  for (i = STRIPCFGMASK_FIRST_BIT; i  <= STRIPCFGMASK_LAST_BIT; i++)
    {
      n = 1 << i;
      
      if (n & mask)
	{
	  p = cbuf;
	  sprintf (p, "%s", SCFTokenStr[STRIP]);
	  p += strlen (p);

	  /* Time */
	  if (n & STRIPCFGMASK_TIME)
	    {
	      sprintf
		(p, "%s%s%s%s",
		 SCFTokenStr[SEPARATOR], SCFTokenStr[TIME],
		 SCFTokenStr[SEPARATOR], SCFTokenStr[i]);
	      sprintf
		(fbuf, "%-*s", LEFT_COLUMNWIDTH, cbuf);

	      switch (n)
		{
		case STRIPCFGMASK_TIME_TIMESPAN:
		  fprintf (f, "%s%lu\n", fbuf, scfg->Time.timespan);
		  break;
		case STRIPCFGMASK_TIME_SAMPLE_INTERVAL:
		  fprintf (f, "%s%lf\n", fbuf, scfg->Time.sample_interval);
		  break;
		case STRIPCFGMASK_TIME_REFRESH_INTERVAL:
		  fprintf (f, "%s%lf\n", fbuf, scfg->Time.refresh_interval);
		  break;
		}
	    }

	  /* Color */
	  else if (n & STRIPCFGMASK_COLOR)
	    {
	      sprintf
		(p, "%s%s%s%s",
		 SCFTokenStr[SEPARATOR], SCFTokenStr[COLOR],
		 SCFTokenStr[SEPARATOR], SCFTokenStr[i]);
	      sprintf
		(fbuf, "%-*s", LEFT_COLUMNWIDTH, cbuf);

	      switch (n)
		{
		case STRIPCFGMASK_COLOR_BACKGROUND:
		  get_xcolor (scfg, scfg->Color.background, &color_def);
		  break;
		case STRIPCFGMASK_COLOR_FOREGROUND:
		  get_xcolor (scfg, scfg->Color.foreground, &color_def);
		  break;
		case STRIPCFGMASK_COLOR_GRID:
		  get_xcolor (scfg, scfg->Color.grid, &color_def);
		  break;
		case STRIPCFGMASK_COLOR_LEGENDTEXT:
		  get_xcolor (scfg, scfg->Color.legendtext, &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR1:
		  get_xcolor (scfg, scfg->Color.color[0], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR2:
		  get_xcolor (scfg, scfg->Color.color[1], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR3:
		  get_xcolor (scfg, scfg->Color.color[2], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR4:
		  get_xcolor (scfg, scfg->Color.color[3], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR5:
		  get_xcolor (scfg, scfg->Color.color[4], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR6:
		  get_xcolor (scfg, scfg->Color.color[5], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR7:
		  get_xcolor (scfg, scfg->Color.color[6], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR8:
		  get_xcolor (scfg, scfg->Color.color[7], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR9:
		  get_xcolor (scfg, scfg->Color.color[8], &color_def);
		  break;
		case STRIPCFGMASK_COLOR_COLOR10:
		  get_xcolor (scfg, scfg->Color.color[9], &color_def);
		  break;
		}
	      fprintf (f, "%s%-*hu%-*hu%-*hu\n",
		       fbuf,
		       NUMERIC_COLUMNWIDTH, color_def.red,
		       NUMERIC_COLUMNWIDTH, color_def.green,
		       NUMERIC_COLUMNWIDTH, color_def.blue);
	    }

	  /* Option */
	  else if (n & STRIPCFGMASK_OPTION)
	    {
	      sprintf
		(p, "%s%s%s%s",
		 SCFTokenStr[SEPARATOR], SCFTokenStr[OPTION],
		 SCFTokenStr[SEPARATOR], SCFTokenStr[i]);
	      sprintf
		(fbuf, "%-*s", LEFT_COLUMNWIDTH, cbuf);

	      switch (n)
		{
		case STRIPCFGMASK_OPTION_GRID_XON:
		  j = scfg->Option.grid_xon;
		  break;
		case STRIPCFGMASK_OPTION_GRID_YON:
		  j = scfg->Option.grid_yon;
		  break;
		case STRIPCFGMASK_OPTION_AXIS_XNUMTICS:
		  j = scfg->Option.axis_xnumtics;
		  break;
		case STRIPCFGMASK_OPTION_AXIS_YNUMTICS:
		  j = scfg->Option.axis_ynumtics;
		  break;
		case STRIPCFGMASK_OPTION_AXIS_YCOLORSTAT:
		  j = scfg->Option.axis_ycolorstat;
		  break;
		case STRIPCFGMASK_OPTION_GRAPH_LINEWIDTH:
		  j = scfg->Option.graph_linewidth;
		  break;
		case STRIPCFGMASK_OPTION_LEGEND_VISIBLE:
		  j = scfg->Option.legend_visible;
		  break;
		}
	      fprintf (f, "%s%d\n", fbuf, j);
	    }

	  /* Curves */
	  else if (n & STRIPCFGMASK_CURVE)
	    {
	      sprintf
		(p, "%s%s",
		 SCFTokenStr[SEPARATOR],
		 SCFTokenStr[CURVE]);

	      for (j = 0; j < STRIP_MAX_CURVES; j++)
		{
		  if (scfg->Curves.Detail[j]->id == NULL)
		    continue;
		  sprintf
		    (fbuf, "%s%s%d%s%s",
		     cbuf, SCFTokenStr[SEPARATOR],
		     j, SCFTokenStr[SEPARATOR],
		     SCFTokenStr[i]);
		  switch (n)
		    {
		    case STRIPCFGMASK_CURVE_NAME:
		      fprintf
			(f, "%-*s%s\n",
			 LEFT_COLUMNWIDTH, fbuf,
			 scfg->Curves.Detail[j]->name);
		      break;
		    case STRIPCFGMASK_CURVE_EGU:
		      if (scfg->Curves.Detail[j]->egu[0] == 0)
			break;
		      if (strcmp (scfg->Curves.Detail[j]->egu,
				  STRIPDEF_CURVE_EGU) == 0)
			break;
		      fprintf
			(f, "%-*s%s\n",
			 LEFT_COLUMNWIDTH, fbuf,
			 scfg->Curves.Detail[j]->egu);
		      break;
		    case STRIPCFGMASK_CURVE_PRECISION:
		      fprintf
			(f, "%-*s%d\n",
			 LEFT_COLUMNWIDTH, fbuf,
			 scfg->Curves.Detail[j]->precision);
		      break;
		    case STRIPCFGMASK_CURVE_MIN:
		      dbl2str
			(scfg->Curves.Detail[j]->min,
			 scfg->Curves.Detail[j]->precision,
			 num_buf,
			 31);
		      fprintf
			(f, "%-*s%s\n",
			 LEFT_COLUMNWIDTH, fbuf, num_buf);
		      break;
		    case STRIPCFGMASK_CURVE_MAX:
		      dbl2str
			(scfg->Curves.Detail[j]->max,
			 scfg->Curves.Detail[j]->precision,
			 num_buf,
			 31);
		      fprintf
			(f, "%-*s%s\n",
			 LEFT_COLUMNWIDTH, fbuf, num_buf);
		      break;
		    case STRIPCFGMASK_CURVE_PENSTAT:
		      fprintf
			(f, "%-*s%d\n",
			 LEFT_COLUMNWIDTH, fbuf,
			 scfg->Curves.Detail[j]->penstat);
		      break;
		    case STRIPCFGMASK_CURVE_PLOTSTAT:
		      fprintf
			(f, "%-*s%d\n",
			 LEFT_COLUMNWIDTH, fbuf,
			 scfg->Curves.Detail[j]->plotstat);
		      break;
		    }
		}
	    }
	}
    }

  return 1;
}


/*
 * StripConfig_load
 *
 */
int	StripConfig_load	(StripConfig 		*scfg,
				 FILE 			*f,
				 StripConfigMask	mask)
{
  char		fbuf[256], ebuf[256];
  char		*p, *ptok, *pval;
  int		token, token_min, token_max;
  int		curve_idx;
  XColor	color_def;
  long		foffset;
  int		ret_val = 1;
  int		minor_version, major_version;
  union _tmp {
    unsigned	u;
    int		i;
    double	d;
    char	buf[128];
  } tmp;

  foffset = ftell (f);
  
  /* check header */
  if (fgets (fbuf, 256, f) != NULL)
    {
      if (sscanf(fbuf, "%s %d.%d", ebuf, &major_version, &minor_version) != 3)
	{
	  fseek (f, foffset, SEEK_SET);
	  return read_oldformat (scfg, f, mask);
	}
      else if (strcmp (ebuf, STRIPCONFIG_HEADER_STR) != 0)
	{
	  fseek (f, foffset, SEEK_SET);
	  return read_oldformat (scfg, f, mask);
	}
    }

  while (fgets (fbuf, 256, f) != NULL)
    {
      strcpy (ebuf, fbuf);
      
      /* first split the input line into the attribute token and the
       * attribute value */
      
      p = ptok = fbuf;
      while (!isspace (*p)) p++;
      *p = '\0';

      p++;
      while (isspace (*p)) p++;
      pval = p;

      /* now parse the token string to get a token value */
      if (ret_val = ((p = strtok (ptok, SCFTokenStr[SEPARATOR])) != NULL))
	ret_val = (strcmp (p, SCFTokenStr[STRIP]) == 0);
      if (!ret_val)
	{
	  /* if here, then an unknown character was found, so rewind the
	   * stream pointer to the position immediately prior to the read. */
	  fseek (f, foffset, SEEK_SET);
	  break;
	}

      if (ret_val = ((p = strtok (NULL, SCFTokenStr[SEPARATOR])) != NULL))
	{
	  for (token = TIME; token <= CURVE; token++)
	    if (ret_val = (strcmp (p, SCFTokenStr[token]) == 0))
	      break;
	}
      if (!ret_val) continue;

      switch (token)
	{
	case TIME:
	  token_min = TIMESPAN;
	  token_max = REFRESH_INTERVAL;
	  break;
	case COLOR:
	  token_min = BACKGROUND;
	  token_max = COLOR10;
	  break;
	case OPTION:
	  token_min = GRID_XON;
	  token_max = LEGEND_VISIBLE;
	  break;
	case CURVE:
	  token_min = NAME;
	  token_max = PLOTSTAT;
	  /* must read the curve index */
	  if (ret_val = ((p = strtok (NULL, SCFTokenStr[SEPARATOR])) != NULL))
	    if (ret_val = sscanf (p, "%d", &curve_idx) == 1)
	      ret_val = (curve_idx >= 0) && (curve_idx <= STRIP_MAX_CURVES);
	  break;
	}

      if (!ret_val)
	{
	  fprintf (stderr, "***\n");
	  fprintf (stderr, "StripConfig_load: unknown token, \"%s\"\n", p);
	  fprintf (stderr, "==> %s\n", ebuf);
	  fprintf (stderr, "***\n");
	  foffset = ftell (f);
	  continue;
	}

      /* the next token should be the the last in the attribute so the
       * separator will be whitespace */

      if (ret_val = ((p = strtok (NULL, " \t")) != NULL))
	{
	  for (token = token_min; token <= token_max; token++)
	    if (ret_val = (strcmp (p, SCFTokenStr[token]) == 0))
	      break;
	}

      if (!ret_val)
	{
	  fprintf (stderr, "***\n");
	  fprintf (stderr, "StripConfig_load: syntax error\n");
	  fprintf (stderr, "==> %s\n", ebuf);
	  fprintf (stderr, "***\n");
	  foffset = ftell (f);
	  continue;
	}

      /* If here, then the attribute token is valid and now known.  So now
       * load the value if the attribute is not masked off */
      if (!((1 << token) & mask)) continue;
      
      switch (token)
	{
	case TIMESPAN:
	  ret_val = (sscanf (pval, "%ud", &tmp.u) == 1);
	  if (ret_val)
	    StripConfig_setattr
	      (scfg, STRIPCONFIG_TIME_TIMESPAN, tmp.u, 0);
	  break;
	case SAMPLE_INTERVAL:
	  ret_val = (sscanf (pval, "%lf", &tmp.d) == 1);
	  if (ret_val)
	    StripConfig_setattr
	      (scfg, STRIPCONFIG_TIME_SAMPLE_INTERVAL, tmp.d, 0);
	  break;
	case REFRESH_INTERVAL:
	  ret_val = (sscanf (pval, "%lf", &tmp.d) == 1);
	  if (ret_val)
	    StripConfig_setattr
	      (scfg, STRIPCONFIG_TIME_REFRESH_INTERVAL, tmp.d, 0);
	  break;
	case BACKGROUND:
	case FOREGROUND:
	case GRID:
	case LEGENDTEXT:
	case COLOR1:
	case COLOR2:
	case COLOR3:
	case COLOR4:
	case COLOR5:
	case COLOR6:
	case COLOR7:
	case COLOR8:
	case COLOR9:
	case COLOR10:
	  ret_val = (sscanf (pval, "%hu%hu%hu",
			     &color_def.red, &color_def.green, &color_def.blue)
		     == 3);
	  if (ret_val)
	    get_pixel
	      (scfg, (StripCfgColorIdx)(token - BACKGROUND), NULL,
	       color_def.red, color_def.green, color_def.blue);
	  break;
	case GRID_XON:
	  ret_val = (sscanf (pval, "%d", &scfg->Option.grid_xon) == 1);
	  break;
	case GRID_YON:
	  ret_val = (sscanf (pval, "%d", &scfg->Option.grid_yon) == 1);
	  break;
	case AXIS_XNUMTICS:
	  ret_val = (sscanf (pval, "%d", &tmp.i) == 1);
	  if (ret_val)
	    StripConfig_setattr
	      (scfg, STRIPCONFIG_OPTION_AXIS_XNUMTICS, tmp.i, 0);
	  break;
	case AXIS_YNUMTICS:
	  ret_val = (sscanf (pval, "%d", &tmp.i) == 1);
	  if (ret_val)
	    StripConfig_setattr
	      (scfg, STRIPCONFIG_OPTION_AXIS_YNUMTICS, tmp.i, 0);
	  break;
	case AXIS_YCOLORSTAT:
	  ret_val = (sscanf (pval, "%d", &scfg->Option.axis_ycolorstat) == 1);
	  break;
	case GRAPH_LINEWIDTH:
	  ret_val = (sscanf (pval, "%d", &tmp.i) == 1);
	  if (ret_val)
	    StripConfig_setattr
	      (scfg, STRIPCONFIG_OPTION_GRAPH_LINEWIDTH, tmp.i, 0);
	  break;
	case LEGEND_VISIBLE:
	  ret_val = (sscanf (pval, "%d", &scfg->Option.legend_visible) == 1);
	  break;
	case NAME:
	  ret_val =
	    (sscanf (pval, "%s", scfg->Curves.Detail[curve_idx]->name) == 1);
	  break;
	case EGU:
	  ret_val =
	    (sscanf (pval, "%s", scfg->Curves.Detail[curve_idx]->egu) == 1);
	  break;
	case PRECISION:
	  ret_val =
	    (sscanf (pval, "%d", &tmp.i)
	     == 1);
	  if (ret_val)
	    {
	      scfg->Curves.Detail[curve_idx]->precision =
		max (tmp.i, STRIPMIN_CURVE_PRECISION);
	      scfg->Curves.Detail[curve_idx]->precision =
		min (tmp.i, STRIPMAX_CURVE_PRECISION);
	    }
	  break;
	case MIN:
	  ret_val =
	    (sscanf (pval, "%lf", &scfg->Curves.Detail[curve_idx]->min)
	     == 1);
	  break;
	case MAX:
	  ret_val =
	    (sscanf (pval, "%lf", &scfg->Curves.Detail[curve_idx]->max)
	     == 1);
	  break;
	case PENSTAT:
	  ret_val =
	    (sscanf (pval, "%d", &scfg->Curves.Detail[curve_idx]->penstat)
	     == 1);
	  break;
	case PLOTSTAT:
	  ret_val =
	    (sscanf (pval, "%d", &scfg->Curves.Detail[curve_idx]->plotstat)
	     == 1);
	  break;
	default:
	  ret_val = 0;
	}

      if (ret_val)
	{
	  scfg->UpdateInfo.update_mask |= (1 << token);
	  if ((1 << token) & STRIPCFGMASK_CURVE)
	    {
	      scfg->Curves.Detail[curve_idx]->update_mask |= (1 << token);
	      scfg->Curves.Detail[curve_idx]->set_mask |= (1 << token);
	    }
	}
      else
	{
	  fprintf (stderr, "***\n");
	  fprintf (stderr, "StripConfig_load: bad value specification\n");
	  fprintf (stderr, "==> %s\n", ebuf);
	  fprintf (stderr, "***\n");
	  foffset = ftell (f);
	  continue;
	}
    }
  
  return ret_val;
}


/*
 * StripConfig_addcallback
 *
 */
int	StripConfig_addcallback	(StripConfig 		*scfg,
				 StripConfigMask	mask,
				 StripConfigUpdateFunc	func,
				 void 			*data)
{
  int	i;
  int	ret_val;

  if (ret_val = (scfg->UpdateInfo.callback_count < STRIPCONFIG_MAX_CALLBACKS))
    {
      i = scfg->UpdateInfo.callback_count;
      
      scfg->UpdateInfo.Callbacks[i].call_func = func;
      scfg->UpdateInfo.Callbacks[i].call_data = data;
      scfg->UpdateInfo.Callbacks[i].call_event = mask;

      scfg->UpdateInfo.callback_count++;
    }
  
  return ret_val;
}


/*
 * StripConfig_update
 *
 */
void	StripConfig_update	(StripConfig *scfg, StripConfigMask mask)
{
  int	i;

  if (scfg->UpdateInfo.update_mask & mask)
    {
      for (i = 0; i < scfg->UpdateInfo.callback_count; i++)
	if (scfg->UpdateInfo.Callbacks[i].call_func != NULL)
	  if (scfg->UpdateInfo.Callbacks[i].call_event & mask)
	    scfg->UpdateInfo.Callbacks[i].call_func
	      (scfg->UpdateInfo.Callbacks[i].call_event & mask,
	       scfg->UpdateInfo.Callbacks[i].call_data);

      scfg->UpdateInfo.update_mask &= ~mask;
      for (i = 0; i < STRIP_MAX_CURVES; i++)
	scfg->Curves.Detail[i]->update_mask &= ~mask;
    }
}


/*
 * StripConfig_getcmap
 */
Colormap	StripConfig_getcmap	(StripConfig *scfg)
{
  StripCfgColorInfo	*clr;

  clr = (StripCfgColorInfo *)scfg->private_data;
  return clr->cmap;
}



/* ====== Static Functions ====== */
static int	read_oldformat	(StripConfig 		*scfg,
				 FILE 			*f,
				 StripConfigMask	mask)
{
  char		fbuf[256];
  char		*pattr, *pval;
  int		curve_idx = -1;
  union 	_tmp
  {
    unsigned	u;
    double	d;
    char	buf[128];
  } tmp;
  int		ret_val = 0;
  
  while (fgets (fbuf, 256, f) != NULL)
    {
      if ((pattr = strtok (fbuf, " \t")) != NULL)
	if ((pval = strtok (NULL, " \t")) != NULL)
	  {
	    if (strcmp (pattr, "SAMPLEFREQUENCY") == 0)
	      {
		if (mask & STRIPCFGMASK_TIME_SAMPLE_INTERVAL)
		  if (sscanf (pval, "%lf", &tmp.d) == 1)
		    {
		      StripConfig_setattr
			(scfg, STRIPCONFIG_TIME_SAMPLE_INTERVAL, tmp.d, 0);
		      scfg->UpdateInfo.update_mask |=
			STRIPCFGMASK_TIME_SAMPLE_INTERVAL;
		      
		      /* make sample frequency default update frequency */
		      StripConfig_setattr
			(scfg, STRIPCONFIG_TIME_REFRESH_INTERVAL, tmp.d, 0);
		      scfg->UpdateInfo.update_mask |=
			STRIPCFGMASK_TIME_REFRESH_INTERVAL;
		      
		      ret_val = 1;
		    }
	      }
	    
	    else if (strcmp (pattr, "TIMESPAN") == 0)
	      {
		if (mask & STRIPCFGMASK_TIME_TIMESPAN)
		  if (sscanf (pval, "%ud", &tmp.u) == 1)
		    {
		      StripConfig_setattr
			(scfg, STRIPCONFIG_TIME_TIMESPAN, tmp.u, 0);
		      scfg->UpdateInfo.update_mask |=
			STRIPCFGMASK_TIME_TIMESPAN;
		      ret_val = 1;
		    }
	      }

	    else if (strcmp (pattr, "CHANNEL") == 0)
	      {
		if (mask & STRIPCFGMASK_CURVE_NAME)
		  if (sscanf (pval, "%s", tmp.buf) == 1)
		    {
		      curve_idx++;
		      strcpy (scfg->Curves.Detail[curve_idx]->name, tmp.buf);
		      scfg->UpdateInfo.update_mask |= STRIPCFGMASK_CURVE_NAME;
		      scfg->Curves.Detail[curve_idx]->update_mask |=
			STRIPCFGMASK_CURVE_NAME;
		      scfg->Curves.Detail[curve_idx]->set_mask |=
			STRIPCFGMASK_CURVE_NAME;		      
		      ret_val = 1;
		    }
	      }

	    else if (strcmp (pattr, "MAXIMUM") == 0)
	      {
		if (mask & STRIPCFGMASK_CURVE_MAX)
		  if ((curve_idx >= 0) && (curve_idx < STRIP_MAX_CURVES))
		    {
		      if (sscanf (pval, "%lf", &tmp.d) == 1)
			{
			  scfg->Curves.Detail[curve_idx]->max = tmp.d;
			  scfg->UpdateInfo.update_mask |=
			    STRIPCFGMASK_CURVE_MAX;
			  scfg->Curves.Detail[curve_idx]->update_mask |=
			    STRIPCFGMASK_CURVE_MAX;
			  scfg->Curves.Detail[curve_idx]->set_mask |=
			    STRIPCFGMASK_CURVE_MAX;		      
			  ret_val = 1;
			}
		    }
	      }

	    else if (strcmp (pattr, "MINIMUM") == 0)
	      {
		if (mask & STRIPCFGMASK_CURVE_MIN)
		  if ((curve_idx >= 0) && (curve_idx < STRIP_MAX_CURVES))
		    {
		      if (sscanf (pval, "%lf", &tmp.d) == 1)
			{
			  scfg->Curves.Detail[curve_idx]->min = tmp.d;
			  scfg->UpdateInfo.update_mask |=
			    STRIPCFGMASK_CURVE_MIN;
			  scfg->Curves.Detail[curve_idx]->update_mask |=
			    STRIPCFGMASK_CURVE_MIN;
			  scfg->Curves.Detail[curve_idx]->set_mask |=
			    STRIPCFGMASK_CURVE_MIN;		      
			  ret_val = 1;
			}
		    }
	      }
	  }
    }
  return 1;
}


static Pixel	get_pixel	(StripConfig 		*scfg,
				 StripCfgColorIdx	idx,
				 char 			*name,
				 unsigned short		red,
				 unsigned short		green, 
				 unsigned short		blue)
{
  StripCfgColorInfo	*clr = (StripCfgColorInfo *)scfg->private_data;
  XColor		color_def, dummy;

  if (name != NULL)	/* allocate color by name */
    {
      if (!XLookupColor (clr->display, clr->cmap, name, &dummy, &color_def))
	{
	  fprintf
	    (stderr,
	     "StripConfig: get_pixel(): color %s is not in database.\n"
	     "... using default %s instead.\n",
	     name, StripCfgColorStr[idx]);
	  if (!XLookupColor
	      (clr->display, clr->cmap, StripCfgColorStr[idx],
	       &dummy, &color_def))
	    {
	      fprintf
		(stderr, "Oops! that didn't work.  Something's wrong!\n");
	      color_def.red = color_def.green = color_def.blue = 0;
	    }
	}
    }
  else			/* allocate color by rgb values */
    {
      color_def.red = red;
      color_def.green = green;
      color_def.blue = blue;
    }

  color_def.pixel = clr->pixels[idx];
  color_def.flags = DoRed | DoGreen | DoBlue;

  XStoreColor (clr->display, clr->cmap, &color_def);
  return color_def.pixel;
}

static void	get_xcolor	(StripConfig 	*scfg,
				 Pixel		pixel,
				 XColor 	*color_def)
{
  StripCfgColorInfo	*clr = (StripCfgColorInfo *)scfg->private_data;

  color_def->pixel = pixel;
  XQueryColor (clr->display, clr->cmap, color_def);
}


#if defined (STRIPCONFIG_MAIN)

int main (int argc, char *argv[])
{
  Widget	toplevel;
  XtAppContext	app;
  Colormap	cmap;
  StripConfig	*config;
  FILE		*f;

  XtSetLanguageProc (NULL, NULL, NULL);

  toplevel = XtVaAppInitialize
    (&app, "StripConfig Test", NULL, 0, &argc, argv, NULL, NULL);
  if (DefaultDepthOfScreen (XtScreen (toplevel)) < 2)
    {
      puts ("This program requires a color screen.");
      exit (1);
    }
  cmap = DefaultColormapOfScreen (XtScreen (toplevel));

  if (argc < 3)
    {
      config = StripConfig_init
	(XtDisplay (toplevel), cmap, NULL, STRIPCFGMASK_ALL);
      if (config) StripConfig_delete (config);
      else fprintf (stdout, "error creating config!\n");
    }
  else
    {
      if ((f = fopen (argv[1], "r")) != NULL)
	{
	  fprintf (stdout, "reading config from file:  %s...\n", argv[1]);
	  config = StripConfig_init
	    (XtDisplay (toplevel), cmap, f, STRIPCFGMASK_ALL);
	  fclose (f);

	  if ((f = fopen (argv[2], "w")) != NULL)
	    {
	      fprintf (stdout, "writing config to file:  %s...\n", argv[2]);
	      StripConfig_write (config, f, STRIPCFGMASK_ALL);
	      fclose (f);
	    }
	  StripConfig_delete (config);
	}
    }
  fprintf (stdout, "Done.\n");
  return 0;
}

#endif
