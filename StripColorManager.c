/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#include "StripColorManager.h"
#include "StripMisc.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* StripVisualGetClassName
 *
 *	Given a class (from the XVisualInfo structure), returns
 *	the appropriate string describing it.
 */
#define StripVisualGetClassName(visual_class) \
(visual_class == DirectColor? "DirectColor" :	\
 visual_class == GrayScale?   "GrayScale"   : 	\
 visual_class == PseudoColor? "PseudoColor" :	\
 visual_class == StaticColor? "StaticColor" :	\
 visual_class == StaticGray?  "StaticGray"  :	\
 visual_class == TrueColor?   "TrueColor"  :	\
 "[Unknown!]")

/* StripVisualGetClass
 *
 *	Given the address of an XVisualInfo structure, returns
 *	the class member.  This is needed because the XVisualInfo
 *	strucutre is defined differently when compiled with C than
 *	when compiled with C++.
 */
#if defined(__cplusplus) || defined(C_plusplus)
#  define StripVisualGetClass(visual_info_ptr)	((visual_info_ptr)->c_class)
#else
#  define StripVisualGetClass(visual_info_ptr)	((visual_info_ptr)->class)
#endif

/* StripVisualSetClass
 *
 *	Sets the class member of the XVisualInfo structure whose
 *	address is passed in.
 */
#if defined(__cplusplus) || defined(C_plusplus)
#  define StripVisualSetClass(visual_info_ptr, class_type) \
((visual_info_ptr)->c_class = class_type)
#else
#  define StripVisualSetClass(visual_info_ptr, class_type) \
((visual_info_ptr)->class = class_type)
#endif


/* StripVisualGetClassIsWritable
 *
 *	Returns true if the visual class is writable visual.
 */
#define StripVisualIsWritable(visual_class) \
((visual_class == DirectColor) || \
 (visual_class == PseudoColor) || \
 (visual_class == GrayScale))


typedef struct _scmKeepStruct
{
  Pixel	pixel;
  short	writable;
  short	ref;
}
scmKeepStruct;


typedef struct _SCMinfo
{
  Display	*display;
  XVisualInfo	xvi;
  Colormap	cmap;
  int		private_cmap;

  /* preallocated pixels */
  Pixel		grab_pix[SCM_MAX_PREALLOC_CELLS];
  int		n_grab;

  /* colors currently being used */
  scmKeepStruct	keep[SCM_MAX_KEEP_CELLS];
  int		n_keep;

  /* current palette */
  XColor	palette[SCM_MAX_PALETTE_SIZE];
  int		palette_size;
}
SCMinfo;


#if !defined (DEBUG_COLORALLOC)
#  define XwAllocColor		XAllocColor
#  define XwAllocColorCells	XAllocColorCells
#  define XwFreeColors		XFreeColors
#else
/*********************************************************************
 *                 D E B U G G I N G    C O D E
 *********************************************************************/
#define MAX_ALLOCATED	1024
static struct 	_ColorDebugInfo
{
  Pixel	pixel;
  int	ref;
} allocated[MAX_ALLOCATED];

static int	n_allocated = 0;

static Status	XwAllocColor		(Display	*dpy,
                                         Colormap	cmap,
                                         XColor		*colorcell_in_out)
{
  Status 	stat;
  int		i;

  if (stat = XAllocColor (dpy, cmap, colorcell_in_out))
  {
    for (i = 0; i < n_allocated; i++)
      if (allocated[i].pixel == colorcell_in_out->pixel)
        break;

    if (i < n_allocated)
      allocated[i].ref++;
    else if (n_allocated < MAX_ALLOCATED) {
      allocated[n_allocated].pixel = colorcell_in_out->pixel;
      allocated[n_allocated].ref = 1;
      n_allocated++;
    }
    else fprintf (stderr, "XwAllocColor: buffer overflow!\n");
  }

  return stat;
}

static Status 	XwAllocColorCells	(Display 	*dpy,
                                         Colormap 	cmap,
                                         Bool 		contig,
                                         unsigned long 	*plane_masks_return,
                                         unsigned int 	nplanes,
                                         unsigned long	*pixels_return,
                                         unsigned int	npixels_return)
{
  Status 	stat;
  int		i, j;

  stat = XAllocColorCells
    (dpy, cmap, contig,
     plane_masks_return, nplanes,
     pixels_return, npixels_return);

  if (stat)
  {
    /* search through list of allocated colors and increment reference
     * count for pixels already allocated, add new pixels. */
    for (i = 0; i < npixels_return; i++)
    {
      for (j = 0; j < n_allocated; j++)
        if (pixels_return[i] == allocated[j].pixel)
          break;

      if (j < n_allocated)	/* found? */
        allocated[j].ref++;
      else if (n_allocated < MAX_ALLOCATED) {
        allocated[n_allocated].pixel = pixels_return[i];
        allocated[n_allocated].ref = 1;
        n_allocated++;
      }
      else fprintf (stderr, "XwAllocColorCells: buffer overflow!\n");
    }
  }

  return stat;
}


