/*-----------------------------------------------------------------------------
 * Copyright (c) 1996 Southeastern Universities Research Association,
 *               Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 *-----------------------------------------------------------------------------
 */


#ifndef _StripCDEV
#define _StripCDEV

#include "Strip.h"
#include "StripCurve.h"
#include "StripDefines.h"
#include "StripMisc.h"

typedef void *	StripCDEV;

/*
 * StripCDEV_initialize
 *
 *	Initializes cdev and allocates internal resources.
 */
StripCDEV	StripCDEV_initialize	(Strip);


/*
 * StripCDEV_terminate
 *
 *	Frees cdev and internal resources allocated by initialize().
 */
void		StripCDEV_terminate	(StripCDEV);



/*
 * StripCDEV_request_connect
 *
 *	Requests that the signal specified in the StripCurve be connected
 *	to a cdev device.
 */
int		StripCDEV_request_connect	(StripCurve, StripCDEV);


/*
 * StripCDEV_request_disconnect
 *
 *	Requests that the signal specified in the StripCurve be disconnected
 *	from its cdev device.
 */
int		StripCDEV_request_disconnect	(StripCurve, StripCDEV);

#endif
