/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripColorManager
#define _StripColorManager

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>

#define SCM_MAX_PREALLOC_CELLS	16
#define SCM_MAX_KEEP_CELLS	256
#define SCM_MAX_PALETTE_SIZE	256


typedef struct _StripColor
{
  XColor	xcolor;
  short		writable;	/* true if access is read-write */
}
StripColor;


typedef	void *	StripColorManager;

/*
 * init
 *
 *	Allocates memory for the internal structure and finds a
 *	suitable visual from which the specified number of writable
 *	color cells can be allocated.  If the number of requested color
 *	cells is zero, then a read only visual may be used.  If the
 *	requested number of writable cells cannot be allocated because
 *	there are not enough available in the default colormap, then a
 *	private colormap will be created.  If a writable visual cannot
 *	be found then a read-only one will be used and no writable color
 *	cells allocated.
 */
StripColorManager	StripColorManager_init	(Display *,
                                                 int);	/* max writable */


/*
 * delete
 *
 *	Frees all resources allocated to support the color manager.
 */
void	StripColorManager_delete	(StripColorManager);


/*
 * build_palette
 *
 *	Allocates from the X server as many sharable (and non-writable)
 *	color-cells as possible, but no more than max.  The return value
 *	is the number of cells actually allocated.  The palette is written
 *	into the user-allocated buffer the supplied pointer is not
 *	null, and a local copy is also maintained.
 */
int	StripColorManager_build_palette	(StripColorManager,
                                         XColor *,	/* buffer */
                                         int);		/* max */


/*
 * free_palette
 *
 *	Frees all unused color cells previously allocated by build_palette.
 *	That is, every color cell which is not referenced in the table of
 *	used colors is returned to the server.
 */
void	StripColorManager_free_palette	(StripColorManager);


/*
 * make_color
 *
 *	this routine generates a usable color, and stores it in the
 *	caller-supplied StripColor structure, returning true on success,
 *	false otherwise.  The mode mask allows the caller to specify
 *	what type of color to get (read-only or writable) and how to get
 *	it.  This mask is the bitwise OR of several constants.
 *
 *	One of the following must be specified:
 *
 *	SCM_RW_TRY:	get a writable cell, but fail if
 *			none of the pre-allocated ones are left.
 *
 *	SCM_RW_FORCE:	get a writable cell, even if this
 *			involves requesting one from the X server.
 *			Fail if none are available, period.
 *
 *	SCM_RO:		static visual:	request a color from the X server
 *			dynamic visual:	get a color from the current palette.
 *					Note that if no palette is currently
 *					defined, either black or white will
 *					be returned, depending on the
 *					intensity of the requested color.
 *
 *	And one of the following must be specified:
 *
 *	SCM_MATCH_STR:	use the passed in string to lookup values for
 *			returned color.
 *
 *	SCM_MATCH_RGB:	use the RGB values in the passed in StripColor
 *			structure to find a suitable color.
 */
#define SCM_RW_TRY	(unsigned)1	/* 00001 */
#define SCM_RW_FORCE	(unsigned)3	/* 00011 */
#define SCM_RO		(unsigned)4	/* 00100 */
#define SCM_MATCH_STR	(unsigned)8	/* 01000 */
#define SCM_MATCH_RGB	(unsigned)16	/* 10000 */

int	StripColorManager_make_color	(StripColorManager,
                                         StripColor *,	/* in/out */
                                         char *,	/* name */
                                         unsigned);	/* mode */


/*
 * keep_color
 *
 *	Marks the given color "in use" so that it will not be returned
 *	to the server when the current palette is freed.  If the color
 *	is already marked as "in-use" then its reference count is upped.
 */
void	StripColorManager_keep_color	(StripColorManager, StripColor *);


/*
 * free_color
 *
 *	Decrements the number of references to the given color.  When
 *	the reference count reaches 0, the color is returned to the server
 */
void	StripColorManager_free_color	(StripColorManager, StripColor *);


/*
 * change_color
 *
 *	Updates the colormap entry for the given color to reflect
 *	its precise RGB values.  Returns true on success, false
 *	otherwise (if XStoreColor fails or the color is not writable).
 */
int	StripColorManager_change_color	(StripColorManager, StripColor *);


/*
 * get_visinfo
 *
 *	Returns a pointer to the current XVisualInfo structure.
 */
XVisualInfo	*StripColorManager_get_visinfo	(StripColorManager);


/*
 * getcmap
 */
Colormap	StripColorManager_getcmap	(StripColorManager);


/*
 * readonly
 *
 *	Returns true iff the associated colormap is readonly.  This
 *	will be the case when the visual is static, or if the
 *	colormap is private.
 */
int		StripColorManager_readonly	(StripColorManager);


#endif