static Status 	XwFreeColors		(Display	*dpy,
                                         Colormap	cmap,
                                         unsigned long	*pixels,
                                         int		npixels,
                                         unsigned long	planes)
{
  int		i, j;

  /* search through list of allocated colors and decrement reference
   * count for pixels, remove unreferenced pixels. */
  for (i = 0; i < npixels; i++)
  {
    for (j = 0; j < n_allocated; j++)
      if (pixels[i] == allocated[j].pixel)
        break;
    
    if (j < n_allocated)	/* found? */
    {
      if (--allocated[j].ref < 0)
        fprintf
          (stderr,
           "XwFreeColors: pixel freed multiple times: %u\n",
           allocated[j].pixel);
    }
    else
    {
      fprintf
        (stderr,
        "XwFreeColorCells: freeing unallocated pixel: %u!\n",
        pixels[i]);
      fflush (stderr);
    }
  }

  /* pack the array of allocated cells to eliminate those just removed */
  for (i = 0, j = 0; i < n_allocated; i++)
  {
    if (allocated[i].ref <= 0)
      j++;
    else if (j > 0)
      allocated[i-j] = allocated[i];
  }
  n_allocated -= j;
      
  
  return XFreeColors (dpy, cmap, pixels, npixels, planes);
}
#endif




/* compare_pixels
 *
 *	comparison function for quicksort.
 */
int	compare_pixels	(const void *pa, const void *pb)
{
  Pixel	a = *(Pixel *)pa;
  Pixel b = *(Pixel *)pb;

  /* can't just subtract Pixels because they are unsigned */
  if (a < b) return -1;
  else return (a > b);
}


/* compare_xcolors_by_pixel
 *
 *	comparison function for quicksort.
 */
int	compare_xcolors_by_pixel	(const void *pa, const void *pb)
{
  XColor	*a = (XColor *)pa;
  XColor 	*b = (XColor *)pb;
  
  /* can't just subtract Pixels because they are unsigned */
  if (a->pixel < b->pixel) return -1;
  else return (a->pixel > b->pixel);
}


/* compare_keep_by_pixel
 *
 *	comparison function for quicksort.
 */
int	compare_keep_by_pixel	(const void *pa, const void *pb)
{
  scmKeepStruct	*a = (scmKeepStruct *)pa;
  scmKeepStruct	*b = (scmKeepStruct *)pb;
  
  /* can't just subtract Pixels because they are unsigned */
  if (a->pixel < b->pixel) return -1;
  else return (a->pixel > b->pixel);
}


/*
 * StripColorManager_getvisual
 *
 *	Fills the passed-in XVisualInfo structure with info for
 *	the visual which matches the masked template (choosing
 *	the one with greatest depth if several match), and
 *	returns the address of the buffer, or NULL pointer if
 *	no compatible visuals are available.
 */
XVisualInfo	*getvisual	(Display	*display,
                                 XVisualInfo	*pxv,
                                 long		mask)
{
  XVisualInfo	xv_template, *xvlist;
  int		n, i;

  xv_template = *pxv;
  xvlist = XGetVisualInfo (display, mask, &xv_template, &n);
  if (n <= 0)
    pxv = NULL;
  else
  {
    for (i = --n; n >= 0; n--)
      if (xvlist[n].depth > xvlist[i].depth) i = n;
    *pxv = xvlist[i];
    XFree (xvlist);
  }
  return pxv;
}


/*
 * StripColorManager_init
 */
