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

#include "cColorManager.h"
#include "StripConfig.h"

#include <Xm/Xm.h>
#include <X11/Xlib.h>

/* ======= Data Types ======= */
typedef void	(*CDcallback)	(void *, cColor *);	/* client, call data */
typedef void *	ColorDialog;

ColorDialog	ColorDialog_init	(Widget,		/* parent */
					 char *,		/* title */
					 StripConfig *);
void		ColorDialog_delete	(ColorDialog);
void		ColorDialog_popup	(ColorDialog,
					 char *,
					 cColor *,
					 CDcallback, /* called on Ok, Apply */
					 void *);    /* callback data */
void		ColorDialog_popdown	(ColorDialog);

#endif
