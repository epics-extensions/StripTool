/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _ColorDialog
#define _ColorDialog

#include <Xm/Xm.h>
#include <X11/Xlib.h>

typedef void *	ColorDialog;

ColorDialog	ColorDialog_init	(Display *, Colormap, char *);
void		ColorDialog_delete	(ColorDialog);
void		ColorDialog_popup	(ColorDialog, char *, Pixel);
void		ColorDialog_popdown	(ColorDialog);

#endif