StripColorManager	StripColorManager_init	(Display	*dpy,
						 int		n_req)
{
  SCMinfo		*scmi;
  XVisualInfo		xvi;
  XColor		*cdefs;
  Pixel			*pixels;
  int			stat;
  int			i;
  int			visual_class;
  int			n_cells;
  int			r_shift, g_shift, b_shift;
  unsigned short	r_max, g_max, b_max;

  /* get info about default visual */
  xvi.visualid = XVisualIDFromVisual (DefaultVisual (dpy, DefaultScreen(dpy)));
  if (!getvisual (dpy, &xvi, VisualIDMask))
  {
    fprintf (stderr, "unable to get info for installed visual!\n");
    return 0;
  }

  /* If we don't need writable colors, then we can just use the
   * default visual.  If it doesn't support color, too bad.
   *
   * If writable colors are requested, then one of several things
   * must happen.  If the default visual is writable, try to
   * allocate n writable cells, creating a private colormap if
   * necessary.  If the default visual is read-only, search for
   * a writable one.  If successful, create a private colormap,
   * otherwise fall back to default visual.
   */
  n_req = min (n_req, SCM_MAX_PREALLOC_CELLS);
  if (n_req > 0)	/* do we want some writable color cells? */
  {
    visual_class = StripVisualGetClass (&xvi);
    if ((visual_class != PseudoColor) && (visual_class != DirectColor))
    {
      fprintf
        (stderr,
         "Default visual does not support writable color cells.\n"
         "Looking for color visual supporting %d or more writable cells...\n",
         n_req);
    
      /* NOTE: for the following, even if we find a writable visual,
       * we will have still failed to find an adequate one if it can't
       * support the requested number of writable colorcells.
       */

      
      /* PseudoColor ? */
      StripVisualSetClass (&xvi, PseudoColor);
      if (getvisual (dpy, &xvi, VisualClassMask | VisualScreenMask))
        stat = (xvi.colormap_size >= n_req);
      else stat = 0;
      
      /* DirectColor ? */
      if (!stat)
      {
        StripVisualSetClass (&xvi, DirectColor);
        if (getvisual (dpy, &xvi, VisualClassMask | VisualScreenMask))
          stat = (xvi.colormap_size >= n_req);
        else stat = 0;

        if (stat)
        {
          fprintf
            (stderr,
             "Display supports DirectColor, but this application does not\n"
             "yet correctly deal with this visual class\n");
          stat = 0;
        }
      }

      /* failure ? */
      if (!stat)
      {
        fprintf (stderr, "No such luck!  Using default visual.\n");
        /* fall back to default visual */
        xvi.visualid = XVisualIDFromVisual (DefaultVisual (dpy, xvi.screen));
        getvisual (dpy, &xvi, VisualIDMask);
      }

      /* result */
      fprintf
        (stderr,
         "using visual %d\n"
         "depth ...... %u\n"
         "class ...... %s\n",
         xvi.visualid, xvi.depth,
         StripVisualGetClassName (StripVisualGetClass (&xvi)));
    }
  }

  /* we now know that we have a permissible visual.  Create and
   * initialize the info structure */
  if (!(scmi = (SCMinfo *)malloc (sizeof (SCMinfo))))
  {
    fprintf (stderr, "StripColorManager_init: unable to allocate memory\n");
    return 0;
  }

  scmi->display = dpy;
  scmi->xvi = xvi;
  scmi->cmap = DefaultColormap (dpy, xvi.screen);
  scmi->n_grab = 0;
  scmi->n_keep = 0;
  scmi->private_cmap = (xvi.visual != DefaultVisual (dpy, xvi.screen));
  scmi->palette_size = 0;

  visual_class = StripVisualGetClass (&xvi);
 

  /* if private colors were requested and we have a writable
   * visual then try to allocate the desired number of cells.
   * On failure, use a private colormap.
   */
  if ((n_req > 0) && StripVisualIsWritable (visual_class))
  {
    /* if we are using the default visual, and can't allocate
     * from the default colormap, we need a private colormap */
    if (xvi.visual == DefaultVisual (dpy, xvi.screen))
    {
      scmi->private_cmap = !XwAllocColorCells
        (dpy, DefaultColormap (dpy, xvi.screen), 0, 0, 0,
         scmi->grab_pix, n_req);
      if (!scmi->private_cmap) scmi->n_grab = n_req;
    }
  }

    
  /* If we need to create a private colormap, now's the time
   * to do so.
   */
  if (scmi->private_cmap)
  {
    scmi->cmap = XCreateColormap
      (dpy, RootWindow (dpy, xvi.screen), xvi.visual, AllocNone);

    /* now, if we are using a writable visual of the same depth as
     * the default visual, let's get all of the colors from the
     * default colormap and map them into all but n_req of cells
     * in the new private colormap.  This will decrease the number
     * of color cells which will display false colors when the
     * virtual colormaps are swapped in and out of hardware.  This
     * makes sense for PseudoColor visuals, but is of dubious value
     * for DirectColor :(
     */
    if ((stat = (scmi->cmap != DefaultColormap (dpy, xvi.screen))) &&
        StripVisualIsWritable (visual_class) &&
        (xvi.depth == DefaultDepth (dpy, xvi.screen)))
    {
      n_cells = xvi.colormap_size;
      
      cdefs = (XColor *)malloc (n_cells * sizeof (XColor));
      if (!cdefs) {
        fprintf (stderr, "unable to allocate memory\n");
        free (scmi);
        return 0;
      }

      pixels = (Pixel *)malloc (n_cells * sizeof (Pixel));
      if (!pixels) {
        fprintf (stderr, "unable to allocate memory\n");
        free (cdefs); free (scmi);
        return 0;
      }

      /* build array of color cell references */
      if (visual_class == DirectColor)
      {
        /* only need subset of valid pixel values in order
         * to gather all RGB values stored in pixel subfields */

        /* red bits */
        for (r_shift = 0; !((xvi.red_mask >> r_shift) & 1); r_shift++);
        r_max = (unsigned short) (xvi.red_mask >> r_shift);
        
        /* green bits */
        for (g_shift = 0; !((xvi.green_mask >> g_shift) & 1); g_shift++);
        g_max = (unsigned short) (xvi.red_mask >> g_shift);
        
        /* blue bits */
        for (b_shift = 0; !((xvi.blue_mask >> b_shift) & 1); b_shift++);
        b_max = (unsigned short) (xvi.blue_mask >> b_shift);
        
        /* now permute the various combinations of rgb values */
        for (i = 0; i < n_cells; i++)
        {
          cdefs[i].red = (i % r_max) << r_shift;
          cdefs[i].green = (i % g_max) << g_shift;
          cdefs[i].blue = (i % b_max) << b_shift;
          cdefs[i].flags = DoRed | DoGreen | DoBlue;
        }
      }
      else
      {
        /* valid range of pixel values is {0, ..., colormap_size - 1} */
        for (i = 0; i < n_cells; i++)
        {
          cdefs[i].pixel = (Pixel)i;
          cdefs[i].flags = DoRed | DoGreen | DoBlue;
        }
      }
      
      /* query the rgb colors for all pixels */
      stat = XQueryColors
        (dpy, DefaultColormap (dpy, xvi.screen), cdefs, n_cells);

      /* !!! ALERT !!! this is a hack.
       *
       * The following snippet of code assumes that a color cell
       * retains its rgb values after it has been freed by the
       * application.  This assumption makes sense, but is not
       * actually defined anywhere, and is therefore not robust.
       *
       * There seems to be no straight-forward way to copy a colormap
       * without allocating all of its cells non-sharable.  At first,
       * this doesn't seem like a problem --"this application is using
       * a private colormap, so why should it care whether all the color
       * cells are non-sharable?"  However, the motif library wants to
       * use the Xlib color routines to get pixel values for default
       * colors (e.g. XmNbackground), and hence the problem arises.
       *
       * hack: Allocate all colorcells in the private colormap, then
       * write the corresponding values from the old colormap.  After
       * this has been accomplished, free all but the topmost n_req cells,
       * and then reallocate them as sharable.
       */
      if (stat)
        stat = XwAllocColorCells (dpy, scmi->cmap, False, 0, 0, pixels, n_cells);
      if (stat) stat = XStoreColors (dpy, scmi->cmap, cdefs, n_cells);
      if (stat)
      {
        /* make sure the pixels are sorted in ascending order so that when
         * we free the first cells in the array, we are actually freeing the
         * bottom cells from the colormap */
        qsort (pixels, n_cells, sizeof(Pixel), compare_pixels);
        stat = XwFreeColors (dpy, scmi->cmap, pixels, n_cells - n_req, 0);

        for (i = 0; i < (n_cells - n_req); i ++)
          XwAllocColor (dpy, scmi->cmap, &cdefs[i]);
      }
      
      if (stat)
      {
        /* pre-allocate the topmost n_req color cells */
        for (i = 0; i < n_req; i++)
          scmi->grab_pix[i] = cdefs[n_cells - i - 1].pixel;
        scmi->n_grab = n_req;
      }
      
      free (cdefs);
      free (pixels);
    }


    /* if stat is false, then our operation was unsuccessful.
     * fall back to the default visual */
    if (!stat)
    {
      fprintf
        (stderr,
         "Sorry, unable to create private colormap.\n"
         "Falling back to default visual and default colormap\n");
      
      xvi.visualid = XVisualIDFromVisual (DefaultVisual (dpy, xvi.screen));
      getvisual (dpy, &xvi, VisualIDMask);
      scmi->xvi = xvi;
      scmi->cmap = DefaultColormap (dpy, xvi.screen);
      scmi->n_grab = 0;
      scmi->private_cmap = 0;
    }
  }
  
  return (StripColorManager)scmi;
}



/*
 * StripColorManager_delete
 */
void	StripColorManager_delete	(StripColorManager the_scm)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  Pixel		pixels[SCM_MAX_PALETTE_SIZE];
  int		i;

  if (scmi->private_cmap)
    XFreeColormap (scmi->display, scmi->cmap);
  else if (scmi->n_grab > 0)
    XwFreeColors (scmi->display, scmi->cmap, scmi->grab_pix, scmi->n_grab, 0);

  if (scmi->palette_size > 0)
  {
    for (i = 0; i < scmi->palette_size; i++)
      pixels[i] = scmi->palette[i].pixel;
    XwFreeColors (scmi->display, scmi->cmap, pixels, i, 0);
  }

  if (scmi->n_keep > 0)
  {
    for (i = 0; i < scmi->n_keep; i++)
      pixels[i] = scmi->keep[i].pixel;
    XwFreeColors (scmi->display, scmi->cmap, pixels, i, 0);
  }

  free (scmi);
}


/*
 * StripColorManager_build_palette
 */
int	StripColorManager_build_palette	(StripColorManager the_scm,
                                         XColor 	   *buf,
                                         int		   n_max)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  int		visual_class;
  int		i, j, k;
  int		stat;
  int		n_got;
  int		n_cells, n_writable, n_xcolors;
  Pixel		*cells;
  Pixel		pix;
  XColor	*xcolors;
  XColor	xcolor;

  n_max = min (SCM_MAX_PALETTE_SIZE, n_max);
  
  /* ok, we need to build a palette (an ordered assortment of color
   * cells containing a wide variety of colors), comprised of readable
   * but not writable cells contained in the colormap.  If we are
   * using a read-only visual, we just need to select an appropriate
   * assortment of colors.  If, however, we are using a writable visual
   * and a non-private colormap, then we must gather only those cells
   * which are sharable, but not writable (don't want to take potentially
   * writable colorcells away from other apps).
   */

  /* for the time-being, just grab any old color cells.  I'll put
   * in some more sophisticated color selection maximizing contrast
   * once I've got everything else working :) */
  visual_class = StripVisualGetClass (&scmi->xvi);

  /* read-only, or virtual ? */
  if (!StripVisualIsWritable (visual_class) || scmi->private_cmap)
  {
    n_cells = 1 << scmi->xvi.depth;
    n_got = min (n_max, n_cells);

    if (scmi->palette_size != n_got)
    {
      /* for simplicity, break the range of all pixel values into
       * n_got bins, and randomly select a pixel value from within
       * each */
      for (i = 0; i < n_got; i++)
        scmi->palette[i].pixel = (Pixel)
          ((i + (rand()/(float)RAND_MAX)) * (n_cells / (float)n_got));
      
      XQueryColors (scmi->display, scmi->cmap, scmi->palette, n_got);
      scmi->palette_size = n_got;
    }
    if (buf) memcpy (buf, scmi->palette, n_got * sizeof(XColor));
  }

  /* writable ?
   *
   * get only sharable and non-writable pixel values.  This is
   * a little tricky, since this information is not readily
   * available from the server.  We'll first allocate all free
   * writable cells.  Next, run through all the cells in the
   * colormap.  For each of these, if it is not writable (if
   * we can't find it in the list of writable color cells),
   * and isn't already in use, then try to allocate a color cell
   * with the same RGB values from the colormap.  If the cell
   * returned is the same as the reference cell, then we know
   * it's sharable, so put it in the palette, otherwise move on.
   *
   * The specifics vary depending on visual class.
   */
  else
  {
#if 1
    /* first grab as many writable cells as possible.  Need to allocate
     * memory for array of cells then call XAllocColorCells repeatedly
     * until the max number of available cells is found.  Kind of clunky! */
    n_cells = scmi->xvi.colormap_size;
    cells = (Pixel *)malloc (n_cells * sizeof(Pixel));
    if (!cells) {
      fprintf (stderr, "StripColorManager_build_palette: out of memory!\n");
      return 0;
    }
    /* Linear search now --replace with binary search later */
    for (i = n_cells; i > 0; i--)
      if (XwAllocColorCells (scmi->display, scmi->cmap, False, 0, 0, cells, i))
        break;
    n_writable = i;

    /* now build an assortment of non-writable pixels referenced by
     * xcolors --particulars depend upon visual type */
    if (visual_class == DirectColor)
    {
      fprintf
        (stderr, "Sorry, DirectColor palette is not yet fully implemented.\n");
      return 0;
    }
    else 	/* most likely PseudoColor */
    {
      /* sort the resulting pixels in ascending order */
      if (n_writable > 0)
        qsort (cells, n_writable, sizeof (Pixel), compare_pixels);

      /* built array of non-writable pixels */
      n_xcolors = n_cells - n_writable;
      xcolors = (XColor *)malloc (n_xcolors * sizeof (XColor));
      if (!xcolors) {
        fprintf (stderr, "StripColorManager_build_palette: out of memory!\n");
        free (cells);
        return 0;
      }

      for (pix = 0, i = 0, j = 0; pix < n_xcolors; pix++)
      {
        if (i < n_writable)
          if (pix == cells[i])
          {
            i++;
            continue;
          }
        xcolors[j++].pixel = pix;
      }
    }

    
    /* get rgb values for referenced pixels */
    XQueryColors (scmi->display, scmi->cmap, xcolors, n_xcolors);
    
    /* now, for each unwritable cell, attempt to allocate a pixel
     * with the same rgb values.  If the returned pixel references
     * the current cell, then it's sharable, so move on to the next;
     * otherwise (not sharable), swap it with the last unchecked
     * pixel.  In this way, all sharable colors will accumulate at
     * the front of the array, and all unsharable pixels at the end,
     * and i will end up holding the number of sharable cells.
     */

    /* i is index of first uninvestigated color, j is index of last */
    i = 0; j = n_xcolors - 1;
    while (i <= j)
    {
      /* color cell under consideration */
      xcolor = xcolors[i];
      stat = 0;

      /* if the color cell is already in use by this application,
       * and is not writable, then it's good --no need for further
       * investigation.  This search can get potentially time-consuming,
       * since it's executed for each color cell under investigation.
       * There's probably some better way, but this should suffice
       * for the time-being. */
      for (k = 0; k < scmi->n_keep; k++)
        if (xcolor.pixel == scmi->keep[k].pixel)
        {
          stat = !scmi->keep[k].writable;
          break;
        }

      if (!stat && (k == scmi->n_keep))
        if (stat = XwAllocColor (scmi->display, scmi->cmap, &xcolor))
          if (!(stat = (xcolor.pixel == xcolors[i].pixel)))
            /* we allocated a color defined by the RGB values in
             * the current color cell, but got a different color cell!
             * Fortunately, the X server maintains a reference count
             * for all allocated cells (see X Protocol reference for
             * XFreeColors), so we can go ahead and free this one without
             * having to worry abount invalidating the other references
             * to the same cell */
            XwFreeColors (scmi->display, scmi->cmap, &xcolor.pixel, 1, 0);
      
      if (!stat)	/* swap ith item for jth item, decrement j */
      {
        xcolor = xcolors[i];
        xcolors[i] = xcolors[j];
        xcolors[j] = xcolor;
        j--;
      }
      else i++;		/* move to next item */
    }

    /* if there are more sharable colormap entries than requested,
     * we need to get rid of some.  For the time being, just grab
     * the first however many */
    n_got = min (i, n_max);
    memcpy (scmi->palette, xcolors, n_got * sizeof(XColor));
    if (buf) memcpy (buf, scmi->palette, n_got * sizeof(XColor));
    scmi->palette_size = n_got;

    /* clean up --free any allocated writable cells, any unused
     * shared colors and temporarily allocated memory */
    if (n_writable > 0)
      XwFreeColors (scmi->display, scmi->cmap, cells, n_writable, 0);
    if (n_got < i)
    {
      i -= n_got;

      /* must free i cells starting from the one indexed by n_got in the
       * xcolors array.  Might as well use the cells array to free multiple
       * pixels at once, rather than just one at a time. */
      k = n_got;
      do
      {
        for (j = 0; (j < i) && (j < n_cells); j++)
          cells[j] = xcolors[k+j].pixel;
        XwFreeColors (scmi->display, scmi->cmap, cells, j, 0);
        k += j;
      }
      while (k < i);
    }
    free (xcolors);
    free (cells);
    

#else	/* ======================================================*/
    if (visual_class == DirectColor)
    {
      fprintf (stderr, "Shit out of luck.\n");
      return 0;
    }
    else 	/* most likely PseudoColor */
    {
      n_cells = scmi->xvi.colormap_size;
      cells = (Pixel *)malloc (n_cells * sizeof(Pixel));
      if (!cells) {
        fprintf (stderr, "StripColorManager_build_palette: out of memory!\n");
        return 0;
      }
      xcolors = (XColor *)malloc (n_cells * sizeof (XColor));
      if (!xcolors) {
        fprintf (stderr, "StripColorManager_build_palette: out of memory!\n");
        free (cells);
        return 0;
      }
    }

    /* allocate as many writable cells as possible.  Linear search now
     * --replace with binary search later */
    for (i = n_cells; i > 0; i--)
      if (XwAllocColorCells (scmi->display, scmi->cmap, False, 0, 0, cells, i))
        break;

    /* sort the resulting pixels in ascending order */
    if ((n_writable = i) > 0)
      qsort (cells, n_writable, sizeof (Pixel), compare_pixels);

    /* now, query the rgb values for all unwritable pixels.
     * i gets reset to 0, and indexes into the array of
     * writable color cells, for comparing against pixel
     * values to be investigated for sharability */
    for (pix = 0, i = 0, j = 0; pix < n_cells; pix++)
    {
      if (i < n_writable)
        if (pix == cells[i])
        {
          i++;
          continue;
        }
      xcolors[j++].pixel = pix;
    }
    n_cells -= n_writable;	/* should be equivalent to n_cells = j */
    XQueryColors (scmi->display, scmi->cmap, xcolors, n_cells);

    
    /* now, for each unwritable cell, attempt to allocate a pixel
     * with the same rgb values.  If the returned pixel references
     * the current cell, then it's sharable, so move on to the next;
     * otherwise (not sharable), swap it with the last unchecked
     * pixel.  In this way, all sharable colors will accumulate at
     * the front of the array, and all unsharable pixels at the end,
     * and i will end up holding the number of sharable cells.
     */

    /* i is index of first uninvestigated color, j is index of last */
    i = 0; j = n_cells - 1;
    while (i <= j)
    {
      /* color cell under consideration */
      xcolor = xcolors[i];
      stat = 0;

      /* if the color cell is already in use by this application,
       * and is not writable, then it's good --no need for further
       * investigation.  This search can get potentially time-consuming,
       * since it's executed for each color cell under investigation.
       * There's probably some better way, but this should suffice
       * for the time-being. */
      for (k = 0; k < scmi->n_keep; k++)
        if (xcolor.pixel == scmi->keep[k].pixel)
        {
          stat = !scmi->keep[k].writable;
          break;
        }

      if (!stat && (k == scmi->n_keep))
        if (stat = XwAllocColor (scmi->display, scmi->cmap, &xcolor))
          if (!(stat = (xcolor.pixel == xcolors[i].pixel)))
            /* we allocated a color defined by the RGB values in
             * the current color cell, but got a different color cell!
             * Fortunately, the X server maintains a reference count
             * for all allocated cells (see X Protocol reference for
             * XFreeColors), so we can go ahead and free this one without
             * having to worry abount invalidating the other references
             * to the same cell */
            XwFreeColors (scmi->display, scmi->cmap, &xcolor.pixel, 1, 0);
      
      if (!stat)	/* swap ith item for jth item, decrement j */
      {
        xcolor = xcolors[i];
        xcolors[i] = xcolors[j];
        xcolors[j] = xcolor;
        j--;
      }
      else i++;		/* move to next item */
    }

    /* if there are more sharable colormap entries than requested,
     * we need to get rid of some.  For the time being, just grab
     * the first however many */
    n_got = min (i, n_max);
    memcpy (scmi->palette, xcolors, n_got * sizeof(XColor));
    if (buf) memcpy (buf, scmi->palette, n_got * sizeof(XColor));
    scmi->palette_size = n_got;

    /* clean up --free any allocated writable cells, any unused
     * shared colors and temporarily allocated memory */
    if (n_writable > 0)
      XwFreeColors (scmi->display, scmi->cmap, cells, n_writable, 0);
    if (n_got < i)
    {
      i -= n_got;
      for (j = 0; j < i; j++)
        cells[j] = xcolors[n_got+j].pixel;
      XwFreeColors (scmi->display, scmi->cmap, cells, j, 0);
    }
    free (xcolors);
    free (cells);
#endif
  }

  return n_got;
}


/*
 * StripColorManager_free_palette
 */
void	StripColorManager_free_palette	(StripColorManager the_scm)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  int		visual_class;
  int		i, j, k;
  static Pixel	pixels[SCM_MAX_KEEP_CELLS];

  /* don't do anything if the current visual is read-only or if
   * we are using a private colormap: destroying the palette and
   * freeing the colors would just be a waste of time */
  visual_class = StripVisualGetClass(&scmi->xvi);
  if (StripVisualIsWritable (visual_class) && !scmi->private_cmap)
  {
    /* sort the palette and used array */
    qsort
      (scmi->palette, scmi->palette_size, sizeof(XColor),
       compare_xcolors_by_pixel);
    qsort
      (scmi->keep, scmi->n_keep, sizeof(scmKeepStruct), compare_keep_by_pixel);
    
    /* remove the in-use colors from the palette */
    j = 0; k = 0;
    for (i = 0; i < scmi->palette_size; i++)
    {
      /* move kept-pixel index (j) forward to reference smallest pixel value
       * greater than or equal to that referenced by palette index (i) */
      while (j < scmi->n_keep)
        if (scmi->keep[j].pixel < scmi->palette[i].pixel)
          j++;
        else break;

      /* if current pixel is in the keep list, don't add it to the list
       * to be freed */
      if (scmi->palette[i].pixel != scmi->keep[j].pixel)
        pixels[k++] = scmi->palette[i].pixel;
    }
    
    if (k > 0) XwFreeColors (scmi->display, scmi->cmap, pixels, k, 0);
    scmi->palette_size = 0;
  }
}


/*
 * StripColorManager_make_color
 */
int	StripColorManager_make_color	(StripColorManager	the_scm,
                                         StripColor 		*sc,
                                         char 			*cname,
                                         unsigned		mode)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  XColor	dummy, black, white;
  int		visual_class;
  int		stat = 1;
  float		dist_min, dist;
  int		idx_min;
  int		i;
  int		r0, g0, b0;
  float		r, g, b;

  /* look up the RGB values from the name, if required */
  if (mode & SCM_MATCH_STR)
    stat = XLookupColor
      (scmi->display, scmi->cmap, cname, &dummy, &sc->xcolor);
  if (!stat) return 0;
  sc->xcolor.flags = DoRed | DoGreen | DoBlue;

  visual_class = StripVisualGetClass (&scmi->xvi);
  
  /*writable color */
  if ((mode & SCM_RW_TRY) && StripVisualIsWritable (visual_class))
  {
    if (scmi->n_grab > 0)
      sc->xcolor.pixel = scmi->grab_pix[--scmi->n_grab];
    else if (((mode & SCM_RW_FORCE) == SCM_RW_FORCE) && !scmi->private_cmap)
      stat = XwAllocColorCells
        (scmi->display, scmi->cmap, False, 0, 0, &sc->xcolor.pixel, 1);
    else stat = 0;

    if (stat) XStoreColor (scmi->display, scmi->cmap, &sc->xcolor);
    sc->writable = 1;
  }

  /* read-only color */
  else if (mode & SCM_RO)
  {
    /* dynamic visual ? */
    if (StripVisualIsWritable (visual_class))
    {
      /* need to find closest match in the current palette.  Do this
       * by finding the color entry whose position in the color cube
       * is closest to the requested point.  That is, find the point
       * with the smallest cartesion distance from the requested point.
       *
       * This method doesn't guarantee the "best" fit, because color
       * perception is not linearly related to intensity values, though
       * this does provide a suitable alternative in lieu of a more
       * sophisticated approach.
       */
      if (scmi->palette_size > 0)
      {
        r0 = sc->xcolor.red; g0 = sc->xcolor.green; b0 = sc->xcolor.blue;
        r = scmi->palette[0].red - r0;
        g = scmi->palette[0].green - g0;
        b = scmi->palette[0].blue - b0;

        dist_min = sqrt (r*r + g*g + b*b);
        idx_min = 0;

        for (i = 1; (i < scmi->palette_size) && (dist_min != 0); i++)
        {
          r = scmi->palette[i].red - r0;
          g = scmi->palette[i].green - g0;
          b = scmi->palette[i].blue - b0;
          dist = sqrt (r*r + g*g + b*b);

          if (dist < dist_min)
          {
            idx_min = i;
            dist_min = dist;
          }
        }

        sc->xcolor = scmi->palette[idx_min];
      }

      /* if the palette is currently undefined, return either black
       * or white, whichever is closer to requested color */
      else
      {
        black.pixel = BlackPixel (scmi->display, scmi->xvi.screen);
        white.pixel = WhitePixel (scmi->display, scmi->xvi.screen);
        XQueryColor (scmi->display, scmi->cmap, &black);
        XQueryColor (scmi->display, scmi->cmap, &white);

        r = black.red - sc->xcolor.red;
        g = black.green - sc->xcolor.green;
        b = black.blue - sc->xcolor.blue;
        dist = sqrt ((double)r*r + g*g + b*b);

        r = white.red - sc->xcolor.red;
        g = white.green - sc->xcolor.green;
        b = white.blue - sc->xcolor.blue;

        if (sqrt (r*r + g*g + b*b) < dist)
          sc->xcolor = white;
        else sc->xcolor = black;
      }
    }

    /* static visual */
    else stat = XwAllocColor (scmi->display, scmi->cmap, &sc->xcolor);

    sc->writable = 0;
  }

  /* error */
  else stat = 0;

  return stat;
}


/*
 * StripColorManager_keep_color
 */
void	StripColorManager_keep_color	(StripColorManager 	the_scm,
                                         StripColor		*sc)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  int		i;
  int		found;

#if defined (DEBUG_COLORALLOC)
  fprintf (stderr, "+++ keep_color: [%0.3u]\n", sc->xcolor.pixel);
#endif

  /* first, look for the color cell in the (ordered) keep-list */
  found = 0;
  for (i = 0; i < scmi->n_keep; i++)
  {
    if (found = (scmi->keep[i].pixel == sc->xcolor.pixel))
      break;
    else if (scmi->keep[i].pixel > sc->xcolor.pixel)
      break;
  }

  /* i is now the index of the requested color in the palette,
   * or the position where it should be if the color was not found.
   *
   * if the color was found, increment its reference count.
   *
   * if the color was not found, add it if there's room in the
   * palette.
   */
  if (found)
  {
    scmi->keep[i].ref++;
  }
  else if (scmi->n_keep < SCM_MAX_KEEP_CELLS)
  {
    /* shift all elements from position i over one slot */
    memmove
      (&scmi->keep[i+1], &scmi->keep[i],
       (scmi->n_keep-i+1) * sizeof (scmKeepStruct));
    scmi->keep[i].pixel = sc->xcolor.pixel;
    scmi->keep[i].writable = sc->writable;
    scmi->keep[i].ref = 1;
    scmi->n_keep++;
  }
  else fprintf (stderr, "StripColorManager_keep_color: buffer overflow!\n");

#if defined (DEBUG_COLORALLOC)
  fprintf (stderr, "+++ KEEP LIST +++\n");
  for (i = 0; i < scmi->n_keep; i++)
    fprintf
      (stderr, "cell [%0.3u], ref [%d]\n",
       scmi->keep[i].pixel, scmi->keep[i].ref);
  fprintf (stderr, ".................\n");
#endif
}


/*
 * StripColorManager_free_color
 */
void	StripColorManager_free_color	(StripColorManager 	the_scm,
                                         StripColor		*sc)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  int		i;
  int		found_in_keep, found_in_palette;

#if defined (DEBUG_COLORALLOC)
  fprintf (stderr, "--- free_color: [%0.3u]\n", sc->xcolor.pixel);
#endif
  
  /* first look in the palette */
  found_in_palette = 0;
  for (i = 0; i < scmi->palette_size; i++)
    if (found_in_palette = (sc->xcolor.pixel == scmi->palette[i].pixel))
      break;

  /* now, look for the color cell in the (ordered) keep list */
  found_in_keep = 0;
  for (i = 0; i < scmi->n_keep; i++)
  {
    if (found_in_keep = (sc->xcolor.pixel == scmi->keep[i].pixel))
      break;
    else if (scmi->keep[i].pixel > sc->xcolor.pixel)
      break;
  }

  /* i is now the index of the requested color in the keep[] array,
   * if it was found.
   *
   * if the color was found, decrement its reference count, if
   * this becomes zero, remove it from the keep list and, if
   * it is not in the palette, free the cell.
   */
  if (found_in_keep)
  {
    if (--scmi->keep[i].ref == 0)
    {
      /* don't bother calling XFreeColors if the visual is static,
       * or if we are using a private clormap.
       * Also, if the cell is in the palette, it'll get freed along with
       * the palette */
      if (StripVisualIsWritable (StripVisualGetClass (&scmi->xvi)) &&
          !scmi->private_cmap &&
          !found_in_palette)
        XwFreeColors (scmi->display, scmi->cmap, &scmi->keep[i].pixel, 1, 0);

      /* shift all elements from position i+1 back one slot */
      memmove
        (&scmi->keep[i], &scmi->keep[i+1],
         (scmi->n_keep-i) * sizeof (scmKeepStruct));
      scmi->n_keep--;
    }
  }
  else fprintf (stderr, "StripColorManager_free_color: color not found!\n");

#if defined (DEBUG_COLORALLOC)
  fprintf (stderr, "--- KEEP LIST ---\n");
  for (i = 0; i < scmi->n_keep; i++)
    fprintf
      (stderr, "cell [%0.3u], ref [%d]\n",
       scmi->keep[i].pixel, scmi->keep[i].ref);
  fprintf (stderr, ".................\n");
#endif
}


/*
 * StripColorManager_change_color
 */
int	StripColorManager_change_color	(StripColorManager 	the_scm,
                                         StripColor		*sc)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  int		ret;

  if (ret = StripVisualIsWritable (StripVisualGetClass (&scmi->xvi)))
    if (ret = sc->writable)
    {
      sc->xcolor.flags = DoRed | DoGreen | DoBlue;
      ret = XStoreColor (scmi->display, scmi->cmap, &sc->xcolor);
    }

  return ret;
}



/*
 * get_visualinfo
 *
 *	Returns a pointer to the current XVisualInfo structure.
 */
XVisualInfo	*StripColorManager_get_visinfo 	(StripColorManager the_scm)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  return &scmi->xvi;
}


/*
 * StripColorManager_getcmap
 */
Colormap	StripColorManager_getcmap	(StripColorManager the_scm)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  return scmi->cmap;
}
  
/*
 * StripColorManager_readonly
 */
int		StripColorManager_readonly	(StripColorManager the_scm)
{
  SCMinfo	*scmi = (SCMinfo *)the_scm;
  int		visual_class;

  visual_class = StripVisualGetClass (&scmi->xvi);
  return (!StripVisualIsWritable (visual_class) || scmi->private_cmap);
}

